#pragma once
#include "isa.hpp"
#include <cstdint>
#include <array>
#include <string>

namespace risc201 {

enum class BranchPredictorMode {
    STATIC_NOT_TAKEN,   // Baseline: Always predicts Not-Taken (00)
    DYNAMIC_2BIT        // Dynamic 2-bit Saturating Counter with Branch Target Buffer (BTB)
};

enum class BimodalState : uint8_t {
    STRONGLY_NOT_TAKEN = 0, // 00
    WEAKLY_NOT_TAKEN   = 1, // 01
    WEAKLY_TAKEN       = 2, // 10
    STRONGLY_TAKEN     = 3  // 11
};

struct BTBEntry {
    bool valid = false;
    uint32_t tag = 0;              // Branch instruction PC
    uint32_t target_pc = 0;        // Cached branch target address
    BimodalState state = BimodalState::WEAKLY_NOT_TAKEN; // 2-bit saturating counter
};

// 16-Entry Circular Speculative Return Address Stack (RAS) for Subroutines
class ReturnAddressStack {
public:
    static constexpr size_t RAS_SIZE = 16;
private:
    std::array<uint32_t, RAS_SIZE> stack_{};
    size_t top_ = 0;
    size_t count_ = 0;
    uint64_t pushes_ = 0;
    uint64_t pops_ = 0;
    uint64_t hits_ = 0;
    uint64_t misses_ = 0;

public:
    void push(uint32_t ret_addr) {
        stack_[top_] = ret_addr;
        top_ = (top_ + 1) % RAS_SIZE;
        if (count_ < RAS_SIZE) count_++;
        pushes_++;
    }

    uint32_t pop() {
        if (count_ == 0) return 0;
        top_ = (top_ + RAS_SIZE - 1) % RAS_SIZE;
        count_--;
        pops_++;
        return stack_[top_];
    }

    uint32_t peek() const {
        if (count_ == 0) return 0;
        size_t idx = (top_ + RAS_SIZE - 1) % RAS_SIZE;
        return stack_[idx];
    }

    bool empty() const { return count_ == 0; }
    size_t size() const { return count_; }
    void record_hit() { hits_++; }
    void record_miss() { misses_++; }
    uint64_t get_hits() const { return hits_; }
    uint64_t get_misses() const { return misses_; }
    uint64_t get_pushes() const { return pushes_; }
    uint64_t get_pops() const { return pops_; }
    const std::array<uint32_t, RAS_SIZE>& stack() const { return stack_; }
    size_t top_index() const { return top_; }

    void reset() {
        top_ = 0;
        count_ = 0;
        stack_.fill(0);
        pushes_ = 0;
        pops_ = 0;
        hits_ = 0;
        misses_ = 0;
    }
};

struct Prediction {
    bool is_branch = false;
    bool predicted_taken = false;
    uint32_t predicted_target = 0;
};

class BranchPredictor {
public:
    static constexpr size_t BTB_SIZE = 64; // 64-entry direct-mapped BTB

private:
    std::array<BTBEntry, BTB_SIZE> btb_{};
    ReturnAddressStack ras_{};
    BranchPredictorMode mode_ = BranchPredictorMode::STATIC_NOT_TAKEN;

    size_t get_index(uint32_t pc) const {
        return (pc >> 2) & (BTB_SIZE - 1);
    }

public:
    BranchPredictor(BranchPredictorMode m = BranchPredictorMode::STATIC_NOT_TAKEN);

    void set_mode(BranchPredictorMode m);
    BranchPredictorMode get_mode() const { return mode_; }
    void reset();

    // Query prediction during IF (Instruction Fetch) stage
    Prediction predict(uint32_t branch_pc, const Instruction& instr) const;

    // Update predictor during EX / EX2 stage once branch condition is evaluated
    void update(uint32_t branch_pc, bool actually_taken, uint32_t actual_target, Opcode op = Opcode::NOP);

    // Inspection
    const BTBEntry* get_entry(uint32_t branch_pc) const;
    ReturnAddressStack& ras() { return ras_; }
    const ReturnAddressStack& ras() const { return ras_; }
    std::string dump_state() const;
    const char* mode_name() const;
    static const char* state_name(BimodalState s);
};

} // namespace risc201
