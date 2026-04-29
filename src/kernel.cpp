#include <stddef.h>
#include <stdint.h>
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



	pmm_init();

	// Testing the MMU:

	// Initialize:
	mmu_init();

	uart_puts("MMU initialized. It's assumed that map() works properly, too.\n");


	interrupt_init();
	timer_init();

	while (1) {
		uart_putc(uart_getc());
		uart_putc('\n');
	}
}


/*
 * page manager test code.
 *
 * // Testing mem_start at init.
	uart_puts("Current phys_mem_start value: ");
	uart_put_hex(phys_mem_start);
	uart_puts("\n");

	// Testing TOTAL_FRAMES value at init.
	uart_puts("Current TOTAL_FRAMES value: ");
	uart_put_uint(total_frames);
	uart_puts("\n");

	// Testing bitmap address at init
	uart_puts("Current bitmap address: ");
	uart_put_hex(reinterpret_cast<uint64_t>(bitmap));
	uart_puts("\n");

	// Testing frame allocation
	uint64_t new_frame = alloc_frame();
	uart_puts("Newly allocated frame: ");
	uart_put_hex(new_frame);
	uart_puts("\n");

	free_frame(new_frame);

	uint64_t addresses[4];
	int count = 0;
	// Testing multiple frame allocations:
	for (uint64_t i = 0; i < 4; i++) {
		addresses[i] = alloc_frame();

		uart_puts("Address No.");
		uart_put_uint(i);
		uart_puts(" ");
		uart_put_hex(addresses[i]);
		uart_puts("\n");

		if (i != 0) {
			if (addresses[i] - addresses[i-1] == 4096) {
				uart_puts("There is no discrepancy between the current and previous allocations");
				uart_puts("\n");
			} else {
				uart_puts("Each address is not ");  uart_put_uint(4096);
				uart_puts(" bytes apart. There is an issue with allocations. Current values:");
				uart_puts("Previous address: "); uart_put_hex(addresses[i-1]); uart_puts("\n");
				uart_puts("Current address: "); uart_put_hex(addresses[i]); uart_puts("\n");
				uart_puts("\n");
			}
		} else {
			uart_puts("This is the first allocation.");
			uart_puts("\n");
		}
		count++;
	}

	// Testing multiple deallocations.
	for (int i = count - 1; i >= 0; i--) {
		free_frame(addresses[i]);
	}

	uint64_t addr = alloc_frame();
	// Testing reallocation to make sure we get the proper address back.
	if (addr == phys_mem_start) {
		uart_puts("The memory allocator works properly.\n");
	} else {
		uart_puts("The memory allocator does not work properly. phys_mem_start was not received.\n");
		uart_puts("Received value: "); uart_put_uint(addr - 4096); uart_puts("\n");
	}

	uint64_t* p = reinterpret_cast<uint64_t*>(addr);
	*p = 0xDEADBEEFCAFEBABE;

	uart_puts("Readback: ");
	uart_put_hex(*p);
	uart_puts("\n");
 */