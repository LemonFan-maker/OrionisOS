#pragma once
#include <stdint.h>

void pic_remap(int offset1, int offset2);
void pic_send_eoi(unsigned char irq);
void pic_unmask_irq(uint8_t irq);