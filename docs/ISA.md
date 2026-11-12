# RISC201 / SimpleRisc ISA Reference

The CSC-201 course material describes SimpleRisc with 16 registers (`r0..r15`), 32-bit instructions, 5-bit opcodes, an immediate bit, 18-bit signed immediates, and PC-relative branch offsets. `r14` is the stack pointer (SP) and `r15` is the return-address register (RA).

## Registers

| Register | Alias | Purpose |
|----------|-------|---------|
| r0       |       | General purpose |
| r1-r13   |       | General purpose |
| r14      | sp    | Stack Pointer (full-descending) |
| r15      | ra    | Return Address (set by CALL) |

## Condition Flags

- **E** (Equal): Set by CMP when rs1 == operand2
- **GT** (Greater Than): Set by CMP when rs1 > operand2

Flags are persistent until the next CMP instruction. BEQ/BGT read these flags.

## Opcodes

| Mnemonic | Opcode | Format | Description |
|----------|-------:|--------|-------------|
| add | 0 | 3-address | rd = rs1 + rs2/imm |
| sub | 1 | 3-address | rd = rs1 - rs2/imm |
| mul | 2 | 3-address | rd = rs1 * rs2/imm |
| div | 3 | 3-address | rd = rs1 / rs2/imm |
| mod | 4 | 3-address | rd = rs1 % rs2/imm |
| cmp | 5 | compare | Sets E,GT flags from rs1 vs rs2/imm |
| and | 6 | 3-address | rd = rs1 & rs2/imm |
| or  | 7 | 3-address | rd = rs1 \| rs2/imm |
| not | 8 | 2-address | rd = ~rs2/imm |
| mov | 9 | 2-address | rd = rs2/imm |
| lsl | 10 | 3-address | rd = rs1 << rs2/imm |
| lsr | 11 | 3-address | rd = rs1 >> rs2/imm (logical) |
| asr | 12 | 3-address | rd = rs1 >> rs2/imm (arithmetic) |
| nop | 13 | 0-address | No operation |
| ld  | 14 | memory | rd = mem[rs1 + imm] |
| st  | 15 | memory | mem[rs1 + imm] = rd |
| beq | 16 | branch | Branch if E flag set |
| bgt | 17 | branch | Branch if GT flag set |
| b   | 18 | branch | Unconditional branch |
| call | 19 | branch | r15 = PC+4, branch to target |
| ret | 20 | 0-address | PC = r15 |
| halt | 21 | 0-address | Stop execution |

## Instruction Encoding

### Register/Immediate Format
```
[ opcode:5 ][ I:1 ][ rd:4 ][ rs1:4 ][ rs2:4 ][ unused:14 ]    (I=0, register)
[ opcode:5 ][ I:1 ][ rd:4 ][ rs1:4 ][ imm:18 ]                (I=1, immediate)
```
The 18-bit immediate is sign-extended to 32 bits.

### Branch Format
```
[ opcode:5 ][ offset:27 ]
```
The offset is a signed word displacement. Effective target = PC + 4 + (offset * 4).

### Zero-operand Format (NOP, RET, HALT)
```
[ opcode:5 ][ 0:27 ]
```

## Pipeline Stages

### 4-Stage Pipeline: IF -> ID -> EX -> WB
- **IF** (Instruction Fetch): Fetch instruction from memory at PC
- **ID** (Instruction Decode): Decode instruction, read register file
- **EX** (Execute): ALU operation, memory access, branch evaluation
- **WB** (Write Back): Write result to register file

### 6-Stage Pipeline: IF -> ID -> EX1 -> EX2 -> MEM -> WB
- **IF** (Instruction Fetch): Fetch instruction from memory at PC
- **ID** (Instruction Decode): Decode instruction, read register file
- **EX1** (Execute Stage 1): Begin ALU operation, compute addresses
- **EX2** (Execute Stage 2): Complete ALU operation
- **MEM** (Memory Access): Read/write data memory
- **WB** (Write Back): Write result to register file

## Microprogrammed Control Unit

### Horizontal Format (24-bit microinstruction)
Each bit directly controls a datapath signal:
```
[RegW:1][MemR:1][MemW:1][AluSrc:1][AluOp:4][Branch:1][Jump:1][Call:1][Ret:1][Halt:1][NextAddr:8][End:1][unused:2]
```

### Vertical Format (16-bit microinstruction)
Encoded fields require a decoder:
```
[OpField:3][DataField:4][NextAddr:8][End:1]
```
Bits:
- [15:13]: 3-bit operation field (0=NOP, 1=ALU_OP, 2=MEM_READ, 3=MEM_WRITE, 4=REG_WRITE, 5=BRANCH, 6=JUMP/CALL/RET, 7=HALT)
- [12:9]: 4-bit data field (ALU op code or jump/call/ret selector)
- [8:1]: 8-bit microaddress (addresses full 256-word control store)
- [0]: 1-bit end of sequence flag

## Stack Conventions

- Full-descending stack: SP starts at top of memory, grows downward
- `push rX` -> `sub sp, sp, 4; st rX, 0, [sp]`
- `pop rX` -> `ld rX, 0, [sp]; add sp, sp, 4`
- CALL saves return address to r15 (RA)
- Stack boundary checking prevents overflow/underflow

## Assembler Directives

- Labels: `name:` (must end with colon)
- Comments: `; comment text`
- Macros: `push rX`, `pop rX` (expand to 2 instructions each)
- `halt` is the program termination instruction

Because the course material contains some bit-numbering diagrams using one-based bit labels, the implementation uses explicit zero-based C++ shifts and documents them in the source. If your instructor's handout specifies a different bit convention, change only `isa.hpp`/`isa.cpp` and keep the rest of the simulator unchanged.
