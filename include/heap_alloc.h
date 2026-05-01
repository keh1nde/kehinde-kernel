//
// Created by Kehinde Adeoso on 4/29/26.
//

#ifndef RASPBERRY_PI_OPERATING_SYSTEM_HEAP_ALLOC_H
#define RASPBERRY_PI_OPERATING_SYSTEM_HEAP_ALLOC_H

#include <stdint-gcc.h>
#include <stdint.h>


#include "pmm.h"
#include "mmu.h"
#include "uart.h"


constexpr uint64_t HEAP_BASE = 0x100000000; // Was originally 0x100000000ULL
constexpr uint64_t HEAP_MAX_SIZE = 0x100000;
constexpr uint64_t HEAP_ALIGN = 16;
inline uint64_t bump_ptr;
inline uint64_t heap_end;

void kheap_init();

void* kmalloc(uint64_t size);

[[noreturn]] void panic(const char* msg);

#endif //RASPBERRY_PI_OPERATING_SYSTEM_HEAP_ALLOC_H
