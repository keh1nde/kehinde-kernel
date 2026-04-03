#include <stddef.h>
#include <stdint.h>
#include "uart.h"
#include "interrupts.h"
#include "timer.h"

extern "C" void kernel_main(uint32_t r0, uint32_t r1, uint32_t atags)
{
	(void) r0;
	(void) r1;
	(void) atags;

	uart_init();
	interrupt_init();
	timer_init();

	uart_puts("Hello, kernel World!\r\n");

	uint64_t el;
	asm volatile("mrs %0, CurrentEL" : "=r"(el));
	switch (el) {
		case (4):
			uart_puts("You are in Exception Level One \n");
			break;
		case (8):
			uart_puts("You are in Exception Level Two \n");
			break;
		case (12):
			uart_puts("You are in Exception Level Three \n");
		default: uart_puts("No EL found \n");
	}

	while (1) {
		uart_putc(uart_getc());
		uart_putc('\n');
	}
}