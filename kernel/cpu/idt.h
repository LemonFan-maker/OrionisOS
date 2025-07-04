#pragma once
#include <stdint.h>

// IDT 条目/门描述符 结构
struct idt_entry_t {
    uint16_t isr_low;    // ISR 地址的低 16 位
    uint16_t kernel_cs;  // 内核代码段选择子
    uint8_t  ist;        // 中断栈表 (IST) 索引，设为 0
    uint8_t  attributes; // 门类型和属性
    uint16_t isr_mid;    // ISR 地址的中间 16 位
    uint32_t isr_high;   // ISR 地址的高 32 位
    uint32_t reserved;   // 保留
} __attribute__((packed));

// IDT 指针结构 (用于 lidt 指令)
struct idt_ptr_t {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

// 初始化 IDT
void init_idt();