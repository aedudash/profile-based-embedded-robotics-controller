// Author: Aidan Dudash

.global myDelay_ms

myDelay_ms:
    ; r24:r25 = delay_time in ms (16-bit value, r25 is high byte, r24 is low byte)
    ; Save registers
    push r24
    push r25
    push r19

delay_loop:
    ; Call delay_1ms to create a 1ms delay
    rcall delay_1ms
    
    ; Decrement 16-bit delay_time (r25:r24)
    subi r24, 1           ; Subtract 1 from low byte
    sbci r25, 0           ; Subtract carry from high byte
    
    ; Check if delay_time == 0 (both r24 and r25 are 0)
    brne delay_loop       ; Repeat until r24:r25 == 0

    ; Restore registers and return
    pop r19
    pop r25
    pop r24
    ret

; This function creates a 1ms delay
delay_1ms:
    ldi r25, 0x02          ; Load r18 with 2
r19_ldi:
    ldi r19, 0xA8          ; Load r19 with 168 (for the 1 ms delay)
r19_dec:
    dec r19                ; Decrement r19 by 1
    brne r19_dec           ; Repeat until r19 == 0
    dec r25                ; Decrement r18 by 1
    brne r19_ldi           ; Repeat if r18 != 0
    ret                    ; Return to the caller

	/*/ Author: Aidan Dudash

.global myDelay_ms        ; Make the symbol 'myDelay_ms' accessible from other files

// ================================================
// Delay Function: myDelay_ms
// Creates a delay in milliseconds.
// Input: r24 = low byte of delay time
//        r25 = high byte of delay time
// Registers used: r19 (for inner delay loop), r24, r25
// ================================================

myDelay_ms:
    ; Save registers that will be modified to preserve calling context
    push r24              ; Save the low byte of delay_time
    push r25              ; Save the high byte of delay_time
    push r19              ; Save r19 which is used in the inner delay loop

delay_loop:
    ; Call the 1 ms delay subroutine
    rcall delay_1ms       ; Perform a 1 ms delay

    ; Decrement the 16-bit delay counter stored in r25:r24
    subi r24, 1           ; Subtract 1 from the low byte (r24)
    sbci r25, 0           ; Subtract 0 with carry from high byte (r25)

    ; Loop until the full delay period has elapsed
    brne delay_loop       ; Branch if r25:r24 != 0 (delay not complete)

    ; Restore the saved registers
    pop r19               ; Restore r19
    pop r25               ; Restore r25
    pop r24               ; Restore r24

    ret                   ; Return from the function

// ================================================
// Subroutine: delay_1ms
// Creates an approximate 1 millisecond delay
// Assumes 16 MHz system clock
// ================================================

delay_1ms:
    ldi r25, 0x02         ; Load r25 with 2 — outer loop counter

r19_ldi:
    ldi r19, 0xA8         ; Load r19 with 168 — inner loop count
                          ; (Each iteration burns ~6 cycles ? ~1.008 ms total with nested loops)

r19_dec:
    dec r19               ; Decrement inner loop counter
    brne r19_dec          ; If not zero, repeat inner loop

    dec r25               ; Decrement outer loop counter
    brne r19_ldi          ; Repeat outer loop if not zero

    ret                   ; Return to caller
*/