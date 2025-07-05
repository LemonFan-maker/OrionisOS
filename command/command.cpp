#include "command.h"
#include "kernel/drivers/tty.h" // 需要 print
#include "kernel/boot.h"         // 需要 boot_info 来清屏
#include "kernel/cpu/ports.h"
#include "lib/libc.h"
#include "kernel/mem/pmm.h"
#include "kernel/cpu/cpuinfo.h"
#include "kernel/drivers/rtc.h"
#include "kernel/drivers/ethernet/virtio_net.h"

#define COMMAND_VERSION "1.11.0-dirty+"

// 外部 print 函数
extern void print(const char* str, uint32_t color);
extern void print_hex(uint64_t value, uint32_t color);
extern void put_pixel(uint32_t x, uint32_t y, uint32_t color);
extern uint32_t current_bg_color;
extern uint32_t cursor_x, cursor_y;
extern void init_shell();

#ifdef __cplusplus
extern "C" {
    #endif
    void *memcpy(void *dest, const void *src, size_t n); // 声明 memcpy 是 C 语言函数
    #ifdef __cplusplus
}
#endif


//  命令实现 
void cmd_help() {
    print("Available commands:\n", 0xFFFFFF);
    print("  help - Display this message\n", 0xFFFFFF);
    print("  clear - Clear the screen\n", 0xFFFFFF);
    print("  debug - Enable debug mode\n", 0xFFFFFF);
    print("  undebug - Disable debug mode\n", 0xFFFFFF);
    print("  echo <text> - Echo the input text\n", 0xFFFFFF);
    print("  shutdown - Power off the system\n", 0xFFFFFF);
    print("  meminfo - Show memory info\n", 0xFFFFFF);
    print("  cpuinfo - Show CPU info\n", 0xFFFFFF);
    print("  time - Show now time\n", 0xFFFFFF);
    print("  panic - Let's make kernel panic\n", 0xFFFFFF);
    print("  version - Show kernel and command version\n", 0xFFFFFF);
    print("  nettest - Check Network Status\n", 0xFFFFFF);
}

//  新增：debug 模式全局变量 
static int debug_mode = 0;

void cmd_clear() {
    tty_clear(); // 让底层驱动负责清屏和光标归位
}

// echo 命令实现
typedef void (*print_func_t)(const char*, uint32_t);
void cmd_echo(const char* command) {
    // 跳过 "echo " 前缀
    const char* msg = command + 5;
    // 跳过多余空格
    while (*msg == ' ') msg++;
    if (*msg == '\0') {
        print("\n", 0xFFFFFF);
    } else {
        print(msg, 0xFFFFFF);
        print("\n", 0xFFFFFF);
    }
}

//  shutdown 命令 
void cmd_shutdown() {
    asm volatile("outw %%ax, %%dx" : : "a"(0x2000), "d"(0x604));
}

//  meminfo 命令 
void cmd_meminfo() {
    // 1. 从 PMM 获取原始数据
    uint64_t total_pages = pmm_get_total_pages();
    uint64_t used_pages = pmm_get_used_pages();
    uint64_t free_pages = total_pages - used_pages;
    
    // 2. 将页数转换为更易读的单位 (KB 和 MB)
    uint64_t total_kb = total_pages * 4;        // 1页 = 4KB
    uint64_t used_kb = used_pages * 4;
    uint64_t free_kb = free_pages * 4;

    uint64_t total_mb = total_kb / 1024;        // 1MB = 1024KB
    uint64_t used_mb = used_kb / 1024;
    uint64_t free_mb = free_kb / 1024;

    // 3. 格式化并打印输出
    tty_print("\n--- Physical Memory Status ---\n", 0x00FFFF);
    
    tty_print("Total Memory: ", 0xFFFFFF);
    print_dec(total_mb, 0xFFFFFF);
    tty_print(" MiB (", 0xFFFFFF);
    print_dec(total_kb, 0xAAAAAA);
    tty_print(" KiB)\n", 0xFFFFFF);

    tty_print("Used Memory:  ", 0xFFFFFF);
    print_dec(used_mb, 0xFFFFFF);
    tty_print(" MiB (", 0xFFFFFF);
    print_dec(used_kb, 0xAAAAAA);
    tty_print(" KiB)\n", 0xFFFFFF);
    
    tty_print("Free Memory:  ", 0xFFFFFF);
    print_dec(free_mb, 0xFFFFFF);
    tty_print(" MiB (", 0xFFFFFF);
    print_dec(free_kb, 0xAAAAAA);
    tty_print(" KiB)\n", 0xFFFFFF);
}

void cmd_cpuinfo() {
    char vendor[13];
    get_cpu_vendor(vendor);
    tty_print("\nCPU Vendor: ", 0xFFFFFF);
    tty_print(vendor, 0x00FFFF);
    tty_print("\n", 0x00FFFF);
}

void cmd_time() {
    // 1. 在栈上创建一个 rtc_time_t 结构体变量
    rtc_time_t current_time;
    
    // 2. 使用 '&' 运算符，将这个变量的地址传递给 rtc_get_time
    //    rtc_get_time 函数会根据这个地址，直接填充 current_time 的内容
    rtc_get_time(&current_time);
    
    // 3. 格式化并打印填充好的时间信息
    tty_print("\n", 0xFFFFFF); // 先换行
    
    print_dec(current_time.year, 0xFFFFFF);
    tty_print("-", 0xFFFFFF);
    if (current_time.month < 10) tty_print("0", 0xFFFFFF); // 补零，更美观
    print_dec(current_time.month, 0xFFFFFF);
    tty_print("-", 0xFFFFFF);
    if (current_time.day < 10) tty_print("0", 0xFFFFFF);
    print_dec(current_time.day, 0xFFFFFF);
    
    tty_print(" ", 0xFFFFFF);
    
    if (current_time.hour < 10) tty_print("0", 0xFFFFFF);
    print_dec(current_time.hour, 0xFFFFFF);
    tty_print(":", 0xFFFFFF);
    if (current_time.minute < 10) tty_print("0", 0xFFFFFF);
    print_dec(current_time.minute, 0xFFFFFF);
    tty_print(":", 0xFFFFFF);
    if (current_time.second < 10) tty_print("0", 0xFFFFFF);
    print_dec(current_time.second, 0xFFFFFF);
    tty_print("\n", 0xFFFFFF);
}

void cmd_panic() {
    tty_print("\nTriggering kernel panic...", 0xFF6060);
    asm volatile("int $3");
}

void cmd_memtest() {
    tty_clear();

    // 1. 打印第5行的 A（白色）
    cursor_x = 0;  // 从第0列开始打印
    cursor_y = 18 * 5;
    for (int i = 0; i < 50; i++) tty_putc('A', 0xFFFFFF);

    // 2. 打印第10行的 B（红色）
    cursor_x = 0;  // 从第0列开始打印
    cursor_y = 18 * 10;
    for (int i = 0; i < 50; i++) tty_putc('B', 0xFF0000);

    // 3. 执行 memcpy
    uint32_t* fb = (uint32_t*)boot_info->framebuffer_addr;
    uint32_t pitch = boot_info->framebuffer_pitch;
    size_t bytes_per_char = 9 * 4;  // 每个字符占9像素×4字节
    size_t bytes_to_copy = 50 * bytes_per_char;

    for (int row = 0; row < 16; row++) { // 字符高度16行像素
        // 源地址：第5行的第0个字符起始位置
        uint8_t* src = (uint8_t*)(fb) + (18 * 5 + row) * pitch;
        // 目标地址：第10行的第0个字符起始位置
        uint8_t* dst = (uint8_t*)(fb) + (18 * 10 + row) * pitch;
        memcpy(dst, src, bytes_to_copy);
    }

    tty_print("\nMemtest Finish", 0x00FF00);
}

void cmd_nettest_virtio() {
    tty_print("\n--- VirtIO Network Test ---\n", 0x00FFFF);

    // 1. 获取 VirtIO 网卡的 MAC 地址
    const uint8_t* mac = virtio_net_get_mac_address();
    if (mac[0] == 0 && mac[1] == 0 && mac[2] == 0) { // 简单检查 MAC 是否有效
        tty_print("VirtIO MAC address is not set. Aborting test.\n", 0xFF0000);
        return;
    }
    tty_print("VirtIO MAC: ", 0xFFFFFF);
    for (int i = 0; i < 6; i++) {
        print_hex(mac[i], 0x00FF00);
        if (i < 5) tty_print(":", 0x00FF00);
    }
    tty_print("\n", 0x00FF00);

    // 2. 构造一个标准的 ARP 请求包
    uint8_t arp_packet[60] = {0}; // 以太网帧最小长度是 60 字节 (不含 CRC)

    // 以太网头 (14 bytes)
    memset(arp_packet, 0xFF, 6);         // 目的 MAC: 广播
    memcpy(arp_packet + 6, mac, 6);      // 源 MAC:   我们的 MAC
    arp_packet[12] = 0x08; arp_packet[13] = 0x06; // EtherType: ARP

    // ARP 协议部分 (从第 14 字节开始)
    arp_packet[14] = 0x00; arp_packet[15] = 0x01; // 硬件类型: 以太网
    arp_packet[16] = 0x08; arp_packet[17] = 0x00; // 协议类型: IPv4
    arp_packet[18] = 0x06;                       // 硬件地址长度: 6
    arp_packet[19] = 0x04;                       // 协议地址长度: 4
    arp_packet[20] = 0x00; arp_packet[21] = 0x01; // 操作码: 请求
    memcpy(arp_packet + 22, mac, 6);             // 发送方 MAC: 我们的 MAC
    arp_packet[28] = 10; arp_packet[29] = 0;    // 发送方 IP: 10.0.2.15 (QEMU 默认)
    arp_packet[30] = 2; arp_packet[31] = 15;
    memset(arp_packet + 32, 0, 6);               // 目标 MAC: 全 0 (未知)
    arp_packet[38] = 10; arp_packet[39] = 0;    // 目标 IP: 10.0.2.2 (QEMU 网关)
    arp_packet[40] = 2; arp_packet[41] = 2;

    // 3. 发送数据包
    // 注意：以太网帧的最小长度是 60 字节。如果数据包小于这个长度，
    // 驱动或硬件会自动填充。我们直接发送 60 字节是最安全的。
    if (virtio_net_send_packet(arp_packet, 60)) {
        tty_print("ARP Request sent to 10.0.2.2 via VirtIO.\n", 0x00FF00);
    } else {
        tty_print("Failed to send ARP Request via VirtIO.\n", 0xFF0000);
    }
}

//  version 命令 
void cmd_version() {
    print("Kernel version: ", 0x00FFAA);
    print(KERNEL_VERSION, 0x00FFAA);
    print("\nCommand version: ", 0x00FFAA);
    print(COMMAND_VERSION, 0x00FFAA);
    print("\n", 0x00FFAA);
}

void execute_command(const char* command) {
    if (strcmp(command, "debug") == 0) {
        debug_mode = 1;
        print("\nDebug mode enabled.\n", 0x00FF00);
        return;
    }
    if (strcmp(command, "undebug") == 0) {
        if (debug_mode) {
            debug_mode = 0;
            print("\nDebug mode disabled.\n", 0x00FF00);
        } else {
            print("\nNo debug mode found.\n", 0xFF6060);
        }
        return;
    }

    if (debug_mode) {
        print("\n[DEBUG] Received command string: '", 0xFFFF00);
        print(command, 0xFFFF00);
        print("'\n", 0xFFFF00);
        
        print("[DEBUG] String length: ", 0xFFFF00);
        print_hex(strlen(command), 0xFFFF00); // 使用我们 libc 中的 strlen
        print("\n", 0xFFFF00);

        print("[DEBUG] Hex dump: ", 0xFFFF00);
        for (size_t i = 0; i <= strlen(command); i++) { // i <= strlen 是为了打印最后的 \0
            print("0x", 0xAAAAAA);
            print_hex((unsigned char)command[i], 0xAAAAAA);
            print(" ", 0xAAAAAA);
        }
        print("\n", 0xFFFF00);
    }

    if (strcmp(command, "help") == 0) {
        cmd_help();
    } else if (strcmp(command, "clear") == 0) {
        cmd_clear();
    } else if (strncmp(command, "echo", 4) == 0 && (command[4] == ' ' || command[4] == '\0')) {
        cmd_echo(command);
    } else if (strcmp(command, "shutdown") == 0) {
        cmd_shutdown();
    } else if (strcmp(command, "meminfo") == 0) {
        cmd_meminfo();
    } else if (strcmp(command, "cpuinfo") == 0) {
        cmd_cpuinfo();
    } else if (strcmp(command, "version") == 0) {
        cmd_version();
    } else if (strcmp(command, "panic") == 0) {
        cmd_panic();
    } else if (strcmp(command, "time") == 0) {
        cmd_time();
    } else if (strcmp(command, "memtest") == 0) {
        cmd_memtest();
    } else if (strcmp(command, "nettest") == 0) {
        cmd_nettest_virtio();
    } else {
        print("\nUnknown command: ", 0xFF6060);
        print(command, 0xFF6060);
        print("\n", 0xFF6060);
    }
}
