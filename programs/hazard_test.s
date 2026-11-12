; Data Hazard & Pipeline Stall Evaluation Program
; Demonstrates load-use data hazard and branch resolution
;
; In a 4-stage pipeline, load-use hazard introduces 1 stall.
; In a 6-stage pipeline, load data is available after MEM stage, introducing 2 stalls.

mov r1, 100         ; store base data
mov r2, 200
st r1, 0, [r0]      ; mem[0] = 100
st r2, 4, [r0]      ; mem[4] = 200

; Load-use Hazard 1
ld r3, 0, [r0]      ; Load r3 from memory
add r4, r3, 50      ; Dependent instruction immediately uses r3 -> LOAD-USE STALL!

; Load-use Hazard 2
ld r5, 4, [r0]      ; Load r5 from memory
sub r6, r5, r4      ; Dependent instruction immediately uses r5 -> LOAD-USE STALL!

; Branch hazard
cmp r6, 0
bgt positive
mov r7, 0
b done
positive:
mov r7, 1
done:
halt
