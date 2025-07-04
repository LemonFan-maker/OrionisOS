#pragma once
#include <stdint.h>
#include <stivale.h> 

extern const unsigned char console_tty_9x16[];

void tty_init(stivale_struct* boot_info);
void print_hex(uint64_t value, uint32_t color);
void print_dec(uint64_t value, uint32_t color);
void tty_print(const char* str, uint32_t color);
void tty_clear();
void tty_putc(char c, uint32_t color);