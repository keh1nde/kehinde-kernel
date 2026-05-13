#include <stddef.h>
#include <stdint.h>

#include "heap_alloc.h"
#include "uart.h"
#include "interrupts.h"
#include "mmu.h"
#include "pmm.h"
#include "timer.h"
#include "filesystem.h"
#include "shell.h"

extern "C" void kernel_main(uint32_t r0, uint32_t r1, uint32_t atags)
{
	(void) r0;
	(void) r1;
	(void) atags;


	//pmm_init();
	// uart_puts("Page Frame Allocator initialized.\n");

	mmu_init();
	uart_puts("Page Frame Allocator and MMU initialized.\n");

	// Testing the heap allocator
	kheap_init();
	uart_puts("Heap allocator initialized.\n");

	// ---- Filesystem tests ----
	fs_init();
	uart_puts("fs_init done.\n");

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