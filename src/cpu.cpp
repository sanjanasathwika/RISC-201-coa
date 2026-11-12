#include "risc201/cpu.hpp"
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <iostream>

namespace risc201 {

CPU::CPU(Pipeline p, ControlStyle cs) : mem(64 * 1024), cu(cs), pipe(p) {
    r.fill(0);
    r[14] = mem.size();
    stack_rt = new StackRuntime(mem, mem.size() / 2, mem.size());
}

CPU::~CPU() {
    delete stack_rt;
}

void CPU::load(const std::vector<uint32_t>& program) {
    for (size_t i = 0; i < program.size(); ++i) {
        mem.write32(i * 4, program[i]);
    }
    program_end = program.size() * 4;
    reset();
}

void CPU::reset() {
    pc = 0;
    halted = false;
    hit_breakpoint = false;
    stats = Stats{};
    bp.reset();
    icache_.reset();
    dcache_.reset();
    store_buffer_.reset();
    flag_E = false;
    flag_GT = false;
    r.fill(0);
    r[14] = mem.size();
    if (stack_rt) {
        delete stack_rt;
    }
    stack_rt = new StackRuntime(mem, mem.size() / 2, mem.size());
    IF_ID = PipelineRegister{};
    ID_EX = PipelineRegister{};
    EX_MEM = PipelineRegister{};
    MEM_WB = PipelineRegister{};
    EX1_EX2 = PipelineRegister{};
    EX2_MEM = PipelineRegister{};
    WB_OUT = PipelineRegister{};
    micro_pc = 0;
    in_micro_sequence = false;
    micro_macro_instr = Instruction{};
    micro_macro_pc = 0;
    micro_alu_out = 0;
    micro_mem_data = 0;
    micro_next_pc = 0;
}

bool CPU::step() {
    if (halted) return false;
    if (breakpoints.count(pc) && !hit_breakpoint) {
        hit_breakpoint = true;
        return false;
    }
    hit_breakpoint = false;

    if (cu.style() == ControlStyle::HARDWIRED) {
        // Execute actual hardware pipeline clock cycles until an instruction retires at WB
        bool retired = false;
        size_t ticks = 0;
        const size_t max_ticks = 2000;
        while (!retired && !halted && ticks < max_ticks) {
            ticks++;
            if (pipe == Pipeline::P4) {
                retired = pipeline_tick_4();
            } else {
                retired = pipeline_tick_6();
            }
        }
        return !halted;
    } else {
        // Microprogrammed control: step micro-instructions until this macro-instruction finishes
        while (!halted) {
            bool ok = micro_step();
            if (!in_micro_sequence || !ok) break;
        }
        return !halted;
    }
}

void CPU::execute_single_cycle() {
    uint32_t raw = mem.read32(pc);
    Instruction instr = decode(raw);
    ControlSignals ctrl = cu.decode(instr.op, instr.immediate);
    
    int32_t rs1_val = r[instr.rs1];
    int32_t rs2_val = r[instr.rs2];
    int32_t b = instr.immediate ? instr.imm : rs2_val;
    uint32_t next_pc = pc + 4;
    
    try {
        switch (instr.op) {
            case Opcode::HALT:
                halted = true;
                return;
            case Opcode::NOP:
                break;
            case Opcode::CMP: {
                AluResult result = alu.execute(AluOp::CMP, rs1_val, b);
                flag_E = result.E;
                flag_GT = result.GT;
                stats.aluOps++;
                break;
            }
            case Opcode::LD: {
                uint32_t addr = rs1_val + instr.imm;
                if (instr.rs1 == 14 && addr >= stack_rt->high()) {
                    throw StackRuntime::StackUnderflow("Stack underflow: Pop from empty stack (SP >= stack high boundary)");
                }
                r[instr.rd] = static_cast<int32_t>(dcache_.read32(addr, mem));
                if (instr.rd == 14) {
                    stack_rt->set_pointer(r[14]);
                }
                stack_rt->check_bounds(r[14]);
                stats.memReads++;
                break;
            }
            case Opcode::ST: {
                uint32_t addr = rs1_val + instr.imm;
                if (addr >= stack_rt->low() && addr < stack_rt->high() && addr < static_cast<uint32_t>(r[14])) {
                    std::ostringstream err;
                    err << "Stack corruption: store to address 0x" << std::hex << addr 
                        << " below active SP (0x" << r[14] << ")";
                    throw StackRuntime::StackCorruption(err.str());
                }
                dcache_.write32(addr, static_cast<uint32_t>(r[instr.rd]), mem);
                stack_rt->check_bounds(r[14]);
                stats.memWrites++;
                break;
            }
            case Opcode::BEQ: {
                if (flag_E) next_pc = pc + 4 + instr.offset * 4;
                stats.branches++;
                if (flag_E) stats.taken++;
                break;
            }
            case Opcode::BGT: {
                if (flag_GT) next_pc = pc + 4 + instr.offset * 4;
                stats.branches++;
                if (flag_GT) stats.taken++;
                break;
            }
            case Opcode::B: {
                next_pc = pc + 4 + instr.offset * 4;
                stats.branches++;
                stats.taken++;
                break;
            }
            case Opcode::CALL: {
                r[15] = pc + 4;
                next_pc = pc + 4 + instr.offset * 4;
                stack_rt->push_frame(r[15], r[14], "");
                stats.branches++;
                stats.taken++;
                break;
            }
            case Opcode::RET: {
                next_pc = r[15];
                stack_rt->pop_frame();
                stats.branches++;
                stats.taken++;
                break;
            }
            case Opcode::MOV: {
                r[instr.rd] = instr.immediate ? instr.imm : rs2_val;
                if (instr.rd == 14) {
                    stack_rt->set_pointer(r[14]);
                    stack_rt->check_bounds(r[14]);
                }
                break;
            }
            case Opcode::NOT: {
                r[instr.rd] = ~(instr.immediate ? instr.imm : rs2_val);
                if (instr.rd == 14) {
                    stack_rt->set_pointer(r[14]);
                    stack_rt->check_bounds(r[14]);
                }
                break;
            }
            default: {
                if (is_alu(instr.op)) {
                    AluOp alu_op = AluOp::PASS;
                    switch (instr.op) {
                        case Opcode::ADD: alu_op = AluOp::ADD; break;
                        case Opcode::SUB: alu_op = AluOp::SUB; break;
                        case Opcode::MUL: alu_op = AluOp::MUL; break;
                        case Opcode::DIV: alu_op = AluOp::DIV; break;
                        case Opcode::MOD: alu_op = AluOp::MOD; break;
                        case Opcode::AND: alu_op = AluOp::AND; break;
                        case Opcode::OR:  alu_op = AluOp::OR; break;
                        case Opcode::LSL: alu_op = AluOp::LSL; break;
                        case Opcode::LSR: alu_op = AluOp::LSR; break;
                        case Opcode::ASR: alu_op = AluOp::ASR; break;
                        default: break;
                    }
                    if (alu_op != AluOp::PASS) {
                        AluResult result = alu.execute(alu_op, rs1_val, b);
                        if (result.divide_by_zero) throw std::runtime_error("DivideByZero exception: division or modulo by zero");
                        if (ctrl.regWrite) {
                            r[instr.rd] = result.value;
                            if (instr.rd == 14) {
                                stack_rt->set_pointer(r[14]);
                                stack_rt->check_bounds(r[14]);
                            }
                        }
                        stats.aluOps++;
                    }
                }
                break;
            }
        }
    } catch (const StackRuntime::StackOverflow& e) {
        throw std::runtime_error(std::string("Processor Exception: ") + e.what());
    } catch (const StackRuntime::StackUnderflow& e) {
        throw std::runtime_error(std::string("Processor Exception: ") + e.what());
    } catch (const StackRuntime::StackCorruption& e) {
        throw std::runtime_error(std::string("Processor Exception: ") + e.what());
    }

    pc = next_pc;
    r[0] = 0; // r0 is hardwired to 0
    stats.instructions++;
}

bool CPU::micro_step() {
    if (halted) return false;

    if (!in_micro_sequence) {
        if (pc >= program_end) {
            halted = true;
            return false;
        }
        uint32_t raw = mem.read32(pc);
        micro_macro_pc = pc;
        micro_macro_instr = decode(raw);
        micro_pc = cu.dispatch(micro_macro_instr.op);
        in_micro_sequence = true;
        micro_next_pc = pc + 4;
        micro_alu_out = 0;
        micro_mem_data = 0;
    }

    MicroInstruction mi = cu.fetch_micro(micro_pc);
    stats.microOps++;
    stats.cycles++;

    // Execute datapath actions dictated by microinstruction signals
    int32_t a_val = r[micro_macro_instr.rs1];
    int32_t b_val = (mi.signals.aluSrc || micro_macro_instr.immediate) ? micro_macro_instr.imm : r[micro_macro_instr.rs2];

    if (mi.signals.aluOp != AluOp::PASS) {
        AluResult res = alu.execute(mi.signals.aluOp, a_val, b_val);
        if (res.divide_by_zero) throw std::runtime_error("DivideByZero exception: division or modulo by zero");
        stats.aluOps++;
        if (micro_macro_instr.op == Opcode::CMP) {
            flag_E = res.E;
            flag_GT = res.GT;
        } else {
            micro_alu_out = res.value;
        }
    } else if (micro_macro_instr.op == Opcode::MOV) {
        micro_alu_out = b_val;
    } else if (micro_macro_instr.op == Opcode::NOT) {
        micro_alu_out = ~b_val;
    }

    if (mi.signals.memRead) {
        uint32_t addr = a_val + micro_macro_instr.imm;
        if (micro_macro_instr.rs1 == 14 && addr >= stack_rt->high()) {
            throw StackRuntime::StackUnderflow("Stack underflow: Pop from empty stack (SP >= stack high boundary)");
        }
        micro_mem_data = static_cast<int32_t>(dcache_.read32(addr, mem));
        stats.memReads++;
    }

    if (mi.signals.memWrite) {
        uint32_t addr = a_val + micro_macro_instr.imm;
        if (addr >= stack_rt->low() && addr < stack_rt->high() && addr < static_cast<uint32_t>(r[14])) {
            throw StackRuntime::StackCorruption("Stack corruption: Store below active SP");
        }
        dcache_.write32(addr, static_cast<uint32_t>(r[micro_macro_instr.rd]), mem);
        stats.memWrites++;
    }

    if (mi.signals.regWrite && micro_macro_instr.rd != 0) {
        if (micro_macro_instr.op == Opcode::CALL) {
            r[15] = micro_macro_pc + 4;
        } else if (mi.signals.memRead || micro_macro_instr.op == Opcode::LD) {
            r[micro_macro_instr.rd] = micro_mem_data;
        } else {
            r[micro_macro_instr.rd] = micro_alu_out;
        }
        if (micro_macro_instr.rd == 14) {
            stack_rt->set_pointer(r[14]);
            stack_rt->check_bounds(r[14]);
        }
    }

    if (mi.signals.branch) {
        stats.branches++;
        if (micro_macro_instr.op == Opcode::BEQ && flag_E) {
            micro_next_pc = micro_macro_pc + 4 + micro_macro_instr.offset * 4;
            stats.taken++;
        } else if (micro_macro_instr.op == Opcode::BGT && flag_GT) {
            micro_next_pc = micro_macro_pc + 4 + micro_macro_instr.offset * 4;
            stats.taken++;
        }
    }

    if (mi.signals.jump) {
        stats.branches++;
        stats.taken++;
        micro_next_pc = micro_macro_pc + 4 + micro_macro_instr.offset * 4;
    }

    if (mi.signals.call) {
        stats.branches++;
        stats.taken++;
        r[15] = micro_macro_pc + 4;
        micro_next_pc = micro_macro_pc + 4 + micro_macro_instr.offset * 4;
        stack_rt->push_frame(r[15], r[14], "");
    }

    if (mi.signals.ret) {
        stats.branches++;
        stats.taken++;
        micro_next_pc = r[15];
        stack_rt->pop_frame();
    }

    if (mi.signals.halt) {
        halted = true;
    }

    r[0] = 0;

    if (mi.end_of_sequence) {
        pc = micro_next_pc;
        in_micro_sequence = false;
        stats.instructions++;
        micro_pc = 0;
    } else {
        micro_pc = mi.next_address;
    }

    return !halted;
}

std::string CPU::dump_micro_step_summary() const {
    std::ostringstream oss;
    if (halted) {
        oss << "[Micro-Step] CPU is halted.\n";
        return oss.str();
    }
    uint32_t raw = mem.read32(pc);
    Instruction instr = decode(raw);
    uint32_t upc = in_micro_sequence ? micro_pc : cu.dispatch(instr.op);
    auto mi = cu.fetch_micro(upc);

    oss << "[uPC=0x" << std::hex << std::setw(2) << std::setfill('0') << upc << std::dec << "] "
        << "Signals: [" << mi.signals.to_string() << "] "
        << "Next: 0x" << std::hex << std::setw(2) << std::setfill('0') << mi.next_address << std::dec << " "
        << "(End: " << (mi.end_of_sequence ? "YES" : "NO") << ") "
        << "| Inst: " << disassemble(instr) << "\n";
    return oss.str();
}

bool CPU::pipeline_tick_4() {
    if (halted) return false;
    bool retired = false;

    // 1. Stage 4: WB (Writeback)
    bool retire_st = false;
    uint32_t retire_st_addr = 0;
    if (EX_MEM.valid && !EX_MEM.bubble) {
        if (EX_MEM.ctrl.regWrite && EX_MEM.write_reg != 0) {
            r[EX_MEM.write_reg] = EX_MEM.write_data;
            if (EX_MEM.write_reg == 14) {
                stack_rt->set_pointer(r[14]);
                stack_rt->check_bounds(r[14]);
            }
        }
        if (EX_MEM.instr.op == Opcode::ST) {
            retire_st = true;
            retire_st_addr = EX_MEM.alu_result;
        }
        if (EX_MEM.instr.op == Opcode::HALT) {
            halted = true;
        }
        retired = true;
        stats.instructions++;
    }
    MEM_WB = EX_MEM;
    WB_OUT = EX_MEM;

    // 2. Stage 3: EX (Execute & Memory & Branch)
    PipelineRegister next_EX_MEM{};
    bool branch_taken = false;
    bool branch_mispredicted = false;
    uint32_t branch_target = 0;

    if (ID_EX.valid && !ID_EX.bubble) {
        next_EX_MEM.valid = true;
        next_EX_MEM.instr = ID_EX.instr;
        next_EX_MEM.ctrl = ID_EX.ctrl;
        next_EX_MEM.pc = ID_EX.pc;
        next_EX_MEM.write_reg = ID_EX.instr.rd;

        int32_t b_val = ID_EX.instr.immediate ? ID_EX.instr.imm : ID_EX.rs2_val;
        AluResult alu_res;
        if (ID_EX.ctrl.aluOp != AluOp::PASS) {
            alu_res = alu.execute(ID_EX.ctrl.aluOp, ID_EX.rs1_val, b_val);
            if (alu_res.divide_by_zero) throw std::runtime_error("DivideByZero exception: division or modulo by zero");
            stats.aluOps++;
        } else if (ID_EX.instr.op == Opcode::MOV) {
            alu_res.value = b_val;
        } else if (ID_EX.instr.op == Opcode::NOT) {
            alu_res.value = ~b_val;
        }

        if (ID_EX.instr.op == Opcode::NOT) {
            alu_res.value = ~b_val;
        }

        if (ID_EX.instr.op == Opcode::CMP) {
            flag_E = alu_res.E;
            flag_GT = alu_res.GT;
        }

        next_EX_MEM.alu_result = alu_res.value;
        next_EX_MEM.write_data = alu_res.value;

        if (ID_EX.instr.op == Opcode::LD) {
            uint32_t addr = ID_EX.rs1_val + ID_EX.instr.imm;
            if (ID_EX.instr.rs1 == 14 && addr >= stack_rt->high()) {
                throw StackRuntime::StackUnderflow("Stack underflow: Pop from empty stack (SP >= stack high boundary)");
            }
            int32_t fwd_val = 0;
            if (store_buffer_.lookup(addr, fwd_val)) {
                next_EX_MEM.write_data = fwd_val;
                stats.store_to_load_forwards++;
                stats.forwards++;
            } else {
                next_EX_MEM.write_data = static_cast<int32_t>(dcache_.read32(addr, mem));
            }
            stats.memReads++;
        } else if (ID_EX.instr.op == Opcode::ST) {
            uint32_t addr = ID_EX.rs1_val + ID_EX.instr.imm;
            if (addr >= stack_rt->low() && addr < stack_rt->high() && addr < static_cast<uint32_t>(r[14])) {
                throw StackRuntime::StackCorruption("Stack corruption: Store below active SP");
            }
            next_EX_MEM.alu_result = addr;
            store_buffer_.push(addr, ID_EX.rs2_val, &dcache_, &mem, &stats.memWrites);
        }

        // Branch evaluation in EX
        if (ID_EX.instr.op == Opcode::BEQ && flag_E) {
            branch_taken = true;
            branch_target = ID_EX.pc + 4 + ID_EX.instr.offset * 4;
        } else if (ID_EX.instr.op == Opcode::BGT && flag_GT) {
            branch_taken = true;
            branch_target = ID_EX.pc + 4 + ID_EX.instr.offset * 4;
        } else if (ID_EX.instr.op == Opcode::B) {
            branch_taken = true;
            branch_target = ID_EX.pc + 4 + ID_EX.instr.offset * 4;
        } else if (ID_EX.instr.op == Opcode::CALL) {
            branch_taken = true;
            branch_target = ID_EX.pc + 4 + ID_EX.instr.offset * 4;
            next_EX_MEM.write_reg = 15;
            next_EX_MEM.write_data = ID_EX.pc + 4;
            stack_rt->push_frame(ID_EX.pc + 4, r[14], "");
        } else if (ID_EX.instr.op == Opcode::RET) {
            branch_taken = true;
            uint32_t ret_target = r[15];
            if (EX_MEM.valid && !EX_MEM.bubble && EX_MEM.ctrl.regWrite && EX_MEM.write_reg == 15) {
                ret_target = EX_MEM.write_data;
            }
            branch_target = ret_target;
            stack_rt->pop_frame();
        }

        bool is_br = is_branch(ID_EX.instr.op) || ID_EX.instr.op == Opcode::RET;
        if (is_br) {
            stats.branches++;
            if (branch_taken) stats.taken++;
            stats.branch_predictions++;
            bp.update(ID_EX.pc, branch_taken, branch_target, ID_EX.instr.op);

            bool mispredicted = false;
            if (ID_EX.predicted_taken != branch_taken) {
                mispredicted = true;
            } else if (branch_taken && ID_EX.predicted_target != branch_target) {
                mispredicted = true;
            }

            if (mispredicted) {
                stats.branch_misses++;
                stats.pipeline_flushes++;
                branch_mispredicted = true;
                // Flush earlier stage IF_ID
                IF_ID = PipelineRegister{};
                IF_ID.bubble = true;
                pc = branch_taken ? branch_target : (ID_EX.pc + 4);
            } else {
                stats.branch_hits++;
            }
        }
    }
    EX_MEM = next_EX_MEM;

    // 3. Stage 2: ID (Decode & Operand Forwarding & Load-Use Stall Detection)
    PipelineRegister next_ID_EX{};
    bool stall = false;

    if (IF_ID.valid && !IF_ID.bubble) {
        Instruction instr = IF_ID.instr;
        ControlSignals ctrl = cu.decode(instr.op, instr.immediate);

        uint8_t rs2_reg = (instr.op == Opcode::ST) ? instr.rd : instr.rs2;
        int32_t rs1_val = r[instr.rs1];
        int32_t rs2_val = r[rs2_reg];
        bool rs1_needed = (instr.rs1 != 0);
        bool rs2_needed = (instr.op == Opcode::ST) ? (instr.rd != 0) : (!instr.immediate && instr.rs2 != 0);

        // Load-use hazard detection: if instruction currently in EX is a LOAD and writes to an operand needed by ID
        if (EX_MEM.valid && !EX_MEM.bubble && EX_MEM.instr.op == Opcode::LD) {
            if ((rs1_needed && instr.rs1 == EX_MEM.write_reg) ||
                (rs2_needed && rs2_reg == EX_MEM.write_reg)) {
                stall = true;
                stats.stalls++;
                stats.hazards_detected++;
            }
        }

        if (!stall) {
            // Forwarding unit: forward from EX_MEM (most recent) or MEM_WB (older)
            if (EX_MEM.valid && !EX_MEM.bubble && EX_MEM.ctrl.regWrite && EX_MEM.write_reg != 0) {
                if (rs1_needed && instr.rs1 == EX_MEM.write_reg) {
                    rs1_val = EX_MEM.write_data;
                    stats.forwards++;
                    stats.hazards_detected++;
                }
                if (rs2_needed && rs2_reg == EX_MEM.write_reg) {
                    rs2_val = EX_MEM.write_data;
                    stats.forwards++;
                    stats.hazards_detected++;
                }
            }
            if (MEM_WB.valid && !MEM_WB.bubble && MEM_WB.ctrl.regWrite && MEM_WB.write_reg != 0) {
                if (rs1_needed && instr.rs1 == MEM_WB.write_reg && (!EX_MEM.valid || instr.rs1 != EX_MEM.write_reg)) {
                    rs1_val = MEM_WB.write_data;
                    stats.forwards++;
                    stats.hazards_detected++;
                }
                if (rs2_needed && rs2_reg == MEM_WB.write_reg && (!EX_MEM.valid || rs2_reg != EX_MEM.write_reg)) {
                    rs2_val = MEM_WB.write_data;
                    stats.forwards++;
                    stats.hazards_detected++;
                }
            }

            next_ID_EX.valid = true;
            next_ID_EX.instr = instr;
            next_ID_EX.ctrl = ctrl;
            next_ID_EX.pc = IF_ID.pc;
            next_ID_EX.rs1_val = rs1_val;
            next_ID_EX.rs2_val = rs2_val;
            next_ID_EX.write_reg = instr.rd;
            next_ID_EX.predicted_taken = IF_ID.predicted_taken;
            next_ID_EX.predicted_target = IF_ID.predicted_target;
        } else {
            next_ID_EX.bubble = true;
            next_ID_EX.valid = false;
        }
    }
    ID_EX = next_ID_EX;

    // 4. Stage 1: IF (Fetch)
    if (!stall && !branch_mispredicted && !halted) {
        if (pc < program_end) {
            IF_ID.valid = true;
            IF_ID.bubble = false;
            IF_ID.pc = pc;
            icache_.access(pc, false, mem);
            Instruction fetched = decode(mem.read32(pc));
            IF_ID.instr = fetched;

            Prediction pred = bp.predict(pc, fetched);
            IF_ID.predicted_taken = pred.predicted_taken;
            IF_ID.predicted_target = pred.predicted_target;

            if (pred.predicted_taken) {
                pc = pred.predicted_target;
            } else {
                pc += 4;
            }
        } else {
            IF_ID.valid = false;
            // If all pipeline stages are empty after program end, halt CPU
            if (!ID_EX.valid && !EX_MEM.valid && !MEM_WB.valid) {
                halted = true;
            }
        }
    }

    if (retire_st) {
        int32_t val = 0;
        if (store_buffer_.retire(retire_st_addr, val)) {
            dcache_.write32(retire_st_addr, static_cast<uint32_t>(val), mem);
            stats.memWrites++;
        }
    }

    if (halted) {
        store_buffer_.drain_all(dcache_, mem, &stats.memWrites);
        dcache_.flush(mem);
    }

    stats.cycles++;
    r[0] = 0; // r0 is hardwired to 0
    return retired;
}

bool CPU::pipeline_tick_6() {
    if (halted) return false;
    bool retired = false;

    // Stage 6: WB (Writeback)
    bool retire_st = false;
    uint32_t retire_st_addr = 0;
    if (MEM_WB.valid && !MEM_WB.bubble) {
        if (MEM_WB.ctrl.regWrite && MEM_WB.write_reg != 0) {
            r[MEM_WB.write_reg] = MEM_WB.write_data;
            if (MEM_WB.write_reg == 14) {
                stack_rt->set_pointer(r[14]);
                stack_rt->check_bounds(r[14]);
            }
        }
        if (MEM_WB.instr.op == Opcode::ST) {
            retire_st = true;
            retire_st_addr = MEM_WB.alu_result;
        }
        if (MEM_WB.instr.op == Opcode::HALT) {
            halted = true;
        }
        retired = true;
        stats.instructions++;
    }
    WB_OUT = MEM_WB;

    // Stage 5: MEM (Memory Access)
    PipelineRegister next_MEM_WB{};
    if (EX2_MEM.valid && !EX2_MEM.bubble) {
        next_MEM_WB = EX2_MEM;
        if (EX2_MEM.instr.op == Opcode::LD) {
            uint32_t addr = EX2_MEM.alu_result;
            if (EX2_MEM.instr.rs1 == 14 && addr >= stack_rt->high()) {
                throw StackRuntime::StackUnderflow("Stack underflow: Pop from empty stack (SP >= stack high boundary)");
            }
            int32_t fwd_val = 0;
            if (store_buffer_.lookup(addr, fwd_val)) {
                next_MEM_WB.write_data = fwd_val;
                stats.store_to_load_forwards++;
                stats.forwards++;
            } else {
                next_MEM_WB.write_data = static_cast<int32_t>(dcache_.read32(addr, mem));
            }
            stats.memReads++;
        } else if (EX2_MEM.instr.op == Opcode::ST) {
            uint32_t addr = EX2_MEM.alu_result;
            if (addr >= stack_rt->low() && addr < stack_rt->high() && addr < static_cast<uint32_t>(r[14])) {
                throw StackRuntime::StackCorruption("Stack corruption: Store below active SP");
            }
        }
    }
    MEM_WB = next_MEM_WB;

    // Stage 4: EX2 (ALU Stage 2 / Branch Resolution)
    PipelineRegister next_EX2_MEM{};
    bool branch_taken = false;
    bool branch_mispredicted = false;
    uint32_t branch_target = 0;

    if (EX1_EX2.valid && !EX1_EX2.bubble) {
        next_EX2_MEM = EX1_EX2;
        int32_t rs1_val = EX1_EX2.rs1_val;
        int32_t rs2_val = EX1_EX2.rs2_val;
        bool rs1_needed = (EX1_EX2.instr.rs1 != 0);
        bool rs2_needed = (EX1_EX2.instr.op == Opcode::ST) ? (EX1_EX2.instr.rd != 0) : (!EX1_EX2.instr.immediate && EX1_EX2.instr.rs2 != 0);

        // Forwarding into EX2 from MEM_WB (1 stage ahead) or WB_OUT (2 stages ahead)
        if (MEM_WB.valid && !MEM_WB.bubble && MEM_WB.ctrl.regWrite && MEM_WB.write_reg != 0) {
            if (rs1_needed && EX1_EX2.instr.rs1 == MEM_WB.write_reg) {
                rs1_val = MEM_WB.write_data;
                stats.forwards++;
                stats.hazards_detected++;
            }
            uint8_t target_r2 = (EX1_EX2.instr.op == Opcode::ST) ? EX1_EX2.instr.rd : EX1_EX2.instr.rs2;
            if (rs2_needed && target_r2 == MEM_WB.write_reg) {
                rs2_val = MEM_WB.write_data;
                stats.forwards++;
                stats.hazards_detected++;
            }
        }
        if (WB_OUT.valid && !WB_OUT.bubble && WB_OUT.ctrl.regWrite && WB_OUT.write_reg != 0) {
            if (rs1_needed && EX1_EX2.instr.rs1 == WB_OUT.write_reg &&
                (!MEM_WB.valid || EX1_EX2.instr.rs1 != MEM_WB.write_reg)) {
                rs1_val = WB_OUT.write_data;
                stats.forwards++;
                stats.hazards_detected++;
            }
            uint8_t target_r2 = (EX1_EX2.instr.op == Opcode::ST) ? EX1_EX2.instr.rd : EX1_EX2.instr.rs2;
            if (rs2_needed && target_r2 == WB_OUT.write_reg &&
                (!MEM_WB.valid || target_r2 != MEM_WB.write_reg)) {
                rs2_val = WB_OUT.write_data;
                stats.forwards++;
                stats.hazards_detected++;
            }
        }
        next_EX2_MEM.rs1_val = rs1_val;
        next_EX2_MEM.rs2_val = rs2_val;

        if (EX1_EX2.instr.op == Opcode::LD || EX1_EX2.instr.op == Opcode::ST) {
            next_EX2_MEM.alu_result = rs1_val + EX1_EX2.instr.imm;
            next_EX2_MEM.write_data = next_EX2_MEM.alu_result;
            if (EX1_EX2.instr.op == Opcode::ST) {
                store_buffer_.push(next_EX2_MEM.alu_result, rs2_val, &dcache_, &mem, &stats.memWrites);
            }
        } else {
            int32_t b_val = EX1_EX2.instr.immediate ? EX1_EX2.instr.imm : rs2_val;
            AluResult alu_res;
            if (EX1_EX2.ctrl.aluOp != AluOp::PASS) {
                alu_res = alu.execute(EX1_EX2.ctrl.aluOp, rs1_val, b_val);
                if (alu_res.divide_by_zero) throw std::runtime_error("DivideByZero exception: division or modulo by zero");
                stats.aluOps++;
            } else if (EX1_EX2.instr.op == Opcode::MOV) {
                alu_res.value = b_val;
            } else if (EX1_EX2.instr.op == Opcode::NOT) {
                alu_res.value = ~b_val;
            }

            if (EX1_EX2.instr.op == Opcode::CMP) {
                flag_E = alu_res.E;
                flag_GT = alu_res.GT;
            }

            next_EX2_MEM.alu_result = alu_res.value;
            next_EX2_MEM.write_data = alu_res.value;
        }

        // Branch evaluation in EX2
        if (EX1_EX2.instr.op == Opcode::BEQ && flag_E) {
            branch_taken = true;
            branch_target = EX1_EX2.pc + 4 + EX1_EX2.instr.offset * 4;
        } else if (EX1_EX2.instr.op == Opcode::BGT && flag_GT) {
            branch_taken = true;
            branch_target = EX1_EX2.pc + 4 + EX1_EX2.instr.offset * 4;
        } else if (EX1_EX2.instr.op == Opcode::B) {
            branch_taken = true;
            branch_target = EX1_EX2.pc + 4 + EX1_EX2.instr.offset * 4;
        } else if (EX1_EX2.instr.op == Opcode::CALL) {
            branch_taken = true;
            branch_target = EX1_EX2.pc + 4 + EX1_EX2.instr.offset * 4;
            next_EX2_MEM.write_reg = 15;
            next_EX2_MEM.write_data = EX1_EX2.pc + 4;
            stack_rt->push_frame(EX1_EX2.pc + 4, r[14], "");
        } else if (EX1_EX2.instr.op == Opcode::RET) {
            branch_taken = true;
            uint32_t ret_target = r[15];
            if (MEM_WB.valid && !MEM_WB.bubble && MEM_WB.ctrl.regWrite && MEM_WB.write_reg == 15) {
                ret_target = MEM_WB.write_data;
            } else if (WB_OUT.valid && !WB_OUT.bubble && WB_OUT.ctrl.regWrite && WB_OUT.write_reg == 15) {
                ret_target = WB_OUT.write_data;
            }
            branch_target = ret_target;
            stack_rt->pop_frame();
        }

        bool is_br = is_branch(EX1_EX2.instr.op) || EX1_EX2.instr.op == Opcode::RET;
        if (is_br) {
            stats.branches++;
            if (branch_taken) stats.taken++;
            stats.branch_predictions++;
            bp.update(EX1_EX2.pc, branch_taken, branch_target, EX1_EX2.instr.op);

            bool mispredicted = false;
            if (EX1_EX2.predicted_taken != branch_taken) {
                mispredicted = true;
            } else if (branch_taken && EX1_EX2.predicted_target != branch_target) {
                mispredicted = true;
            }

            if (mispredicted) {
                stats.branch_misses++;
                stats.pipeline_flushes += 2; // 2-cycle flush penalty in 6-stage
                branch_mispredicted = true;
                // Flush earlier stages IF_ID and ID_EX
                IF_ID = PipelineRegister{}; IF_ID.bubble = true;
                ID_EX = PipelineRegister{}; ID_EX.bubble = true;
                pc = branch_taken ? branch_target : (EX1_EX2.pc + 4);
            } else {
                stats.branch_hits++;
            }
        }
    }
    EX2_MEM = next_EX2_MEM;

    // Stage 3: EX1 (Address Calculation / Operand Preparation)
    PipelineRegister next_EX1_EX2{};
    if (branch_mispredicted) {
        next_EX1_EX2.bubble = true;
    } else if (ID_EX.valid && !ID_EX.bubble) {
        next_EX1_EX2 = ID_EX;
        next_EX1_EX2.predicted_taken = ID_EX.predicted_taken;
        next_EX1_EX2.predicted_target = ID_EX.predicted_target;
        int32_t rs1_val = ID_EX.rs1_val;
        int32_t rs2_val = ID_EX.rs2_val;
        bool rs1_needed = (ID_EX.instr.rs1 != 0);
        bool rs2_needed = (ID_EX.instr.op == Opcode::ST) ? (ID_EX.instr.rd != 0) : (!ID_EX.instr.immediate && ID_EX.instr.rs2 != 0);

        // Forwarding into EX1 from EX2_MEM (completed EX2), MEM_WB, WB_OUT
        if (EX2_MEM.valid && !EX2_MEM.bubble && EX2_MEM.ctrl.regWrite && EX2_MEM.write_reg != 0 && EX2_MEM.instr.op != Opcode::LD) {
            if (rs1_needed && ID_EX.instr.rs1 == EX2_MEM.write_reg) {
                rs1_val = EX2_MEM.write_data;
                stats.forwards++;
                stats.hazards_detected++;
            }
            uint8_t target_r2 = (ID_EX.instr.op == Opcode::ST) ? ID_EX.instr.rd : ID_EX.instr.rs2;
            if (rs2_needed && target_r2 == EX2_MEM.write_reg) {
                rs2_val = EX2_MEM.write_data;
                stats.forwards++;
                stats.hazards_detected++;
            }
        }
        if (MEM_WB.valid && !MEM_WB.bubble && MEM_WB.ctrl.regWrite && MEM_WB.write_reg != 0) {
            if (rs1_needed && ID_EX.instr.rs1 == MEM_WB.write_reg &&
                (!EX2_MEM.valid || ID_EX.instr.rs1 != EX2_MEM.write_reg || EX2_MEM.instr.op == Opcode::LD)) {
                rs1_val = MEM_WB.write_data;
                stats.forwards++;
                stats.hazards_detected++;
            }
            uint8_t target_r2 = (ID_EX.instr.op == Opcode::ST) ? ID_EX.instr.rd : ID_EX.instr.rs2;
            if (rs2_needed && target_r2 == MEM_WB.write_reg &&
                (!EX2_MEM.valid || target_r2 != EX2_MEM.write_reg || EX2_MEM.instr.op == Opcode::LD)) {
                rs2_val = MEM_WB.write_data;
                stats.forwards++;
                stats.hazards_detected++;
            }
        }
        if (WB_OUT.valid && !WB_OUT.bubble && WB_OUT.ctrl.regWrite && WB_OUT.write_reg != 0) {
            if (rs1_needed && ID_EX.instr.rs1 == WB_OUT.write_reg &&
                (!EX2_MEM.valid || ID_EX.instr.rs1 != EX2_MEM.write_reg) &&
                (!MEM_WB.valid || ID_EX.instr.rs1 != MEM_WB.write_reg)) {
                rs1_val = WB_OUT.write_data;
                stats.forwards++;
                stats.hazards_detected++;
            }
            uint8_t target_r2 = (ID_EX.instr.op == Opcode::ST) ? ID_EX.instr.rd : ID_EX.instr.rs2;
            if (rs2_needed && target_r2 == WB_OUT.write_reg &&
                (!EX2_MEM.valid || target_r2 != EX2_MEM.write_reg) &&
                (!MEM_WB.valid || target_r2 != MEM_WB.write_reg)) {
                rs2_val = WB_OUT.write_data;
                stats.forwards++;
                stats.hazards_detected++;
            }
        }

        next_EX1_EX2.rs1_val = rs1_val;
        next_EX1_EX2.rs2_val = rs2_val;

        if (ID_EX.instr.op == Opcode::LD || ID_EX.instr.op == Opcode::ST) {
            next_EX1_EX2.alu_result = rs1_val + ID_EX.instr.imm;
            next_EX1_EX2.write_data = next_EX1_EX2.alu_result;
        }
    }
    EX1_EX2 = next_EX1_EX2;

    // Stage 2: ID (Decode & 2-Cycle Load-Use Stall & Forwarding)
    PipelineRegister next_ID_EX{};
    bool stall = false;

    if (IF_ID.valid && !IF_ID.bubble && !branch_mispredicted) {
        Instruction instr = IF_ID.instr;
        ControlSignals ctrl = cu.decode(instr.op, instr.immediate);

        uint8_t rs2_reg = (instr.op == Opcode::ST) ? instr.rd : instr.rs2;
        int32_t rs1_val = r[instr.rs1];
        int32_t rs2_val = r[rs2_reg];
        bool rs1_needed = (instr.rs1 != 0);
        bool rs2_needed = (instr.op == Opcode::ST) ? (instr.rd != 0) : (!instr.immediate && instr.rs2 != 0);

        // In 6-stage pipeline, if a load instruction is currently in EX1 (EX1_EX2) or EX2 (EX2_MEM),
        // its memory data is not ready yet -> 2-cycle load-use stall
        if ((EX1_EX2.valid && !EX1_EX2.bubble && EX1_EX2.instr.op == Opcode::LD &&
             ((rs1_needed && instr.rs1 == EX1_EX2.write_reg) || (rs2_needed && rs2_reg == EX1_EX2.write_reg))) ||
            (EX2_MEM.valid && !EX2_MEM.bubble && EX2_MEM.instr.op == Opcode::LD &&
             ((rs1_needed && instr.rs1 == EX2_MEM.write_reg) || (rs2_needed && rs2_reg == EX2_MEM.write_reg)))) {
            stall = true;
            stats.stalls++;
            stats.hazards_detected++;
        }

        if (!stall) {
            // Forwarding from MEM_WB (just finished MEM) and WB_OUT (just finished WB)
            if (MEM_WB.valid && !MEM_WB.bubble && MEM_WB.ctrl.regWrite && MEM_WB.write_reg != 0) {
                if (rs1_needed && instr.rs1 == MEM_WB.write_reg) {
                    rs1_val = MEM_WB.write_data;
                    stats.forwards++;
                    stats.hazards_detected++;
                }
                if (rs2_needed && rs2_reg == MEM_WB.write_reg) {
                    rs2_val = MEM_WB.write_data;
                    stats.forwards++;
                    stats.hazards_detected++;
                }
            }
            if (WB_OUT.valid && !WB_OUT.bubble && WB_OUT.ctrl.regWrite && WB_OUT.write_reg != 0) {
                if (rs1_needed && instr.rs1 == WB_OUT.write_reg &&
                    (!MEM_WB.valid || instr.rs1 != MEM_WB.write_reg)) {
                rs1_val = WB_OUT.write_data;
                stats.forwards++;
                stats.hazards_detected++;
            }
            if (rs2_needed && rs2_reg == WB_OUT.write_reg &&
                (!MEM_WB.valid || rs2_reg != MEM_WB.write_reg)) {
                rs2_val = WB_OUT.write_data;
                stats.forwards++;
                stats.hazards_detected++;
            }
        }

            next_ID_EX.valid = true;
            next_ID_EX.instr = instr;
            next_ID_EX.ctrl = ctrl;
            next_ID_EX.pc = IF_ID.pc;
            next_ID_EX.rs1_val = rs1_val;
            next_ID_EX.rs2_val = rs2_val;
            next_ID_EX.write_reg = instr.rd;
            next_ID_EX.predicted_taken = IF_ID.predicted_taken;
            next_ID_EX.predicted_target = IF_ID.predicted_target;
        } else {
            next_ID_EX.bubble = true;
            next_ID_EX.valid = false;
        }
    }
    ID_EX = next_ID_EX;

    // Stage 1: IF (Fetch)
    if (!stall && !branch_mispredicted && !halted) {
        if (pc < program_end) {
            IF_ID.valid = true;
            IF_ID.bubble = false;
            IF_ID.pc = pc;
            icache_.access(pc, false, mem);
            Instruction fetched = decode(mem.read32(pc));
            IF_ID.instr = fetched;

            Prediction pred = bp.predict(pc, fetched);
            IF_ID.predicted_taken = pred.predicted_taken;
            IF_ID.predicted_target = pred.predicted_target;

            if (pred.predicted_taken) {
                pc = pred.predicted_target;
            } else {
                pc += 4;
            }
        } else {
            IF_ID.valid = false;
            if (!ID_EX.valid && !EX1_EX2.valid && !EX2_MEM.valid && !MEM_WB.valid) {
                halted = true;
            }
        }
    }

    if (retire_st) {
        int32_t val = 0;
        if (store_buffer_.retire(retire_st_addr, val)) {
            dcache_.write32(retire_st_addr, static_cast<uint32_t>(val), mem);
            stats.memWrites++;
        }
    }

    if (halted) {
        store_buffer_.drain_all(dcache_, mem, &stats.memWrites);
        dcache_.flush(mem);
    }

    stats.cycles++;
    r[0] = 0;
    return retired;
}

bool CPU::detect_data_hazard(const PipelineRegister& id_ex, const PipelineRegister& later) const {
    if (!later.valid || later.bubble || !later.ctrl.regWrite || later.write_reg == 0) return false;
    return id_ex.instr.rs1 == later.write_reg || (!id_ex.instr.immediate && id_ex.instr.rs2 == later.write_reg);
}

int32_t CPU::try_forward(uint8_t reg) const {
    if (reg == 0) return 0;
    if (EX_MEM.valid && EX_MEM.ctrl.regWrite && EX_MEM.write_reg == reg) {
        return EX_MEM.write_data;
    }
    if (MEM_WB.valid && MEM_WB.ctrl.regWrite && MEM_WB.write_reg == reg) {
        return MEM_WB.write_data;
    }
    return r[reg];
}

bool CPU::needs_forward(uint8_t reg) const {
    if (reg == 0) return false;
    if (EX_MEM.valid && EX_MEM.ctrl.regWrite && EX_MEM.write_reg == reg) return true;
    if (MEM_WB.valid && MEM_WB.ctrl.regWrite && MEM_WB.write_reg == reg) return true;
    return false;
}

void CPU::run(size_t max_instructions) {
    size_t count = 0;
    while (!halted && count < max_instructions) {
        if (!step()) break;
        count++;
    }
    store_buffer_.drain_all(dcache_, mem, &stats.memWrites);
    dcache_.flush(mem);
}

int32_t CPU::get_reg(uint8_t i) const { return r[i]; }
void CPU::set_reg(uint8_t i, int32_t v) { if (i != 0) r[i] = v; }

void CPU::add_breakpoint(uint32_t addr) { breakpoints.insert(addr); }
void CPU::remove_breakpoint(uint32_t addr) { breakpoints.erase(addr); }
void CPU::clear_breakpoints() { breakpoints.clear(); }

std::string CPU::dump_registers() const {
    std::stringstream ss;
    ss << "Registers:\n";
    for (int i = 0; i < 16; ++i) {
        if (i == 14) ss << "  SP = " << std::setw(8) << r[i];
        else if (i == 15) ss << "  RA = " << std::setw(8) << r[i];
        else ss << "  r" << std::left << std::setw(2) << i << "= " << std::right << std::setw(8) << r[i];
        if ((i + 1) % 4 == 0) ss << "\n";
    }
    ss << "  PC = 0x" << std::hex << std::setfill('0') << std::setw(8) << pc << std::dec;
    ss << "  Flags: E=" << flag_E << " GT=" << flag_GT << "\n";
    return ss.str();
}

static std::string format_stage_row(const std::string& name, const PipelineRegister& reg) {
    std::ostringstream oss;
    oss << "| " << std::left << std::setw(7) << name << " | ";
    if (!reg.valid) {
        oss << std::setw(10) << "[IDLE]" << " | " << std::setw(10) << "-" << " | " << std::setw(30) << "-" << " |\n";
    } else if (reg.bubble) {
        oss << std::setw(10) << "[BUBBLE]" << " | " << std::setw(10) << "-" << " | " << std::setw(30) << "Stall Bubble" << " |\n";
    } else {
        std::string status = "[ACTIVE]";
        std::ostringstream pc_oss;
        pc_oss << "0x" << std::hex << std::setw(4) << std::setfill('0') << reg.pc;
        std::string inst_str = disassemble(reg.instr);
        oss << std::setw(10) << status << " | " 
            << std::setw(10) << pc_oss.str() << " | " 
            << std::setw(30) << inst_str << " |\n";
    }
    return oss.str();
}

std::string CPU::dump_pipeline() const {
    std::ostringstream oss;
    if (pipe == Pipeline::P4) {
        oss << "+-------------------------------------------------------------------+\n";
        oss << "|                       4-STAGE PIPELINE STATE                      |\n";
        oss << "+---------+------------+------------+--------------------------------+\n";
        oss << "| Stage   | Status     | PC         | Instruction                    |\n";
        oss << "+---------+------------+------------+--------------------------------+\n";
        oss << format_stage_row("IF", IF_ID);
        oss << format_stage_row("ID", ID_EX);
        oss << format_stage_row("EX", EX_MEM);
        oss << format_stage_row("WB", MEM_WB);
        oss << "+---------+------------+------------+--------------------------------+\n";
    } else {
        oss << "+-------------------------------------------------------------------+\n";
        oss << "|                       6-STAGE PIPELINE STATE                      |\n";
        oss << "+---------+------------+------------+--------------------------------+\n";
        oss << "| Stage   | Status     | PC         | Instruction                    |\n";
        oss << "+---------+------------+------------+--------------------------------+\n";
        oss << format_stage_row("IF", IF_ID);
        oss << format_stage_row("ID", ID_EX);
        oss << format_stage_row("EX1", EX1_EX2);
        oss << format_stage_row("EX2", EX2_MEM);
        oss << format_stage_row("MEM", MEM_WB);
        oss << format_stage_row("WB", WB_OUT);
        oss << "+---------+------------+------------+--------------------------------+\n";
    }
    oss << "| Hazards: " << std::setw(4) << stats.hazards_detected
        << " | Forwards: " << std::setw(4) << stats.forwards
        << " | Stalls: " << std::setw(4) << stats.stalls
        << " | Flushes: " << std::setw(4) << stats.pipeline_flushes << "    |\n";
    oss << "+-------------------------------------------------------------------+\n";
    return oss.str();
}

std::string CPU::dump_micro_state() const {
    std::ostringstream oss;
    oss << "===================== CONTROL UNIT STATE =====================\n";
    oss << "  Control Style : " << cu.style_name() << "\n";
    
    if (cu.style() == ControlStyle::HARDWIRED) {
        uint32_t raw = mem.read32(pc);
        Instruction instr = decode(raw);
        ControlSignals cs = cu.decode(instr.op, instr.immediate);
        oss << "  Macro PC      : 0x" << std::hex << std::setw(4) << std::setfill('0') << pc << std::dec << "\n";
        oss << "  Instruction   : " << disassemble(instr) << "\n";
        oss << "  Active Signals: " << cs.to_string() << "\n";
    } else {
        uint32_t raw = mem.read32(pc);
        Instruction instr = decode(raw);
        uint32_t upc = in_micro_sequence ? micro_pc : cu.dispatch(instr.op);
        auto mi = cu.fetch_micro(upc);
        
        oss << "  Control Memory: " << cu.control_memory_size() << " microinstructions (" << cu.word_width() << "-bit words)\n";
        oss << "  MicroPC       : 0x" << std::hex << std::setw(2) << std::setfill('0') << upc << std::dec 
            << " (" << (in_micro_sequence ? "In Sequence" : "Entry Point") << ")\n";
        oss << "  Macro PC      : 0x" << std::hex << std::setw(4) << std::setfill('0') << pc << std::dec << "\n";
        oss << "  Instruction   : " << disassemble(instr) << "\n";
        oss << "  Micro Raw Bits: 0x" << std::hex << std::setw(6) << std::setfill('0') << mi.raw_bits << std::dec << "\n";
        oss << "  Active Signals: " << mi.signals.to_string() << "\n";
        oss << "  Next MicroPC  : 0x" << std::hex << std::setw(2) << std::setfill('0') << mi.next_address << std::dec << "\n";
        oss << "  End of Seq    : " << (mi.end_of_sequence ? "YES" : "NO") << "\n";
        oss << "  Micro-Ops Ran : " << stats.microOps << "\n";
    }
    oss << "==============================================================\n";
    return oss.str();
}

std::string CPU::dump_memory(uint32_t start, uint32_t len) const { return mem.dump(start, len); }
std::string CPU::dump_stack() const { return stack_rt ? stack_rt->render() : ""; }

std::string CPU::dump_stats() const {
    std::stringstream ss;
    ss << "Execution Statistics:\n"
       << "  Instructions: " << stats.instructions << "\n"
       << "  Cycles: " << stats.cycles << "\n"
       << "  CPI: " << (stats.instructions ? (double)stats.cycles / stats.instructions : 0.0) << "\n"
       << "  Stalls: " << stats.stalls << "\n"
       << "  Branches: " << stats.branches << " (Taken: " << stats.taken << ")\n";
    if (stats.branch_predictions > 0) {
        double hit_pct = (double)stats.branch_hits / stats.branch_predictions * 100.0;
        ss << "  Branch Predictor: " << bp.mode_name() << "\n"
           << "  Branch Accuracy: " << stats.branch_hits << " / " << stats.branch_predictions
           << " (" << std::fixed << std::setprecision(1) << hit_pct << "% hit rate, "
           << stats.branch_misses << " misses)\n";
    }
    ss << "  Memory Reads: " << stats.memReads << ", Writes: " << stats.memWrites << "\n"
       << "  ALU Operations: " << stats.aluOps << "\n"
       << "  Micro-Operations: " << stats.microOps << "\n"
       << "  Data Hazards: " << stats.hazards_detected << ", Forwards: " << stats.forwards << "\n";
    if (stats.store_to_load_forwards > 0) {
        ss << "  Store-to-Load Forwards: " << stats.store_to_load_forwards << "\n";
    }
    ss << "  Pipeline Flushes: " << stats.pipeline_flushes << "\n";
    if (icache_.accesses() > 0) {
        ss << "  " << icache_.dump_stats();
    }
    if (dcache_.accesses() > 0) {
        ss << "  " << dcache_.dump_stats();
    }
    if (bp.ras().get_pushes() > 0 || bp.ras().get_pops() > 0) {
        ss << "  Return Address Stack (RAS): Pushes=" << bp.ras().get_pushes()
           << ", Pops=" << bp.ras().get_pops()
           << ", Hits=" << bp.ras().get_hits() << "\n";
    }
    return ss.str();
}

std::string CPU::pipeline_name() const { return pipe == Pipeline::P4 ? "4-stage" : "6-stage"; }

void CPU::set_alu_algorithms(AluAlgorithm mul, AluAlgorithm add, AluAlgorithm div, AluAlgorithm shift) {
    alu.set_mul_algorithm(mul);
    alu.set_add_algorithm(add);
    alu.set_div_algorithm(div);
    alu.set_shift_algorithm(shift);
}

} // namespace risc201
