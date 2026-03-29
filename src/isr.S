// ISR stub table for x86_64
// Each stub is aligned to 32 bytes so addresses can be computed from the base.

.extern isr_handler_main

.altmacro

// Macro for ISR that does NOT push an error code
.macro ISR_NOERR vector
.align 32
isr_stub_\vector:
    pushq $0
    pushq $\vector
    jmp isr_common
.endm

// Macro for ISR that DOES push an error code
.macro ISR_ERR vector
.align 32
isr_stub_\vector:
    pushq $\vector
    jmp isr_common
.endm

// Helper for .rept expansion
.macro GEN_ISR_NOERR n
    ISR_NOERR %n
.endm

// All stubs start here
.align 32
.global isr_stubs_start
isr_stubs_start:

// CPU exceptions 0-31
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_ERR   29
ISR_ERR   30
ISR_NOERR 31

// IRQs 32-47 and remaining vectors 48-255
.set i, 32
.rept 224
    GEN_ISR_NOERR i
    .set i, i + 1
.endr

// Common handler
isr_common:
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    movq %rsp, %rdi
    call isr_handler_main

    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax

    addq $16, %rsp
    iretq

.section .note.GNU-stack, "", @progbits
