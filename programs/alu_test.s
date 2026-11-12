; ALU operations comprehensive test
mov r1, 15
mov r2, 3
add r3, r1, r2     ; r3 = 18
sub r4, r1, r2     ; r4 = 12
mul r5, r1, r2     ; r5 = 45
div r6, r1, r2     ; r6 = 5
mod r7, r1, r2     ; r7 = 0
and r8, r1, r2     ; r8 = 3 (1111 & 0011 = 0011)
or r9, r1, r2      ; r9 = 15 (1111 | 0011 = 1111)
not r10, r1        ; r10 = ~15 = -16
lsl r11, r1, 2     ; r11 = 60 (15 << 2)
lsr r12, r1, 1     ; r12 = 7 (15 >> 1 logical)
asr r13, r1, 1     ; r13 = 7 (15 >> 1 arithmetic, positive)
halt
