# RISC201 — Processor Design & Simulation Framework

## Course
**CSC-201: Computer Organization & Architecture**

## Team Members
1. Spoorthi
2. Sanjana
3. Sai Teja
4. Anu
5. Satwika

---

## Project Overview
**RISC201** is a C++17 processor design and cycle-accurate simulation framework implementing a custom 32-bit SimpleRisc instruction set architecture (ISA). The project models the complete processor datapath, control logic, memory hierarchy, cache subsystem, and pipelined execution with hazard resolution and branch prediction.

---

## Instruction Set Architecture (ISA)

### Register File & Flags
- **16 General-Purpose 32-bit Registers** (`r0`–`r15`):
  - `r0`–`r13`: General-purpose registers
  - `r14` (`sp`): Stack Pointer (full-descending stack)
  - `r15` (`ra`): Return Address register (updated by `call`)
- **Condition Flags**:
  - `E` (Equal): Set by `cmp` when `rs1 == operand2`
  - `GT` (Greater Than): Set by `cmp` when `rs1 > operand2`

### Instruction Formats (32-bit)
- **Register (`I = 0`)**: `[ opcode:5 ][ I:1 ][ rd:4 ][ rs1:4 ][ rs2:4 ][ unused:14 ]`
- **Immediate (`I = 1`)**: `[ opcode:5 ][ I:1 ][ rd:4 ][ rs1:4 ][ imm:18 ]` (sign-extended to 32 bits)
- **Branch**: `[ opcode:5 ][ offset:27 ]` (PC-relative word displacement: `PC + 4 + offset * 4`)
- **Zero-Operand (`nop`, `ret`, `halt`)**: `[ opcode:5 ][ 0:27 ]`

### Supported Instructions (22 Opcodes)
| Category | Mnemonics |
|---|---|
| Arithmetic | `add`, `sub`, `mul`, `div`, `mod`, `cmp` |
| Logical & Shift | `and`, `or`, `not`, `mov`, `lsl`, `lsr`, `asr` |
| Memory | `ld`, `st` |
| Control Flow | `beq`, `bgt`, `b`, `call`, `ret` |
| System | `nop`, `halt` |

Detailed encoding specifications are documented in [`docs/ISA.md`](docs/ISA.md).

---

## Hardware Subsystems

### 1. Arithmetic Logic Unit (`alu`)
Supports selectable hardware algorithms for arithmetic and bit-manipulation operations:
- **Addition**: Naive, Carry-Lookahead Adder (`CLA`), Carry-Select Adder (`CSLA`), and Kogge-Stone parallel prefix adder (`KOGGE_STONE`)
- **Multiplication**: Naive, Booth's algorithm (`BOOTH`), and Radix-4 Booth multiplication (`BOOTH_RADIX4`)
- **Division & Modulo**: Naive, Non-Restoring division (`NONRESTORING`), and Newton-Raphson fast division (`NEWTON_RAPHSON`)
- **Shifting**: Naive and Barrel Shifter (`BARREL`) for logical (`lsl`, `lsr`) and arithmetic (`asr`) shifts
- **Status Flags**: Computes `E`, `GT`, `overflow`, and `divide_by_zero`

### 2. Control Units (`control`)
Models three distinct control unit architectures:
- **Hardwired Control**: Direct combinational control signal generation
- **Horizontal Microcode (24-bit)**: Unencoded control store where bits directly drive datapath signals (`RegW`, `MemR`, `MemW`, `AluSrc`, `AluOp`, `Branch`, `Jump`, `Call`, `Ret`, `Halt`, `NextAddr`, `End`)
- **Vertical Microcode (16-bit)**: Encoded microinstructions (`[OpField:3][DataField:4][NextAddr:8][End:1]`) decoded against a 256-word control store

### 3. Memory Hierarchy, L1 Cache & Store Buffer (`memory`, `cache`, `stack`)
- **Main Memory**: 64 KB little-endian byte-addressable memory (`read8`, `read32`, `write8`, `write32`)
- **L1 Cache**: Split 1 KB 2-way set-associative instruction and data caches (16 sets × 2 ways × 32-byte cache lines) with LRU replacement and dirty-line tracking
- **Store Buffer**: 4-entry FIFO hardware store buffer with store-to-load forwarding and automatic drain on capacity
- **Stack Subsystem**: Full-descending stack conventions (`push` / `pop` macro expansion) with boundary checking

### 4. Pipeline & Branch Prediction (`cpu`, `branch_predictor`)
- **Configurable Pipelines**:
  - **4-Stage Pipeline**: `IF -> ID -> EX -> WB`
  - **6-Stage Pipeline**: `IF -> ID -> EX1 -> EX2 -> MEM -> WB`
- **Hazard Handling**: Data hazard detection, register forwarding paths, pipeline stalls, and control-hazard flushes
- **Branch Prediction**:
  - Static Not-Taken baseline
  - Dynamic 2-bit saturating counter (`STRONGLY_NOT_TAKEN`, `WEAKLY_NOT_TAKEN`, `WEAKLY_TAKEN`, `STRONGLY_TAKEN`) with a Branch Target Buffer (`BTB`)
  - 16-entry circular speculative Return Address Stack (`RAS`) for subroutine `call`/`ret` prediction

### 5. Assembler & Disassembler (`assembler`)
- Two-pass assembler supporting labels, comments (`;`), register aliases (`sp`, `ra`), and `push`/`pop` stack macros
- Generates 32-bit machine code and supports disassembly back to human-readable mnemonics

---

## Repository Structure
```
risc201/
├── CMakeLists.txt          # CMake build configuration
├── docs/
│   └── ISA.md              # Detailed ISA & microcode reference
├── include/risc201/        # Public C++ headers for all processor modules
│   ├── alu.hpp
│   ├── assembler.hpp
│   ├── branch_predictor.hpp
│   ├── cache.hpp
│   ├── control.hpp
│   ├── cpu.hpp
│   ├── isa.hpp
│   ├── memory.hpp
│   └── stack.hpp
├── src/                    # C++17 implementations and simulator entry point
├── programs/               # Sample SimpleRisc assembly programs
└── tests/                  # Unit and integration test suite
```
