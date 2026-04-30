//

// Created by Kehinde Adeoso on 4/15/26.

//



#include <stdint-gcc.h>
#include <stdint.h>

#include "pmm.h"
#include "uart.h"
#include "mmu.h"

#include "../../include/pmm.h"

/*
 * A cool mnemonic for remembering what does what:
 *	"Think of it as a sequence of "who? what? where? go!:

	1. MAIR — who are the memory types? (Define the vocabulary.)
	2. TCR — what rules does the walker follow? (Define the grammar.)
	3. TTBR0 — where is my page table? (Hand over the map.)
	4. SCTLR — go! (Flip the switch.)"
 */

/*
 * A barrier/TLB instruction glossary:
 * dsb ish = Data Synchronization Barrier, Inner Sharable
 * - dsb = Stop until all preceding memory accesses have completed
 * - ish = Inner Shareable Domain (applies to all cores at current point)
 * If not used when needed, we risk invalidating a stale entry while a
 * new discriptor is being written.
 *
 * dsb ishst = DSB, Inner Shareable, Stores only
 * Similar to the last, but only waits for stores, not loads. Used when only writes
 * need to be visible.
 *
 * tlbi vmalle1 = TLB Invalidate, VM-ALL, EL1
 * Invalidates all TLB entries for current translation regime.
 * Currently used for clearing out page table during initialization
 *
 * tlbi vaae1is, va = TLB Invalidate, VA, All ASIDS, EL1, Inner Shareable
 * Invalidates a VA's entry across all ASIDs, broadcast to inner-shareable domain
 * Used when a single page has been changed.
 *
 * dsb ish = Also used to allow a TLB invalidate to propagate through machine.
 *
 * isb = Instruction Synchronization Barrier
 * Flushes instruction pipeline by clearing queue and refetching any instructions
 * after the point of call. Necessary after SCTLR, TTBR and TCR calls since
 * old instructions may use old values.
 *
 * Page table edit sequence:
 * dsb ishst // Make sure descriptor writes are visible
 * tlbi vaae1is, X // Invalidate stale translation for VA X
 * dsb ish // wait for the invalidate to finish everywhere
 * isb // flush pipeline so subsequent instructions use the new mapping
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
	pmm_init();
	uint64_t* page_table_address = reinterpret_cast<uint64_t*>(alloc_frame());
	if (page_table_address) {
		for (int i = 0; i < 512; i++) {
			page_table_address[i] = 0;
		}
	} else {
		return; // Implement errors and exceptions later.
	}

	l1_table = page_table_address;

	asm volatile("dsb ish" ::: "memory");
	asm volatile("tlbi vmalle1" ::: "memory");
	asm volatile("dsb ish" ::: "memory");
	asm volatile("isb" ::: "memory");

	asm volatile("msr TTBR0_EL1, %0" :: "r"(page_table_address));

	// Set up the memory map
	uint64_t kernel_start = 0x80000;
	uint64_t kernel_end_aligned = round_up(static_cast<uint64_t>(phys_mem_start), PAGE_SIZE);
	map(0x0, 0x0, 0x80000, PTE_NORMAL_RW);

	map(kernel_start, kernel_start, kernel_end_aligned - kernel_start, PTE_NORMAL_RW);
	map(phys_mem_start, phys_mem_start, PHYS_MEM_END - phys_mem_start, PTE_NORMAL_RW);
	map(0x3F000000, 0x3F000000, 0x01000000, PTE_DEVICE_RW);


	map(0x00000000, 0x00000000, 0x00080000, PTE_NORMAL_RW);
	map(0x00080000, 0x00080000, phys_mem_start - 0x80000, PTE_NORMAL_RW);
	map(phys_mem_start, phys_mem_start, 0x3F000000 - phys_mem_start, PTE_NORMAL_RW);
	map(0x3F000000, 0x3F000000, 0x01000000, PTE_DEVICE_RW);
	map(0x40000000, 0x40000000, 0x00001000, PTE_DEVICE_RW);


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

void map(const uint64_t va, const uint64_t phys, const uint64_t size, uint64_t flags) {
	uint64_t num_pages = size / PAGE_SIZE;

	asm volatile("dsb ishst" ::: "memory");

	for (int i = 0; i < num_pages; i++) {
		uint64_t current_va = va + i * PAGE_SIZE;
		uint64_t current_phys = phys + i * PAGE_SIZE;

		uint64_t L1_INDEX = (current_va >> 30) & 0x1FF;
		uint64_t L2_INDEX = (current_va >> 21) & 0x1FF;

		uint64_t* l2_table = _get_or_alloc_table(l1_table, L1_INDEX);
		if (!l2_table) {
			return; // TODO: Implement error handling.
		}

		uint64_t* l3_table = _get_or_alloc_table(l2_table, L2_INDEX);
		if (!l3_table) {
			return; // TODO: Implement error handling.
		}

		uint64_t L3_INDEX = (current_va >> 12) & 0x1FF;
		uint64_t PAGE_VALID_BITS = 0b11;

		l3_table[L3_INDEX] = current_phys | flags | PAGE_VALID_BITS;
	}

	// Invalidate all pages
	for (int i = 0; i < num_pages; i++) {
		uint64_t va_page = (va + i * PAGE_SIZE) >> 12;
		asm volatile("tlbi vaae1is, %0" :: "r"(va_page) : "memory");
	}
	asm volatile("dsb ish" ::: "memory");
	asm volatile("isb" ::: "memory");
}

uint64_t* _get_or_alloc_table(uint64_t* table, const uint64_t index) {
	uint64_t* entry = &table[index];
	if (*entry & 1) { // Ask about pointer * operation
		uint64_t* child_phys = reinterpret_cast<uint64_t*>(*entry & 0x0000FFFFFFFFF000);
		return child_phys;
	}
	uint64_t* child = reinterpret_cast<uint64_t*>(alloc_frame());
	if (!child) return nullptr; // out of memory

	for (int i = 0; i < 512; i++) {
		child[i] = 0;
	}

	table[index] = reinterpret_cast<uint64_t>(child) | 0b11;
	return child;
}

void unmap(uint64_t va, uint64_t size) {

}

void translate(uint64_t virt) {

}