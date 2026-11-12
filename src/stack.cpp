#include "risc201/stack.hpp"
#include <sstream>
#include <iomanip>

namespace risc201 {

StackRuntime::StackRuntime(Memory& m, uint32_t low, uint32_t high)
    : low_(low), high_(high), sp_(high), mem_(&m) {
    if (low >= high || high > mem_->size()) {
        throw std::invalid_argument("Invalid stack boundaries");
    }
}

void StackRuntime::push(uint32_t value) {
    if (sp_ < low_ + 4) {
        throw StackOverflow("Stack overflow: SP crossed lower bound");
    }
    sp_ -= 4;
    mem_->write32(sp_, value);
}

uint32_t StackRuntime::pop() {
    if (sp_ + 4 > high_) {
        throw StackUnderflow("Stack underflow: SP crossed upper bound");
    }
    uint32_t value = mem_->read32(sp_);
    sp_ += 4;
    return value;
}

void StackRuntime::set_pointer(uint32_t sp) {
    sp_ = sp;
}

void StackRuntime::check_bounds(uint32_t sp) const {
    if (sp < low_ || sp > high_) {
        std::ostringstream oss;
        oss << "Stack corruption: SP (0x" << std::hex << sp << ") out of bounds [0x" << low_ << " - 0x" << high_ << "]";
        throw StackCorruption(oss.str());
    }
}

void StackRuntime::push_frame(uint32_t ret_addr, uint32_t sp, const std::string& label) {
    frames_.push_back(Frame{ret_addr, sp, label});
}

void StackRuntime::pop_frame() {
    if (!frames_.empty()) {
        frames_.pop_back();
    }
}

std::string StackRuntime::render() const {
    std::ostringstream oss;
    oss << "╔══════════════════════ STACK MEMORY ══════════════════════╗\n";
    oss << "║ Address    Value       Description                       ║\n";
    oss << "╠══════════════════════════════════════════════════════════╣\n";

    // Dump stack words from current sp up to high_ (or low_, whichever makes sense; up to high_)
    if (sp_ <= high_) {
        for (uint32_t addr = sp_; addr < high_ && addr >= sp_; addr += 4) {
            oss << "║ 0x" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << addr << "     ";
            
            uint32_t val = mem_->read32(addr);
            oss << "0x" << std::setw(8) << val << "  ";

            std::string desc;
            if (addr == sp_) {
                desc = "<- SP";
            }
            
            for (const auto& frame : frames_) {
                if (frame.saved_sp == addr) {
                    std::ostringstream f_oss;
                    f_oss << "[Frame: " << (frame.label.empty() ? "anonymous" : frame.label) 
                          << ", RA=0x" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << frame.return_addr << "]";
                    if (!desc.empty()) {
                        desc += "  " + f_oss.str();
                    } else {
                        desc = f_oss.str();
                    }
                }
            }
            
            oss << std::left << std::setw(33) << std::setfill(' ') << desc << " ║\n";
        }
    }

    if (!frames_.empty()) {
        oss << "╠══════════════════════ CALL FRAMES ═══════════════════════╣\n";
        for (size_t fi = 0; fi < frames_.size(); ++fi) {
            const auto& fr = frames_[fi];
            std::ostringstream fr_oss;
            fr_oss << "Frame #" << (fi + 1) << ": RA=0x" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << fr.return_addr
                   << "  Caller SP=0x" << std::setw(4) << fr.saved_sp;
            if (!fr.label.empty()) fr_oss << " (" << fr.label << ")";
            oss << "║ " << std::left << std::setw(56) << std::setfill(' ') << fr_oss.str() << " ║\n";
        }
    }

    oss << "╠══════════════════════════════════════════════════════════╣\n";
    
    uint32_t used = high_ - sp_;
    uint32_t capacity = high_ - low_;
    double percent = capacity > 0 ? (used * 100.0) / capacity : 0.0;
    
    // Format usage string
    std::ostringstream usage_oss;
    usage_oss << "Stack Usage: " << std::dec << used << " / " << capacity << " bytes (" << std::fixed << std::setprecision(2) << percent << "%)";
    oss << "║ " << std::left << std::setw(56) << std::setfill(' ') << usage_oss.str() << " ║\n";

    std::ostringstream info_oss;
    info_oss << "Frames: " << std::dec << frames_.size() 
             << "  SP: 0x" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << sp_
             << "  Range: [0x" << low_ << " - 0x" << high_ << "]";
    oss << "║ " << std::left << std::setw(56) << std::setfill(' ') << info_oss.str() << " ║\n";

    oss << "╚══════════════════════════════════════════════════════════╝\n";
    
    return oss.str();
}

} // namespace risc201
