#pragma once
#include <stdint.h>

#define KBC_COMMAND_PORT 0x64 // 键盘控制器命令/状态端口
#define KBC_DATA_PORT    0x60 // 键盘控制器数据端口

// --- 键盘控制器命令 ---
#define KBC_COMMAND_REBOOT_SYSTEM 0xFE // 重启系统命令 (发送给 KBC_COMMAND_PORT)

// --- 键盘控制器状态位 ---
#define KBC_STATUS_OUTPUT_BUFFER_FULL 0x01 // 键盘输出缓冲区满
#define KBC_STATUS_INPUT_BUFFER_FULL  0x02 // 键盘输入缓冲区满

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    asm volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outl(uint16_t port, uint32_t val) {
    asm volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    asm volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// 辅助函数：等待 KBC 输入缓冲区为空
static inline void kbc_wait_for_input_buffer_empty() {
    while (inb(KBC_COMMAND_PORT) & KBC_STATUS_INPUT_BUFFER_FULL) {
        // 忙等待，可以加短暂延迟 if needed (但通常很快)
        asm volatile("pause"); // 处理器暂停指令，避免占满 CPU 周期
    }
}

static inline void kbc_wait_for_output_buffer_full() {
    while (!(inb(KBC_COMMAND_PORT) & KBC_STATUS_OUTPUT_BUFFER_FULL)) {
        asm volatile("pause");
    }
}
