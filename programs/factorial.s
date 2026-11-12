; Factorial of 5 using CALL/RET
; Expected: r1 = 120, r2 = 120
mov r1, 5          ; input n=5
mov r2, 1          ; accumulator = 1
call loop
b done
loop:
cmp r1, 0
beq return
mul r2, r2, r1
sub r1, r1, 1
b loop
return:
ret
done:
mov r1, r2         ; move result to r1 (120)
halt
