#pragma once
#include <stdint.h>

// 获取 CPU 制造商字符串
void get_cpu_vendor(char* buffer);

// 获取 CPU 品牌字符串
void get_cpu_brand_string(char* buffer);

// 获取 CPU 功能特性
void get_cpu_features(uint32_t* ecx, uint32_t* edx);