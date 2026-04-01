//
// Created by Kehinde Adeoso on 3/20/26.
//

#include "interrupts.h"

#include <stdint-gcc.h>

#include "uart.h"
#include <stdint.h>

/**
 * handle_synchronous_interrupts: Calls ESR_EL1 to identify exception type
 */
extern "C" void handle_synchronous_interrupts() {


	uint64_t esr;
	asm volatile("mrs %0, ESR_EL1" : "=r"(esr));

	uart_puts("handle_synchronous_interrupts has been reached");
	uart_puts(" code reached: ", (esr >> 26) & 0x3F);
}

/**
 * handle_interrupt_requests: Do something idk yet
 */
extern "C" void handle_interrupt_requests() {
	uart_puts("handle_interrupt_requests has been reached");
}

enum {
	TIMER_BASE = 0x3E003000, // make sure to amend

	TIMER_CS = (TIMER_BASE + 0x0),
	TIMER_CLO = (TIMER_BASE + 0x4),
	TIMER_CHI = (TIMER_BASE + 0x8),

	TIMER_C0 = (TIMER_BASE + 0xC),
	TIMER_C1 = (TIMER_BASE + 0x10),
	TIMER_C2 = (TIMER_BASE + 0x14),
	TIMER_C3 = (TIMER_BASE + 0x18),

	IRQ_BASE = 0x3E00B000,
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


};

void interrupt_init() {
	// Disable all
	mmio_write(IRQ_1DS, 0xFFFFFFFF);
	mmio_write(IRQ_2DS, 0xFFFFFFFF);
	mmio_write(IRQ_BDS, 0xFFFFFFFF);

	// Enable ARM Timer and UART0
	mmio_write(IRQ_BEN, 1 << 0);
	mmio_write(IRQ_EN2, 1 << 25);
}

void timer_init() {
	uint64_t freq;
	asm volatile("mrs %0, CNTFRQ_EL0" : "=r"(freq));
	asm volatile("msr CNTP_TVAL_EL0, %0" :: "r"(freq / 10));
}