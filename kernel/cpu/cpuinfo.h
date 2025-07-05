#pragma once
#include <stdint.h>

// 获取 CPU 制造商字符串
void get_cpu_vendor(char* buffer);

// 获取 CPU 品牌字符串
void get_cpu_brand_string(char* buffer);

// 获取 CPU 功能特性
void get_cpu_features(uint32_t* ecx, uint32_t* edx);

// 获取 CPU 各级缓存大小（单位：KB），如不支持返回0
void get_cpu_cache_info(uint32_t* l1_kb, uint32_t* l2_kb, uint32_t* l3_kb);