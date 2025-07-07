#include "pci.h"
#include "ports.h" // 需要 inb/outb/inl/outl
#include "kernel/drivers/tty.h" // 需要 tty_print/print_hex

// 辅助函数：生成 PCI 配置地址
static uint32_t pci_get_address(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    return (uint32_t)((bus << 16) | (device << 11) | (function << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
}

// 读取 PCI 配置空间中的 DWORD
uint32_t pci_read_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    outl(PCI_CONFIG_ADDRESS, pci_get_address(bus, device, function, offset));
    return inl(PCI_CONFIG_DATA);
}

// 辅助函数：根据 Class Code 打印设备类型
void pci_print_device_type(uint8_t class_code, uint8_t subclass_code, uint8_t prog_if) {
    uint32_t default_color = 0xFFFFFF; // 白色
    uint32_t network_color = 0x00FFFF; // 青色
    tty_print("Class: ", default_color); print_hex(class_code, default_color); tty_print(" Sub: ", default_color); print_hex(subclass_code, default_color); tty_print(" PI: ", default_color); print_hex(prog_if, default_color);
    tty_print(" (", default_color);
    if (class_code == PCI_CLASS_NETWORK_CONTROLLER) {
        tty_print("Network Controller", network_color);
    } else if (class_code == PCI_CLASS_STORAGE_CONTROLLER) {
        tty_print("Storage Controller", network_color);
    } else if (class_code == PCI_CLASS_DISPLAY_CONTROLLER) {
        tty_print("Display Controller", network_color);
    } else if (class_code == PCI_CLASS_BRIDGE_DEVICE) {
        tty_print("Bridge Device", network_color);
    } else {
        tty_print("Unknown", network_color);
    }
    tty_print(")\n", default_color);
}

// 扫描单个 PCI 功能
static void pci_check_function(uint8_t bus, uint8_t device, uint8_t function, void (*callback)(uint8_t bus, uint8_t device, uint8_t function, uint16_t vendor_id, uint16_t device_id)) {
    uint32_t attr_header_color = 0xFFD700; // 金色
    uint32_t attr_info_color = 0xFFFFFF; // 白色
    uint32_t dword0 = pci_read_dword(bus, device, function, 0);
    uint16_t vendor_id = dword0 & 0xFFFF;
    if (vendor_id == 0xFFFF) return; // 设备不存在

    uint16_t device_id = (dword0 >> 16) & 0xFFFF;

    uint32_t dword2 = pci_read_dword(bus, device, function, 0x08); // Class Code and Revision ID
    uint8_t class_code = (dword2 >> 24) & 0xFF;
    uint8_t subclass_code = (dword2 >> 16) & 0xFF;
    uint8_t prog_if = (dword2 >> 8) & 0xFF;

    tty_print("PCI Device Found: Bus ", attr_header_color); print_hex(bus, attr_header_color);
    tty_print(" Dev ", attr_header_color); print_hex(device, attr_header_color);
    tty_print(" Func ", attr_header_color); print_hex(function, attr_header_color);
    tty_print(" VendorID: ", attr_header_color); print_hex(vendor_id, attr_header_color);
    tty_print(" DeviceID: ", attr_header_color); print_hex(device_id, attr_header_color);
    tty_print("\n", 0x0E);
    pci_print_device_type(class_code, subclass_code, prog_if);

    if (callback) {
        callback(bus, device, function, vendor_id, device_id);
    }
}

// 扫描单个 PCI 设备 (可能包含多个功能)
static void pci_check_device(uint8_t bus, uint8_t device, void (*callback)(uint8_t bus, uint8_t device, uint8_t function, uint16_t vendor_id, uint16_t device_id)) {
    uint32_t dword0 = pci_read_dword(bus, device, 0, 0);
    uint16_t vendor_id = dword0 & 0xFFFF;
    if (vendor_id == 0xFFFF) return; // 设备不存在

    uint32_t header_type_dword = pci_read_dword(bus, device, 0, 0x0C); // Header Type
    uint8_t header_type = (header_type_dword >> 16) & 0xFF;

    if (header_type & 0x80) { // 多功能设备
        for (uint8_t function = 0; function < 8; function++) {
            pci_check_function(bus, device, function, callback);
        }
    } else { // 单功能设备
        pci_check_function(bus, device, 0, callback);
    }
}

// 扫描所有 PCI 总线
void pci_scan_bus(void (*callback)(uint8_t bus, uint8_t device, uint8_t function, uint16_t vendor_id, uint16_t device_id)) {
    tty_print("Scanning PCI Bus...\n", 0xFFFFFF); // 白色
    for (uint8_t bus = 0; bus < 255; bus++) { // 最多 256 条总线
        for (uint8_t device = 0; device < 32; device++) { // 每条总线最多 32 个设备
            pci_check_device(bus, device, callback);
        }
    }
    tty_print("PCI Scan Complete.\n", 0xFFFFFF); // 白色
}

void pci_write_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value) {
    outl(PCI_CONFIG_ADDRESS, pci_get_address(bus, device, function, offset));
    outl(PCI_CONFIG_DATA, value);
}

void pci_init() {
    // 不需要额外的初始化，直接调用 pci_scan_bus 即可
}

