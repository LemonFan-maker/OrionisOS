#pragma once
#include "cpu/isr.h" // 需要 registers_t 结构

// __attribute__((noreturn)) 告诉编译器这个函数永远不会返回
void kernel_panic(registers_t* regs, const char* message);
