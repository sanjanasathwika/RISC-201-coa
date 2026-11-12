#pragma once
#include "isa.hpp"
#include "alu.hpp"
#include "memory.hpp"
#include "control.hpp"
#include "stack.hpp"
#include "branch_predictor.hpp"
#include "cache.hpp"
#include <array>
#include <set>
#include <cstdint>
#include <string>
#include <vector>

namespace risc201 {

enum class Pipeline { P4 = 4, P6 = 6 };

// Pipeline register (data passed between stages)
struct PipelineRegister {
    Instruction instr{};
    uint32_t pc = 0;
    int32_t alu_result = 0;
    int32_t mem_data = 0;
    int32_t rs1_val = 0, rs2_val = 0;
    int32_t write_data = 0;
    uint8_t write_reg = 0;
    ControlSignals ctrl{};
    bool valid = false;
    bool bubble = false;
    bool predicted_taken = false;
    uint32_t predicted_target = 0;
};

struct Stats {
    uint64_t instructions = 0, cycles = 0, stalls = 0;
    uint64_t branches = 0, taken = 0;
    uint64_t memReads = 0, memWrites = 0;
    uint64_t aluOps = 0, microOps = 0;
    uint64_t hazards_detected = 0, forwards = 0;
    uint64_t pipeline_flushes = 0;
    uint64_t branch_predictions = 0;
    uint64_t branch_hits = 0;
    uint64_t branch_misses = 0;
    uint64_t store_to_load_forwards = 0;
};

// 4-Entry Hardware Store Buffer with Store-to-Load Forwarding
struct StoreBufferEntry {
    uint32_t addr = 0;
    int32_t value = 0;
    bool valid = false;
};

class StoreBuffer {
    static constexpr size_t QUEUE_SIZE = 4;
    std::array<StoreBufferEntry, QUEUE_SIZE> entries_{};
    size_t head_ = 0, tail_ = 0, count_ = 0;
    uint64_t forwards_ = 0;

public:
    bool is_full() const { return count_ == QUEUE_SIZE; }
    bool is_empty() const { return count_ == 0; }
    size_t size() const { return count_; }
    uint64_t get_forwards() const { return forwards_; }

    // Enqueue store into FIFO.
    // If full, automatically drains the oldest entry to cache/memory if provided,
    // guaranteeing no silent store loss. If draining is not possible, safely prevents loss.
    bool push(uint32_t addr, int32_t value, L1Cache* cache = nullptr, Memory* mem = nullptr, uint64_t* mem_writes = nullptr) {
        if (count_ == QUEUE_SIZE) {
            if (mem != nullptr) {
                // Drain oldest entry at head so it is never lost
                if (entries_[head_].valid) {
                    if (cache != nullptr) {
                        cache->write32(entries_[head_].addr, static_cast<uint32_t>(entries_[head_].value), *mem);
                    } else {
                        mem->write32(entries_[head_].addr, static_cast<uint32_t>(entries_[head_].value));
                    }
                    if (mem_writes) (*mem_writes)++;
                }
                entries_[head_].valid = false;
                head_ = (head_ + 1) % QUEUE_SIZE;
                count_--;
                while (count_ > 0 && !entries_[head_].valid) {
                    head_ = (head_ + 1) % QUEUE_SIZE;
                    count_--;
                }
            } else {
                // Backing memory is not provided: draining cannot be performed safely.
                // If this is an overwrite to an address already in the buffer, update it in place.
                for (size_t i = 0; i < count_; ++i) {
                    size_t idx = (tail_ + QUEUE_SIZE - 1 - i) % QUEUE_SIZE;
                    if (entries_[idx].valid && entries_[idx].addr == addr) {
                        entries_[idx].value = value;
                        return true;
                    }
                }
                // Cannot drain and address is not present: fail rather than discarding pending data.
                return false;
            }
        }
        entries_[tail_] = {addr, value, true};
        tail_ = (tail_ + 1) % QUEUE_SIZE;
        count_++;
        return true;
    }

    // Lookup most recent store to addr (newest to oldest for repeated writes)
    bool lookup(uint32_t addr, int32_t& value) {
        for (size_t i = 0; i < count_; ++i) {
            size_t idx = (tail_ + QUEUE_SIZE - 1 - i) % QUEUE_SIZE;
            if (entries_[idx].valid && entries_[idx].addr == addr) {
                value = entries_[idx].value;
                forwards_++;
                return true;
            }
        }
        return false;
    }

    // Retire entry matching addr (FIFO order from head), retrieving its value
    bool retire(uint32_t addr, int32_t& value) {
        for (size_t i = 0; i < count_; ++i) {
            size_t idx = (head_ + i) % QUEUE_SIZE;
            if (entries_[idx].valid && entries_[idx].addr == addr) {
                value = entries_[idx].value;
                entries_[idx].valid = false;
                // Clean up retired entries at head
                while (count_ > 0 && !entries_[head_].valid) {
                    head_ = (head_ + 1) % QUEUE_SIZE;
                    count_--;
                }
                return true;
            }
        }
        return false;
    }

    bool retire(uint32_t addr) {
        int32_t dummy = 0;
        return retire(addr, dummy);
    }

    // Drain oldest entry from head (drain FIFO)
    bool drain_one(uint32_t& addr, int32_t& val) {
        while (count_ > 0 && !entries_[head_].valid) {
            head_ = (head_ + 1) % QUEUE_SIZE;
            count_--;
        }
        if (count_ == 0) return false;
        addr = entries_[head_].addr;
        val = entries_[head_].value;
        entries_[head_].valid = false;
        head_ = (head_ + 1) % QUEUE_SIZE;
        count_--;
        return true;
    }

    // Drain all pending entries to cache/memory
    void drain_all(L1Cache& cache, Memory& mem, uint64_t* mem_writes = nullptr) {
        uint32_t addr = 0;
        int32_t val = 0;
        while (drain_one(addr, val)) {
            cache.write32(addr, static_cast<uint32_t>(val), mem);
            if (mem_writes) (*mem_writes)++;
        }
    }

    void drain_all(Memory& mem, uint64_t* mem_writes = nullptr) {
        uint32_t addr = 0;
        int32_t val = 0;
        while (drain_one(addr, val)) {
            mem.write32(addr, static_cast<uint32_t>(val));
            if (mem_writes) (*mem_writes)++;
        }
    }

    void reset() {
        entries_.fill(StoreBufferEntry{});
        head_ = tail_ = count_ = 0;
        forwards_ = 0;
    }
};

class CPU {
    // Register file
    std::array<int32_t, 16> r{};
    uint32_t pc = 0;
    uint32_t program_end = 0;

    // Condition flags (set by CMP)
    bool flag_E = false;
    bool flag_GT = false;

    // Components
    Memory mem;
    ALU alu;
    ControlUnit cu;
    BranchPredictor bp;
    StackRuntime* stack_rt = nullptr;
    L1Cache icache_{"L1 I-Cache"};
    L1Cache dcache_{"L1 D-Cache"};
    StoreBuffer store_buffer_{};

    // Pipeline
    Pipeline pipe;
    PipelineRegister IF_ID, ID_EX, EX_MEM, MEM_WB;
    // Extra stages for 6-stage
    PipelineRegister EX1_EX2, EX2_MEM, WB_OUT;

    // Microcode state & datapath registers
    uint32_t micro_pc = 0;
    bool in_micro_sequence = false;
    Instruction micro_macro_instr{};
    uint32_t micro_macro_pc = 0;
    int32_t micro_alu_out = 0;
    int32_t micro_mem_data = 0;
    uint32_t micro_next_pc = 0;

    // Debugger state
    std::set<uint32_t> breakpoints;
    bool halted = false;
    bool hit_breakpoint = false;

    // Statistics
    Stats stats;

    // Internal helpers
    void execute_single_cycle();  // reference single-cycle execution
    bool pipeline_tick_4();       // one clock cycle of 4-stage pipeline (returns true if instruction retired)
    bool pipeline_tick_6();       // one clock cycle of 6-stage pipeline (returns true if instruction retired)
    bool detect_data_hazard(const PipelineRegister& id_ex, const PipelineRegister& later) const;
    int32_t try_forward(uint8_t reg) const;  // returns forwarded value or register value
    bool needs_forward(uint8_t reg) const;   // checks if forwarding is available

public:
    CPU(Pipeline p = Pipeline::P4, ControlStyle cs = ControlStyle::HARDWIRED);
    ~CPU();

    // Prohibit copying to prevent double-free of StackRuntime
    CPU(const CPU&) = delete;
    CPU& operator=(const CPU&) = delete;

    // Program loading
    void load(const std::vector<uint32_t>& program);
    void reset();

    // Execution
    bool step();           // Execute one instruction/cycle, returns false if halted
    bool micro_step();     // Execute one micro-instruction (for microprogrammed modes)
    void run(size_t max_instructions = 100000);

    // Register access
    int32_t get_reg(uint8_t i) const;
    void set_reg(uint8_t i, int32_t v);
    const std::array<int32_t, 16>& regs() const { return r; }
    uint32_t get_pc() const { return pc; }

    // Flags
    bool get_flag_E() const { return flag_E; }
    bool get_flag_GT() const { return flag_GT; }

    // State
    bool is_halted() const { return halted; }
    bool at_breakpoint() const { return hit_breakpoint; }
    const Stats& get_stats() const { return stats; }

    // Memory access
    const Memory& memory() const { return mem; }
    Memory& memory() { return mem; }

    // Debugger
    void add_breakpoint(uint32_t addr);
    void remove_breakpoint(uint32_t addr);
    void clear_breakpoints();
    const std::set<uint32_t>& get_breakpoints() const { return breakpoints; }

    // Microcode inspection
    uint32_t get_micro_pc() const { return micro_pc; }
    bool is_in_micro_sequence() const { return in_micro_sequence; }
    std::string dump_micro_step_summary() const;

    // Display/dump methods
    std::string dump_registers() const;
    std::string dump_pipeline() const;
    std::string dump_micro_state() const;
    std::string dump_memory(uint32_t start, uint32_t len) const;
    std::string dump_stack() const;
    std::string dump_stats() const;
    std::string pipeline_name() const;

    // Configuration
    void set_alu_algorithms(AluAlgorithm mul, AluAlgorithm add, AluAlgorithm div, AluAlgorithm shift);
    void set_branch_predictor_mode(BranchPredictorMode m) { bp.set_mode(m); }
    BranchPredictorMode get_branch_predictor_mode() const { return bp.get_mode(); }
    BranchPredictor& get_branch_predictor() { return bp; }
    const BranchPredictor& get_branch_predictor() const { return bp; }
    std::string dump_branch_predictor() const { return bp.dump_state(); }
    Pipeline get_pipeline() const { return pipe; }
    ControlStyle get_control_style() const { return cu.style(); }
    ALU& get_alu() { return alu; }
    const ALU& get_alu() const { return alu; }
    StackRuntime& get_stack() { return *stack_rt; }
    const StackRuntime& get_stack() const { return *stack_rt; }
    const ControlUnit& get_control_unit() const { return cu; }
    L1Cache& icache() { return icache_; }
    const L1Cache& icache() const { return icache_; }
    L1Cache& dcache() { return dcache_; }
    const L1Cache& dcache() const { return dcache_; }
    StoreBuffer& store_buffer() { return store_buffer_; }
    const StoreBuffer& store_buffer() const { return store_buffer_; }
};

} // namespace risc201
