#pragma once
#include <stdint.h>

void timer_init(uint32_t frequency);
void timer_sleep_ms(uint32_t ms);
void timer_sleep_us(uint32_t us);