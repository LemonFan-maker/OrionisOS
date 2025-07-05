#include "pmm.h"
#include "kernel/boot.h"
#include "tty.h"

//  外部依赖 
extern void print(const char* str, uint32_t color);

#define PAGE_SIZE 4096

//  全局 PMM 变量 
uint8_t* pmm_bitmap = nullptr;
uint64_t total_pages = 0;
uint64_t last_checked_page = 0; // 用于优化查找速度

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