#include "risc201/alu.hpp"
#include <cstdint>
#include <cmath>
#include <stdexcept>
#include <limits>
#include <array>
#include <utility>

namespace risc201 {

void ALU::set_mul_algorithm(AluAlgorithm a) { mul_algo_ = a; }
void ALU::set_add_algorithm(AluAlgorithm a) { add_algo_ = a; }
void ALU::set_div_algorithm(AluAlgorithm a) { div_algo_ = a; }
void ALU::set_shift_algorithm(AluAlgorithm a) { shift_algo_ = a; }

const char* ALU::op_name(AluOp op) {
    switch (op) {
        case AluOp::ADD: return "ADD";
        case AluOp::SUB: return "SUB";
        case AluOp::MUL: return "MUL";
        case AluOp::DIV: return "DIV";
        case AluOp::MOD: return "MOD";
        case AluOp::AND: return "AND";
        case AluOp::OR: return "OR";
        case AluOp::NOT: return "NOT";
        case AluOp::LSL: return "LSL";
        case AluOp::LSR: return "LSR";
        case AluOp::ASR: return "ASR";
        case AluOp::CMP: return "CMP";
        case AluOp::PASS: return "PASS";
        default: return "UNKNOWN";
    }
}

const char* ALU::algo_name(AluAlgorithm a) {
    switch (a) {
        case AluAlgorithm::NAIVE: return "Naive";
        case AluAlgorithm::BOOTH: return "Booth's Multiplication (Radix-2, 32 cycles)";
        case AluAlgorithm::BOOTH_RADIX4: return "Radix-4 Modified Booth's Multiplication (16 cycles)";
        case AluAlgorithm::CLA: return "Carry-Lookahead Adder";
        case AluAlgorithm::CSLA: return "Carry-Select Adder (CSLA)";
        case AluAlgorithm::KOGGE_STONE: return "Kogge-Stone Parallel-Prefix Adder (5 levels)";
        case AluAlgorithm::NONRESTORING: return "Non-Restoring Division";
        case AluAlgorithm::NEWTON_RAPHSON: return "Double-Precision Newton-Raphson Fast Divider & Modulo (5 iterations)";
        case AluAlgorithm::BARREL: return "Barrel Shifter";
        default: return "Unknown";
    }
}

AluResult ALU::execute(AluOp op, int32_t a, int32_t b) const {
    AluResult result;
    
    switch (op) {
        case AluOp::ADD: {
            if (add_algo_ == AluAlgorithm::CLA) {
                result.value = cla_add(a, b, result.overflow);
            } else if (add_algo_ == AluAlgorithm::KOGGE_STONE) {
                result.value = kogge_stone_add(a, b, result.overflow);
            } else if (add_algo_ == AluAlgorithm::CSLA) {
                result.value = carry_select_add(a, b, result.overflow);
            } else {
                int64_t val = static_cast<int64_t>(a) + b;
                result.value = static_cast<int32_t>(val);
                result.overflow = (val > INT32_MAX || val < INT32_MIN);
            }
            break;
        }
        case AluOp::SUB: {
            int64_t val = static_cast<int64_t>(a) - b;
            result.value = static_cast<int32_t>(val);
            result.overflow = (val > INT32_MAX || val < INT32_MIN);
            break;
        }
        case AluOp::MUL: {
            if (mul_algo_ == AluAlgorithm::BOOTH) {
                result.value = booth_multiply(a, b, result.overflow);
            } else if (mul_algo_ == AluAlgorithm::BOOTH_RADIX4) {
                result.value = booth_radix4_multiply(a, b, result.overflow);
            } else {
                int64_t val = static_cast<int64_t>(a) * b;
                result.value = static_cast<int32_t>(val);
                result.overflow = (val > INT32_MAX || val < INT32_MIN);
            }
            break;
        }
        case AluOp::DIV: {
            if (b == 0) {
                result.divide_by_zero = true;
                result.value = 0;
            } else if (a == INT32_MIN && b == -1) {
                result.overflow = true;
                result.value = INT32_MIN;
            } else {
                if (div_algo_ == AluAlgorithm::NONRESTORING) {
                    result.value = nonrestoring_divide(a, b, result.divide_by_zero);
                } else if (div_algo_ == AluAlgorithm::NEWTON_RAPHSON) {
                    result.value = newton_raphson_divide(a, b, result.divide_by_zero);
                } else {
                    result.value = a / b;
                }
            }
            break;
        }
        case AluOp::MOD: {
            if (b == 0) {
                result.divide_by_zero = true;
                result.value = 0;
            } else if (a == INT32_MIN && b == -1) {
                result.overflow = true;
                result.value = 0;
            } else {
                if (div_algo_ == AluAlgorithm::NEWTON_RAPHSON) {
                    result.value = newton_raphson_modulo(a, b, result.divide_by_zero);
                } else {
                    result.value = a % b;
                }
            }
            break;
        }
        case AluOp::AND: {
            result.value = a & b;
            break;
        }
        case AluOp::OR: {
            result.value = a | b;
            break;
        }
        case AluOp::NOT: {
            result.value = ~b;
            break;
        }
        case AluOp::LSL:
        case AluOp::LSR:
        case AluOp::ASR: {
            if (shift_algo_ == AluAlgorithm::BARREL) {
                result.value = barrel_shift(a, b, op);
            } else {
                uint32_t shift = static_cast<uint32_t>(b) & 31;
                if (op == AluOp::LSL) {
                    result.value = static_cast<int32_t>(static_cast<uint32_t>(a) << shift);
                } else if (op == AluOp::LSR) {
                    result.value = static_cast<int32_t>(static_cast<uint32_t>(a) >> shift);
                } else { // ASR
                    result.value = a >> shift;
                }
            }
            break;
        }
        case AluOp::CMP: {
            result.value = 0; 
            result.E = (a == b);
            result.GT = (a > b);
            return result;
        }
        case AluOp::PASS: {
            result.value = a;
            break;
        }
    }

    if (op != AluOp::CMP) {
        result.E = (result.value == 0);
        result.GT = (result.value > 0);
    }

    return result;
}

int32_t ALU::booth_multiply(int32_t a, int32_t b, bool& overflow) {
    int32_t A = 0;
    int32_t Q = a;
    int32_t M = b;
    int Q_neg1 = 0;

    for (int i = 0; i < 32; ++i) {
        int Q0 = Q & 1;
        if (Q0 == 0 && Q_neg1 == 1) {
            A = A + M;
        } else if (Q0 == 1 && Q_neg1 == 0) {
            A = A - M;
        }

        Q_neg1 = Q0;
        
        // Arithmetic right shift {A, Q}
        uint32_t A_lsb = A & 1;
        uint32_t sign = (A & 0x80000000);
        A = static_cast<int32_t>((static_cast<uint32_t>(A) >> 1) | sign); 
        Q = static_cast<int32_t>((static_cast<uint32_t>(Q) >> 1) | (A_lsb << 31));
    }
    
    // Check overflow: A should be sign extension of Q
    bool sign_match = (Q & 0x80000000) ? (A == -1) : (A == 0);
    overflow = !sign_match;

    return Q;
}

int32_t ALU::booth_radix4_multiply(int32_t a, int32_t b, bool& overflow) {
    // a = Multiplier (Q), b = Multiplicand (M)
    int64_t A = 0;
    uint32_t Q = static_cast<uint32_t>(a);
    int64_t M = static_cast<int64_t>(b);
    int q_neg1 = 0;

    // Radix-4 Modified Booth processes 2 bits per cycle using a 3-bit sliding window:
    // exactly 16 iterations for 32-bit operands (50% fewer cycles than Radix-2 Booth)
    for (int i = 0; i < 16; ++i) {
        int q0 = Q & 1;
        int q1 = (Q >> 1) & 1;
        int code = (q1 << 2) | (q0 << 1) | q_neg1;

        switch (code) {
            case 1: // 001 -> +1 * M
            case 2: // 010 -> +1 * M
                A += M;
                break;
            case 3: // 011 -> +2 * M
                A += (2 * M);
                break;
            case 4: // 100 -> -2 * M
                A -= (2 * M);
                break;
            case 5: // 101 -> -1 * M
            case 6: // 110 -> -1 * M
                A -= M;
                break;
            default: // 000, 111 -> 0 * M
                break;
        }

        q_neg1 = q1;

        // Combined Arithmetic Right Shift of {A, Q} by 2 bits
        uint32_t low_two_bits_A = static_cast<uint32_t>(A & 3);
        Q = (Q >> 2) | (low_two_bits_A << 30);
        A >>= 2;
    }

    // Check signed 32-bit overflow: upper register A must be the sign extension of lower register Q
    int32_t final_q = static_cast<int32_t>(Q);
    bool sign_match = (final_q < 0) ? (A == -1) : (A == 0);
    overflow = !sign_match;

    return final_q;
}

int32_t ALU::cla_add(int32_t a, int32_t b, bool& overflow) {
    uint32_t ua = static_cast<uint32_t>(a);
    uint32_t ub = static_cast<uint32_t>(b);
    uint32_t sum = 0;
    
    uint32_t p = ua ^ ub; // Bit-level propagate
    uint32_t g = ua & ub; // Bit-level generate
    
    uint32_t c_in = 0;
    uint32_t c_in_msb = 0;
    uint32_t c_out_msb = 0;
    
    // 32-bit adder composed of eight 4-bit Carry-Lookahead Adder (CLA) blocks
    for (int block = 0; block < 8; ++block) {
        int base = block * 4;
        uint32_t p0 = (p >> (base + 0)) & 1;
        uint32_t p1 = (p >> (base + 1)) & 1;
        uint32_t p2 = (p >> (base + 2)) & 1;
        uint32_t p3 = (p >> (base + 3)) & 1;
        
        uint32_t g0 = (g >> (base + 0)) & 1;
        uint32_t g1 = (g >> (base + 1)) & 1;
        uint32_t g2 = (g >> (base + 2)) & 1;
        uint32_t g3 = (g >> (base + 3)) & 1;
        
        // 4-bit CLA lookahead carry equations computed in parallel (no serial ripple)
        uint32_t c0 = c_in;
        uint32_t c1 = g0 | (p0 & c0);
        uint32_t c2 = g1 | (p1 & g0) | (p1 & p0 & c0);
        uint32_t c3 = g2 | (p2 & g1) | (p2 & p1 & g0) | (p2 & p1 & p0 & c0);
        
        // Sum bits: S_i = P_i ^ C_i
        uint32_t s0 = p0 ^ c0;
        uint32_t s1 = p1 ^ c1;
        uint32_t s2 = p2 ^ c2;
        uint32_t s3 = p3 ^ c3;
        
        sum |= (s0 << (base + 0));
        sum |= (s1 << (base + 1));
        sum |= (s2 << (base + 2));
        sum |= (s3 << (base + 3));
        
        if (block == 7) {
            c_in_msb = c3;
        }
        
        // Block generate G and propagate P for inter-block carry lookahead
        uint32_t block_G = g3 | (p3 & g2) | (p3 & p2 & g1) | (p3 & p2 & p1 & g0);
        uint32_t block_P = p3 & p2 & p1 & p0;
        c_in = block_G | (block_P & c0);
        
        if (block == 7) {
            c_out_msb = c_in;
        }
    }
    
    // Signed overflow: carry into sign bit != carry out of sign bit
    overflow = (c_in_msb != c_out_msb);
    return static_cast<int32_t>(sum);
}

int32_t ALU::kogge_stone_add(int32_t a, int32_t b, bool& overflow) {
    uint32_t ua = static_cast<uint32_t>(a);
    uint32_t ub = static_cast<uint32_t>(b);

    // Step 1: Pre-processing (Bit-level Generate and Propagate)
    std::array<uint8_t, 32> g{}, p{}, p_init{};
    for (int i = 0; i < 32; ++i) {
        uint8_t ai = (ua >> i) & 1;
        uint8_t bi = (ub >> i) & 1;
        g[i] = ai & bi;
        p[i] = ai ^ bi;
        p_init[i] = p[i];
    }

    // Step 2: 5-Level Parallel Prefix Carry Generation Network (Kogge-Stone Tree)
    // Strides: 1, 2, 4, 8, 16 for log2(32) = 5 logic levels
    for (int level = 0; level < 5; ++level) {
        int stride = 1 << level;
        std::array<uint8_t, 32> next_g = g;
        std::array<uint8_t, 32> next_p = p;

        for (int i = stride; i < 32; ++i) {
            // Black-cell prefix operator: G_new = G_high | (P_high & G_low)
            //                             P_new = P_high & P_low
            next_g[i] = g[i] | (p[i] & g[i - stride]);
            next_p[i] = p[i] & p[i - stride];
        }
        g = next_g;
        p = next_p;
    }

    // Step 3: Carry Calculation
    // C[0] = 0 (assuming Cin = 0)
    // C[i] = group generate g[i - 1] for i in 1..31
    // C[32] = group generate g[31] (carry out of bit 31)
    std::array<uint8_t, 33> c{};
    c[0] = 0;
    for (int i = 1; i <= 32; ++i) {
        c[i] = g[i - 1];
    }

    // Step 4: Post-processing (Sum computation & Overflow)
    uint32_t sum = 0;
    for (int i = 0; i < 32; ++i) {
        uint8_t s_bit = p_init[i] ^ c[i];
        sum |= (static_cast<uint32_t>(s_bit) << i);
    }

    // Signed 2's complement overflow: carry into sign bit (c[31]) != carry out of sign bit (c[32])
    overflow = (c[31] != c[32]);

    return static_cast<int32_t>(sum);
}

int32_t ALU::carry_select_add(int32_t a, int32_t b, bool& overflow) {
    uint32_t ua = static_cast<uint32_t>(a);
    uint32_t ub = static_cast<uint32_t>(b);
    uint32_t sum = 0;

    auto add4 = [](uint32_t a4, uint32_t b4, uint32_t cin, uint32_t& cin_msb) -> std::pair<uint32_t, uint32_t> {
        uint32_t s = 0;
        uint32_t c = cin;
        for (int i = 0; i < 4; ++i) {
            uint32_t bit_a = (a4 >> i) & 1;
            uint32_t bit_b = (b4 >> i) & 1;
            if (i == 3) cin_msb = c;
            uint32_t sum_bit = bit_a ^ bit_b ^ c;
            c = (bit_a & bit_b) | (c & (bit_a ^ bit_b));
            s |= (sum_bit << i);
        }
        return {s, c};
    };

    uint32_t c_in = 0;
    uint32_t c_in_msb = 0;
    uint32_t c_out_msb = 0;

    for (int block = 0; block < 8; ++block) {
        int shift = block * 4;
        uint32_t a_blk = (ua >> shift) & 0xF;
        uint32_t b_blk = (ub >> shift) & 0xF;

        if (block == 0) {
            uint32_t dummy_cin_msb = 0;
            auto [s0, cout0] = add4(a_blk, b_blk, 0, dummy_cin_msb);
            sum |= (s0 << shift);
            c_in = cout0;
        } else {
            uint32_t cin_msb0 = 0;
            auto [s0, cout0] = add4(a_blk, b_blk, 0, cin_msb0);
            uint32_t cin_msb1 = 0;
            auto [s1, cout1] = add4(a_blk, b_blk, 1, cin_msb1);

            uint32_t selected_sum = (c_in == 1) ? s1 : s0;
            uint32_t selected_cout = (c_in == 1) ? cout1 : cout0;
            uint32_t selected_cin_msb = (c_in == 1) ? cin_msb1 : cin_msb0;

            sum |= (selected_sum << shift);
            c_in = selected_cout;

            if (block == 7) {
                c_in_msb = selected_cin_msb;
                c_out_msb = selected_cout;
            }
        }
    }

    overflow = (c_in_msb != c_out_msb);
    return static_cast<int32_t>(sum);
}

int32_t ALU::nonrestoring_divide(int32_t a, int32_t b, bool& div_zero) {
    if (b == 0) {
        div_zero = true;
        return 0;
    }
    div_zero = false;
    if (a == INT32_MIN && b == -1) {
        return INT32_MIN;
    }
    
    bool neg_res = (a < 0) ^ (b < 0);
    uint32_t Q = (a == INT32_MIN) ? 0x80000000u : static_cast<uint32_t>(std::abs(a));
    uint32_t M = (b == INT32_MIN) ? 0x80000000u : static_cast<uint32_t>(std::abs(b));
    
    int64_t A = 0;
    
    for (int i = 0; i < 32; ++i) {
        A = (2 * A) | ((Q >> 31) & 1);
        Q <<= 1;
        
        if (A >= 0) {
            A -= M;
        } else {
            A += M;
        }
        
        if (A >= 0) {
            Q |= 1;
        } else {
            Q &= ~1;
        }
    }
    
    if (A < 0) {
        A += M;
    }
    
    if (neg_res) {
        return -static_cast<int32_t>(Q);
    }
    return static_cast<int32_t>(Q);
}

int32_t ALU::newton_raphson_divide(int32_t a, int32_t b, bool& div_zero) {
    if (b == 0) {
        div_zero = true;
        return 0;
    }
    div_zero = false;
    if (a == INT32_MIN && b == -1) {
        return INT32_MIN;
    }
    if (a == 0) return 0;

    bool neg_res = (a < 0) ^ (b < 0);
    uint64_t ua = (a == INT32_MIN) ? 0x80000000ULL : static_cast<uint64_t>(std::abs(static_cast<int64_t>(a)));
    uint64_t ub = (b == INT32_MIN) ? 0x80000000ULL : static_cast<uint64_t>(std::abs(static_cast<int64_t>(b)));

    if (ua < ub) {
        return 0;
    }

    // Double-Precision Newton-Raphson Reciprocal Estimation: X_{k+1} = X_k * (2 - D * X_k)
    // Normalize divisor into [0.5, 1.0)
    double D = static_cast<double>(ub);
    int exp = 0;
    double d_norm = std::frexp(D, &exp); // D = d_norm * 2^exp, where 0.5 <= d_norm < 1.0

    // Initial linear minimax seed for reciprocal 1/d_norm: X_0 = 2.8235 - 1.8824 * d_norm
    double X = 2.8235 - 1.8824 * d_norm;

    // 5 quadratically converging Newton-Raphson iterations:
    // Precision doubles each iteration: ~2 -> 4 -> 8 -> 16 -> 32 -> 64 bits (< 10^-15 error)
    for (int iter = 0; iter < 5; ++iter) {
        X = X * (2.0 - d_norm * X);
    }

    // Unscale reciprocal: 1/D = X * 2^(-exp)
    double recip_D = std::ldexp(X, -exp);

    // Initial quotient estimate: Q = floor(ua * recip_D)
    uint64_t Q = static_cast<uint64_t>(static_cast<double>(ua) * recip_D);

    // Exact precision boundary adjustment (compensating for floating-point rounding):
    while ((Q + 1) * ub <= ua) {
        Q++;
    }
    while (Q * ub > ua) {
        Q--;
    }

    int64_t signed_q = neg_res ? -static_cast<int64_t>(Q) : static_cast<int64_t>(Q);
    return static_cast<int32_t>(signed_q);
}

int32_t ALU::newton_raphson_modulo(int32_t a, int32_t b, bool& div_zero) {
    if (b == 0) {
        div_zero = true;
        return 0;
    }
    // 1-Cycle Modulo Identity: R = A - (Q * B)
    int32_t q = newton_raphson_divide(a, b, div_zero);
    return a - (q * b);
}

int32_t ALU::barrel_shift(int32_t a, int32_t b, AluOp direction) {
    uint32_t shift = static_cast<uint32_t>(b) & 31;
    uint32_t val = static_cast<uint32_t>(a);
    
    if (direction == AluOp::LSL) {
        for (int i = 0; i < 5; ++i) {
            if ((shift >> i) & 1) {
                val <<= (1 << i);
            }
        }
        return static_cast<int32_t>(val);
    } else if (direction == AluOp::LSR) {
        for (int i = 0; i < 5; ++i) {
            if ((shift >> i) & 1) {
                val >>= (1 << i);
            }
        }
        return static_cast<int32_t>(val);
    } else { // ASR
        int32_t sval = a;
        for (int i = 0; i < 5; ++i) {
            if ((shift >> i) & 1) {
                sval >>= (1 << i);
            }
        }
        return sval;
    }
}

} // namespace risc201
