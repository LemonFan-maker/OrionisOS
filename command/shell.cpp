#include "shell.h"
#include "command.h"
#include "kernel/drivers/tty.h"
#include <stdint.h>
#include "kernel/boot.h"

//  外部依赖 
extern void tty_print(const char* str, uint32_t color);
// 我们需要直接访问光标变量
extern uint32_t cursor_x; 
extern uint32_t cursor_y;

//  模块内全局变量 
#define CMD_BUFFER_SIZE 256
static char cmd_buffer[CMD_BUFFER_SIZE];
static int cmd_buffer_index = 0;
//  新增：记录当前命令行的起始 X 坐标 
static uint32_t line_start_x = 0;

void init_shell() {
    for(int i = 0; i < CMD_BUFFER_SIZE; ++i) cmd_buffer[i] = 0;
    cmd_buffer_index = 0;
    tty_print("> ", 0x4EC9B0);
}

void execute_and_reset_shell() {
    cmd_buffer[cmd_buffer_index] = '\0';
    tty_putc('\n', 0xFFFFFF); // 换行

    if (cmd_buffer_index > 0) {
        execute_command(cmd_buffer); // 执行命令
    }

    init_shell(); // 准备好新的一行
}

void shell_handle_char(char c) {
    if (c == '\n') { // 回车
        execute_and_reset_shell();
    } else if (c == '\b') { // 退格
        if (cmd_buffer_index > 0) {
            cmd_buffer_index--;
            tty_putc('\b', 0xFFFFFF);
        }
    } else { // 普通字符
        if (cmd_buffer_index < CMD_BUFFER_SIZE - 1) {
            cmd_buffer[cmd_buffer_index++] = c;
            char str[2] = {c, '\0'};
            tty_print(str, 0xFFFFFF);
        }
    }
}