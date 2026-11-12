#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace risc201 {

class Memory {
    std::vector<uint8_t> b;
public:
    explicit Memory(size_t bytes = 64 * 1024) : b(bytes, 0) {}
    size_t size() const { return b.size(); }
    uint8_t read8(uint32_t addr) const;
    uint32_t read32(uint32_t addr) const;
    void write8(uint32_t addr, uint8_t val);
    void write32(uint32_t addr, uint32_t val);
    // Hex dump of memory region
    std::string dump(uint32_t start, uint32_t len) const;
};

} // namespace risc201
