.section ".text"
.balign 2048
.global vector_table


.macro save_registers
    sub SP, SP, #272
    stp x0, x1, [SP, #0]
    stp x2, x3, [SP, #16]
    stp x4, x5, [SP, #32]
    stp x6, x7, [SP, #48]
    stp x8, x9, [SP, #64]
    stp x10, x11, [SP, #80]
    stp x12, x13, [SP, #96]
    stp x14, x15, [SP, #112]
    stp x16, x17, [SP, #128]
    stp x18, x19, [SP, #144]
    stp x20, x21, [SP, #160]
    stp x22, x23, [SP, #176]
    stp x24, x25, [SP, #192]
    stp x26, x27, [SP, #208]
    stp x28, x29, [SP, #224]
    stp x30, xzr, [SP, #240]
    mrs x0, ELR_EL1
    mrs x1, SPSR_EL1
    stp x0, x1, [SP, #256]
.endm

.macro load_registers
    ldp x0, x1, [SP, #256]
    msr ELR_EL1, x0
    msr SPSR_EL1, x1
    ldp x30, xzr, [SP, #240]
    ldp x28, x29, [SP, #224]
    ldp x26, x27, [SP, #208]
    ldp x24, x25, [SP, #192]
    ldp x22, x23, [SP, #176]
    ldp x20, x21, [SP, #160]
    ldp x18, x19, [SP, #144]
    ldp x16, x17, [SP, #128]
    ldp x14, x15, [SP, #112]
    ldp x12, x13, [SP, #96]
    ldp x10, x11, [SP, #80]
    ldp x8, x9, [SP, #64]
    ldp x6, x7, [SP, #48]
    ldp x4, x5, [SP, #32]
    ldp x2, x3, [SP, #16]
    ldp x0, x1, [SP, #0]
    add SP, SP, #272
.endm


// --- Vector table: 16 entries, on 128 byte boundaries. ---

vector_table:

// Current EL with SP0
.balign 128
    b .                             // Synchronous
.balign 128
    b .                             // IRQ
.balign 128
    b .                             // FIQ
.balign 128
    b .                             // SError

// Current EL with SPx
.balign 128
    b sync_handler                  // Synchronous
.balign 128
    b irq_handler                   // IRQ
.balign 128
    b .                             // FIQ
.balign 128
    b .                             // SError

// Lower EL using AArch64
.balign 128
    b .                             // Synchronous
.balign 128
    b .                             // IRQ
.balign 128
    b .                             // FIQ
.balign 128
    b .                             // SError

// Lower EL using AArch32
.balign 128
    b .                             // Synchronous
.balign 128
    b .                             // IRQ
.balign 128
    b .                             // FIQ
.balign 128
    b .                             // SError


// --- Handler bodies. Outside function and implemented to abide by 128-byte limit ---

sync_handler:
    save_registers
    bl handle_synchronous_interrupts
    load_registers
    eret

irq_handler:
    save_registers
    bl handle_interrupt_requests
    load_registers
    eret