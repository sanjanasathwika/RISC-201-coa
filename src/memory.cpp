#include "risc201/memory.hpp"
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <cctype>

namespace risc201 {

uint8_t Memory::read8(uint32_t addr) const {
    if (addr >= b.size()) {
        throw std::out_of_range("Memory read8 out of bounds");
    }
    return b[addr];
}

uint32_t Memory::read32(uint32_t addr) const {
    if (static_cast<uint64_t>(addr) + 3 >= b.size()) {
        throw std::out_of_range("Memory read32 out of bounds");
    }
    return static_cast<uint32_t>(b[addr]) |
           (static_cast<uint32_t>(b[addr + 1]) << 8) |
           (static_cast<uint32_t>(b[addr + 2]) << 16) |
           (static_cast<uint32_t>(b[addr + 3]) << 24);
}

void Memory::write8(uint32_t addr, uint8_t val) {
    if (addr >= b.size()) {
        throw std::out_of_range("Memory write8 out of bounds");
    }
    b[addr] = val;
}

void Memory::write32(uint32_t addr, uint32_t val) {
    if (static_cast<uint64_t>(addr) + 3 >= b.size()) {
        throw std::out_of_range("Memory write32 out of bounds");
    }
    b[addr] = static_cast<uint8_t>(val & 0xFF);
    b[addr + 1] = static_cast<uint8_t>((val >> 8) & 0xFF);
    b[addr + 2] = static_cast<uint8_t>((val >> 16) & 0xFF);
    b[addr + 3] = static_cast<uint8_t>((val >> 24) & 0xFF);
}

std::string Memory::dump(uint32_t start, uint32_t len) const {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::uppercase;
    
    uint32_t end = start + len;
    if (end < start || end > b.size()) {
        end = static_cast<uint32_t>(b.size());
    }
    
    for (uint32_t i = start; i < end; i += 16) {
        oss << "0x" << std::setw(4) << i << ": ";
        
        std::string ascii = "";
        for (uint32_t j = 0; j < 16; ++j) {
            if (i + j < end) {
                uint8_t val = b[i + j];
                oss << std::setw(2) << static_cast<int>(val) << " ";
                if (j == 7) oss << " ";
                
                if (std::isprint(val)) {
                    ascii += static_cast<char>(val);
                } else {
                    ascii += '.';
                }
            } else {
                oss << "   ";
                if (j == 7) oss << " ";
            }
        }
        
        oss << " |" << ascii << "|\n";
    }
    
    return oss.str();
}

} // namespace risc201
