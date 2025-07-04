#include "pit.h"
#include "ports.h"

void init_pit(uint32_t frequency) {
    // PIT 的基础时钟频率是 1193180 Hz
    uint32_t divisor = 1193180 / frequency;
    // 命令字节：通道0，低/高字节访问模式，方波模式
    outb(0x43, 0x36);
    // 发送分频值的低字节和高字节
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}
