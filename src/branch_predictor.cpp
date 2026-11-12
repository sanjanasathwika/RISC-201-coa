#include "risc201/branch_predictor.hpp"
#include <sstream>
#include <iomanip>

namespace risc201 {

BranchPredictor::BranchPredictor(BranchPredictorMode m) : mode_(m) {
    reset();
}

void BranchPredictor::set_mode(BranchPredictorMode m) {
    mode_ = m;
}

void BranchPredictor::reset() {
    for (auto& entry : btb_) {
        entry = BTBEntry{};
    }
    ras_.reset();
}

Prediction BranchPredictor::predict(uint32_t branch_pc, const Instruction& instr) const {
    Prediction pred;
    bool is_br = is_branch(instr.op) || instr.op == Opcode::RET;
    if (!is_br) {
        return pred;
    }
    pred.is_branch = true;

    // Static Always-Not-Taken mode
    if (mode_ == BranchPredictorMode::STATIC_NOT_TAKEN) {
        pred.predicted_taken = false;
        pred.predicted_target = branch_pc + 4;
        return pred;
    }

    // Speculative Return Address Stack (RAS) for RET:
    // Slashes subroutine return penalty to 0 cycles by popping exact target!
    if (instr.op == Opcode::RET) {
        if (!ras_.empty()) {
            pred.predicted_taken = true;
            pred.predicted_target = ras_.peek();
            return pred;
        }
    }

    // Dynamic 2-Bit Predictor mode
    // 1. Unconditional branches (B and CALL) are always taken
    if (instr.op == Opcode::B || instr.op == Opcode::CALL) {
        pred.predicted_taken = true;
        pred.predicted_target = branch_pc + 4 + instr.offset * 4;
        return pred;
    }

    // 2. Conditional branches (BEQ, BGT) and RET look up BTB
    size_t idx = get_index(branch_pc);
    const auto& entry = btb_[idx];

    if (entry.valid && entry.tag == branch_pc) {
        bool taken = (entry.state == BimodalState::WEAKLY_TAKEN || 
                      entry.state == BimodalState::STRONGLY_TAKEN);
        pred.predicted_taken = taken;
        pred.predicted_target = taken ? entry.target_pc : (branch_pc + 4);
    } else {
        // Cold miss: loop branches (negative PC offset) default to Weakly Taken,
        // forward branches default to Weakly Not Taken.
        if (instr.offset < 0) {
            pred.predicted_taken = true;
            pred.predicted_target = branch_pc + 4 + instr.offset * 4;
        } else {
            pred.predicted_taken = false;
            pred.predicted_target = branch_pc + 4;
        }
    }

    return pred;
}

void BranchPredictor::update(uint32_t branch_pc, bool actually_taken, uint32_t actual_target, Opcode op) {
    if (op == Opcode::CALL) {
        ras_.push(branch_pc + 4);
    } else if (op == Opcode::RET) {
        uint32_t pred_ret = ras_.pop();
        if (pred_ret == actual_target) {
            ras_.record_hit();
        } else {
            ras_.record_miss();
        }
    }

    if (mode_ == BranchPredictorMode::STATIC_NOT_TAKEN) {
        return;
    }

    size_t idx = get_index(branch_pc);
    auto& entry = btb_[idx];

    if (!entry.valid || entry.tag != branch_pc) {
        // Allocate new BTB entry
        entry.valid = true;
        entry.tag = branch_pc;
        entry.target_pc = actual_target;
        entry.state = actually_taken ? BimodalState::WEAKLY_TAKEN : BimodalState::WEAKLY_NOT_TAKEN;
    } else {
        // Update cached target address if branch was taken
        if (actually_taken) {
            entry.target_pc = actual_target;
        }

        // 2-bit Saturating Counter State Transitions
        if (actually_taken) {
            switch (entry.state) {
                case BimodalState::STRONGLY_NOT_TAKEN: entry.state = BimodalState::WEAKLY_NOT_TAKEN; break;
                case BimodalState::WEAKLY_NOT_TAKEN:   entry.state = BimodalState::WEAKLY_TAKEN; break;
                case BimodalState::WEAKLY_TAKEN:       entry.state = BimodalState::STRONGLY_TAKEN; break;
                case BimodalState::STRONGLY_TAKEN:     entry.state = BimodalState::STRONGLY_TAKEN; break;
            }
        } else {
            switch (entry.state) {
                case BimodalState::STRONGLY_TAKEN:     entry.state = BimodalState::WEAKLY_TAKEN; break;
                case BimodalState::WEAKLY_TAKEN:       entry.state = BimodalState::WEAKLY_NOT_TAKEN; break;
                case BimodalState::WEAKLY_NOT_TAKEN:   entry.state = BimodalState::STRONGLY_NOT_TAKEN; break;
                case BimodalState::STRONGLY_NOT_TAKEN: entry.state = BimodalState::STRONGLY_NOT_TAKEN; break;
            }
        }
    }
}

const BTBEntry* BranchPredictor::get_entry(uint32_t branch_pc) const {
    size_t idx = get_index(branch_pc);
    if (btb_[idx].valid && btb_[idx].tag == branch_pc) {
        return &btb_[idx];
    }
    return nullptr;
}

const char* BranchPredictor::mode_name() const {
    switch (mode_) {
        case BranchPredictorMode::STATIC_NOT_TAKEN: return "Static (Always Not-Taken)";
        case BranchPredictorMode::DYNAMIC_2BIT:     return "Dynamic 2-Bit Saturating Counter (Bimodal)";
        default: return "Unknown";
    }
}

const char* BranchPredictor::state_name(BimodalState s) {
    switch (s) {
        case BimodalState::STRONGLY_NOT_TAKEN: return "00 (Strongly Not-Taken)";
        case BimodalState::WEAKLY_NOT_TAKEN:   return "01 (Weakly Not-Taken)";
        case BimodalState::WEAKLY_TAKEN:       return "10 (Weakly Taken)";
        case BimodalState::STRONGLY_TAKEN:     return "11 (Strongly Taken)";
        default: return "??";
    }
}

std::string BranchPredictor::dump_state() const {
    std::ostringstream oss;
    oss << "+-------------------------------------------------------------------------+\n";
    oss << "|                       BRANCH TARGET BUFFER (BTB)                        |\n";
    oss << "+-------+--------+------------+------------+------------------------------+\n";
    oss << "| Index | Valid  | Branch PC  | Target PC  | 2-Bit Counter State          |\n";
    oss << "+-------+--------+------------+------------+------------------------------+\n";

    size_t count = 0;
    for (size_t i = 0; i < BTB_SIZE; ++i) {
        const auto& e = btb_[i];
        if (e.valid) {
            count++;
            oss << "| " << std::setw(5) << i << " | "
                << std::setw(6) << "YES" << " | 0x"
                << std::hex << std::setw(8) << std::setfill('0') << e.tag << std::dec << " | 0x"
                << std::hex << std::setw(8) << std::setfill('0') << e.target_pc << std::dec << " | "
                << std::setfill(' ') << std::left << std::setw(28) << state_name(e.state) << "|\n";
        }
    }
    if (count == 0) {
        oss << "|                              [BTB IS EMPTY]                             |\n";
    }
    oss << "+-------+--------+------------+------------+------------------------------+\n";
    oss << " Mode: " << mode_name() << " | Active Entries: " << count << " / " << BTB_SIZE << "\n";
    oss << " Return Address Stack (RAS 16-entry): "
        << "Pushes=" << ras_.get_pushes() << ", Pops=" << ras_.get_pops()
        << ", Hits=" << ras_.get_hits() << ", Misses=" << ras_.get_misses()
        << " (Active Stack Depth: " << ras_.size() << ")\n";
    return oss.str();
}

} // namespace risc201
