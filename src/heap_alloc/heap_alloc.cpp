//
// Created by Kehinde Adeoso on 4/29/26.
//


#include "../../include/heap_alloc.h"

void kheap_init() {
	bump_ptr = HEAP_BASE;
	heap_end = HEAP_BASE; // Since nothing's mapped yet.
}

void *kmalloc(const uint64_t size) {
#ifdef TRACE
	uart_puts("&bump_ptr = "); uart_put_hex(reinterpret_cast<uint64_t>(&bump_ptr)); uart_puts("\n");
	uart_puts("&heap_end = "); uart_put_hex(reinterpret_cast<uint64_t>(&heap_end)); uart_puts("\n");
	uart_puts("bitmap = ");    uart_put_hex(reinterpret_cast<uint64_t>(bitmap));    uart_puts("\n");
	uart_puts("__kernel_end = "); uart_put_hex(reinterpret_cast<uint64_t>(&__kernel_end)); uart_puts("\n");
#endif

	const uint64_t aligned = (size + 15) & ~15ULL;
	const uint64_t required_end = bump_ptr + aligned;

	if (required_end > HEAP_BASE + HEAP_MAX_SIZE) {
		panic("Out of virtual memories, I guess???");
	}

	while (heap_end < required_end) {
		uint64_t pa = alloc_frame();
		if (pa == 0) panic("Out of memory");

		map(heap_end, pa, PAGE_SIZE, PTE_NORMAL_RW);
		heap_end += PAGE_SIZE;
	}

	uint64_t result = bump_ptr;
	bump_ptr += aligned;

	return reinterpret_cast<void*>(result);
}

// NOTE: Since the current implementation uses a bump allocator, there is no kfree.
// Memory is freed at the end of execution

[[noreturn]] void panic(const char* msg) {
	uart_puts(msg);
	for (;;) asm volatile("wfe");
}