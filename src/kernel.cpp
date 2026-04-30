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
	uart_puts("MMU initialized.\n");

	// Test un_map():
	uint64_t frame_pa = alloc_frame();
	uart_puts("Frame successfully allocated\n");

	map(0x100000000, frame_pa, PAGE_SIZE, PTE_NORMAL_RW);
	uart_puts("Mapping successful, the new physical address is the following: ");
	uart_put_hex(translate(0x100000000));

	uart_puts("\n");
	uint64_t* p = reinterpret_cast<uint64_t*>(0x100000000);
	*p = 0xDEADBEEFCAFEBABE;
	uart_puts("Sentinel successfully written to allocated frame.\n");

	uart_puts("Readback: ");
	uart_put_hex(*p);
	uart_puts("\n");

	unmap(0x100000000, PAGE_SIZE);
	uart_puts("Address successfully unmapped. The translation should now read 0ULL: ");
	uart_put_hex(translate(0x100000000));
	uart_puts("\n");

	uart_puts("Readback (should fail): ");
	uart_put_hex(*p);
	uart_puts("\n");




	interrupt_init();
	timer_init();

	while (1) {
		uart_putc(uart_getc());

	}
}