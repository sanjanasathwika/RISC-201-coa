; Basic arithmetic test
; Expected: r3 = 10, r4 = 2, r5 = 24, r6 = 6, r7 = 0
mov r1, 4
mov r2, 6
add r3, r1, r2     ; r3 = 10
sub r4, r2, r1     ; r4 = 2
mul r5, r1, r2     ; r5 = 24
div r6, r5, r1     ; r6 = 6
mod r7, r5, r1     ; r7 = 0
halt
