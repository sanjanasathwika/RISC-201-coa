; RISC201 Demonstration Program: Labels & Stack Macro Expansion
; Demonstrates two-pass label resolution and push/pop macros
push r1            ; Expands to: sub sp, sp, 4; st r1, 0, [sp]
push r2            ; Expands to: sub sp, sp, 4; st r2, 0, [sp]

start:
mov r1, 10
mov r2, 0

loop:
cmp r1, r2
beq done           ; Forward branch target resolved in pass 2
sub r1, r1, 1
b loop             ; Backward branch target resolved in pass 2

done:
pop r2             ; Expands to: ld r2, 0, [sp]; add sp, sp, 4
pop r1             ; Expands to: ld r1, 0, [sp]; add sp, sp, 4
halt
