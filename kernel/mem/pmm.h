#pragma once
#include <stdint.h>
#include <stivale.h>

#define PAGE_SIZE 4096

// 初始化物理内存管理器
void init_pmm(stivale_struct* boot_info);

// 分配一个物理页
void* pmm_alloc_page();

// 释放一个物理页
void pmm_free_page(void* page);

uint64_t pmm_get_total_pages();
uint64_t pmm_get_used_pages();

uint64_t size_for_order(int order);
int get_order(uint64_t size);
void* get_buddy(void* addr, int order);
void buddy_init(stivale_struct* boot_info);
void* buddy_alloc(uint64_t size);
void buddy_free(void* addr, uint64_t size);

// Buddy分配器统计函数
uint64_t buddy_get_total_pages();
uint64_t buddy_get_used_pages();
uint64_t buddy_get_free_pages();

struct MemoryBlock {
    MemoryBlock* next;
};
