#include "timer.h"
#include "ports.h"

static volatile uint64_t ticks = 0;
static uint32_t timer_freq = 1000; // 默认频率1000Hz (1ms/tick)

// 添加io_wait函数声明（如果ports.h中没有）
static inline void io_wait() {
    outb(0x80, 0); // 使用传统调试端口制造短延迟
}

void timer_handle_interrupt() {
    ticks++;
}

void timer_init(uint32_t frequency) {
    timer_freq = frequency;
    uint32_t divisor = 1193180 / frequency;
    outb(0x43, 0x36);
    outb(0x40, divisor & 0xFF);
    outb(0x40, (divisor >> 8) & 0xFF);
}

void timer_sleep_us(uint32_t us) {
    // 微秒级延迟使用端口I/O等待
    const uint32_t iterations = us * 3; // 经验值调整
    for (volatile uint32_t i = 0; i < iterations; i++) {
        io_wait();
    }
}

void timer_sleep_ms(uint32_t ms) {
    // 毫秒级延迟使用定时器计数
    uint64_t target_ticks = ticks + (ms * timer_freq / 1000);
    while (ticks < target_ticks) {
        asm volatile("pause"); // 降低CPU占用
    }
}
