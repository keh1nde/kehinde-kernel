//
// Created by Kehinde Adeoso on 4/15/26.
//

#include "../../include/mmu.h"

#include <stdint-gcc.h>
#include <stdint.h>

#include "pmm.h"
#include "uart.h"
#include "../../include/pmm.h"



/*
 * A cool mnemonic for remembering what does what:
 *	"Think of it as a sequence of "who? what? where? go!:

	1. MAIR — who are the memory types? (Define the vocabulary.)
	2. TCR — what rules does the walker follow? (Define the grammar.)
	3. TTBR0 — where is my page table? (Hand over the map.)
	4. SCTLR — go! (Flip the switch.)"
 *
 */

static uint64_t* l1_table;

void mmu_init() {
	// Set up the Memory Attribute Indirection Register: A dictionary of memory behaviors
	uint64_t mair = 0;
	mair |= (0xFFULL << (0 * 8)); // Signals to machine that this is normal RAM; do not use limits
	mair |= (0x00ULL << (1 * 8)); // Signals to machine that this is NOT device memory; do not use limits
	mair |= (0x04ULL << (2 * 8)); // Temporarily set.
	asm volatile("msr MAIR_EL1, %0" :: "r"(mair));

	// Set up TCR_EL1: Translation Control Register. Outlines how the CPU traverses page tables.
	uint64_t tcr = 0;
	tcr |=
		(25ULL << 0) // T0SZ (Size offset of memory region): VAs are 39 bits wide (64 - 25). Maximum virtual space is 512GiB.
	| (1ULL << 8) // IRGN0 (Inner Cacheability Attribute for Translation Walks): Treat reads as cacheable.
	| (1ULL << 10) // ORGN0: Treat reads as cacheable. Caching page table walks means that translations are faster.
	| (3ULL << 12) // SH0 (Sharability Attribute): Inner-Shareable. Not important now but useful for multicore impl.
	| (0ULL << 14) // TG0 (Page Size): Set to 4KiB. Most common page size.
	| (1ULL << 23) // EPD1 (Translation walk disable): Disable walks for upper address space (held by TBR1, which is not in use).
	| (1ULL << 32); // IPS (Intermediate Physical Address Size): Set to 1 temporarily.
	asm volatile("msr TCR_EL1, %0" :: "r"(tcr));

	// Set up the page table
	uint64_t* page_table_address = reinterpret_cast<uint64_t*>(alloc_frame());
	if (page_table_address) {
		for (int i = 0; i < 512; i++) {
			page_table_address[i] = 0;
		}
	} else {
		return; // Implement errors and exceptions later.
	}

	asm volatile("dsb ish" ::: "memory");
	asm volatile("tlbi vmalle1" ::: "memory");
	asm volatile("dsb ish" ::: "memory");
	asm volatile("isb" ::: "memory");

	asm volatile("msr TTBR0_EL1, %0" :: "r"(page_table_address));

	// Turn on the MMU via SCTLR_EL1: System Control Register.
	// The master control for all system functions in EL1, including MMUs
	uint64_t sctlr;
	asm volatile ("mrs %0, SCTLR_EL1" : "=r"(sctlr));
	sctlr |=
		(1ULL << 0) // Turn MMU on. All addresses going forward are virtual and are translated.
	| (1ULL << 2) // Enable data cache.
	| (1ULL << 12); // Enable instruction cache.
	asm volatile("msr SCTLR_EL1, %0" :: "r"(sctlr));

	asm volatile("isb" ::: "memory");
}

void map(uint64_t va, uint64_t phys, uint64_t size, uint64_t flags) {
	uint64_t num_pages = size / PAGE_SIZE;

	for (int i = 0; i < num_pages - 1; i++) {
		uint64_t* current_va = reinterpret_cast<uint64_t *>(va + i * PAGE_SIZE);
		uint64_t* current_phys = reinterpret_cast<uint64_t *>(phys + (i + 1) * PAGE_SIZE);

		uint64_t* l2_table = _get_or_alloc_table(l1_table, num_pages);
		uint64_t* l3_table = _get_or_alloc_table(l2_table, num_pages);

		uint64_t L1_INDEX = (va >> 30) & 0x1FF;
		uint64_t L2_INDEX = (va >> 21) & 0x1FF;
		uint64_t L3_INDEX = (va >> 12) & 0x1FF;
		uint64_t PAGE_VALID_BITS = 0b11;

		l3_table[L3_INDEX] = current_phys | flags | PAGE_VALID_BITS;


	}
}

uint64_t* _get_or_alloc_table(uint64_t* table, const uint64_t index) {
	uint64_t* entry = &table[index];
	if (entry) {
		uint64_t* child_phys = reinterpret_cast<uint64_t*>(reinterpret_cast<uint64_t>(entry) & 0x0000FFFFFFFFF000);
		return child_phys;
	}
	uint64_t* child = reinterpret_cast<uint64_t*>(alloc_frame());
	if (!child) return nullptr; // out of memory
