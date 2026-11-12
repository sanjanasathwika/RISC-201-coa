#pragma once
#include <cstdint>
#include <string>

namespace risc201 {

enum class AluOp { ADD, SUB, MUL, DIV, MOD, AND, OR, NOT, LSL, LSR, ASR, CMP, PASS };
enum class AluAlgorithm { NAIVE, BOOTH, CLA, NONRESTORING, BARREL, BOOTH_RADIX4, CSLA, KOGGE_STONE, NEWTON_RAPHSON };

struct AluResult {
    int32_t value = 0;
    bool E = false;       // Equal flag (a == b for CMP, value == 0 otherwise)
    bool GT = false;      // Greater-than flag (a > b for CMP, value > 0 otherwise)
    bool overflow = false;
    bool divide_by_zero = false;
};

class ALU {
    AluAlgorithm mul_algo_ = AluAlgorithm::NAIVE;
    AluAlgorithm add_algo_ = AluAlgorithm::NAIVE;
    AluAlgorithm div_algo_ = AluAlgorithm::NAIVE;
    AluAlgorithm shift_algo_ = AluAlgorithm::NAIVE;
public:
    // Algorithm selection
    void set_mul_algorithm(AluAlgorithm a);
    void set_add_algorithm(AluAlgorithm a);
    void set_div_algorithm(AluAlgorithm a);
    void set_shift_algorithm(AluAlgorithm a);
    AluAlgorithm get_mul_algorithm() const { return mul_algo_; }
    AluAlgorithm get_add_algorithm() const { return add_algo_; }
    AluAlgorithm get_div_algorithm() const { return div_algo_; }
    AluAlgorithm get_shift_algorithm() const { return shift_algo_; }

    // Main execute
    AluResult execute(AluOp op, int32_t a, int32_t b) const;

    // Optimized algorithm implementations (static so they can be tested independently)
    static int32_t booth_multiply(int32_t a, int32_t b, bool& overflow);
    static int32_t booth_radix4_multiply(int32_t a, int32_t b, bool& overflow);
    static int32_t cla_add(int32_t a, int32_t b, bool& overflow);
    static int32_t carry_select_add(int32_t a, int32_t b, bool& overflow);
    static int32_t kogge_stone_add(int32_t a, int32_t b, bool& overflow);
    static int32_t nonrestoring_divide(int32_t a, int32_t b, bool& div_zero);
    static int32_t newton_raphson_divide(int32_t a, int32_t b, bool& div_zero);
    static int32_t newton_raphson_modulo(int32_t a, int32_t b, bool& div_zero);
    static int32_t barrel_shift(int32_t a, int32_t b, AluOp direction);

    // Naming
    static const char* op_name(AluOp op);
    static const char* algo_name(AluAlgorithm a);
};

} // namespace risc201
