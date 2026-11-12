; RISC201 Demonstration Program: Bitwise Operations & Shifts
mov r1, 0x0F
mov r2, 0xF0
and r3, r1, r2     ; r3 = 0x0F & 0xF0 = 0x00
or  r4, r1, r2     ; r4 = 0x0F | 0xF0 = 0xFF
not r5, r1         ; r5 = ~0x0F
lsl r6, r1, 4      ; r6 = 0x0F << 4 = 0xF0
lsr r7, r2, 4      ; r7 = 0xF0 >> 4 = 0x0F
asr r8, r5, 2      ; Arithmetic shift right
halt
