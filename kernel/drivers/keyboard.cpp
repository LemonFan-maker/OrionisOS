#include "keyboard.h"
#include "tty.h"       // 需要 print 函数
#include <stivale.h>     // 需要 stivale_struct
#include "command/shell.h"

#define CMD_BUFFER_SIZE 256
char cmd_buffer[CMD_BUFFER_SIZE];
int cmd_buffer_index = 0;

// --- 外部依赖 ---
extern void print(const char* str, uint32_t color);
extern stivale_struct* temp_boot_info;

// --- 键盘状态标志 ---
static bool shift_pressed = false;
static bool caps_lock = false;

void shell_execute_command(const char* command);

// --- 完整的扫描码映射表 ---
// 未按下Shift的普通键
const char scancode_to_char_lower[] = {
    0,   0,   '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0,   0,
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,   '\\',
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,   '*', 0,    ' ',
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

// 按下Shift时的映射
const char scancode_to_char_upper[] = {
    0,   0,   '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 0,   0,
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0,   '|',
    'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,   '*', 0,   ' ',
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

void init_keyboard() {
    // 初始化键盘状态
    shift_pressed = false;
    caps_lock = false;
}

void keyboard_handle_scancode(uint8_t scancode) {
    // 处理特殊键
    switch(scancode) {
        case 0x2A: // 左Shift按下
        case 0x36: // 右Shift按下
            shift_pressed = true;
            return;
        case 0xAA: // 左Shift释放
        case 0xB6: // 右Shift释放
            shift_pressed = false;
            return;
        case 0x3A: // Caps Lock
            caps_lock = !caps_lock;
            return;
        case 0x0E: // Backspace
            shell_handle_char('\b'); // 交给 shell 处理
            return;
        case 0x1C: // Enter
            shell_handle_char('\n'); // 交给 shell 处理
            return;
        case 0x39: // Space
            shell_handle_char(' '); // 交给 shell 处理
            return;
    }

    // 忽略释放事件
    if (scancode >= 0x80) {
        return;
    }

    // 获取字符
    char c = 0;
    bool should_upper = (shift_pressed != caps_lock); // XOR操作
    
    if (should_upper && scancode < sizeof(scancode_to_char_upper)) {
        c = scancode_to_char_upper[scancode];
    } else if (scancode < sizeof(scancode_to_char_lower)) {
        c = scancode_to_char_lower[scancode];
    }

    if (c != 0) {
        shell_handle_char(c); // <--- 将所有字符都交给 shell 处理
    }
}
