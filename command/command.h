#pragma once

// shell 命令主入口
void execute_command(const char* command);

// 各命令实现
void cmd_help();
void cmd_clear();
void cmd_echo(const char* command);
void cmd_shutdown();
void cmd_meminfo();
void cmd_cpuinfo();
void cmd_time();
void cmd_panic();
void cmd_memtest();
void cmd_nettest_virtio();
void cmd_version();
void cmd_lspci();