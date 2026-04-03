//
// Created by Kehinde Adeoso on 4/1/26.
//

#include "timer.h"


#include <stdint.h>

#include "../../include/uart.h"

static volatile uint64_t time;
static volatile uint64_t freq;

void timer_init() {
	asm volatile("mrs %0, CNTFRQ_EL0" : "=r"(freq));
	asm volatile("msr CNTP_TVAL_EL0, %0" :: "r"(freq / 10));
	asm volatile("msr CNTP_CTL_EL0, %0" :: "r"(1));
}

void increment_time() {
	time += 1;
}

uint64_t get_time() {
	return time;
}


uint64_t get_freq() {
	return freq;
}

void print_time() {
	uart_puts("\rUptime: ");
	uart_put_uint(get_time());
}