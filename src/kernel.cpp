#include <stddef.h>
#include <stdint.h>

#include "heap_alloc.h"
#include "uart.h"
#include "interrupts.h"
#include "mmu.h"
#include "pmm.h"
#include "timer.h"

extern "C" void kernel_main(uint32_t r0, uint32_t r1, uint32_t atags)
{
	(void) r0;
	(void) r1;
	(void) atags;

	uart_init();

	uart_puts("Today is the ");
	uart_put_uint(29);
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



	//pmm_init();
	// uart_puts("Page Frame Allocator initialized.\n");

	mmu_init();
	uart_puts("Page Frame Allocator and MMU initialized.\n");

	// Testing the heap allocator
	kheap_init();
	uart_puts("Heap allocator initialized.\n");

	// Testing kmalloc
	void* p = kmalloc(64);
	uart_puts("kmalloc was called and returned the following address: ");
	uart_put_hex(reinterpret_cast<uint64_t>(p));
	uart_puts("\n");

	// Testing if memory is usable
	uint64_t* p1 = static_cast<uint64_t*>(kmalloc(64));
	p1[0] = 0xDEADBEEFCAFEBABE;
	p1[1] = 0x1234567890ABCDEF;

	if (p1[0] == 0xDEADBEEFCAFEBABE && p1[1] == 0x1234567890ABCDEF) {
		uart_puts("kmalloc R/W ok\n");
	} else {
		uart_puts("kmalloc R/W fail\n");
	}

	interrupt_init();
	timer_init();

	while (1) {
		uart_putc(uart_getc());

	}
}