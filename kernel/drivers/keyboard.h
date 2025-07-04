#pragma once
#include <stdint.h>

// 键盘驱动的初始化函数 (虽然现在没做什么，但好习惯)
void init_keyboard();

// 这个函数将被 isr_handler 调用
void keyboard_handle_scancode(uint8_t scancode);