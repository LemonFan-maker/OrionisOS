#pragma once
#include <stdint.h>

// 这个结构体精确地匹配了我们在 isr.asm 中压栈的寄存器顺序
struct registers_t {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no, err_code;
    uint64_t rip, cs, rflags, userrsp, ss; // 由CPU在中断时自动压栈
};

// 声明我们的 C++ 处理器，它会被汇编调用
extern "C" void isr_handler(registers_t* regs);