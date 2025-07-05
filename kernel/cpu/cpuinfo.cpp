#include "cpuinfo.h"
#include "lib/libc.h"

// 声明我们新的、专门的汇编函数
extern "C" void cpuid_vendor(char* buffer);
extern "C" void cpuid_brand_string(char* buffer);
extern "C" void cpuid_features(uint32_t* ecx, uint32_t* edx);

void get_cpu_vendor(char* buffer) {
    // 直接调用汇编函数，让它填充缓冲区
    cpuid_vendor(buffer);
    // 在 C++ 中安全地设置字符串结尾
    buffer[12] = '\0';
}

void get_cpu_brand_string(char* buffer) {
    cpuid_brand_string(buffer);
    // 在 C++ 中安全地设置字符串结尾
    buffer[48] = '\0';
}

void get_cpu_features(uint32_t* ecx, uint32_t* edx) {
    cpuid_features(ecx, edx);
}

// 获取 CPU 各级缓存大小（单位：KB），如不支持返回0
void get_cpu_cache_info(uint32_t* l1_kb, uint32_t* l2_kb, uint32_t* l3_kb) {
    *l1_kb = 0; *l2_kb = 0; *l3_kb = 0;
    uint32_t eax, ebx, ecx, edx;
    // L2/L3: CPUID 0x80000006
    __asm__ __volatile__ (
        "mov $0x80000006, %%eax\n\t"
        "cpuid\n\t"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        :
        :
    );
    *l2_kb = (ecx >> 16) & 0xFFFF; // L2 Cache size in KB
    *l3_kb = (edx >> 18) & 0x3FFF; // L3 Cache size in KB (部分CPU)
    // L1: CPUID 0x80000005
    __asm__ __volatile__ (
        "mov $0x80000005, %%eax\n\t"
        "cpuid\n\t"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        :
        :
    );
    *l1_kb = (ecx >> 24) & 0xFF; // L1 Data Cache size in KB
}