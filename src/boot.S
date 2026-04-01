.section ".text.boot"

.global _start
.extern vector_table

_start:
    // Read the CPU ID from MPIDR_EL1; only core 0 should continue
    mrs     x1, mpidr_el1
    and     x1, x1, #3
    cmp     x1, #0
    bne     halt

    // Set up the stack pointer (grows downward from 0x80000)
    ldr     x1, =_start
    mov     sp, x1

    // Zero out the BSS section
    ldr     x1, =__bss_start
    ldr     x2, =__bss_end
    b       2f

1:
    stp     xzr, xzr, [x1], #16

2:
    cmp     x1, x2
    blo     1b

    // Before starting the kernel, set location of interrupt table
    ldr x0, =vector_table
    msr VBAR_EL1, x0

    // Then, unmask interrupts
    msr DAIFClr, #2

    // Jump to the C++ kernel entry point
    bl      kernel_main

halt:
    wfe
    b       halt