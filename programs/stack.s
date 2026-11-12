; Stack operations test
; Tests push/pop macro expansion
; Expected: r4=7, r5=99, r6=42
mov r1, 42
mov r2, 99
mov r3, 7
push r1
push r2
push r3
pop r4            ; r4 = 7
pop r5            ; r5 = 99
pop r6            ; r6 = 42
halt
