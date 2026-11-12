#pragma once
#include "memory.hpp"
#include <cstdint>
#include <array>
#include <string>
#include <sstream>
#include <iomanip>

namespace risc201 {

// 1KB 2-Way Set-Associative L1 Cache with LRU Replacement & 32-Byte Cache Lines
class L1Cache {
public:
    static constexpr size_t LINE_SIZE = 32;        // 32-byte cache line (8 words)
    static constexpr size_t NUM_WAYS = 2;          // 2-way set-associative
    static constexpr size_t NUM_SETS = 16;         // 16 sets (16 * 2 * 32 = 1024 bytes)

    struct CacheLine {
        bool valid = false;
        bool dirty = false;
        uint32_t tag = 0;
        uint8_t lru = 0; // 0 or 1
        std::array<uint8_t, LINE_SIZE> data{};
    };

private:
    std::string name_;
    std::array<std::array<CacheLine, NUM_WAYS>, NUM_SETS> sets_{};
    uint64_t accesses_ = 0;
    uint64_t hits_ = 0;
    uint64_t misses_ = 0;
    uint64_t evictions_ = 0;

    static uint32_t get_offset(uint32_t addr) { return addr & (LINE_SIZE - 1); }
    static uint32_t get_set(uint32_t addr) { return (addr / LINE_SIZE) & (NUM_SETS - 1); }
    static uint32_t get_tag(uint32_t addr) { return (addr / (LINE_SIZE * NUM_SETS)); }

public:
    explicit L1Cache(const std::string& name = "L1") : name_(name) {
        reset();
    }

    void reset() {
        for (auto& set : sets_) {
            for (auto& line : set) {
                line = CacheLine{};
            }
        }
        accesses_ = 0;
        hits_ = 0;
        misses_ = 0;
        evictions_ = 0;
    }

    // Access cache line; returns true if Hit, false if Miss (and fills line from backing memory)
    bool access(uint32_t addr, bool is_write, Memory& mem) {
        accesses_++;
        uint32_t set_idx = get_set(addr);
        uint32_t tag = get_tag(addr);
        auto& set = sets_[set_idx];

        // 1. Check for Hit in Way 0 or Way 1
        for (size_t way = 0; way < NUM_WAYS; ++way) {
            if (set[way].valid && set[way].tag == tag) {
                hits_++;
                set[way].lru = 1;
                set[1 - way].lru = 0; // other way is now LRU
                if (is_write) {
                    set[way].dirty = true;
                }
                return true;
            }
        }

        // 2. Miss: Select victim way (LRU)
        misses_++;
        size_t victim_way = 0;
        if (set[0].valid && !set[1].valid) {
            victim_way = 1;
        } else if (!set[0].valid) {
            victim_way = 0;
        } else {
            victim_way = (set[0].lru == 0) ? 0 : 1;
            evictions_++;
        }

        // Write-back if victim is dirty
        if (set[victim_way].valid && set[victim_way].dirty) {
            uint32_t line_addr = (set[victim_way].tag * NUM_SETS + set_idx) * LINE_SIZE;
            for (size_t i = 0; i < LINE_SIZE; ++i) {
                if (line_addr + i < mem.size()) {
                    mem.write8(line_addr + i, set[victim_way].data[i]);
                }
            }
        }

        // Line fill from backing memory
        uint32_t block_base = addr & ~(LINE_SIZE - 1);
        set[victim_way].valid = true;
        set[victim_way].dirty = is_write;
        set[victim_way].tag = tag;
        set[victim_way].lru = 1;
        set[1 - victim_way].lru = 0;

        for (size_t i = 0; i < LINE_SIZE; ++i) {
            if (block_base + i < mem.size()) {
                set[victim_way].data[i] = mem.read8(block_base + i);
            }
        }

        return false;
    }

    // Read 32-bit word from cache; loads line from memory if miss
    uint32_t read32(uint32_t addr, Memory& mem) {
        if (static_cast<uint64_t>(addr) + 3 >= mem.size()) {
            throw std::out_of_range("Memory read32 out of bounds");
        }
        uint32_t offset = get_offset(addr);
        if (offset + 4 <= LINE_SIZE) {
            access(addr, false, mem);
            uint32_t set_idx = get_set(addr);
            uint32_t tag = get_tag(addr);
            auto& set = sets_[set_idx];

            for (size_t way = 0; way < NUM_WAYS; ++way) {
                if (set[way].valid && set[way].tag == tag) {
                    return static_cast<uint32_t>(set[way].data[offset]) |
                           (static_cast<uint32_t>(set[way].data[offset + 1]) << 8) |
                           (static_cast<uint32_t>(set[way].data[offset + 2]) << 16) |
                           (static_cast<uint32_t>(set[way].data[offset + 3]) << 24);
                }
            }
            return mem.read32(addr);
        } else {
            // Cross-boundary read across two cache lines
            size_t bytes_first = LINE_SIZE - offset;
            size_t bytes_second = 4 - bytes_first;

            access(addr, false, mem);
            access(addr + bytes_first, false, mem);

            uint32_t set_idx1 = get_set(addr);
            uint32_t tag1 = get_tag(addr);
            auto& set1 = sets_[set_idx1];

            uint32_t set_idx2 = get_set(addr + bytes_first);
            uint32_t tag2 = get_tag(addr + bytes_first);
            auto& set2 = sets_[set_idx2];

            const CacheLine* line1 = nullptr;
            for (size_t way = 0; way < NUM_WAYS; ++way) {
                if (set1[way].valid && set1[way].tag == tag1) {
                    line1 = &set1[way];
                    break;
                }
            }

            const CacheLine* line2 = nullptr;
            for (size_t way = 0; way < NUM_WAYS; ++way) {
                if (set2[way].valid && set2[way].tag == tag2) {
                    line2 = &set2[way];
                    break;
                }
            }

            if (line1 && line2) {
                uint32_t val = 0;
                for (size_t i = 0; i < bytes_first; ++i) {
                    val |= (static_cast<uint32_t>(line1->data[offset + i]) << (8 * i));
                }
                for (size_t i = 0; i < bytes_second; ++i) {
                    val |= (static_cast<uint32_t>(line2->data[i]) << (8 * (bytes_first + i)));
                }
                return val;
            }
            return mem.read32(addr);
        }
    }

    // Write 32-bit word to cache with write allocation & write-back
    void write32(uint32_t addr, uint32_t val, Memory& mem) {
        if (static_cast<uint64_t>(addr) + 3 >= mem.size()) {
            throw std::out_of_range("Memory write32 out of bounds");
        }
        uint32_t offset = get_offset(addr);
        if (offset + 4 <= LINE_SIZE) {
            access(addr, true, mem);
            uint32_t set_idx = get_set(addr);
            uint32_t tag = get_tag(addr);
            auto& set = sets_[set_idx];

            for (size_t way = 0; way < NUM_WAYS; ++way) {
                if (set[way].valid && set[way].tag == tag) {
                    set[way].dirty = true;
                    set[way].data[offset]     = static_cast<uint8_t>(val & 0xFF);
                    set[way].data[offset + 1] = static_cast<uint8_t>((val >> 8) & 0xFF);
                    set[way].data[offset + 2] = static_cast<uint8_t>((val >> 16) & 0xFF);
                    set[way].data[offset + 3] = static_cast<uint8_t>((val >> 24) & 0xFF);
                    return;
                }
            }
        } else {
            // Cross-boundary write across two cache lines
            size_t bytes_first = LINE_SIZE - offset;
            size_t bytes_second = 4 - bytes_first;

            access(addr, true, mem);
            access(addr + bytes_first, true, mem);

            uint32_t set_idx1 = get_set(addr);
            uint32_t tag1 = get_tag(addr);
            auto& set1 = sets_[set_idx1];

            uint32_t set_idx2 = get_set(addr + bytes_first);
            uint32_t tag2 = get_tag(addr + bytes_first);
            auto& set2 = sets_[set_idx2];

            for (size_t way = 0; way < NUM_WAYS; ++way) {
                if (set1[way].valid && set1[way].tag == tag1) {
                    set1[way].dirty = true;
                    for (size_t i = 0; i < bytes_first; ++i) {
                        set1[way].data[offset + i] = static_cast<uint8_t>((val >> (8 * i)) & 0xFF);
                    }
                    break;
                }
            }

            for (size_t way = 0; way < NUM_WAYS; ++way) {
                if (set2[way].valid && set2[way].tag == tag2) {
                    set2[way].dirty = true;
                    for (size_t i = 0; i < bytes_second; ++i) {
                        set2[way].data[i] = static_cast<uint8_t>((val >> (8 * (bytes_first + i))) & 0xFF);
                    }
                    break;
                }
            }
        }
    }

    // Flush dirty lines back to memory
    void flush(Memory& mem) {
        for (size_t set_idx = 0; set_idx < NUM_SETS; ++set_idx) {
            for (size_t way = 0; way < NUM_WAYS; ++way) {
                if (sets_[set_idx][way].valid && sets_[set_idx][way].dirty) {
                    uint32_t line_addr = (sets_[set_idx][way].tag * NUM_SETS + set_idx) * LINE_SIZE;
                    for (size_t i = 0; i < LINE_SIZE; ++i) {
                        if (line_addr + i < mem.size()) {
                            mem.write8(line_addr + i, sets_[set_idx][way].data[i]);
                        }
                    }
                    sets_[set_idx][way].dirty = false;
                }
            }
        }
    }

    uint64_t accesses() const { return accesses_; }
    uint64_t hits() const { return hits_; }
    uint64_t misses() const { return misses_; }
    uint64_t evictions() const { return evictions_; }

    double hit_rate() const {
        return accesses_ ? (static_cast<double>(hits_) / accesses_ * 100.0) : 0.0;
    }

    std::string dump_stats() const {
        std::ostringstream oss;
        oss << name_ << " Cache (1KB, 2-Way, 32B Line): "
            << hits_ << " / " << accesses_ << " hits ("
            << std::fixed << std::setprecision(1) << hit_rate() << "% hit rate, "
            << misses_ << " misses, " << evictions_ << " evictions)\n";
        return oss.str();
    }
};

} // namespace risc201

