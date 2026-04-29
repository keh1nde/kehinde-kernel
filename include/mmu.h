//
// Created by Kehinde Adeoso on 4/15/26.
//

#pragma once
#include <stdint-gcc.h>
#include <stdint.h>


/**
 * Configures MAIR_EL1, TCR_EL1, TTBR0_EL1, and TTBR1_EL1
 * Installs a top-level table
 * Flush TBLs
 * Flip SCTLR_EL1.M to 1
 */
void mmu_init();


/**
 * Walk and allocate via alloc_frame the L0 to L3 tables, and install a descriptor at the terminal level
 * Flags encode cacheablilty (Normal vs Device via MAIR index), permissions and access control.
 */
static void map(uint64_t va, uint64_t phys, uint64_t size, uint64_t flags);

// Clear descriptor, invalidate TLB entry
static void unmap(uint64_t va, uint64_t size);

static uint64_t* _get_or_alloc_table(uint64_t* table, uint64_t index);

// Walks tables in software and returns physical addresses (or find faults)
uint64_t translate(uint64_t virt);