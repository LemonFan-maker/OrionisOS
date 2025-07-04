#include "pic.h"
#include "ports.h"
#include "kernel/drivers/tty.h"
#include <stdint.h>

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

// 告诉 PIC 我们中断处理完了
void pic_send_eoi(unsigned char irq) {
    if(irq >= 8)
        outb(PIC2_COMMAND, 0x20); // 发送 EOI 到从片
    outb(PIC1_COMMAND, 0x20); // 发送 EOI 到主片
}

void pic_remap(int offset1, int offset2) {
    unsigned char a1, a2;

    a1 = inb(PIC1_DATA); // 保存掩码
    a2 = inb(PIC2_DATA);

    outb(PIC1_COMMAND, 0x11); // 初始化主片和从片
    outb(PIC2_COMMAND, 0x11);

    outb(PIC1_DATA, offset1); // 主片 PIC 从 offset1 (32) 开始
    outb(PIC2_DATA, offset2); // 从片 PIC 从 offset2 (40) 开始

    outb(PIC1_DATA, 4);  // 告诉主片，从片在 IRQ2 (0000 0100)
    outb(PIC2_DATA, 2);  // 告诉从片，它的级联身份是 2

    outb(PIC1_DATA, 0x01); // 8086/88 (MCS-80) 模式
    outb(PIC2_DATA, 0x01);

    outb(PIC1_DATA, a1); // 恢复掩码
    outb(PIC2_DATA, a2);
}

void pic_unmask_irq(uint8_t irq) {
    uint16_t port;
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    uint8_t value = inb(port) & ~(1 << irq);
    outb(port, value);
}

void pic_dump_masks() {
    uint8_t master_mask = inb(PIC1_DATA);
    uint8_t slave_mask  = inb(PIC2_DATA);

    tty_print("\n--- PIC Interrupt Mask Register (IMR) Dump ---\n", 0xFFFF00);
    tty_print("Master (IRQ 0-7):  0b", 0xFFFFFF);
    for (int i = 7; i >= 0; i--) {
        tty_print((master_mask & (1 << i)) ? "1" : "0", 0xFFFFFF);
    }
    tty_print(" (0x", 0xFFFFFF); print_hex(master_mask, 0xFFFFFF); tty_print(")\n", 0xFFFFFF);

    tty_print("Slave  (IRQ 8-15): 0b", 0xFFFFFF);
    for (int i = 7; i >= 0; i--) {
        tty_print((slave_mask & (1 << i)) ? "1" : "0", 0xFFFFFF);
    }
    tty_print(" (0x", 0xFFFFFF); print_hex(slave_mask, 0xFFFFFF); tty_print(")\n", 0xFFFFFF);
    tty_print("A '0' in a bit position means the IRQ is ENABLED (unmasked).\n", 0x00FF00);
}
