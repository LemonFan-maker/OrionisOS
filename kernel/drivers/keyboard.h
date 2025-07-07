#pragma once
#include <stdint.h>

// 键盘驱动的初始化函数
void init_keyboard();

// 这个函数将被 isr_handler 调用
void keyboard_handle_scancode(uint8_t scancode);