#include "risc201/cpu.hpp"
#include "risc201/assembler.hpp"
#include "risc201/isa.hpp"
#include "risc201/alu.hpp"
#include <iostream>
#include <cassert>
#include <exception>

using namespace risc201;

int passed = 0;
int total = 0;

#define TEST_ASSERT(cond) \
    do { if (!(cond)) throw std::runtime_error("Assertion failed: " #cond); } while(0)

void run_test(const std::string& name, void (*test_fn)()) {
    total++;
    try {
        test_fn();
        std::cout << "[PASS] " << name << "\n";
        passed++;
    } catch (const std::exception& e) {
        std::cout << "[FAIL] " << name << " - " << e.what() << "\n";
    }
}

// 1. ISA: encode/decode roundtrip for all instruction types
void test_isa_encode_decode() {
    Instruction i;
    i.op = Opcode::ADD;
    i.rd = 1; i.rs1 = 2; i.rs2 = 3;
    i.immediate = false;
    uint32_t enc = encode(i);
    Instruction d = decode(enc);
    TEST_ASSERT(d.op == Opcode::ADD && d.rd == 1 && d.rs1 == 2 && d.rs2 == 3 && !d.immediate);

    i.op = Opcode::MOV;
    i.rd = 4; i.immediate = true; i.imm = -42;
    enc = encode(i);
    d = decode(enc);
    TEST_ASSERT(d.op == Opcode::MOV && d.rd == 4 && d.immediate && d.imm == -42);
}

// 2. ISA: disassemble output correctness
void test_isa_disasm() {
    Instruction i;
    i.op = Opcode::ADD; i.rd = 1; i.rs1 = 2; i.rs2 = 3; i.immediate = false;
    TEST_ASSERT(disassemble(i) == "add r1, r2, r3");
    i.op = Opcode::MOV; i.rd = 4; i.imm = -42; i.immediate = true;
    TEST_ASSERT(disassemble(i) == "mov r4, -42");
}

// 3. ALU: all operations with naive algorithm
void test_alu_naive() {
    ALU alu;
    TEST_ASSERT(alu.execute(AluOp::ADD, 5, 3).value == 8);
    TEST_ASSERT(alu.execute(AluOp::SUB, 5, 3).value == 2);
    TEST_ASSERT(alu.execute(AluOp::MUL, 5, 3).value == 15);
    TEST_ASSERT(alu.execute(AluOp::DIV, 15, 3).value == 5);
}

// 4. ALU: Booth's multiplication correctness
void test_alu_booth() {
    bool ov = false;
    TEST_ASSERT(ALU::booth_multiply(7, 13, ov) == 91);
    TEST_ASSERT(ALU::booth_multiply(-7, 13, ov) == -91);
    TEST_ASSERT(ALU::booth_multiply(-7, -13, ov) == 91);
}

// 4b. ALU: Radix-4 Booth's multiplication correctness
void test_alu_booth_radix4() {
    bool ov = false;
    TEST_ASSERT(ALU::booth_radix4_multiply(7, 13, ov) == 91);
    TEST_ASSERT(!ov);
    TEST_ASSERT(ALU::booth_radix4_multiply(-7, 13, ov) == -91);
    TEST_ASSERT(!ov);
    TEST_ASSERT(ALU::booth_radix4_multiply(-7, -13, ov) == 91);
    TEST_ASSERT(!ov);
    TEST_ASSERT(ALU::booth_radix4_multiply(0, 500, ov) == 0);
    TEST_ASSERT(!ov);
    TEST_ASSERT(ALU::booth_radix4_multiply(1234, -5678, ov) == (1234 * -5678));
    TEST_ASSERT(!ov);

    // ALU execute with BOOTH_RADIX4
    ALU alu;
    alu.set_mul_algorithm(AluAlgorithm::BOOTH_RADIX4);
    AluResult res = alu.execute(AluOp::MUL, 15, -4);
    TEST_ASSERT(res.value == -60);
    TEST_ASSERT(!res.overflow);

    // Overflow detection test (product exceeding 32-bit signed range)
    bool ov2 = false;
    ALU::booth_radix4_multiply(100000, 100000, ov2);
    TEST_ASSERT(ov2 == true);

    // CPU execution with Radix-4 Booth multiplier
    CPU cpu(Pipeline::P4);
    cpu.set_alu_algorithms(AluAlgorithm::BOOTH_RADIX4, AluAlgorithm::CLA, AluAlgorithm::NONRESTORING, AluAlgorithm::BARREL);
    Assembler a;
    cpu.load(a.assemble("mov r1, -12\nmov r2, 8\nmul r3, r1, r2\nhalt"));
    cpu.run();
    TEST_ASSERT(cpu.get_reg(3) == -96);
}

// 5. ALU: CLA addition correctness
void test_alu_cla() {
    bool ov = false;
    TEST_ASSERT(ALU::cla_add(10, 20, ov) == 30);
    TEST_ASSERT(ALU::cla_add(-10, 20, ov) == 10);
}

// 5b. ALU: Kogge-Stone Parallel-Prefix Adder (KSA) correctness
void test_alu_kogge_stone() {
    bool ov = false;
    TEST_ASSERT(ALU::kogge_stone_add(10, 20, ov) == 30);
    TEST_ASSERT(!ov);
    TEST_ASSERT(ALU::kogge_stone_add(-10, 20, ov) == 10);
    TEST_ASSERT(!ov);
    TEST_ASSERT(ALU::kogge_stone_add(123456, 654321, ov) == 777777);
    TEST_ASSERT(!ov);
    TEST_ASSERT(ALU::kogge_stone_add(-500, -300, ov) == -800);
    TEST_ASSERT(!ov);

    // Overflow test (INT32_MAX + 1 = INT32_MIN)
    bool ov2 = false;
    TEST_ASSERT(ALU::kogge_stone_add(INT32_MAX, 1, ov2) == INT32_MIN);
    TEST_ASSERT(ov2 == true);

    // ALU execute with KOGGE_STONE
    ALU alu;
    alu.set_add_algorithm(AluAlgorithm::KOGGE_STONE);
    AluResult res = alu.execute(AluOp::ADD, 250, 750);
    TEST_ASSERT(res.value == 1000);
    TEST_ASSERT(!res.overflow);

    // CPU execution with Kogge-Stone Adder
    CPU cpu(Pipeline::P4);
    cpu.set_alu_algorithms(AluAlgorithm::BOOTH_RADIX4, AluAlgorithm::KOGGE_STONE, AluAlgorithm::NONRESTORING, AluAlgorithm::BARREL);
    Assembler a;
    cpu.load(a.assemble("mov r1, 45\nmov r2, 55\nadd r3, r1, r2\nhalt"));
    cpu.run();
    TEST_ASSERT(cpu.get_reg(3) == 100);
}

// 5c. ALU: Carry-Select Adder (CSLA) correctness
void test_alu_csla() {
    bool ov = false;
    TEST_ASSERT(ALU::carry_select_add(10, 20, ov) == 30);
    TEST_ASSERT(!ov);
    TEST_ASSERT(ALU::carry_select_add(-10, 20, ov) == 10);
    TEST_ASSERT(!ov);

    bool ov2 = false;
    TEST_ASSERT(ALU::carry_select_add(INT32_MAX, 1, ov2) == INT32_MIN);
    TEST_ASSERT(ov2 == true);
}

// 6. ALU: non-restoring division correctness
void test_alu_div() {
    bool dz = false;
    TEST_ASSERT(ALU::nonrestoring_divide(100, 3, dz) == 33);
}

// 7. ALU: barrel shifter correctness
void test_alu_barrel() {
    TEST_ASSERT(ALU::barrel_shift(0xF, 2, AluOp::LSL) == 0x3C);
    TEST_ASSERT(ALU::barrel_shift(0xF, 1, AluOp::LSR) == 0x7);
    TEST_ASSERT(ALU::barrel_shift(-2, 1, AluOp::ASR) == -1);
}

// 8. ALU: edge cases (overflow, divide-by-zero, INT32_MIN)
void test_alu_edge() {
    bool dz = false;
    ALU::nonrestoring_divide(10, 0, dz);
    TEST_ASSERT(dz == true);
    
    ALU alu;
    auto res = alu.execute(AluOp::DIV, 10, 0);
    TEST_ASSERT(res.divide_by_zero);
}

// 9. Assembler: basic program assembly
void test_asm_basic() {
    Assembler a;
    auto p = a.assemble("mov r1, 10\nadd r2, r1, r1");
    TEST_ASSERT(p.size() == 2);
    TEST_ASSERT(decode(p[0]).op == Opcode::MOV);
    TEST_ASSERT(decode(p[1]).op == Opcode::ADD);
}

// 10. Assembler: label resolution with macros
void test_asm_labels() {
    Assembler a;
    auto p = a.assemble("push r1\nlabel:\nmov r2, 0\nb label");
    TEST_ASSERT(p.size() > 0);
}

// 11. Assembler: push/pop macro expansion
void test_asm_push_pop() {
    Assembler a;
    auto p = a.assemble("push r1\npop r2");
    TEST_ASSERT(p.size() >= 4); // each macro is 2 insts
}

// 12. Assembler: disassemble_hex roundtrip
void test_asm_disasm() {
    Assembler a;
    auto p = a.assemble("mov r1, 42\nhalt\n");
    std::string hex;
    for(auto w : p) {
        char buf[20];
        snprintf(buf, sizeof(buf), "%08X\n", w);
        hex += buf;
    }
    std::string dis = a.disassemble_hex(hex);
    TEST_ASSERT(dis.find("mov r1, 42") != std::string::npos);
}

// 13. CPU: simple arithmetic program (4-stage)
void test_cpu_arith_4() {
    CPU cpu(Pipeline::P4);
    Assembler a;
    cpu.load(a.assemble("mov r1, 10\nmov r2, 20\nadd r3, r1, r2\nhalt"));
    cpu.run();
    TEST_ASSERT(cpu.get_reg(3) == 30);
}

// 14. CPU: simple arithmetic program (6-stage)
void test_cpu_arith_6() {
    CPU cpu(Pipeline::P6);
    Assembler a;
    cpu.load(a.assemble("mov r1, 10\nmov r2, 20\nadd r3, r1, r2\nhalt"));
    cpu.run();
    TEST_ASSERT(cpu.get_reg(3) == 30);
}

// 15. CPU: conditional branching (CMP + BEQ/BGT)
void test_cpu_branch() {
    CPU cpu(Pipeline::P4);
    Assembler a;
    cpu.load(a.assemble("mov r1, 10\nmov r2, 10\ncmp r1, r2\nbeq l1\nmov r3, 0\nhalt\nl1:\nmov r3, 1\nhalt"));
    cpu.run();
    TEST_ASSERT(cpu.get_reg(3) == 1);
}

// 16. CPU: unconditional branch
void test_cpu_uncond_branch() {
    CPU cpu(Pipeline::P4);
    Assembler a;
    cpu.load(a.assemble("b l1\nmov r1, 0\nhalt\nl1:\nmov r1, 1\nhalt"));
    cpu.run();
    TEST_ASSERT(cpu.get_reg(1) == 1);
}

// 17. CPU: CALL and RET
void test_cpu_call_ret() {
    CPU cpu(Pipeline::P4);
    Assembler a;
    cpu.load(a.assemble("call sub\nmov r2, 1\nhalt\nsub:\nmov r1, 99\nret"));
    cpu.run();
    TEST_ASSERT(cpu.get_reg(1) == 99);
    TEST_ASSERT(cpu.get_reg(2) == 1);
}

// 18. CPU: stack push/pop via macros
void test_cpu_stack() {
    CPU cpu(Pipeline::P4);
    Assembler a;
    cpu.load(a.assemble("mov r1, 42\npush r1\nmov r1, 0\npop r2\nhalt"));
    cpu.run();
    TEST_ASSERT(cpu.get_reg(2) == 42);
}

// 19. CPU: NOP does not halt
void test_cpu_nop() {
    CPU cpu(Pipeline::P4);
    Assembler a;
    cpu.load(a.assemble("nop\nmov r1, 1\nhalt"));
    cpu.run();
    TEST_ASSERT(cpu.get_reg(1) == 1);
}

// 20. CPU: HALT stops execution
void test_cpu_halt() {
    CPU cpu(Pipeline::P4);
    Assembler a;
    cpu.load(a.assemble("halt\nmov r1, 1\n"));
    cpu.run();
    TEST_ASSERT(cpu.get_reg(1) == 0);
    TEST_ASSERT(cpu.is_halted());
}

// 21. CPU: hardwired control
void test_cpu_hardwired() {
    CPU cpu(Pipeline::P4, ControlStyle::HARDWIRED);
    Assembler a;
    cpu.load(a.assemble("mov r1, 5\nhalt"));
    cpu.run();
    TEST_ASSERT(cpu.get_reg(1) == 5);
}

// 22. CPU: horizontal microprogrammed control
void test_cpu_horizontal() {
    CPU cpu(Pipeline::P4, ControlStyle::HORIZONTAL);
    Assembler a;
    cpu.load(a.assemble("mov r1, 5\nhalt"));
    cpu.run();
    TEST_ASSERT(cpu.get_reg(1) == 5);
}

// 23. CPU: vertical microprogrammed control
void test_cpu_vertical() {
    CPU cpu(Pipeline::P4, ControlStyle::VERTICAL);
    Assembler a;
    cpu.load(a.assemble("mov r1, 5\nhalt"));
    cpu.run();
    TEST_ASSERT(cpu.get_reg(1) == 5);
}

// 24. Control: horizontal encode/decode roundtrip
void test_ctrl_horizontal_roundtrip() {
    ControlSignals cs;
    cs.regWrite = true;
    cs.memRead = false;
    cs.memWrite = false;
    cs.aluOp = AluOp::ADD;
    cs.aluSrc = true;
    uint32_t raw = ControlUnit::encode_horizontal(cs, 7, true);
    ControlSignals dec = ControlUnit::decode_horizontal(raw);
    TEST_ASSERT(dec.regWrite == cs.regWrite);
    TEST_ASSERT(dec.aluOp == cs.aluOp);
    TEST_ASSERT(dec.aluSrc == cs.aluSrc);
}

// 25. Control: vertical encode/decode roundtrip
void test_ctrl_vertical_roundtrip() {
    ControlSignals cs;
    cs.regWrite = true;
    cs.aluOp = AluOp::SUB;
    uint32_t raw = ControlUnit::encode_vertical(cs, 4, false);
    ControlSignals dec = ControlUnit::decode_vertical(raw);
    TEST_ASSERT(dec.regWrite == cs.regWrite);
    TEST_ASSERT(dec.aluOp == cs.aluOp);
}

// 26. Memory: read/write operations
void test_mem_rw() {
    Memory m(1024);
    m.write32(0, 0xDEADBEEF);
    TEST_ASSERT(m.read32(0) == 0xDEADBEEF);
    m.write8(4, 0xFF);
    TEST_ASSERT(m.read8(4) == 0xFF);
}

// 27. Memory: bounds checking
void test_mem_bounds() {
    Memory m(16);
    try {
        m.write32(16, 0);
        TEST_ASSERT(false);
    } catch(...) {}
}

// 28. Stack: push/pop operations
void test_stack_push_pop() {
    Memory m(1024);
    StackRuntime s(m, 512, 1024);
    s.push(0x11223344);
    s.push(0x55667788);
    TEST_ASSERT(s.pointer() == 1016);
    TEST_ASSERT(s.pop() == 0x55667788);
    TEST_ASSERT(s.pop() == 0x11223344);
    TEST_ASSERT(s.pointer() == 1024);
}

// 29. Stack: overflow/underflow detection
void test_stack_overflow() {
    Memory m(1024);
    StackRuntime s(m, 512, 520);
    s.push(1);
    s.push(2);
    bool caught_of = false;
    try { s.push(3); } catch (const StackRuntime::StackOverflow&) { caught_of = true; }
    TEST_ASSERT(caught_of);
    s.pop();
    s.pop();
    bool caught_uf = false;
    try { s.pop(); } catch (const StackRuntime::StackUnderflow&) { caught_uf = true; }
    TEST_ASSERT(caught_uf);
}

// 30. Stack: corruption bounds check
void test_stack_corruption() {
    Memory m(1024);
    StackRuntime s(m, 512, 1024);
    bool caught_corr = false;
    try { s.check_bounds(128); } catch (const StackRuntime::StackCorruption&) { caught_corr = true; }
    TEST_ASSERT(caught_corr);
}

// 31. Assembler: binary translation roundtrip
void test_asm_binary() {
    Assembler a;
    auto p = a.assemble("mov r1, 10\nadd r2, r1, r1\nhalt");
    std::string bin = a.to_binary_string(p);
    auto words = a.parse_binary(bin);
    TEST_ASSERT(words == p);
    std::string dis = a.disassemble_binary(bin);
    TEST_ASSERT(dis.find("mov r1, 10") != std::string::npos);
}

// 32. CPU: micro-step execution
void test_cpu_micro_step() {
    CPU cpu(Pipeline::P4, ControlStyle::HORIZONTAL);
    Assembler a;
    cpu.load(a.assemble("mov r1, 42\nhalt"));
    TEST_ASSERT(cpu.micro_step());
    auto ustate = cpu.dump_micro_state();
    TEST_ASSERT(ustate.find("Control Style") != std::string::npos);
    while (!cpu.is_halted()) cpu.micro_step();
    TEST_ASSERT(cpu.get_reg(1) == 42);
}

// 33. CPU: factorial with subroutine CALL and RET
void test_cpu_factorial() {
    CPU cpu(Pipeline::P4);
    Assembler a;
    cpu.load(a.assemble(
        "mov r1, 5\n"
        "mov r2, 1\n"
        "call fact\n"
        "b done\n"
        "fact:\n"
        "cmp r1, 0\n"
        "beq ret_fact\n"
        "mul r2, r2, r1\n"
        "sub r1, r1, 1\n"
        "b fact\n"
        "ret_fact:\n"
        "ret\n"
        "done:\n"
        "mov r1, r2\n"
        "halt\n"
    ));
    cpu.run();
    TEST_ASSERT(cpu.get_reg(1) == 120);
}

// 34. CPU: execution with optimized ALU algorithms
void test_cpu_optimized_alu() {
    CPU cpu(Pipeline::P4);
    cpu.set_alu_algorithms(AluAlgorithm::BOOTH, AluAlgorithm::CLA, AluAlgorithm::NONRESTORING, AluAlgorithm::BARREL);
    Assembler a;
    cpu.load(a.assemble("mov r1, 7\nmov r2, 6\nmul r3, r1, r2\nadd r4, r1, r2\nhalt"));
    cpu.run();
    TEST_ASSERT(cpu.get_reg(3) == 42);
    TEST_ASSERT(cpu.get_reg(4) == 13);
}

// 35. CPU: 4-stage vs 6-stage pipeline comparison and dump
void test_pipeline_comparison() {
    Assembler a;
    auto prog = a.assemble("mov r1, 10\nmov r2, 20\nadd r3, r1, r2\nhalt");
    CPU cpu4(Pipeline::P4);
    cpu4.load(prog);
    cpu4.run();
    CPU cpu6(Pipeline::P6);
    cpu6.load(prog);
    cpu6.run();
    TEST_ASSERT(cpu4.get_reg(3) == 30);
    TEST_ASSERT(cpu6.get_reg(3) == 30);
    TEST_ASSERT(cpu4.dump_pipeline().find("4-STAGE") != std::string::npos);
    TEST_ASSERT(cpu6.dump_pipeline().find("6-STAGE") != std::string::npos);
}

// 36. Microcoded CALL/RET frame tracking test
void test_microcode_call_ret_frames() {
    CPU cpu(Pipeline::P4, ControlStyle::HORIZONTAL);
    Assembler a;
    cpu.load(a.assemble("call sub\nmov r2, 1\nhalt\nsub:\nmov r1, 99\nret"));
    cpu.run();
    TEST_ASSERT(cpu.get_reg(1) == 99);
    TEST_ASSERT(cpu.get_reg(2) == 1);
    TEST_ASSERT(cpu.get_stack().frames().empty());
}

// 37. Vertical microcode 16-bit encoding & word width test
void test_vertical_microcode_16bit() {
    ControlSignals cs;
    cs.regWrite = true;
    cs.aluOp = AluOp::ADD;
    uint32_t raw = ControlUnit::encode_vertical(cs, 200, true);
    TEST_ASSERT(((raw >> 1) & 0xFF) == 200);
    TEST_ASSERT((raw & 1) == 1);
    ControlSignals dec = ControlUnit::decode_vertical(raw);
    TEST_ASSERT(dec.regWrite == cs.regWrite);
    TEST_ASSERT(dec.aluOp == cs.aluOp);

    ControlUnit cu(ControlStyle::VERTICAL);
    TEST_ASSERT(cu.word_width() == 16);
}

// 38. Stack pointer initialization consistency test
void test_stack_sp_initialization() {
    CPU cpu(Pipeline::P4);
    TEST_ASSERT(cpu.get_reg(14) == 0x10000);
    TEST_ASSERT(cpu.get_stack().pointer() == 0x10000);
    TEST_ASSERT(cpu.get_stack().high() == 0x10000);
    cpu.reset();
    TEST_ASSERT(cpu.get_reg(14) == 0x10000);
    TEST_ASSERT(cpu.get_stack().pointer() == 0x10000);
}

// 39. NOT instruction across all pipeline and control configurations
void test_not_instruction_all_configs() {
    Assembler a;
    auto prog = a.assemble("mov r1, 15\nnot r2, r1\nnot r3, 5\nhalt");
    
    // 4-stage hardwired
    {
        CPU cpu(Pipeline::P4, ControlStyle::HARDWIRED);
        cpu.load(prog);
        cpu.run();
        TEST_ASSERT(cpu.get_reg(2) == -16);
        TEST_ASSERT(cpu.get_reg(3) == -6);
    }
    // 6-stage hardwired
    {
        CPU cpu(Pipeline::P6, ControlStyle::HARDWIRED);
        cpu.load(prog);
        cpu.run();
        TEST_ASSERT(cpu.get_reg(2) == -16);
        TEST_ASSERT(cpu.get_reg(3) == -6);
    }
    // Horizontal microcoded
    {
        CPU cpu(Pipeline::P4, ControlStyle::HORIZONTAL);
        cpu.load(prog);
        cpu.run();
        TEST_ASSERT(cpu.get_reg(2) == -16);
        TEST_ASSERT(cpu.get_reg(3) == -6);
    }
    // Vertical microcoded
    {
        CPU cpu(Pipeline::P4, ControlStyle::VERTICAL);
        cpu.load(prog);
        cpu.run();
        TEST_ASSERT(cpu.get_reg(2) == -16);
        TEST_ASSERT(cpu.get_reg(3) == -6);
    }
}

// 40. Assembler error reporting with line numbers
void test_assembler_error_handling() {
    Assembler a;
    
    // Undefined label
    try {
        a.assemble("b nonexistent\nhalt");
        TEST_ASSERT(false);
    } catch (const std::exception& e) {
        std::string msg = e.what();
        TEST_ASSERT(msg.find("Undefined label") != std::string::npos);
        TEST_ASSERT(msg.find("Line 1") != std::string::npos);
    }

    // Unknown opcode
    try {
        a.assemble("foo r1, r2, r3\nhalt");
        TEST_ASSERT(false);
    } catch (const std::exception& e) {
        std::string msg = e.what();
        TEST_ASSERT(msg.find("Unknown opcode") != std::string::npos);
        TEST_ASSERT(msg.find("Line 1") != std::string::npos);
    }

    // Duplicate label
    try {
        a.assemble("start:\nmov r1, 1\nstart:\nhalt");
        TEST_ASSERT(false);
    } catch (const std::exception& e) {
        std::string msg = e.what();
        TEST_ASSERT(msg.find("Duplicate label") != std::string::npos);
        TEST_ASSERT(msg.find("Line 3") != std::string::npos);
    }

    // Invalid register (r99)
    try {
        a.assemble("mov r99, 1\nhalt");
        TEST_ASSERT(false);
    } catch (const std::exception& e) {
        std::string msg = e.what();
        TEST_ASSERT(msg.find("Invalid register") != std::string::npos);
        TEST_ASSERT(msg.find("Line 1") != std::string::npos);
    }

    // Immediate out of 18-bit range
    try {
        a.assemble("mov r1, 300000\nhalt");
        TEST_ASSERT(false);
    } catch (const std::exception& e) {
        std::string msg = e.what();
        TEST_ASSERT(msg.find("out of range") != std::string::npos);
        TEST_ASSERT(msg.find("Line 1") != std::string::npos);
    }
}

// 41. Stack underflow detection on empty stack pop
void test_stack_underflow_pop_empty() {
    CPU cpu(Pipeline::P4);
    Assembler a;
    cpu.load(a.assemble("pop r1\nhalt"));
    bool caught = false;
    try {
        cpu.run();
    } catch (const StackRuntime::StackUnderflow& e) {
        caught = true;
    }
    TEST_ASSERT(caught);
}

// 42. Branch Predictor: 2-bit saturating counter state machine transitions & BTB
void test_branch_predictor_2bit_state() {
    BranchPredictor bp(BranchPredictorMode::DYNAMIC_2BIT);
    uint32_t branch_pc = 0x1000;
    uint32_t target_pc = 0x2000;
    Instruction beq_instr;
    beq_instr.op = Opcode::BEQ;
    beq_instr.offset = 4; // Forward branch

    // Initial state: not in BTB, forward branch predicts Not-Taken
    Prediction p0 = bp.predict(branch_pc, beq_instr);
    TEST_ASSERT(!p0.predicted_taken);

    // 1st update: taken -> allocates BTB entry in WEAKLY_TAKEN
    bp.update(branch_pc, true, target_pc);
    const auto* e1 = bp.get_entry(branch_pc);
    TEST_ASSERT(e1 != nullptr && e1->valid && e1->target_pc == target_pc);
    TEST_ASSERT(e1->state == BimodalState::WEAKLY_TAKEN);

    // Now predict: should predict TAKEN to target_pc
    Prediction p1 = bp.predict(branch_pc, beq_instr);
    TEST_ASSERT(p1.predicted_taken && p1.predicted_target == target_pc);

    // 2nd update: taken -> saturates to STRONGLY_TAKEN
    bp.update(branch_pc, true, target_pc);
    TEST_ASSERT(bp.get_entry(branch_pc)->state == BimodalState::STRONGLY_TAKEN);

    // 3rd update: taken again -> remains STRONGLY_TAKEN (saturating upper bound)
    bp.update(branch_pc, true, target_pc);
    TEST_ASSERT(bp.get_entry(branch_pc)->state == BimodalState::STRONGLY_TAKEN);

    // 4th update: not taken -> transitions down to WEAKLY_TAKEN
    bp.update(branch_pc, false, target_pc);
    TEST_ASSERT(bp.get_entry(branch_pc)->state == BimodalState::WEAKLY_TAKEN);

    // 5th update: not taken -> transitions down to WEAKLY_NOT_TAKEN
    bp.update(branch_pc, false, target_pc);
    TEST_ASSERT(bp.get_entry(branch_pc)->state == BimodalState::WEAKLY_NOT_TAKEN);

    // Now predict: should predict NOT TAKEN
    Prediction p2 = bp.predict(branch_pc, beq_instr);
    TEST_ASSERT(!p2.predicted_taken);

    // 6th update: not taken -> saturates to STRONGLY_NOT_TAKEN (00)
    bp.update(branch_pc, false, target_pc);
    TEST_ASSERT(bp.get_entry(branch_pc)->state == BimodalState::STRONGLY_NOT_TAKEN);

    // 7th update: not taken again -> remains STRONGLY_NOT_TAKEN (saturating lower bound)
    bp.update(branch_pc, false, target_pc);
    TEST_ASSERT(bp.get_entry(branch_pc)->state == BimodalState::STRONGLY_NOT_TAKEN);
}

// 43. CPU: Execution with Dynamic 2-Bit Branch Prediction & Accuracy Verification
void test_cpu_dynamic_branch_prediction() {
    Assembler asm_obj;
    // Loop program: calculates 1 + 2 + 3 + 4 + 5 = 15
    std::string src = R"(
        mov r1, 0      ; sum
        mov r2, 5      ; counter
    loop:
        add r1, r1, r2 ; sum += r2
        sub r2, r2, 1  ; r2--
        cmp r2, 0
        bgt loop
        halt
    )";
    auto prog = asm_obj.assemble(src);

    // 1. Run with Baseline Static Always Not-Taken
    CPU cpu_static(Pipeline::P4);
    cpu_static.set_branch_predictor_mode(BranchPredictorMode::STATIC_NOT_TAKEN);
    cpu_static.load(prog);
    cpu_static.run();
    TEST_ASSERT(cpu_static.get_reg(1) == 15);
    uint64_t static_flushes = cpu_static.get_stats().pipeline_flushes;

    // 2. Run with Dynamic 2-Bit Saturating Counter Predictor
    CPU cpu_dyn(Pipeline::P4);
    cpu_dyn.set_branch_predictor_mode(BranchPredictorMode::DYNAMIC_2BIT);
    cpu_dyn.load(prog);
    cpu_dyn.run();
    TEST_ASSERT(cpu_dyn.get_reg(1) == 15);
    const auto& s = cpu_dyn.get_stats();

    // The loop branch was taken 4 times and not taken 1 time (5 total evaluations).
    // With dynamic 2-bit predictor, once learned, it predicts TAKEN with 0 flushes!
    TEST_ASSERT(s.branch_predictions > 0);
    TEST_ASSERT(s.branch_hits > 0);
    TEST_ASSERT(s.pipeline_flushes < static_flushes); // Reduced flushes!

    // 3. Test on 6-stage pipeline as well
    CPU cpu6_dyn(Pipeline::P6);
    cpu6_dyn.set_branch_predictor_mode(BranchPredictorMode::DYNAMIC_2BIT);
    cpu6_dyn.load(prog);
    cpu6_dyn.run();
    TEST_ASSERT(cpu6_dyn.get_reg(1) == 15);
    TEST_ASSERT(cpu6_dyn.get_stats().branch_hits > 0);
}

// 42. ALU: Double-Precision Newton-Raphson Fast Divider & 1-Cycle Modulo
void test_alu_newton_raphson() {
    bool div0 = false;
    TEST_ASSERT(ALU::newton_raphson_divide(100, 5, div0) == 20);
    TEST_ASSERT(!div0);
    TEST_ASSERT(ALU::newton_raphson_divide(355, 113, div0) == 3);
    TEST_ASSERT(!div0);
    TEST_ASSERT(ALU::newton_raphson_divide(-355, 113, div0) == -3);
    TEST_ASSERT(!div0);
    TEST_ASSERT(ALU::newton_raphson_divide(355, -113, div0) == -3);
    TEST_ASSERT(!div0);
    TEST_ASSERT(ALU::newton_raphson_divide(-355, -113, div0) == 3);
    TEST_ASSERT(!div0);
    TEST_ASSERT(ALU::newton_raphson_divide(1234567, 1234, div0) == 1000);
    TEST_ASSERT(!div0);
    TEST_ASSERT(ALU::newton_raphson_divide(10, 0, div0) == 0);
    TEST_ASSERT(div0);

    // 1-Cycle Modulo Identity: R = A - (Q * B)
    bool m0 = false;
    TEST_ASSERT(ALU::newton_raphson_modulo(355, 113, m0) == 16);
    TEST_ASSERT(ALU::newton_raphson_modulo(100, 7, m0) == 2);
    TEST_ASSERT(ALU::newton_raphson_modulo(100, 10, m0) == 0);

    // Test ALU::execute with NEWTON_RAPHSON algorithm
    ALU alu;
    alu.set_div_algorithm(AluAlgorithm::NEWTON_RAPHSON);
    AluResult res = alu.execute(AluOp::DIV, 144, 12);
    TEST_ASSERT(res.value == 12);
    TEST_ASSERT(!res.divide_by_zero);
    AluResult res_mod = alu.execute(AluOp::MOD, 147, 12);
    TEST_ASSERT(res_mod.value == 3);
}

// 43. Branch Predictor: Speculative Return Address Stack (RAS)
void test_branch_predictor_ras() {
    BranchPredictor bp(BranchPredictorMode::DYNAMIC_2BIT);
    TEST_ASSERT(bp.ras().empty());

    // 1. Push return address on CALL
    bp.update(0x1000, true, 0x2000, Opcode::CALL); // CALL from 0x1000 saves 0x1004
    TEST_ASSERT(bp.ras().size() == 1);
    TEST_ASSERT(bp.ras().peek() == 0x1004);

    // 2. Nested CALL from 0x2010 saves 0x2014
    bp.update(0x2010, true, 0x3000, Opcode::CALL);
    TEST_ASSERT(bp.ras().size() == 2);
    TEST_ASSERT(bp.ras().peek() == 0x2014);

    // 3. Predict RET in IF stage
    Instruction ret_instr;
    ret_instr.op = Opcode::RET;
    Prediction pred = bp.predict(0x3050, ret_instr);
    TEST_ASSERT(pred.is_branch);
    TEST_ASSERT(pred.predicted_taken);
    TEST_ASSERT(pred.predicted_target == 0x2014); // predicted via top of RAS!

    // 4. Update on RET popping RAS
    bp.update(0x3050, true, 0x2014, Opcode::RET);
    TEST_ASSERT(bp.ras().get_hits() == 1);
    TEST_ASSERT(bp.ras().size() == 1);
    TEST_ASSERT(bp.ras().peek() == 0x1004); // now outer return address
}

// 44. Memory Hierarchy: Split L1 Cache (2-Way Set-Associative, LRU)
void test_l1_cache_split() {
    Memory mem(64 * 1024);
    L1Cache cache("Test L1");
    TEST_ASSERT(cache.accesses() == 0);

    // Write some data in memory
    mem.write32(0x1000, 0x11223344);

    // 1. First access is a compulsory miss (fills 32B line from memory)
    bool hit1 = cache.access(0x1000, false, mem);
    TEST_ASSERT(!hit1);
    TEST_ASSERT(cache.misses() == 1);

    // 2. Immediate second access to same block is a hit!
    bool hit2 = cache.access(0x1000, false, mem);
    TEST_ASSERT(hit2);
    TEST_ASSERT(cache.hits() == 1);

    // 3. Spatial locality hit: adjacent word in same 32B block (0x1004)
    bool hit3 = cache.access(0x1004, false, mem);
    TEST_ASSERT(hit3);
    TEST_ASSERT(cache.hits() == 2);

    // 4. 2-way set-associativity: access second block mapping to same set
    uint32_t same_set_addr = 0x1000 + (16 * 32); // same set index
    bool hit4 = cache.access(same_set_addr, false, mem);
    TEST_ASSERT(!hit4); // miss, but fits in way 1 without eviction!
    TEST_ASSERT(cache.evictions() == 0);

    // Both ways are now cached
    TEST_ASSERT(cache.access(0x1000, false, mem));
    TEST_ASSERT(cache.access(same_set_addr, false, mem));
}

// 45. Hazard Mitigation: Hardware Store-to-Load Forwarding Buffer
void test_store_to_load_forwarding() {
    StoreBuffer sb;
    TEST_ASSERT(sb.size() == 0);

    sb.push(0x2000, 0xCAFE);
    TEST_ASSERT(sb.size() == 1);

    int32_t val = 0;
    TEST_ASSERT(sb.lookup(0x2000, val));
    TEST_ASSERT(val == 0xCAFE);
    TEST_ASSERT(sb.get_forwards() == 1);

    int32_t dummy = 0;
    TEST_ASSERT(!sb.lookup(0x3000, dummy));

    // Pipeline integration test: store followed immediately by load to same address
    Assembler a;
    std::string s = 
        "mov r1, 12345\n"
        "st r1, 0, [r0]\n"
        "ld r2, 0, [r0]\n"
        "halt\n";
    auto prog = a.assemble(s);
    CPU cpu(Pipeline::P4);
    cpu.load(prog);
    cpu.run();
    TEST_ASSERT(cpu.get_reg(2) == 12345);
    TEST_ASSERT(cpu.get_stats().store_to_load_forwards > 0);
}

// 46. Assembler Optimization: Static Load-Use Delay Slot Scheduler
void test_assembler_delay_slot_optimizer() {
    Assembler a;
    std::string unoptimized =
        "ld r1, 0, [r0]\n"
        "add r2, r1, 10\n"
        "mov r3, 50\n"
        "halt\n";

    std::string optimized = a.optimize_load_delay_slots(unoptimized);
    
    size_t pos_mov = optimized.find("mov r3, 50");
    size_t pos_add = optimized.find("add r2, r1, 10");
    TEST_ASSERT(pos_mov != std::string::npos && pos_add != std::string::npos);
    TEST_ASSERT(pos_mov < pos_add);

    CPU cpu1(Pipeline::P4);
    cpu1.memory().write32(0, 100);
    cpu1.load(a.assemble(unoptimized));
    cpu1.run();

    CPU cpu2(Pipeline::P4);
    cpu2.memory().write32(0, 100);
    cpu2.load(a.assemble(optimized));
    cpu2.run();

    TEST_ASSERT(cpu1.get_reg(1) == cpu2.get_reg(1));
    TEST_ASSERT(cpu1.get_reg(2) == cpu2.get_reg(2));
    TEST_ASSERT(cpu1.get_reg(3) == cpu2.get_reg(3));
}

// 47. Fix Verification: Disassembler operand correctness for MOV, NOT, CMP
void test_disassembler_operand_fixes() {
    Assembler a;
    
    // Register-form MOV: mov r1, r2 -> rd=1, rs2=2, rs1=0
    std::string s_mov_reg = "mov r1, r2\nhalt\n";
    auto prog_mov_reg = a.assemble(s_mov_reg);
    Instruction dec_mov_reg = decode(prog_mov_reg[0]);
    TEST_ASSERT(dec_mov_reg.op == Opcode::MOV);
    TEST_ASSERT(dec_mov_reg.rd == 1 && dec_mov_reg.rs2 == 2 && !dec_mov_reg.immediate);
    TEST_ASSERT(disassemble(dec_mov_reg) == "mov r1, r2");

    // Immediate-form MOV: mov r3, 42 -> rd=3, imm=42
    std::string s_mov_imm = "mov r3, 42\nhalt\n";
    auto prog_mov_imm = a.assemble(s_mov_imm);
    Instruction dec_mov_imm = decode(prog_mov_imm[0]);
    TEST_ASSERT(dec_mov_imm.op == Opcode::MOV);
    TEST_ASSERT(dec_mov_imm.rd == 3 && dec_mov_imm.imm == 42 && dec_mov_imm.immediate);
    TEST_ASSERT(disassemble(dec_mov_imm) == "mov r3, 42");

    // Register-form NOT: not r3, r4 -> rd=3, rs2=4
    std::string s_not_reg = "not r3, r4\nhalt\n";
    auto prog_not_reg = a.assemble(s_not_reg);
    Instruction dec_not_reg = decode(prog_not_reg[0]);
    TEST_ASSERT(dec_not_reg.op == Opcode::NOT);
    TEST_ASSERT(dec_not_reg.rd == 3 && dec_not_reg.rs2 == 4 && !dec_not_reg.immediate);
    TEST_ASSERT(disassemble(dec_not_reg) == "not r3, r4");

    // Immediate-form NOT: not r5, 255 -> rd=5, imm=255
    std::string s_not_imm = "not r5, 255\nhalt\n";
    auto prog_not_imm = a.assemble(s_not_imm);
    Instruction dec_not_imm = decode(prog_not_imm[0]);
    TEST_ASSERT(dec_not_imm.op == Opcode::NOT);
    TEST_ASSERT(dec_not_imm.rd == 5 && dec_not_imm.imm == 255 && dec_not_imm.immediate);
    TEST_ASSERT(disassemble(dec_not_imm) == "not r5, 255");

    // Register-form CMP: cmp r1, r2 -> rs1=1, rs2=2
    std::string s_cmp_reg = "cmp r1, r2\nhalt\n";
    auto prog_cmp_reg = a.assemble(s_cmp_reg);
    Instruction dec_cmp_reg = decode(prog_cmp_reg[0]);
    TEST_ASSERT(dec_cmp_reg.op == Opcode::CMP);
    TEST_ASSERT(dec_cmp_reg.rs1 == 1 && dec_cmp_reg.rs2 == 2 && !dec_cmp_reg.immediate);
    TEST_ASSERT(disassemble(dec_cmp_reg) == "cmp r1, r2");

    // Immediate-form CMP: cmp r4, 100 -> rs1=4, imm=100
    std::string s_cmp_imm = "cmp r4, 100\nhalt\n";
    auto prog_cmp_imm = a.assemble(s_cmp_imm);
    Instruction dec_cmp_imm = decode(prog_cmp_imm[0]);
    TEST_ASSERT(dec_cmp_imm.op == Opcode::CMP);
    TEST_ASSERT(dec_cmp_imm.rs1 == 4 && dec_cmp_imm.imm == 100 && dec_cmp_imm.immediate);
    TEST_ASSERT(disassemble(dec_cmp_imm) == "cmp r4, 100");
}

// 48. Fix Verification: ALU Undefined Behavior & Edge Cases (UBSan checks)
void test_alu_ubsan_edge_cases() {
    bool overflow = false;
    bool div_zero = false;

    // Booth Radix-4 negative operands and INT32_MIN boundaries
    int32_t r1 = ALU::booth_radix4_multiply(-15, 3, overflow);
    TEST_ASSERT(r1 == -45);

    int32_t r2 = ALU::booth_radix4_multiply(-20, -4, overflow);
    TEST_ASSERT(r2 == 80);

    int32_t r3 = ALU::booth_radix4_multiply(int32_t(0x7FFFFFFF), 1, overflow);
    TEST_ASSERT(r3 == int32_t(0x7FFFFFFF));

    int32_t r4 = ALU::booth_radix4_multiply(int32_t(0x80000000), 1, overflow);
    TEST_ASSERT(r4 == int32_t(0x80000000));

    // Non-restoring divide negative operands and INT32_MIN boundaries
    int32_t d1 = ALU::nonrestoring_divide(-50, 5, div_zero);
    TEST_ASSERT(d1 == -10 && !div_zero);

    int32_t d2 = ALU::nonrestoring_divide(50, -5, div_zero);
    TEST_ASSERT(d2 == -10 && !div_zero);

    int32_t d3 = ALU::nonrestoring_divide(-50, -5, div_zero);
    TEST_ASSERT(d3 == 10 && !div_zero);

    int32_t d4 = ALU::nonrestoring_divide(0, 100, div_zero);
    TEST_ASSERT(d4 == 0 && !div_zero);

    div_zero = false;
    int32_t d_zero = ALU::nonrestoring_divide(100, 0, div_zero);
    (void)d_zero;
    TEST_ASSERT(div_zero);

    // Barrel shifter boundary tests
    int32_t s_lsl = ALU::barrel_shift(1, 31, AluOp::LSL);
    TEST_ASSERT(s_lsl == int32_t(0x80000000));

    int32_t s_lsr = ALU::barrel_shift(int32_t(0x80000000), 1, AluOp::LSR);
    TEST_ASSERT(s_lsr == 0x40000000);

    int32_t s_asr = ALU::barrel_shift(int32_t(0x80000000), 1, AluOp::ASR);
    TEST_ASSERT(s_asr == int32_t(0xC0000000));

    int32_t s_zero_amt = ALU::barrel_shift(42, 0, AluOp::LSL);
    TEST_ASSERT(s_zero_amt == 42);
}

// 49. Fix Verification: Complete L1 Cache Read32/Write32 & Dirty Eviction
void test_l1_cache_functional() {
    Memory mem;
    L1Cache cache; // 1KB, 2-way, 32B block size, 16 sets

    // Initialize memory words
    mem.write32(0x200, 0xAABBCCDD);
    mem.write32(0x204, 0x11223344);

    // Test cache read32 with line fill
    int32_t val1 = cache.read32(0x200, mem);
    TEST_ASSERT(val1 == int32_t(0xAABBCCDD));
    TEST_ASSERT(cache.misses() == 1);
    TEST_ASSERT(cache.hits() == 0);

    // Cache hit on same word
    int32_t val1_hit = cache.read32(0x200, mem);
    TEST_ASSERT(val1_hit == int32_t(0xAABBCCDD));
    TEST_ASSERT(cache.hits() == 1);

    // Spatial locality hit on adjacent word in same cache line
    int32_t val2_hit = cache.read32(0x204, mem);
    TEST_ASSERT(val2_hit == int32_t(0x11223344));
    TEST_ASSERT(cache.hits() == 2);

    // Test cache write32 (write-allocate & dirty line update)
    cache.write32(0x200, 0x55667788, mem);
    // Read back through cache
    TEST_ASSERT(cache.read32(0x200, mem) == 0x55667788);

    // Evict line by filling set with 2 other lines (Set index = (addr >> 5) & 15)
    // 0x200 >> 5 = 16, 16 & 15 = Set 0
    uint32_t way1_addr = 0x200 + (16 * 32);     // Set 0, tag 1
    uint32_t way2_addr = 0x200 + (2 * 16 * 32); // Set 0, tag 2

    cache.write32(way1_addr, 0x11111111, mem);
    cache.write32(way2_addr, 0x22222222, mem); // Causes eviction of dirty 0x200

    TEST_ASSERT(cache.evictions() > 0);
    // Verify write-back to backing memory took place!
    TEST_ASSERT(mem.read32(0x200) == 0x55667788);
}

// 50. Fix Verification: Store Buffer Retirement, Forwarding, and Buffer Full
void test_store_buffer_extended() {
    StoreBuffer sb;

    // Push 4 entries (full capacity)
    sb.push(0x100, 10);
    sb.push(0x104, 20);
    sb.push(0x108, 30);
    sb.push(0x10C, 40);
    TEST_ASSERT(sb.is_full());
    TEST_ASSERT(sb.size() == 4);

    // Verify lookup finds newest entry
    int32_t val = 0;
    TEST_ASSERT(sb.lookup(0x108, val) && val == 30);

    // Overwrite to same address: newest value must be forwarded
    sb.push(0x108, 999);
    TEST_ASSERT(sb.lookup(0x108, val) && val == 999);

    // Retire entry
    TEST_ASSERT(sb.retire(0x104));
    TEST_ASSERT(!sb.lookup(0x104, val));

    // Draining
    uint32_t d_addr = 0;
    int32_t d_val = 0;
    while (sb.drain_one(d_addr, d_val)) {}
    TEST_ASSERT(sb.is_empty());
}

// 51. Fix Verification: Load-Delay Slot Optimizer Hazard Safety
void test_assembler_optimizer_safety() {
    Assembler a;

    // Test RAW hazard: instruction using loaded register must NOT be moved into delay slot
    std::string raw_prog =
        "ld r1, 0, [r0]\n"
        "add r2, r1, 10\n"
        "mov r3, 50\n"
        "halt\n";
    std::string raw_opt = a.optimize_load_delay_slots(raw_prog);
    size_t pos_mov = raw_opt.find("mov r3, 50");
    size_t pos_add = raw_opt.find("add r2, r1, 10");
    TEST_ASSERT(pos_mov < pos_add); // mov safely moved into delay slot between ld and add

    // Test Store/Memory instruction: store MUST NOT be moved into delay slot
    std::string st_prog =
        "ld r1, 0, [r0]\n"
        "st r2, 4, [r0]\n"
        "mov r3, 50\n"
        "halt\n";
    std::string st_opt = a.optimize_load_delay_slots(st_prog);
    // st should NOT be jumped over; order remains preserved
    size_t p_ld = st_opt.find("ld r1");
    size_t p_st = st_opt.find("st r2");
    TEST_ASSERT(p_ld < p_st);

    // Test Branch instruction: branch MUST NOT be hoisted into delay slot
    std::string br_prog =
        "ld r1, 0, [r0]\n"
        "add r2, r1, 5\n"
        "b target\n"
        "target: halt\n";
    std::string br_opt = a.optimize_load_delay_slots(br_prog);
    size_t p_b = br_opt.find("b target");
    size_t p_target = br_opt.find("target: halt");
    TEST_ASSERT(p_b < p_target);
}

// 57. CPU Cache Integration: ST then LD through D-Cache (4-stage)
void test_cpu_cache_st_ld_4stage() {
    Assembler a;
    std::string s =
        "mov r1, 999\n"
        "st r1, 0, [r0]\n"
        "ld r2, 0, [r0]\n"
        "halt\n";
    CPU cpu(Pipeline::P4);
    cpu.load(a.assemble(s));
    cpu.run();
    TEST_ASSERT(cpu.get_reg(2) == 999);
    TEST_ASSERT(cpu.dcache().accesses() > 0);
}

// 58. CPU Cache Integration: Repeated stores then load (D-Cache write-back)
void test_cpu_cache_repeated_stores() {
    Assembler a;
    std::string s =
        "mov r1, 111\n"
        "st r1, 0, [r0]\n"
        "mov r1, 222\n"
        "st r1, 0, [r0]\n"
        "mov r1, 333\n"
        "st r1, 0, [r0]\n"
        "ld r2, 0, [r0]\n"
        "halt\n";
    CPU cpu(Pipeline::P4);
    cpu.load(a.assemble(s));
    cpu.run();
    TEST_ASSERT(cpu.get_reg(2) == 333);
}

// 59. CPU Cache Integration: 6-stage pipeline LD/ST through D-Cache
void test_cpu_cache_6stage() {
    Assembler a;
    std::string s =
        "mov r1, 777\n"
        "st r1, 100, [r0]\n"
        "ld r2, 100, [r0]\n"
        "halt\n";
    CPU cpu(Pipeline::P6);
    cpu.load(a.assemble(s));
    cpu.run();
    TEST_ASSERT(cpu.get_reg(2) == 777);
    TEST_ASSERT(cpu.dcache().accesses() > 0);
}

// 60. CPU Cache Integration: Multiple words in one cache line
void test_cpu_cache_spatial_locality() {
    Assembler a;
    std::string s =
        "mov r1, 10\n"
        "st r1, 0, [r0]\n"
        "mov r1, 20\n"
        "st r1, 4, [r0]\n"
        "mov r1, 30\n"
        "st r1, 8, [r0]\n"
        "ld r2, 0, [r0]\n"
        "ld r3, 4, [r0]\n"
        "ld r4, 8, [r0]\n"
        "halt\n";
    CPU cpu(Pipeline::P4);
    cpu.load(a.assemble(s));
    cpu.run();
    TEST_ASSERT(cpu.get_reg(2) == 10);
    TEST_ASSERT(cpu.get_reg(3) == 20);
    TEST_ASSERT(cpu.get_reg(4) == 30);
}

// 61. CPU Cache Integration: Dirty Eviction & Load-After-Eviction (4-Stage Pipeline)
void test_cpu_cache_dirty_eviction_4stage() {
    Assembler a;
    std::string s =
        "mov r1, 100\n"
        "st r1, 4096, [r0]\n"
        "mov r1, 200\n"
        "st r1, 4608, [r0]\n"
        "mov r1, 300\n"
        "st r1, 5120, [r0]\n"
        "ld r2, 4096, [r0]\n"
        "halt\n";
    CPU cpu(Pipeline::P4);
    cpu.load(a.assemble(s));
    cpu.run();
    TEST_ASSERT(cpu.get_reg(2) == 100);
    TEST_ASSERT(cpu.dcache().evictions() > 0);
    TEST_ASSERT(cpu.memory().read32(4096) == 100);
    TEST_ASSERT(cpu.memory().read32(4608) == 200);
    TEST_ASSERT(cpu.memory().read32(5120) == 300);
}

// 62. CPU Cache Integration: Dirty Eviction & Load-After-Eviction (6-Stage Pipeline)
void test_cpu_cache_dirty_eviction_6stage() {
    Assembler a;
    std::string s =
        "mov r1, 111\n"
        "st r1, 4096, [r0]\n"
        "mov r1, 222\n"
        "st r1, 4608, [r0]\n"
        "mov r1, 333\n"
        "st r1, 5120, [r0]\n"
        "ld r2, 4096, [r0]\n"
        "halt\n";
    CPU cpu(Pipeline::P6);
    cpu.load(a.assemble(s));
    cpu.run();
    TEST_ASSERT(cpu.get_reg(2) == 111);
    TEST_ASSERT(cpu.dcache().evictions() > 0);
    TEST_ASSERT(cpu.memory().read32(4096) == 111);
    TEST_ASSERT(cpu.memory().read32(4608) == 222);
    TEST_ASSERT(cpu.memory().read32(5120) == 333);
}

// 63. Store Buffer Integration: Full-Capacity Draining & Final Memory State
void test_cpu_store_buffer_lifecycle_draining() {
    Assembler a;
    std::string s =
        "mov r1, 10\n"
        "st r1, 0, [r0]\n"
        "mov r1, 20\n"
        "st r1, 4, [r0]\n"
        "mov r1, 30\n"
        "st r1, 8, [r0]\n"
        "mov r1, 40\n"
        "st r1, 12, [r0]\n"
        "mov r1, 50\n"
        "st r1, 16, [r0]\n"
        "halt\n";
    CPU cpu(Pipeline::P4);
    cpu.load(a.assemble(s));
    cpu.run();
    TEST_ASSERT(cpu.store_buffer().is_empty());
    TEST_ASSERT(cpu.memory().read32(0) == 10);
    TEST_ASSERT(cpu.memory().read32(4) == 20);
    TEST_ASSERT(cpu.memory().read32(8) == 30);
    TEST_ASSERT(cpu.memory().read32(12) == 40);
    TEST_ASSERT(cpu.memory().read32(16) == 50);
}

// 64. Cache Boundary-Crossing: Normal aligned read/write
void test_cache_boundary_normal_aligned() {
    Memory mem(64 * 1024);
    L1Cache cache("TestCache");
    for (uint32_t i = 0; i < 8; ++i) {
        cache.write32(4096 + i * 4, 0x10000000 + i, mem);
    }
    for (uint32_t i = 0; i < 8; ++i) {
        TEST_ASSERT(cache.read32(4096 + i * 4, mem) == (0x10000000 + i));
    }
    cache.flush(mem);
    for (uint32_t i = 0; i < 8; ++i) {
        TEST_ASSERT(mem.read32(4096 + i * 4) == (0x10000000 + i));
    }
}

// 65. Cache Boundary-Crossing: Access at last byte positions of a cache line
void test_cache_boundary_last_byte_positions() {
    Memory mem(64 * 1024);
    L1Cache cache("TestCache");
    // Offset 28: last fully contained word in cache line (bytes 28..31)
    cache.write32(4096 + 28, 0x11223344, mem);
    TEST_ASSERT(cache.read32(4096 + 28, mem) == 0x11223344);

    // Offset 29: 3 bytes in line 1, 1 byte in line 2
    cache.write32(4096 + 29, 0xAABBCCDD, mem);
    TEST_ASSERT(cache.read32(4096 + 29, mem) == 0xAABBCCDD);

    // Offset 30: 2 bytes in line 1, 2 bytes in line 2
    cache.write32(4096 + 30, 0x55667788, mem);
    TEST_ASSERT(cache.read32(4096 + 30, mem) == 0x55667788);

    // Offset 31: 1 byte in line 1, 3 bytes in line 2
    cache.write32(4096 + 31, 0x99AABBCC, mem);
    TEST_ASSERT(cache.read32(4096 + 31, mem) == 0x99AABBCC);

    cache.flush(mem);
    TEST_ASSERT(mem.read32(4096 + 31) == 0x99AABBCC);
}

// 66. Cache Boundary-Crossing: Cross-boundary 32-bit read
void test_cache_boundary_cross_read() {
    Memory mem(64 * 1024);
    L1Cache cache("TestCache");
    // Pre-populate backing memory across boundary (Line 1 ends at 4127, Line 2 begins at 4128)
    mem.write8(4124, 0x99);
    mem.write8(4125, 0x12);
    mem.write8(4126, 0xAA);
    mem.write8(4127, 0xBB);
    mem.write8(4128, 0xCC);
    mem.write8(4129, 0xDD);
    mem.write8(4130, 0x77);
    mem.write8(4131, 0x88);

    // Test offset 29 (3 bytes first line, 1 byte second line)
    TEST_ASSERT(cache.read32(4125, mem) == 0xCCBBAA12);

    // Test offset 30 (2 bytes first line, 2 bytes second line)
    TEST_ASSERT(cache.read32(4126, mem) == 0xDDCCBBAA);

    // Test offset 31 (1 byte first line, 3 bytes second line)
    TEST_ASSERT(cache.read32(4127, mem) == 0x77DDCCBB);

    // Test offset 0 of next line (aligned)
    TEST_ASSERT(cache.read32(4128, mem) == 0x8877DDCC);
}

// 67. Cache Boundary-Crossing: Cross-boundary 32-bit write
void test_cache_boundary_cross_write() {
    Memory mem(64 * 1024);
    L1Cache cache("TestCache");
    uint32_t test_val = 0x01020304;
    cache.write32(4126, test_val, mem);

    TEST_ASSERT(cache.read32(4126, mem) == test_val);

    cache.flush(mem);
    TEST_ASSERT(mem.read8(4126) == 0x04);
    TEST_ASSERT(mem.read8(4127) == 0x03);
    TEST_ASSERT(mem.read8(4128) == 0x02);
    TEST_ASSERT(mem.read8(4129) == 0x01);
    TEST_ASSERT(mem.read32(4126) == test_val);
}

// 68. Cache Boundary-Crossing: Read immediately after cross-boundary write
void test_cache_boundary_read_after_write() {
    Memory mem(64 * 1024);
    L1Cache cache("TestCache");
    cache.write32(4126, 0xCAFEBABE, mem);

    // Verify backing memory is not yet modified before flush
    TEST_ASSERT(mem.read32(4126) != 0xCAFEBABE);

    // Immediate read from cache returns updated word
    TEST_ASSERT(cache.read32(4126, mem) == 0xCAFEBABE);

    // Surrounding word access
    uint32_t w4124 = cache.read32(4124, mem);
    TEST_ASSERT(((w4124 >> 16) & 0xFF) == 0xBE);
    TEST_ASSERT(((w4124 >> 24) & 0xFF) == 0xBA);
}

// 69. Cache Boundary-Crossing: Dirty eviction after cross-boundary write
void test_cache_boundary_dirty_eviction() {
    Memory mem(64 * 1024);
    L1Cache cache("TestCache");
    cache.write32(4126, 0xDEADBEEF, mem);
    TEST_ASSERT(mem.read32(4126) == 0);

    // Evict Set 0 (Line 1: 4096)
    cache.access(0, false, mem);
    cache.access(512, false, mem);
    TEST_ASSERT(mem.read8(4126) == 0xEF);
    TEST_ASSERT(mem.read8(4127) == 0xBE);

    // Evict Set 1 (Line 2: 4128)
    cache.access(32, false, mem);
    cache.access(544, false, mem);
    TEST_ASSERT(mem.read8(4128) == 0xAD);
    TEST_ASSERT(mem.read8(4129) == 0xDE);

    TEST_ASSERT(mem.read32(4126) == 0xDEADBEEF);
    TEST_ASSERT(cache.read32(4126, mem) == 0xDEADBEEF);
}

// 70. Cache Boundary-Crossing: Repeated writes across same boundary
void test_cache_boundary_repeated_writes() {
    Memory mem(64 * 1024);
    L1Cache cache("TestCache");
    cache.write32(4126, 0x11111111, mem);
    TEST_ASSERT(cache.read32(4126, mem) == 0x11111111);

    cache.write32(4126, 0x22222222, mem);
    TEST_ASSERT(cache.read32(4126, mem) == 0x22222222);

    cache.write32(4126, 0x33333333, mem);
    TEST_ASSERT(cache.read32(4126, mem) == 0x33333333);

    cache.flush(mem);
    TEST_ASSERT(mem.read32(4126) == 0x33333333);
}

// 71. Store Buffer: Enqueue four stores (full capacity)
void test_store_buffer_enqueue_four() {
    StoreBuffer sb;
    TEST_ASSERT(sb.is_empty());
    TEST_ASSERT(sb.size() == 0);

    TEST_ASSERT(sb.push(0x1000, 10));
    TEST_ASSERT(sb.push(0x1004, 20));
    TEST_ASSERT(sb.push(0x1008, 30));
    TEST_ASSERT(sb.push(0x100C, 40));

    TEST_ASSERT(sb.size() == 4);
    TEST_ASSERT(sb.is_full());
}

// 72. Store Buffer: Enqueue fifth store handling
void test_store_buffer_enqueue_fifth() {
    StoreBuffer sb;
    sb.push(0x1000, 10);
    sb.push(0x1004, 20);
    sb.push(0x1008, 30);
    sb.push(0x100C, 40);
    TEST_ASSERT(sb.is_full());

    bool ok_no_mem = sb.push(0x1010, 50);
    TEST_ASSERT(!ok_no_mem);
    TEST_ASSERT(sb.size() == 4);

    Memory mem(64 * 1024);
    L1Cache cache;
    bool ok_with_mem = sb.push(0x1010, 50, &cache, &mem);
    TEST_ASSERT(ok_with_mem);
    TEST_ASSERT(sb.size() == 4);
    TEST_ASSERT(cache.read32(0x1000, mem) == 10);
}

// 73. Store Buffer: Full-buffer handling without backing memory
void test_store_buffer_full_no_backing_mem() {
    StoreBuffer sb;
    sb.push(0x100, 101);
    sb.push(0x104, 102);
    sb.push(0x108, 103);
    sb.push(0x10C, 104);

    bool success = sb.push(0x110, 105, nullptr, nullptr);
    TEST_ASSERT(!success);

    int32_t val = 0;
    TEST_ASSERT(sb.lookup(0x100, val) && val == 101);
    TEST_ASSERT(sb.lookup(0x104, val) && val == 102);
    TEST_ASSERT(sb.lookup(0x108, val) && val == 103);
    TEST_ASSERT(sb.lookup(0x10C, val) && val == 104);
    TEST_ASSERT(sb.size() == 4);
}

// 74. Store Buffer: Full-buffer handling with valid memory/cache
void test_store_buffer_full_with_backing_mem() {
    StoreBuffer sb;
    Memory mem(64 * 1024);
    L1Cache cache;
    uint64_t mem_writes = 0;

    sb.push(0x200, 201);
    sb.push(0x204, 202);
    sb.push(0x208, 203);
    sb.push(0x20C, 204);
    TEST_ASSERT(sb.is_full());

    bool ok = sb.push(0x210, 205, &cache, &mem, &mem_writes);
    TEST_ASSERT(ok);
    TEST_ASSERT(sb.size() == 4);
    TEST_ASSERT(mem_writes == 1);
    TEST_ASSERT(cache.read32(0x200, mem) == 201);

    ok = sb.push(0x214, 206, &cache, &mem, &mem_writes);
    TEST_ASSERT(ok);
    TEST_ASSERT(mem_writes == 2);
    TEST_ASSERT(cache.read32(0x204, mem) == 202);
}

// 75. Store Buffer: FIFO ordering verification
void test_store_buffer_fifo_ordering() {
    StoreBuffer sb;
    sb.push(0x300, 301);
    sb.push(0x304, 302);
    sb.push(0x308, 303);
    sb.push(0x30C, 304);

    uint32_t a = 0;
    int32_t v = 0;

    TEST_ASSERT(sb.drain_one(a, v));
    TEST_ASSERT(a == 0x300 && v == 301);

    TEST_ASSERT(sb.drain_one(a, v));
    TEST_ASSERT(a == 0x304 && v == 302);

    TEST_ASSERT(sb.drain_one(a, v));
    TEST_ASSERT(a == 0x308 && v == 303);

    TEST_ASSERT(sb.drain_one(a, v));
    TEST_ASSERT(a == 0x30C && v == 304);

    TEST_ASSERT(!sb.drain_one(a, v));
    TEST_ASSERT(sb.is_empty());
}

// 76. Store Buffer: Repeated stores to same address
void test_store_buffer_repeated_stores_same_address() {
    StoreBuffer sb;
    sb.push(0x400, 111);
    sb.push(0x400, 222);

    int32_t val = 0;
    TEST_ASSERT(sb.lookup(0x400, val));
    TEST_ASSERT(val == 222);

    int32_t ret_val = 0;
    TEST_ASSERT(sb.retire(0x400, ret_val));
    TEST_ASSERT(ret_val == 111);

    TEST_ASSERT(sb.retire(0x400, ret_val));
    TEST_ASSERT(ret_val == 222);

    TEST_ASSERT(!sb.retire(0x400, ret_val));
}

// 77. Store Buffer: Store-to-load forwarding
void test_store_buffer_forwarding_behavior() {
    StoreBuffer sb;
    sb.push(0x500, 501);
    sb.push(0x504, 502);

    int32_t val = 0;
    TEST_ASSERT(sb.lookup(0x500, val) && val == 501);
    TEST_ASSERT(sb.lookup(0x504, val) && val == 502);
    TEST_ASSERT(!sb.lookup(0x508, val));
    TEST_ASSERT(sb.get_forwards() == 2);
}

// 78. Store Buffer: Final memory correctness after full drain
void test_store_buffer_final_memory_correctness() {
    StoreBuffer sb;
    Memory mem(64 * 1024);
    L1Cache cache;

    sb.push(0x600, 601);
    sb.push(0x604, 602);
    sb.push(0x608, 603);
    sb.push(0x60C, 604);

    sb.push(0x610, 605, &cache, &mem);
    sb.push(0x614, 606, &cache, &mem);
    sb.push(0x618, 607, &cache, &mem);

    sb.drain_all(cache, mem);
    TEST_ASSERT(sb.is_empty());

    cache.flush(mem);

    TEST_ASSERT(mem.read32(0x600) == 601);
    TEST_ASSERT(mem.read32(0x604) == 602);
    TEST_ASSERT(mem.read32(0x608) == 603);
    TEST_ASSERT(mem.read32(0x60C) == 604);
    TEST_ASSERT(mem.read32(0x610) == 605);
    TEST_ASSERT(mem.read32(0x614) == 606);
    TEST_ASSERT(mem.read32(0x618) == 607);
}

int main() {
    run_test("ISA encode/decode roundtrip", test_isa_encode_decode);
    run_test("ISA disassemble output correctness", test_isa_disasm);
    run_test("ALU naive ops", test_alu_naive);
    run_test("ALU Booth multiply", test_alu_booth);
    run_test("ALU Booth Radix-4 multiply", test_alu_booth_radix4);
    run_test("ALU CLA add", test_alu_cla);
    run_test("ALU Kogge-Stone add", test_alu_kogge_stone);
    run_test("ALU Carry-Select add", test_alu_csla);
    run_test("ALU nonrestoring divide", test_alu_div);
    run_test("ALU barrel shift", test_alu_barrel);
    run_test("ALU edge cases", test_alu_edge);
    run_test("Assembler basic", test_asm_basic);
    run_test("Assembler labels", test_asm_labels);
    run_test("Assembler push/pop", test_asm_push_pop);
    run_test("Assembler disasm hex", test_asm_disasm);
    run_test("CPU arith 4-stage", test_cpu_arith_4);
    run_test("CPU arith 6-stage", test_cpu_arith_6);
    run_test("CPU branch test", test_cpu_branch);
    run_test("CPU unconditional branch", test_cpu_uncond_branch);
    run_test("CPU call ret", test_cpu_call_ret);
    run_test("CPU stack macros", test_cpu_stack);
    run_test("CPU nop", test_cpu_nop);
    run_test("CPU halt", test_cpu_halt);
    run_test("CPU hardwired", test_cpu_hardwired);
    run_test("CPU horizontal", test_cpu_horizontal);
    run_test("CPU vertical", test_cpu_vertical);
    run_test("Control horizontal", test_ctrl_horizontal_roundtrip);
    run_test("Control vertical", test_ctrl_vertical_roundtrip);
    run_test("Memory read/write", test_mem_rw);
    run_test("Memory bounds", test_mem_bounds);
    run_test("Stack operations", test_stack_push_pop);
    run_test("Stack bounds", test_stack_overflow);
    run_test("Stack corruption check", test_stack_corruption);
    run_test("Assembler binary translation", test_asm_binary);
    run_test("CPU micro-step execution", test_cpu_micro_step);
    run_test("CPU factorial subroutine", test_cpu_factorial);
    run_test("CPU optimized ALU execution", test_cpu_optimized_alu);
    run_test("Pipeline 4 vs 6 comparison", test_pipeline_comparison);
    run_test("Microcoded CALL/RET frames", test_microcode_call_ret_frames);
    run_test("Vertical microcode 16-bit", test_vertical_microcode_16bit);
    run_test("Stack SP initialization", test_stack_sp_initialization);
    run_test("NOT instruction across configs", test_not_instruction_all_configs);
    run_test("Assembler error handling", test_assembler_error_handling);
    run_test("Stack underflow on pop", test_stack_underflow_pop_empty);
    run_test("Branch Predictor 2-bit state machine", test_branch_predictor_2bit_state);
    run_test("CPU Dynamic 2-bit branch prediction", test_cpu_dynamic_branch_prediction);
    run_test("ALU Newton-Raphson Fast Divide & Modulo", test_alu_newton_raphson);
    run_test("Branch Predictor Return Address Stack (RAS)", test_branch_predictor_ras);
    run_test("Split L1 Cache (2-Way LRU)", test_l1_cache_split);
    run_test("Hardware Store-to-Load Forwarding", test_store_to_load_forwarding);
    run_test("Assembler Load Delay Slot Optimizer", test_assembler_delay_slot_optimizer);
    run_test("Disassembler Operand Correctness (Bug A)", test_disassembler_operand_fixes);
    run_test("ALU UBSan Boundary & Negative Shifts (Bug B)", test_alu_ubsan_edge_cases);
    run_test("L1 Cache Read32/Write32 & Dirty Eviction (Bug C)", test_l1_cache_functional);
    run_test("Store Buffer Multi-Entry & Draining (Bug D)", test_store_buffer_extended);
    run_test("Assembler Optimizer Safety Hazards (Bug E)", test_assembler_optimizer_safety);
    run_test("CPU D-Cache ST then LD (4-stage)", test_cpu_cache_st_ld_4stage);
    run_test("CPU D-Cache Repeated Stores Write-Back", test_cpu_cache_repeated_stores);
    run_test("CPU D-Cache ST/LD 6-stage", test_cpu_cache_6stage);
    run_test("CPU D-Cache Spatial Locality", test_cpu_cache_spatial_locality);
    run_test("CPU D-Cache Dirty Eviction & Reload (4-stage)", test_cpu_cache_dirty_eviction_4stage);
    run_test("CPU D-Cache Dirty Eviction & Reload (6-stage)", test_cpu_cache_dirty_eviction_6stage);
    run_test("CPU Store Buffer Draining & Architectural State", test_cpu_store_buffer_lifecycle_draining);

    // Focused Tests for Cache Boundary-Crossing Access Bug Fix
    run_test("Cache Boundary: Normal Aligned Read/Write", test_cache_boundary_normal_aligned);
    run_test("Cache Boundary: Access at Last Byte Positions", test_cache_boundary_last_byte_positions);
    run_test("Cache Boundary: Cross-Boundary 32-Bit Read", test_cache_boundary_cross_read);
    run_test("Cache Boundary: Cross-Boundary 32-Bit Write", test_cache_boundary_cross_write);
    run_test("Cache Boundary: Read Immediately After Cross-Boundary Write", test_cache_boundary_read_after_write);
    run_test("Cache Boundary: Dirty Eviction After Cross-Boundary Write", test_cache_boundary_dirty_eviction);
    run_test("Cache Boundary: Repeated Writes Across Same Boundary", test_cache_boundary_repeated_writes);

    // Focused Tests for Store Buffer Silent Data-Loss Bug Fix
    run_test("Store Buffer: Enqueue Four Stores (Full Capacity)", test_store_buffer_enqueue_four);
    run_test("Store Buffer: Enqueue Fifth Store Handling", test_store_buffer_enqueue_fifth);
    run_test("Store Buffer: Full-Buffer Handling Without Backing Memory", test_store_buffer_full_no_backing_mem);
    run_test("Store Buffer: Full-Buffer Handling With Valid Memory/Cache", test_store_buffer_full_with_backing_mem);
    run_test("Store Buffer: FIFO Ordering Verification", test_store_buffer_fifo_ordering);
    run_test("Store Buffer: Repeated Stores to Same Address", test_store_buffer_repeated_stores_same_address);
    run_test("Store Buffer: Store-to-Load Forwarding", test_store_buffer_forwarding_behavior);
    run_test("Store Buffer: Final Memory Correctness After Full Drain", test_store_buffer_final_memory_correctness);

    std::cout << passed << "/" << total << " tests passed\n";
    return passed == total ? 0 : 1;
}
