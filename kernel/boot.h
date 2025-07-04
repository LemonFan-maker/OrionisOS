#pragma once

// 我们需要 stivale_struct 的定义
#include <stivale.h>

// 使用 extern 关键字声明 boot_info 是一个 stivale_struct 指针，
// 它的实体 (定义) 在别的文件中 (具体来说，在 kernel.cpp 中)。
// 这样，任何包含了 boot.h 的文件，都可以安全地使用 boot_info 变量，
// 链接器会在最后阶段将它们连接到 kernel.cpp 中那个真正的 boot_info。
extern stivale_struct* boot_info;

extern uint32_t cursor_x;
extern uint32_t cursor_y;
extern uint32_t current_bg_color;
extern const char* KERNEL_VERSION;