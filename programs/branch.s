; Branch test - tests CMP, BEQ, BGT, B
; Expected: r3 = 1, r4 = 10, r5 = 0
mov r1, 5
mov r2, 5
cmp r1, r2
beq equal
mov r3, 0         ; should be skipped
b done
equal:
mov r3, 1         ; should execute
b done
done:
mov r4, 10
cmp r1, r4
bgt greater
mov r5, 0
b end
greater:
mov r5, 1         ; should not execute (5 > 10 is false)
b end
end:
halt
