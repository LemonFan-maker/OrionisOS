#include "isr.h"
#include "kernel/drivers/tty.h"
#include "kernel/drivers/keyboard.h"
#include "kernel/boot.h" // 需要全局 boot_info
#include <stivale.h>
#include "pic.h"
#include "ports.h"
#include "kernel/panic.h"
#include "kernel/drivers/ethernet/e1000.h"
#include "kernel/drivers/ethernet/virtio_net.h"

// 外部声明：从 tty.h 中获取
extern void print_hex(uint64_t value, uint32_t color);

// 全局变量
static uint64_t tick = 0;

// 核心修改：将所有 ISR 蹦床函数的声明放在全局作用域，由 idt.cpp 提供 
// isr_handler 不直接调用 isrXX，所以这里不需要声明它们。
// 它只关心 regs->int_no。

extern "C" void isr_handler(registers_t* regs) {
    //  1. 处理 CPU 异常 (0-31) 
    if (regs->int_no < 32) {
       kernel_panic(regs, regs->int_no);
    }
    //  2. 处理硬件中断 (32-47) 
    else if (regs->int_no >= 32 && regs->int_no < 48) {
        // IRQ0: PIT (中断号 32)
        if (regs->int_no == 32) { 
            tick++;
            extern bool cursor_visible; 
            if (tick % 500 == 0) {
                cursor_visible = !cursor_visible;
            }
        } 
        // IRQ1: Keyboard (中断号 33)
        else if (regs->int_no == 33) {
            uint8_t scancode = inb(0x60);
            keyboard_handle_scancode(scancode);
        }
        // IRQ10: E1000 Network Card (中断号 42)
        else if (regs->int_no == 42) {
            tty_print("INT: ", 0xFFFFFF); print_hex(regs->int_no, 0xFFFFFF); tty_print("\n", 0xFFFFFF);
            tty_print("E1000 Interrupt hit! Calling handler.\n", 0xFF00FF);
            e1000_handle_interrupt(); // < 这是对 e1000.h 中声明函数的调用
        } 
        else if (regs->int_no == 43) {
            // VirtIO 中断处理函数内部会读取并清除中断状态，非常干净
            virtio_net_handle_interrupt(); 
        }
        // 如果是其他 IRQ，可以打印未知 IRQ 信息
        else {
            tty_print("Unhandled IRQ: ", 0xFFFF00);
            print_hex(regs->int_no - 32, 0xFFFF00);
            tty_print("\n", 0xFFFF00);
        }

        //  核心：只在这里发送一次 EOI 
        pic_send_eoi(regs->int_no - 32); 
    }
    //  3. 处理其他未知的/自定义中断 
    else {
        tty_print("Received Unknown Interrupt: ", 0xFFFFFF);
        print_hex(regs->int_no, 0xFFFFFF);
        tty_print(" - Halting.\n", 0xFFFFFF);
        kernel_panic(regs, regs->int_no);
    }
}