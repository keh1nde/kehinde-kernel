//
// Created by Kehinde Adeoso on 3/20/26.
//

#include "interrupts.h"

#include <stdint.h>

#include "uart.h"
#include "timer.h"
#include <stdint.h>

#include "../../include/timer.h"

/**
 * handle_synchronous_interrupts: Calls ESR_EL1 to identify exception type
 */

enum {
	TIMER_BASE = 0x3F003000, // make sure to amend

	TIMER_CS = (TIMER_BASE + 0x0),
	TIMER_CLO = (TIMER_BASE + 0x4),
	TIMER_CHI = (TIMER_BASE + 0x8),

	TIMER_C0 = (TIMER_BASE + 0xC),
	TIMER_C1 = (TIMER_BASE + 0x10),
	TIMER_C2 = (TIMER_BASE + 0x14),
	TIMER_C3 = (TIMER_BASE + 0x18),

	IRQ_BASE = 0x3F00B000,
	IRQ_BASIC = (IRQ_BASE + 0x200),
	IRQ_P1 = (IRQ_BASE + 0x204),
	IRQ_P2 = (IRQ_BASE + 0x208),
	IRQ_FC = (IRQ_BASE + 0x20C),
	IRQ_EN1 = (IRQ_BASE + 0x210),
	IRQ_EN2 = (IRQ_BASE + 0x214),
	IRQ_BEN = (IRQ_BASE + 0x218),
	IRQ_1DS = (IRQ_BASE + 0x21C),
	IRQ_2DS = (IRQ_BASE + 0x220),
	IRQ_BDS = (IRQ_BASE + 0x224),

	IRQ_ARM_BASE = 0x40000000,
	IRQ_EN_C0 = 0x40000040,
	IRQ_C0_SOURCE = 0x40000060,

};
extern "C" void handle_synchronous_interrupts() {


	uint64_t esr, far, elr;
	asm volatile("mrs %0, ESR_EL1" : "=r"(esr));
	asm volatile("mrs %0, FAR_EL1" : "=r"(far));
	asm volatile("mrs %0, ELR_EL1" : "=r"(elr)); // PC at faulting instruction


	uint64_t ec = (esr >> 26) & 0x3F;
	uint64_t iss = esr & 0x1FFFFFF;
	uint64_t dfsc = iss & 0x3F;

	switch (ec) {
		case 0x20:
			uart_puts("Instruction Abort from a lower EL.");
			uart_puts("FAR_EL1: ");
			uart_put_hex(far);
			uart_puts("\nDFSC: ");
			uart_put_hex(dfsc);
			while (true) {}
		case 0x21:
			uart_puts("Instruction Abort from the same EL.\n");
			uart_puts("There is likely no VA to PA mapping, or you did it wrong.");
			uart_puts("FAR_EL1: ");
			uart_put_hex(far);
			uart_puts("\nDFSC: ");
			uart_put_hex(dfsc);
			uart_put_hex(far);
			while (true) {}
		case 0x24:
			uart_puts("Data Abort from a lower EL \n");
			uart_puts("FAR_EL1: ");
			uart_put_hex(far);
			uart_puts("\nDFSC: ");
			uart_put_hex(dfsc);
			uart_put_hex(far);
			while (true) {}
		case 0x25:
			uart_puts("Data Abort from the same EL \n");
			uart_puts("Kernel did a load/store to an unmapped or wrong-perm VA.");
			uart_puts("FAR_EL1: ");
			uart_put_hex(far);
			uart_puts("\nDFSC: ");
			uart_put_hex(dfsc);
			while (true) {}
		case 0x0E:
			uart_puts("Illegal execution state \n");
			uart_puts("FAR_EL1: ");
			uart_put_hex(far);
			uart_puts("\nDFSC: ");
			uart_put_hex(dfsc);
			while (true) {}
		case 0x15:
			uart_puts("SVC from AArch64: System call. \n");

	}

	/*
	 * Note the following EC Values (in bits [31:26]
	 * 0x00 (d 0): unknown
	 * 0x01 (d 1): Trapped WFI/WFE
	 * 0x0E (d 14): Illegal execution state
	 * 0x15 (d 21): SVC from AArch64 (aka syscall)
	 * 0x20 (d 32): Instruction abort from lower EL
	 * 0x24 (d 36): Data abort from lower EL
	 * 0x25 (d 37): Data abort from same EL
	 * 0x2C (d 44): Stack pointer alignment fault
	 * 0x3C (d 60): BRK instruction (aka debug breakpoint)
	 */

}

// Core0 timer interrupt source is at 0x40000...60 on bit 1
extern "C" void interrupt_init() {
	// Disable all
	mmio_write(IRQ_1DS, 0xFFFFFFFF);
	mmio_write(IRQ_2DS, 0xFFFFFFFF);
	mmio_write(IRQ_BDS, 0xFFFFFFFF);

	// Enable ARM Timer and UART0
	mmio_write(IRQ_BEN, 1 << 0);
	// mmio_write(IRQ_EN2, 1 << 25);
	mmio_write(IRQ_EN_C0, (1 << 1));
}

/**
 * handle_interrupt_requests: Do something idk yet
 */
extern "C" void handle_interrupt_requests() {
	uint64_t irq_pend_status;
	irq_pend_status = mmio_read(IRQ_C0_SOURCE);

	// Check the timer register
	uint64_t freq = get_freq();
	if (irq_pend_status & (1 << 1)) {
		// Complete timer operation.
		asm volatile("msr CNTP_TVAL_EL0, %0" :: "r"(freq / 10)); // reload timer
		increment_time();
		print_time();

	}

}