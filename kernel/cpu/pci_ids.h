#pragma once
#include <stdint.h>

struct pci_vendor {
    uint16_t id;
    const char* name;
};

struct pci_device {
    uint16_t vendor_id;
    uint16_t device_id;
    const char* name;
};

static const pci_vendor pci_vendors[] = {
    {0x8086, "Intel"},
    {0x10DE, "NVIDIA"},
    {0x1022, "AMD"},
    {0x1AF4, "Virtio"},
    {0x1234, "QEMU"},
    {0x1B36, "Red Hat"},
    {0xFFFF, "Unknown"}
};

static const pci_device pci_devices[] = {
    // 现有设备
    {0x8086, 0x100E, "Intel 82540EM Gigabit Ethernet"},
    {0x8086, 0x2922, "Intel ICH9 SATA AHCI Controller"},
    {0x10DE, 0x1CB3, "NVIDIA GeForce GTX 1050"},
    {0x1AF4, 0x1000, "Virtio Network"},
    {0x1AF4, 0x1041, "Virtio Block"},
    {0x1234, 0x1111, "QEMU VGA"},
    {0x1B36, 0x0008, "Red Hat QXL VGA"},
    
    // 新增 QEMU 标准设备
    {0x1234, 0x1111, "QEMU Standard VGA"}, // 已存在，确认保留
    {0x1B36, 0x0001, "Red Hat Virtio Balloon"},
    {0x1B36, 0x1000, "Red Hat Virtio Network"},
    {0x1B36, 0x1001, "Red Hat Virtio Block"},
    {0x1B36, 0x1002, "Red Hat Virtio Console"},
    {0x1B36, 0x1003, "Red Hat Virtio RNG"},
    {0x1B36, 0x1004, "Red Hat Virtio Entropy"},
    {0x1B36, 0x1005, "Red Hat Virtio SCSI"},
    {0x1B36, 0x1009, "Red Hat Virtio Filesystem"},
    {0x1B36, 0x1050, "Red Hat QXL GPU"},

    // 新增 Intel 设备 (来自您的 lspci 输出)
    {0x8086, 0x1237, "Intel 440FX Host Bridge"},    // QEMU 标准主机桥
    {0x8086, 0x7000, "Intel 82371SB PIIX3 ISA Bridge"}, // ISA 桥接器
    {0x8086, 0x7010, "Intel 82371SB PIIX3 IDE Controller"}, // IDE 控制器
    {0x8086, 0x7113, "Intel 82371AB PIIX4 ACPI Controller"}, // ACPI 控制器
    
    // 常见 Intel 设备扩展
    {0x8086, 0x7190, "Intel 440BX/ZX Host Bridge"},
    {0x8086, 0x7192, "Intel 440BX/ZX AGP Bridge"},
    {0x8086, 0x2415, "Intel 82801AA AC'97 Audio"},
    {0x8086, 0x2829, "Intel ICH9 SATA Controller"},
    {0x8086, 0x2930, "Intel ICH9 USB Controller"},
    {0x8086, 0x29C0, "Intel Q35 Host Bridge"},
    {0x8086, 0x29B0, "Intel Q35 I/O Controller"},
    
    // 结束标记
    {0xFFFF, 0xFFFF, "Unknown Device"}
};

const char* pci_lookup_device(uint16_t vendor_id, uint16_t device_id);
const char* pci_lookup_vendor(uint16_t vendor_id);