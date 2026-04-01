#pragma once

void uart_init();

void uart_putc(unsigned char c);

unsigned char uart_getc();

void uart_puts(const char* str);

static inline void mmio_write(uint32_t reg, uint32_t data)
{
	*reinterpret_cast<volatile uint32_t*>(reg) = data;  // or *(volatile uint32_t*)reg = data;
}

static inline uint32_t mmio_read(uint32_t reg)
{
	return *reinterpret_cast<volatile uint32_t*>(reg); // or *(volatile uint32_t*)reg;
}

static inline void delay(int32_t count) {
	asm volatile("__delay_%=: subs %[count], %[count], #1; bne __delay_%=\n"
						: "=r"(count): [count]"0"(count) : "cc");
}
