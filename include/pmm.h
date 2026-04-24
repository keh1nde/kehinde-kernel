//
// Created by Kehinde Adeoso on 4/23/26.
//

#ifndef RASPBERRY_PI_OPERATING_SYSTEM_PMM_H
#define RASPBERRY_PI_OPERATING_SYSTEM_PMM_H

#include <stdint.h>


constexpr uint64_t PAGE_SIZE = 4096;
constexpr uint64_t PHYS_MEM_END = 0x3F000000; // Start of Raspi MMIO

extern "C" {
	extern uint64_t phys_mem_start;
	extern uint64_t total_frames;
	extern "C" uint8_t __kernel_end[];
	extern "C" uint64_t* bitmap;
}
static inline uint64_t round_up(const uint64_t value, const uint64_t align) {
	return (value + align - 1) & ~(align - 1);
}

void pmm_init();

void free_frame(uint64_t addr);

uint64_t alloc_frame();

#endif //RASPBERRY_PI_OPERATING_SYSTEM_PMM_H