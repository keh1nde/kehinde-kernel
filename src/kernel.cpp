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

	uart_puts("Hello, kernel World!\r\n");

	uart_puts("Today is the ");
	uart_put_uint(3);
	uart_puts(" of April. \n");

	uint64_t el;
	asm volatile("mrs %0, CurrentEL" : "=r"(el));
	switch (el) {
		case (4):
			uart_puts("This is Exception Level One \n");
			break;
		case (8):
			uart_puts("This is Exception Level Two \n");
			break;
		case (12):
			uart_puts("This is Exception Level Three \n");
		default: uart_puts("No EL found \n");
	}

	interrupt_init();
	timer_init();

	while (1) {
		uart_putc(uart_getc());
		uart_putc('\n');
	}
}