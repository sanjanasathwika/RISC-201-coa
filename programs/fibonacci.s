; Fibonacci Sequence Calculator
; Computes F(7) = 13
; F(0)=0, F(1)=1, F(2)=1, F(3)=2, F(4)=3, F(5)=5, F(6)=8, F(7)=13
; Expected final registers:
;   r1 = 7  (n)
;   r2 = 13 (F(n))

mov r1, 7           ; n = 7
mov r2, 0           ; a = 0 (F(0))
mov r3, 1           ; b = 1 (F(1))
mov r4, 0           ; counter i = 0

fib_loop:
cmp r4, r1
beq fib_done

add r5, r2, r3      ; next = a + b
mov r2, r3          ; a = b
mov r3, r5          ; b = next
add r4, r4, 1       ; i++
b fib_loop

fib_done:
halt
