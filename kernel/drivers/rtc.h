#pragma once
#include <stdint.h>

// 定义一个结构体来保存时间和日期
struct rtc_time_t {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint32_t year;
};

// 读取当前的时间和日期
void rtc_get_time(rtc_time_t* time);
