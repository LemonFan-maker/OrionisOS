#include "command.h"
#include "kernel/drivers/tty.h" // 需要 print/tty_print
#include "kernel/boot.h"         // 需要 boot_info 来清屏
#include "kernel/cpu/ports.h"
#include "lib/libc.h"
#include "kernel/mem/pmm.h"
#include "kernel/cpu/cpuinfo.h"
#include "kernel/drivers/rtc.h"
#include "kernel/drivers/ethernet/virtio_net.h"
#include "kernel/cpu/pci_ids.h"

#define COMMAND_VERSION "1.11.0-dirty+"

// 外部符号声明
extern void print(const char* str, uint32_t color);
extern void print_hex(uint64_t value, uint32_t color);
extern void put_pixel(uint32_t x, uint32_t y, uint32_t color);
extern uint32_t current_bg_color;
extern uint32_t cursor_x, cursor_y;
extern void init_shell();

#ifdef __cplusplus
extern "C" {
#endif
void *memcpy(void *dest, const void *src, size_t n);
#ifdef __cplusplus
}
#endif

// Debug 模式全局变量
static int debug_mode = 0;

// ================== 命令实现 ==================

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
    print("  lspci - List PCI devices\n", 0xFFFFFF);
}

void cmd_clear() {
    tty_clear();
}

void cmd_echo(const char* command) {
    const char* msg = command + 5;
    while (*msg == ' ') msg++;
    if (*msg == '\0') {
        print("\n", 0xFFFFFF);
    } else {
        print(msg, 0xFFFFFF);
        print("\n", 0xFFFFFF);
    }
}

void cmd_shutdown() {
    asm volatile("outw %%ax, %%dx" : : "a"(0x2000), "d"(0x604));
}

void cmd_meminfo() {
    uint64_t total_pages = pmm_get_total_pages();
    uint64_t used_pages = pmm_get_used_pages();
    uint64_t free_pages = total_pages - used_pages;
    uint64_t total_kb = total_pages * 4;
    uint64_t used_kb = used_pages * 4;
    uint64_t free_kb = free_pages * 4;
    uint64_t total_mb = total_kb / 1024;
    uint64_t used_mb = used_kb / 1024;
    uint64_t free_mb = free_kb / 1024;
    tty_print("\n--- Physical Memory Status ---\n", 0x00FFFF);
    tty_print("Total Memory: ", 0xFFFFFF); print_dec(total_mb, 0xFFFFFF); tty_print(" MiB (", 0xFFFFFF); print_dec(total_kb, 0xAAAAAA); tty_print(" KiB)\n", 0xFFFFFF);
    tty_print("Used Memory:  ", 0xFFFFFF); print_dec(used_mb, 0xFFFFFF); tty_print(" MiB (", 0xFFFFFF); print_dec(used_kb, 0xAAAAAA); tty_print(" KiB)\n", 0xFFFFFF);
    tty_print("Free Memory:  ", 0xFFFFFF); print_dec(free_mb, 0xFFFFFF); tty_print(" MiB (", 0xFFFFFF); print_dec(free_kb, 0xAAAAAA); tty_print(" KiB)\n", 0xFFFFFF);
}

void cmd_cpuinfo() {
    char vendor[13];
    get_cpu_vendor(vendor);
    tty_print("\nCPU Vendor: ", 0xFFFFFF); tty_print(vendor, 0x00FFFF);
    char brand[49];
    get_cpu_brand_string(brand);
    tty_print("\nCPU Brand: ", 0xFFFFFF); tty_print(brand, 0x00FFFF);
    uint32_t ecx, edx;
    get_cpu_features(&ecx, &edx);
    tty_print("\nFeatures: ", 0xFFFFFF);
    if (edx & (1 << 23)) tty_print(" MMX", 0x00FF00);
    if (edx & (1 << 25)) tty_print(" SSE", 0x00FF00);
    if (edx & (1 << 26)) tty_print(" SSE2", 0x00FF00);
    if (ecx & (1 << 0)) tty_print(" SSE3", 0x00FF00);
    if (ecx & (1 << 19)) tty_print(" SSE4.1", 0x00FF00);
    if (ecx & (1 << 20)) tty_print(" SSE4.2", 0x00FF00);
    if (ecx & (1 << 28)) tty_print(" AVX", 0x00FF00);
    tty_print("\n", 0x00FFFF);
}

void cmd_time() {
    rtc_time_t current_time;
    rtc_get_time(&current_time);
    tty_print("\n", 0xFFFFFF);
    print_dec(current_time.year, 0xFFFFFF); tty_print("-", 0xFFFFFF);
    if (current_time.month < 10) tty_print("0", 0xFFFFFF);
    print_dec(current_time.month, 0xFFFFFF); tty_print("-", 0xFFFFFF);
    if (current_time.day < 10) tty_print("0", 0xFFFFFF);
    print_dec(current_time.day, 0xFFFFFF); tty_print(" ", 0xFFFFFF);
    if (current_time.hour < 10) tty_print("0", 0xFFFFFF);
    print_dec(current_time.hour, 0xFFFFFF); tty_print(":", 0xFFFFFF);
    if (current_time.minute < 10) tty_print("0", 0xFFFFFF);
    print_dec(current_time.minute, 0xFFFFFF); tty_print(":", 0xFFFFFF);
    if (current_time.second < 10) tty_print("0", 0xFFFFFF);
    print_dec(current_time.second, 0xFFFFFF); tty_print("\n", 0xFFFFFF);
}

void cmd_panic() {
    tty_print("\nTriggering kernel panic...", 0xFF6060);
    asm volatile("int $3");
}

void cmd_memtest() {
    tty_clear();
    cursor_x = 0; cursor_y = 18 * 5;
    for (int i = 0; i < 50; i++) tty_putc('A', 0xFFFFFF);
    cursor_x = 0; cursor_y = 18 * 10;
    for (int i = 0; i < 50; i++) tty_putc('B', 0xFF0000);
    uint32_t* fb = (uint32_t*)boot_info->framebuffer_addr;
    uint32_t pitch = boot_info->framebuffer_pitch;
    size_t bytes_per_char = 9 * 4;
    size_t bytes_to_copy = 50 * bytes_per_char;
    for (int row = 0; row < 16; row++) {
        uint8_t* src = (uint8_t*)(fb) + (18 * 5 + row) * pitch;
        uint8_t* dst = (uint8_t*)(fb) + (18 * 10 + row) * pitch;
        memcpy(dst, src, bytes_to_copy);
    }
    tty_print("\nMemtest Finish", 0x00FF00);
}

void cmd_nettest_virtio() {
    tty_print("\n--- VirtIO Network Test ---\n", 0x00FFFF);
    const uint8_t* mac = virtio_net_get_mac_address();
    if (mac[0] == 0 && mac[1] == 0 && mac[2] == 0) {
        tty_print("VirtIO MAC address is not set. Aborting test.\n", 0xFF0000);
        return;
    }
    tty_print("VirtIO MAC: ", 0xFFFFFF);
    for (int i = 0; i < 6; i++) {
        print_hex(mac[i], 0x00FF00);
        if (i < 5) tty_print(":", 0x00FF00);
    }
    tty_print("\n", 0x00FF00);
    uint8_t arp_packet[60] = {0};
    memset(arp_packet, 0xFF, 6);
    memcpy(arp_packet + 6, mac, 6);
    arp_packet[12] = 0x08; arp_packet[13] = 0x06;
    arp_packet[14] = 0x00; arp_packet[15] = 0x01;
    arp_packet[16] = 0x08; arp_packet[17] = 0x00;
    arp_packet[18] = 0x06;
    arp_packet[19] = 0x04;
    arp_packet[20] = 0x00; arp_packet[21] = 0x01;
    memcpy(arp_packet + 22, mac, 6);
    arp_packet[28] = 10; arp_packet[29] = 0;
    arp_packet[30] = 2; arp_packet[31] = 15;
    memset(arp_packet + 32, 0, 6);
    arp_packet[38] = 10; arp_packet[39] = 0;
    arp_packet[40] = 2; arp_packet[41] = 2;
    if (virtio_net_send_packet(arp_packet, 60)) {
        tty_print("ARP Request sent to 10.0.2.2 via VirtIO.\n", 0x00FF00);
    } else {
        tty_print("Failed to send ARP Request via VirtIO.\n", 0xFF0000);
    }
}

void cmd_version() {
    print("Kernel version: ", 0x00FFAA);
    print(KERNEL_VERSION, 0x00FFAA);
    print("\nCommand version: ", 0x00FFAA);
    print(COMMAND_VERSION, 0x00FFAA);
    print("\n", 0x00FFAA);
}

void cmd_lspci() {
    extern void pci_scan_bus(void (*callback)(uint8_t, uint8_t, uint8_t, uint16_t, uint16_t));
    auto print_pci_info = [](uint8_t bus, uint8_t device, uint8_t function, uint16_t vendor_id, uint16_t device_id) {
        const char* vendor = pci_lookup_vendor(vendor_id);
        const char* dev = pci_lookup_device(vendor_id, device_id);
        tty_print("  Vendor: ", 0xAAAAFF); tty_print(vendor, 0xAAAAFF);
        tty_print("  Device: ", 0xAAFFAA); tty_print(dev, 0xAAFFAA);
        tty_print("\n", 0xAAAAAA);
    };
    pci_scan_bus(print_pci_info);
}

// ================== 命令分发 ==================

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
        print_hex(strlen(command), 0xFFFF00);
        print("\n", 0xFFFF00);
        print("[DEBUG] Hex dump: ", 0xFFFF00);
        for (size_t i = 0; i <= strlen(command); i++) {
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
    } else if (strcmp(command, "lspci") == 0) {
        cmd_lspci();
    } else {
        print("\nUnknown command: ", 0xFF6060);
        print(command, 0xFF6060);
        print("\n", 0xFF6060);
    }
}
