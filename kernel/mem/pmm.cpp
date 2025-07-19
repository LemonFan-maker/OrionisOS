#include "pmm.h"
#include "kernel/boot.h"
#include "tty.h"
#include "lib/libc.h"

//  外部依赖 
extern void print(const char* str, uint32_t color);

#define PAGE_SIZE 4096
#define MAX_ORDER 16   // 最大块阶数，比如 2^16 = 64KB
#define MIN_ORDER 12   // 最小块阶数，比如 2^12 = 4KB (页大小)

MemoryBlock* free_list[MAX_ORDER - MIN_ORDER + 1];  // 每阶空闲块链表

//  全局 PMM 变量 
uint8_t* pmm_bitmap = nullptr;
uint64_t total_pages = 0;
uint64_t last_checked_page = 0; // 用于优化查找速度
uint64_t total_physical_pages = 0;
uint64_t buddy_used_pages = 0;
uint64_t buddy_total_managed_pages = 0;

//  位图操作函数 
void pmm_bitmap_set(uint64_t page_index) {
    pmm_bitmap[page_index / 8] |= (1 << (page_index % 8));
}

void pmm_bitmap_clear(uint64_t page_index) {
    pmm_bitmap[page_index / 8] &= ~(1 << (page_index % 8));
}

bool pmm_bitmap_test(uint64_t page_index) {
    return pmm_bitmap[page_index / 8] & (1 << (page_index % 8));
}

uint64_t buddy_get_total_pages() {
    return total_physical_pages;
}

uint64_t buddy_get_used_pages() {
    return buddy_used_pages;
}

uint64_t buddy_get_free_pages() {
    return buddy_total_managed_pages - buddy_used_pages;
}

//  PMM 初始化 
void init_pmm(stivale_struct* boot_info) {
    // 确认 stivale_mmap_entry 的定义，这在 stivale.h 中
    typedef struct {
        uint64_t base;
        uint64_t length;
        uint32_t type;
        uint32_t unused;
    } stivale_mmap_entry;

    stivale_mmap_entry* mmap = (stivale_mmap_entry*)boot_info->memory_map_addr;

    uint64_t highest_addr = 0;
    for (uint64_t i = 0; i < boot_info->memory_map_entries; i++) {
        if (mmap[i].type == 1) { // STIVALE_MMAP_USABLE 通常被定义为 1
            //  核心修正：使用 base 和 length 
            uint64_t top = mmap[i].base + mmap[i].length;
            if (top > highest_addr) {
                highest_addr = top;
            }
        }
    }

    total_pages = highest_addr / PAGE_SIZE;
    uint64_t bitmap_size = total_pages / 8;
    if (bitmap_size * 8 < total_pages) { bitmap_size++; }

    for (uint64_t i = 0; i < boot_info->memory_map_entries; i++) {
        //  核心修正：使用 base 和 length 
        if (mmap[i].type == 1 && mmap[i].length >= bitmap_size) {
            pmm_bitmap = (uint8_t*)mmap[i].base;
            break;
        }
    }

    // 将所有内存页标记为已使用 (初始化)
    for (uint64_t i = 0; i < bitmap_size; i++) pmm_bitmap[i] = 0xFF;

    // 将可用的页标记为空闲
    for (uint64_t i = 0; i < boot_info->memory_map_entries; i++) {
        if (mmap[i].type == 1) { // STIVALE_MMAP_USABLE
            //  核心修正：使用 base 和 length 
            for (uint64_t j = 0; j < mmap[i].length / PAGE_SIZE; j++) {
                pmm_bitmap_clear((mmap[i].base / PAGE_SIZE) + j);
            }
        }
    }

    // 将位图本身占用的区域标记为已使用
    uint64_t bitmap_pages = bitmap_size / PAGE_SIZE;
    if (bitmap_size % PAGE_SIZE) bitmap_pages++;
    for (uint64_t i = 0; i < bitmap_pages; i++) {
        pmm_bitmap_set(((uint64_t)pmm_bitmap / PAGE_SIZE) + i);
    }
    
    print("\nPMM Initialized. Bitmap at 0x", 0xFFFFFF);
    print_hex((uint64_t)pmm_bitmap, 0xFFFFFF);
}

//  分配和释放函数 
void* pmm_alloc_page() {
    // 从上一次检查的地方开始，避免每次都从头扫描
    for (uint64_t i = last_checked_page; i < total_pages; i++) {
        if (!pmm_bitmap_test(i)) {
            pmm_bitmap_set(i);
            last_checked_page = i + 1;
            return (void*)(i * PAGE_SIZE);
        }
    }
    
    // 如果从上次的位置找不到，再从头完整扫描一次
    for (uint64_t i = 0; i < last_checked_page; i++) {
        if (!pmm_bitmap_test(i)) {
            pmm_bitmap_set(i);
            last_checked_page = i + 1;
            return (void*)(i * PAGE_SIZE);
        }
    }

    // 没有可用内存了
    return nullptr;
}

void pmm_free_page(void* page) {
    if (page == nullptr) return;
    uint64_t page_index = (uint64_t)page / PAGE_SIZE;
    pmm_bitmap_clear(page_index);
    // 可以优化，让下一次分配从释放的页面附近开始
    if (page_index < last_checked_page) {
        last_checked_page = page_index;
    }
}

uint64_t pmm_get_total_pages() {
    return total_pages;
}

uint64_t pmm_get_used_pages() {
    uint64_t used_pages = 0;
    for (uint64_t i = 0; i < total_pages; i++) {
        if (pmm_bitmap_test(i)) {
            used_pages++;
        }
    }
    return used_pages;
}

// 计算order对应大小
uint64_t size_for_order(int order) {
    return 1 << order;
}

// 计算请求大小对应order（向上取整）
int get_order(uint64_t size) {
    int order = MIN_ORDER;
    uint64_t block_size = 1 << order;
    while (block_size < size) {
        order++;
        block_size <<= 1;
    }
    return order;
}

// 计算伙伴地址
void* get_buddy(void* addr, int order) {
    uint64_t block_size = size_for_order(order);
    uintptr_t base = (uintptr_t)addr;
    uintptr_t buddy = base ^ block_size;
    return (void*)buddy;
}

void buddy_init(stivale_struct* boot_info) {
    // 确认 stivale_mmap_entry 的定义，这在 stivale.h 中
    typedef struct {
        uint64_t base;
        uint64_t length;
        uint32_t type;
        uint32_t unused;
    } stivale_mmap_entry;

    stivale_mmap_entry* mmap = (stivale_mmap_entry*)boot_info->memory_map_addr;
    
    // 初始化统计变量
    total_physical_pages = 0;
    buddy_used_pages = 0;
    buddy_total_managed_pages = 0;
    
    // 计算总物理内存页数
    uint64_t highest_addr = 0;
    for (uint64_t i = 0; i < boot_info->memory_map_entries; i++) {
        uint64_t top = mmap[i].base + mmap[i].length;
        if (top > highest_addr) {
            highest_addr = top;
        }
        if (mmap[i].type == 1) { // 可用内存
            total_physical_pages += mmap[i].length / PAGE_SIZE;
        }
    }
    
    for (uint64_t i = 0; i < boot_info->memory_map_entries; i++) {
        print("Base:", 0xFFFFFF);
        print_hex(mmap[i].base, 0xFFFFFF);
        print(" Length:", 0xFFFFFF);
        print_hex(mmap[i].length, 0xFFFFFF);
        print(" Type:", 0xFFFFFF);
        print_hex(mmap[i].type, 0xFFFFFF);
        print("\n", 0xFFFFFF);
    }

    // 清空空闲链表
    memset(free_list, 0, sizeof(free_list));

    // 遍历所有可用的内存区域
    for (uint64_t i = 0; i < boot_info->memory_map_entries; i++) {
        if (mmap[i].type != 1) continue;  // 只使用可用的内存（type == 1）

        uintptr_t current = (uintptr_t)mmap[i].base;
        uint64_t remaining = mmap[i].length;

        // 对当前可用区域分配为buddy块
        while (remaining >= PAGE_SIZE) {
            // 找到当前剩余大小下最大的 order
            int order = MAX_ORDER;
            while ((1UL << order) > remaining || (current % (1UL << order)) != 0) {
                order--;
                if (order < MIN_ORDER) break; // 防止无穷循环
            }

            if (order < MIN_ORDER) break; // 不再能分配最小块了

            // 插入 free_list[order - MIN_ORDER]
            MemoryBlock* block = (MemoryBlock*)current;
            block->next = free_list[order - MIN_ORDER];
            free_list[order - MIN_ORDER] = block;

            // 更新管理的页数统计
            buddy_total_managed_pages += (1UL << order) / PAGE_SIZE;

            current += (1UL << order);
            remaining -= (1UL << order);
        }
    }
}

// 分配内存块
void* buddy_alloc(uint64_t size) {
    int order = get_order(size);
    for (int i = order; i <= MAX_ORDER; i++) {
        if (free_list[i - MIN_ORDER] != nullptr) {
            // 找到合适阶的块
            MemoryBlock* block = free_list[i - MIN_ORDER];
            free_list[i - MIN_ORDER] = block->next;

            // 拆分成小块直到满足请求阶
            while (i > order) {
                i--;
                void* buddy = (void*)((uintptr_t)block + size_for_order(i));
                ((MemoryBlock*)buddy)->next = free_list[i - MIN_ORDER];
                free_list[i - MIN_ORDER] = (MemoryBlock*)buddy;
            }
            
            // 更新使用统计
            buddy_used_pages += size_for_order(order) / PAGE_SIZE;
            
            return (void*)block;
        }
    }
    print("Buddy: No free block found for size ", 0xFF0000);
    // 无空闲块
    return nullptr;
}

// 释放内存块
void buddy_free(void* addr, uint64_t size) {
    uint8_t order = get_order(size);
    
    // 更新使用统计
    buddy_used_pages -= size_for_order(order) / PAGE_SIZE;
    
    while (order < MAX_ORDER) {
        void* buddy = get_buddy(addr, order);
        // 在free_list[order - MIN_ORDER]中查找buddy
        MemoryBlock** cur = &free_list[order - MIN_ORDER];
        while (*cur && *cur != buddy) {
            cur = &((*cur)->next);
        }
        if (*cur != buddy) {
            // 伙伴不空闲，插入链表结束
            MemoryBlock* block = (MemoryBlock*)addr;
            block->next = free_list[order - MIN_ORDER];
            free_list[order - MIN_ORDER] = block;
            return;
        }
        // 找到伙伴，移除它
        *cur = (*cur)->next;

        // 取两个块中地址较小的作为合并块地址
        if (buddy < addr) {
            addr = buddy;
        }
        order++;
    }
    // 插入最大阶链表
    MemoryBlock* block = (MemoryBlock*)addr;
    block->next = free_list[MAX_ORDER - MIN_ORDER];
    free_list[MAX_ORDER - MIN_ORDER] = block;
}
