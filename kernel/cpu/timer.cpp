#include "timer.h"
#include "ports.h" // 需要 inb/outb

static volatile uint64_t ticks = 0; // PIT 滴答数

// PIT 的中断处理函数，我们会让 isr.cpp 调用它
void timer_handle_interrupt() {
    ticks++;
}

// 初始化 PIT，设置频率
void timer_init(uint32_t frequency) {
    uint32_t divisor = 1193180 / frequency; // PIT 基础频率 1193180 Hz
    outb(0x43, 0x36); // Command port, Channel 0, LSB then MSB, Rate Generator
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

// 忙等待实现延迟 (非常简单，但有效)
void timer_sleep_us(uint32_t us) {
    uint64_t start_ticks = ticks;
    // 粗略计算需要的滴答数，假设每秒 1000 滴答 (1ms/tick)
    // 需要更精确的实现，可以参考 QEMU 的 PMT 计时器，或者读取 CPU TSC
    // 对于 PIO 驱动，简单的 IO 延迟更有效。这里只是一个通用框架。
}

void timer_sleep_ms(uint32_t ms) {
    // 简化为调用 us 版本
    timer_sleep_us(ms * 1000);
}
