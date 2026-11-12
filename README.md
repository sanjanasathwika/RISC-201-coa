# Team Members
1. Spoorthi
2. Sanjana
3. Sai Teja
4. Anu
5. Satwika

## Course: CSC-201: Computer Organization & Architecture
### Project: Processor Design & Simulation Framework (RISC201)

---

## How to Run

### ⚡ Quick Demo in VS Code (One-Click Task)
To demonstrate the entire project in VS Code with a single click:
1. In the top menu, go to **Terminal** -> **Run Task...** (or press `Ctrl+Shift+P` and type `Tasks: Run Task`).
2. Select **`★ DEMONSTRATE ALL UNITS: Complete Architecture Showcase`** (or press `Ctrl+Shift+B` to run the default task directly).
3. The automated showcase will run and present:
   - **Module 1**: ISA & Assembler (Encoding, Decoding, Disassembly)
   - **Module 2**: ALU Arithmetic (Kogge-Stone, CSLA, CLA, Radix-4 Booth, Newton-Raphson Fast Division)
   - **Module 3**: Control Units (Hardwired 1 CPI, 24-bit Horizontal Microcode, 16-bit Vertical Microcode)
   - **Module 4**: Memory Subsystem & L1 Cache (64KB Little-Endian, Split I/D-Cache, Store Buffer Forwarding)
   - **Module 5**: Pipeline & Hazards (4-Stage vs 6-Stage, Data Hazard Forwarding, Dynamic 2-Bit Branch Predictor)
   - **Module 6**: Real-World Programs (Factorial = 120, Fibonacci = 21, Hazard Test)
   - **Module 7**: Automated Test Suite (All 78/78 Unit & Integration Tests Passing)

You can also run individual unit demonstrations directly from the **Terminal -> Run Task...** menu.

---

### Command Line Execution

To run the simulator or tests manually, open a terminal inside the project root directory:

---

### 1. Run Automated Test Suite
To verify the entire project (all 63 unit and integration tests):

```powershell
# Windows
.\build-win\risc201_tests.exe

# Linux / macOS
./build/risc201_tests
```

---

### 2. Run Assembly Programs (`run`)
To execute an assembly program to completion:

```powershell
# Run with default 4-stage pipeline and hardwired control:
.\build-win\risc201.exe run programs/factorial.s

# Run with 6-stage pipeline and horizontal microcode:
.\build-win\risc201.exe run programs/factorial.s 6 horizontal

# Run with vertical microcode and optimized ALU:
.\build-win\risc201.exe run programs/factorial.s 4 vertical optimized
```

Available sample programs in `programs/`:
- `programs/factorial.s` (recursive factorial computation with CALL/RET)
- `programs/fibonacci.s` (Fibonacci sequence calculation)
- `programs/stack.s` (stack push/pop macro operations)
- `programs/hazard_test.s` (pipeline load-use data hazard testing)
- `programs/alu_test.s` (arithmetic and logical operations)
- `programs/branch.s` (branch conditions: BEQ, BGT, B)
- `programs/add.s` (basic arithmetic)

---

### 3. Run Architecture Comparison Benchmark (`compare`)
Compares execution cycles, CPI, stalls, pipeline flushes, branch prediction accuracy, and micro-ops across 11 different architectural configurations:

```powershell
# Compare configurations on Factorial:
.\build-win\risc201.exe compare programs/factorial.s

# Compare configurations on Pipeline Hazards:
.\build-win\risc201.exe compare programs/hazard_test.s
```

---

### 4. Interactive CLI Debugger (`debug`)
Step through instructions, inspect registers, view the pipeline, and step through microcode:

```powershell
.\build-win\risc201.exe debug programs/factorial.s 4 hardwired
```

#### Debugger Commands:
- `step [n]` or `s [n]` : Step `n` instructions (default 1)
- `stepm [n]` or `sm [n]` : Step `n` micro-instructions (in microcoded mode)
- `pipeline` or `pipe` : View live ASCII diagram of all active pipeline stages
- `stack` : View active ASCII stack memory frames and memory usage
- `regs` : Display register file (`r0..r15`, SP, RA, flags)
- `micro` : Inspect current microPC and active control signals
- `mem <addr> [len]` : Dump memory at address in hex format
- `break <addr>` / `b <addr>` : Set a breakpoint at address
- `run` or `r` : Run until program halts or hits breakpoint
- `stats` : View performance statistics (CPI, stalls, flushes, cache hit rate)
- `quit` or `q` : Exit debugger

---

### 5. ALU & Memory Hardware Demonstrations
Demonstrate individual hardware subsystems:

```powershell
# Demo all ALU hardware algorithms (Kogge-Stone, CSLA, CLA, Radix-4 Booth, Newton-Raphson):
.\build-win\risc201.exe alu-demo

# Demo 64KB memory alignment, little-endian byte ordering, and hex dumping:
.\build-win\risc201.exe mem-demo
```

---

### 6. Assembler & Disassembler (`asm`, `disasm`)
Translate assembly to machine code (hexadecimal and binary bitstrings), and disassemble back:

```powershell
# Assemble to hexadecimal (.hex):
.\build-win\risc201.exe asm programs/add.s hex

# Assemble to 32-bit binary strings (.bin):
.\build-win\risc201.exe asm programs/add.s bin

# Disassemble hexadecimal file:
.\build-win\risc201.exe disasm programs/add.s.hex

# Disassemble binary file:
.\build-win\risc201.exe disasm programs/add.s.bin
```

---

### 7. Rebuilding from Source (Optional)

If you make changes to the C++ code and want to recompile:

#### On Windows (using MinGW g++):
```powershell
# Build main simulator:
g++ -std=c++17 -O3 -Iinclude src/isa.cpp src/alu.cpp src/memory.cpp src/stack.cpp src/assembler.cpp src/control.cpp src/cpu.cpp src/branch_predictor.cpp src/main.cpp -o build-win/risc201.exe

# Build test suite:
g++ -std=c++17 -O3 -Iinclude src/isa.cpp src/alu.cpp src/memory.cpp src/stack.cpp src/assembler.cpp src/control.cpp src/cpu.cpp src/branch_predictor.cpp tests/test_main.cpp -o build-win/risc201_tests.exe
```

#### On Linux / WSL:
```bash
g++ -std=c++17 -O3 -Iinclude src/isa.cpp src/alu.cpp src/memory.cpp src/stack.cpp src/assembler.cpp src/control.cpp src/cpu.cpp src/branch_predictor.cpp src/main.cpp -o build/risc201
g++ -std=c++17 -O3 -Iinclude src/isa.cpp src/alu.cpp src/memory.cpp src/stack.cpp src/assembler.cpp src/control.cpp src/cpu.cpp src/branch_predictor.cpp tests/test_main.cpp -o build/risc201_tests
```
