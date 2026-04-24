//
// Created by Kehinde Adeoso on 4/23/26.
//

#include "../../include/pmm.h"

uint64_t* bitmap;
uint64_t phys_mem_start = 0;
uint64_t total_frames = 0;



void pmm_init() {
	// Compute start of bitmap.
	const uint64_t bitmap_addr = round_up(reinterpret_cast<uint64_t>(__kernel_end), PAGE_SIZE);

	// Compute the maximum frames for region.
	const uint64_t managed_bytes = PHYS_MEM_END - bitmap_addr;
	const uint64_t max_frames = managed_bytes / PAGE_SIZE;

	// Compute bitmap size in bytes:
	const uint64_t bitmap_bytes = (max_frames + 63) / 64 * 8;

	// Point bitmap to new address.
	bitmap = reinterpret_cast<uint64_t *>(bitmap_addr);


	// Post bitmap placement, shrink the allocatable region to after the bitmap location to avoid corruption.
	phys_mem_start = round_up(bitmap_addr + bitmap_bytes, PAGE_SIZE);

	// Compute total frames
	total_frames = (PHYS_MEM_END - phys_mem_start) / PAGE_SIZE;

	// Free all pages by setting bitmap to zero.
	for (int i = 0; i < bitmap_bytes / 8; ++i) {
		bitmap[i] = 0;
	}

	// Lock unallocatable pages outside of bitmap to avoid corruption.
	const uint64_t last_word = total_frames / 64;

	if (const uint64_t valid_bits = total_frames % 64; valid_bits != 0) {
		const uint64_t mask = ~((1ULL << valid_bits) - 1);
		bitmap[last_word] |= mask;
	}

}

uint64_t alloc_frame() {
	for (int i = 0; i < (total_frames + 63) / 64; ++i) {
		if (bitmap[i] != ~0ULL) {
			// Find the first 0 in the word (i.e. the first unallocated frame)
			const uint64_t bit_index = __builtin_ctzll(~bitmap[i]);

			// Compute frame index
			const uint64_t idx = i * 64 + bit_index;

			// Set frame to allocated
			bitmap[i] |= (1ULL << bit_index);

			// Compute address
			const uint64_t address = phys_mem_start + idx * PAGE_SIZE;

			// Return the address
			return address;
		}
	}
	return 0; // Out of memory.
}

void free_frame(const uint64_t addr) {
	const uint64_t frame_index = (addr - phys_mem_start) / PAGE_SIZE;

	const uint64_t word_index = frame_index / 64;

	if (const uint64_t bit_index = frame_index % 64; (bitmap[word_index] & (1ULL << bit_index)) != 0) {
		bitmap[word_index] &= ~(1ULL << bit_index);
	}
}