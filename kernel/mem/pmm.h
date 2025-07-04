#pragma once
#include <stdint.h>
#include <stivale.h>

// 初始化物理内存管理器
void init_pmm(stivale_struct* boot_info);

// 分配一个物理页
void* pmm_alloc_page();

// 释放一个物理页
void pmm_free_page(void* page);

uint64_t pmm_get_total_pages();
uint64_t pmm_get_used_pages();
