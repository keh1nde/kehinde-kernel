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

	// Timer omitted: timer IRQ prints "Uptime: N" and would disrupt the shell prompt.
	interrupt_init();

	shell_run();
}