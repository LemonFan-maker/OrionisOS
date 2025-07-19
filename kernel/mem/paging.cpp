#include "paging.h"

static PageTable* pml4;

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