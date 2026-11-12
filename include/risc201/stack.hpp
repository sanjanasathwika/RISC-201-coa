#pragma once
#include "memory.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <stdexcept>

namespace risc201 {

class StackRuntime {
public:
    // Exception types
    struct StackOverflow : std::runtime_error { using runtime_error::runtime_error; };
    struct StackUnderflow : std::runtime_error { using runtime_error::runtime_error; };
    struct StackCorruption : std::runtime_error { using runtime_error::runtime_error; };

    // Stack frame tracking
    struct Frame {
        uint32_t return_addr;
        uint32_t saved_sp;
        std::string label;
    };

private:
    uint32_t low_, high_, sp_;
    Memory* mem_;
    std::vector<Frame> frames_;

public:
    StackRuntime(Memory& m, uint32_t low, uint32_t high);

    // Stack operations
    void push(uint32_t value);
    uint32_t pop();

    // SP management
    uint32_t pointer() const { return sp_; }
    void set_pointer(uint32_t sp);
    void check_bounds(uint32_t sp) const;  // throws if sp out of stack region

    // Frame tracking (for CALL/RET visualization)
    void push_frame(uint32_t ret_addr, uint32_t sp, const std::string& label = "");
    void pop_frame();
    const std::vector<Frame>& frames() const { return frames_; }

    // Visualization
    std::string render() const;

    // Bounds
    uint32_t low() const { return low_; }
    uint32_t high() const { return high_; }
};

} // namespace risc201
