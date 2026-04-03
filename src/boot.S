.section ".text.boot"

.global _start
.extern vector_table

_start:
    // Read the CPU ID from MPIDR_EL1; only core 0 should continue
    mrs     x1, mpidr_el1
    and     x1, x1, #3
    cmp     x1, #0
    bne     halt

    b level_init

level_init:
    // Set EL1PCTEN and EL1PCEN to 1 to allow read and access of physical timer
    mov     x4, #0x3
    msr     CNTHCTL_EL2, x4

    // Set CNTVOFF_EL2 to 0 so physical = virtual
    msr     CNTVOFF_EL2, xzr

    // Set HCR_EL2 bit 31 so EL1 runs in AArch64 mode
    mov     x4, #(1 << 31)
    msr     HCR_EL2, x4

    // Set SPSR_EL2: EL1h mode (0x5) with DAIF masked (0x3C0)
    mov     x4, #0x3C5
    msr     SPSR_EL2, x4

    // Set ELR_EL2 to the EL1 entry point
    adr     x4, el1_entry
    msr     ELR_EL2, x4

    eret

el1_entry:
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

    // Set location of interrupt table
    ldr     x0, =vector_table
    msr     VBAR_EL1, x0

    // Unmask interrupts
    msr     DAIFClr, #2

    // Jump to the C++ kernel entry point
    bl      kernel_main


halt:
    wfe
    b       halt