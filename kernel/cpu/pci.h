#pragma once
#include <stdint.h>

// PCI 配置空间 I/O 端口
#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

// PCI header type
#define PCI_HEADER_TYPE_NORMAL_DEVICE 0x00
#define PCI_HEADER_TYPE_PCI_BRIDGE    0x01
#define PCI_HEADER_TYPE_CARDBUS_BRIDGE 0x02

// PCI base class codes (部分常用)
#define PCI_CLASS_OLD_DEVICE       0x00
#define PCI_CLASS_NETWORK_CONTROLLER 0x02
#define PCI_CLASS_STORAGE_CONTROLLER 0x01
#define PCI_CLASS_DISPLAY_CONTROLLER 0x03
#define PCI_CLASS_BRIDGE_DEVICE      0x06

// PCI 函数声明
void pci_init(); // 初始化 PCI 扫描
void pci_scan_bus(void (*callback)(uint8_t bus, uint8_t device, uint8_t function, uint16_t vendor_id, uint16_t device_id));

// 读取 PCI 配置空间中的 DWORD (32位)
uint32_t pci_read_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);

// 辅助函数：根据 Class Code 打印设备类型
void pci_print_device_type(uint8_t class_code, uint8_t subclass_code, uint8_t prog_if);

uint32_t pci_read_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
// 写入 PCI 配置空间中的 DWORD (32位)
void pci_write_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value);
