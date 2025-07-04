#include "rtc.h"
#include "kernel/cpu/ports.h" // 需要 inb/outb

// CMOS 寄存器地址
#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71

// 辅助函数，用于读取指定的 CMOS 寄存器
static uint8_t read_cmos(uint8_t reg) {
    outb(CMOS_ADDR, reg);
    return inb(CMOS_DATA);
}

// BCD 码转二进制 (十进制)
static uint8_t bcd_to_bin(uint8_t bcd) {
    return (bcd & 0x0F) + ((bcd >> 4) * 10);
}

void rtc_get_time(rtc_time_t* time) {
    // 读取原始的 BCD 格式数据
    time->second = read_cmos(0x00);
    time->minute = read_cmos(0x02);
    time->hour   = read_cmos(0x04);
    time->day    = read_cmos(0x07);
    time->month  = read_cmos(0x08);
    uint16_t year_bcd = (read_cmos(0x09) << 8) | read_cmos(0x32); // 假设世纪寄存器在 0x32

    // 检查 BCD 编码状态
    uint8_t register_b = read_cmos(0x0B);
    if (!(register_b & 0x04)) {
        time->second = bcd_to_bin(time->second);
        time->minute = bcd_to_bin(time->minute);
        time->hour   = bcd_to_bin(time->hour);
        time->day    = bcd_to_bin(time->day);
        time->month  = bcd_to_bin(time->month);
        year_bcd     = bcd_to_bin(read_cmos(0x09)) + bcd_to_bin(read_cmos(0x32)) * 100;
        time->year = year_bcd;
    } else {
        time->year = read_cmos(0x09) + 2000; // 如果是二进制格式，简单处理
    }

    // 处理 12/24 小时制
    if (!(register_b & 0x02) && (time->hour & 0x80)) {
        time->hour = ((time->hour & 0x7F) + 12) % 24;
    }
}
