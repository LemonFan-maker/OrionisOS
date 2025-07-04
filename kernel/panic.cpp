#include "panic.h"
#include "drivers/tty.h"
#include "boot.h"

// 外部依赖
extern uint32_t current_bg_color;

__attribute__((noreturn))
void kernel_panic(registers_t* regs, const char* message) {
    // 1. 关闭中断，防止进一步的混乱
    asm volatile("cli");

    // 2. 用醒目的颜色清空屏幕
    current_bg_color = 0xAA0000; // 深红色
    tty_clear();

    // 3. 打印恐慌信息
    tty_print("\n*** KERNEL PANIC ***\n\n", 0xFFFFFF);
    tty_print("A fatal error has occurred and the system has been halted.\n", 0xFFFFFF);
    tty_print("Reason: ", 0xFFFFFF);
    tty_print(message, 0xFFFF00); // 黄色

    // 4. (可选但推荐) 打印寄存器状态
    if (regs) {
        tty_print("\n\nRegister Dump:\n", 0xFFFFFF);
        tty_print("  RAX=", 0xFFFFFF); print_hex(regs->rax, 0x00FFFF);
        tty_print("  RBX=", 0xFFFFFF); print_hex(regs->rbx, 0x00FFFF);
        // ... 打印所有你感兴趣的寄存器 ...
        tty_print("\n  RIP=", 0xFFFFFF); print_hex(regs->rip, 0x00FFFF);
        tty_print("   CS=", 0xFFFFFF); print_hex(regs->cs, 0x00FFFF);
        tty_print(" RFLAGS=", 0xFFFFFF); print_hex(regs->rflags, 0x00FFFF);
    }
    
    // 5. 永久停机
    for (;;) {
        asm volatile("hlt");
    }
}