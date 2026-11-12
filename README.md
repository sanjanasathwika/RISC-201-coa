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
RISC201 is a C++17 processor design and cycle-accurate simulation framework for a custom 32-bit RISC instruction set architecture. It models the complete hardware datapath, control logic, memory hierarchy, and pipelined execution with hazard handling.

## Implemented Subsystems
- **ISA & Assembler (`isa`, `assembler`)**: 32-bit instruction encoding/decoding, two-pass assembly, and disassembly support.
- **ALU (`alu`)**: Arithmetic and logic unit supporting Kogge-Stone, Carry-Select (CSLA), and Carry-Lookahead (CLA) adders, Radix-4 Booth multiplication, and Newton-Raphson fast division.
- **Control Units (`control`)**: Hardwired single-cycle control, 24-bit horizontal microcode, and 16-bit vertical microcode control.
- **Memory & Cache (`memory`, `cache`, `stack`)**: 64KB little-endian byte-addressable memory, split L1 instruction/data caches, store buffer forwarding, and stack frame management.
- **Pipeline & Branch Prediction (`cpu`, `branch_predictor`)**: Configurable 4-stage and 6-stage pipelines with data hazard forwarding, stall/flush handling, and dynamic 2-bit branch prediction.
- **Programs & Verification (`programs/`, `tests/`)**: Assembly test programs (including factorial, Fibonacci, stack, branch, and hazard tests) and an automated unit and integration test suite.
