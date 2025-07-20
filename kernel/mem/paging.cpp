#include "paging.h"
#include "pmm.h"

static PageTable* pml4;

static inline void flushTLB(void* page) {
	__asm__ volatile ("invlpg (%0)" :: "r" (page) : "memory");
}

uint64_t readCR3(void) {
    uint64_t val;
    __asm__ volatile ( "mov %%cr3, %0" : "=r"(val) );
    return val;
}

PageTable* initPML4() {
	uintptr_t cr3 = (uintptr_t) readCR3();
	pml4 = (PageTable*) ((cr3 >> 12) << 12);

	return pml4;
}

void setPageTableEntry(PageEntry* entry, uint8_t flags, uintptr_t physical_address, uint16_t available) {
    entry->present = (flags >> 1) & 1;
    entry->writable = (flags >> 2) & 1;
    entry->user_accessible = (flags >> 3) & 1;
    entry->write_through_caching = (flags & 0x8) & 1;
    entry->disable_cache = (flags & 0x10) & 1;
    entry->null = (flags & 0x20) & 1;
    entry->global = (flags & 0x40) & 1;
    entry->avl1 = available & 0x3;
    entry->physical_address = physical_address; 
    entry->avl2 = available >> 3;
    entry->no_execute = flags >> 7;
}

void freePageTableEntry(PageEntry* entry) {
    if (entry->present) {
        // 清空页表条目
        entry->present = 0;
        entry->writable = 0;
        entry->user_accessible = 0;
        entry->write_through_caching = 0;
        entry->disable_cache = 0;
        entry->null = 0;
        entry->global = 0;
        entry->avl1 = 0;
        entry->physical_address = 0;
        entry->avl2 = 0;
        entry->no_execute = 0;
    }
}

size_t* getPhysicalAddress(void* virtual_address) 
{
    uintptr_t address = (uintptr_t) virtual_address;

    uint64_t offset = address & 0xFFF;
    uint64_t page_table_index = (address >> 12) & 0x1FF;
    uint64_t page_directory_index = (address >> 21) & 0x1FF;
    uint64_t page_directory_pointer_index = (address >> 30) & 0x1FF;
    uint64_t pml4_index = (address >> 39) & 0x1FF;

    PageTable* page_directory_pointer = (PageTable*) ((uint64_t) (pml4->entries[pml4_index].physical_address) << 12);
    PageTable* page_directory = (PageTable*) ((uint64_t) (page_directory_pointer->entries[page_directory_pointer_index].physical_address) << 12);
    PageTable* page_table = (PageTable*) ((uint64_t) (page_directory->entries[page_directory_index].physical_address) << 12);

    return (size_t*) ((page_table->entries[page_table_index].physical_address << 12) + offset);
}

static void allocateEntry(PageTable* table, size_t index, uint8_t flags) {
    void* physical_address = buddy_alloc(4096);
    setPageTableEntry(&(table->entries[index]), flags, (uintptr_t) physical_address >> 12, 0);
}

static void freeEntry(PageTable* table) {
    buddy_free(table, 4096);
}


void mapPage(void* virtual_address, void* physical_address, uint8_t flags) {
    // 确保两个地址都是对齐的
    uintptr_t virtual_address_int = (uintptr_t) virtual_address;
    uintptr_t physical_address_int = (uintptr_t) physical_address;

    uint64_t pml4_index = (virtual_address_int >> 39) & 0x1FF;
    uint64_t page_directory_pointer_index = (virtual_address_int >> 30) & 0x1FF;
    uint64_t page_directory_index = (virtual_address_int >> 21) & 0x1FF;
    uint64_t page_table_index = (virtual_address_int >> 12) & 0x1FF;
 
    if (!pml4->entries[pml4_index].present)
        allocateEntry(pml4, pml4_index, flags);

    PageTable* page_directory_pointer = (PageTable*) (uint64_t) (pml4->entries[pml4_index].physical_address << 12);

    if (!page_directory_pointer->entries[page_directory_pointer_index].present) 
        allocateEntry(page_directory_pointer, page_directory_pointer_index, flags);

    PageTable* page_directory = (PageTable*) (uint64_t) (page_directory_pointer->entries[page_directory_pointer_index].physical_address << 12);

    if (!page_directory->entries[page_directory_index].present) 
        allocateEntry(page_directory, page_directory_index, flags);

    PageTable* page_table = (PageTable*) (uint64_t) (page_directory->entries[page_directory_index].physical_address << 12);

    setPageTableEntry(&(page_table->entries[page_table_index]), flags, physical_address_int >> 12, 0);
	
    // 刷新TLB
    flushTLB(virtual_address);
}

void unmapPage(void* virtual_address) {
    if (virtual_address == nullptr) return;
    uintptr_t virtual_address_int = (uintptr_t) virtual_address;

    uint64_t pml4_index = (virtual_address_int >> 39) & 0x1FF;
    uint64_t page_directory_pointer_index = (virtual_address_int >> 30) & 0x1FF;
    uint64_t page_directory_index = (virtual_address_int >> 21) & 0x1FF;
    uint64_t page_table_index = (virtual_address_int >> 12) & 0x1FF;

    if (!pml4->entries[pml4_index].present) return; // 如果该页表项不存在，直接返回

    PageTable* page_directory_pointer = (PageTable*) (uint64_t) (pml4->entries[pml4_index].physical_address << 12);

    if (!page_directory_pointer->entries[page_directory_pointer_index].present) return; // 如果该页表项不存在，直接返回

    PageTable* page_directory = (PageTable*) (uint64_t) (page_directory_pointer->entries[page_directory_pointer_index].physical_address << 12);

    if (!page_directory->entries[page_directory_index].present) return; // 如果该页表项不存在，直接返回

    PageTable* page_table = (PageTable*) (uint64_t) (page_directory->entries[page_directory_index].physical_address << 12);

    freePageTableEntry(&(page_table->entries[page_table_index]));

    // 这里要清理上面的各级页表，检查是否需要释放更高层的页表
    if (!anyPageTableEntryUsed(page_table)) {
        freeEntry(page_directory);
    }
    if (!anyPageTableEntryUsed(page_directory)) {
        freeEntry(page_directory_pointer);
    }
    if (!anyPageTableEntryUsed(page_directory_pointer)) {
        freeEntry(pml4);
    }

    // 刷新TLB
    flushTLB(virtual_address);
}

// 检查页表是否仍然被使用（即是否有任何有效条目）
bool anyPageTableEntryUsed(PageTable* table) {
    for (int i = 0; i < 512; i++) {
        if (table->entries[i].present) {
            return true;
        }
    }
    return false;
}
