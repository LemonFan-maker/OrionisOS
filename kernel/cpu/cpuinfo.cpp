#include "cpuinfo.h"

// 声明我们新的、专门的汇编函数
extern "C" void cpuid_vendor(char* buffer);

void get_cpu_vendor(char* buffer) {
    // 直接调用汇编函数，让它填充缓冲区
    cpuid_vendor(buffer);
    // 在 C++ 中安全地设置字符串结尾
    buffer[12] = '\0';
}
