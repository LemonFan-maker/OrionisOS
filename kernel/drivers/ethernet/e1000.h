#pragma once
#include <stdint.h>

// E1000 寄存器偏移 (部分常用)
#define E1000_REG_CTRL      0x00000 // Device Control
#define E1000_REG_STATUS    0x00008 // Device Status
#define E1000_REG_EEPROM    0x00014 // EEPROM/FLASH Read/Write
#define E1000_REG_ICR       0x000C0 // Interrupt Cause Read
#define E1000_REG_IMS       0x000D0 // Interrupt Mask Set/Read
#define E1000_REG_RDMTS     0x02820 // Receive Descriptor Minimum Threshold Size
#define E1000_REG_RCTL      0x00100 // Receive Control
#define E1000_REG_TCTL      0x00400 // Transmit Control
#define E1000_REG_TIPG      0x00410 // Transmit IPG
#define E1000_REG_RDBAL     0x02800 // Receive Descriptor Base Address Low
#define E1000_REG_RDBAH     0x02804 // Receive Descriptor Base Address High
#define E1000_REG_RDLEN     0x02808 // Receive Descriptor Length
#define E1000_REG_RDH       0x02810 // Receive Descriptor Head
#define E1000_REG_RDT       0x02818 // Receive Descriptor Tail
#define E1000_REG_TDBAL     0x03800 // Transmit Descriptor Base Address Low
#define E1000_REG_TDBAH     0x03804 // Transmit Descriptor Base Address High
#define E1000_REG_TDLEN     0x03808 // Transmit Descriptor Length
#define E1000_REG_TDH       0x03810 // Transmit Descriptor Head
#define E1000_REG_TDT       0x03818 // Transmit Descriptor Tail
#define E1000_REG_MTA       0x05200 // Multicast Table Array
#define E1000_REG_RA        0x05400 // Receive Address Register (for MAC)

// E1000 描述符结构
struct e1000_rx_desc {
    uint64_t addr; // 接收缓冲区物理地址
    uint16_t length;
    uint16_t checksum;
    uint8_t  status;
    uint8_t  errors;
    uint16_t special;
} __attribute__((packed));

struct e1000_tx_desc {
    uint64_t addr; // 发送缓冲区物理地址
    uint16_t length;
    uint8_t  cso;
    uint8_t  cmd;
    uint8_t  status;
    uint8_t  css;
    uint16_t special;
} __attribute__((packed));

// 初始化 E1000 网卡
void e1000_init(uint8_t pci_bus, uint8_t pci_device, uint8_t pci_function);

// --- 新增：发送数据包 ---
bool e1000_send_packet(const uint8_t* data, uint16_t len);

// --- 新增：处理网卡中断 ---
void e1000_handle_interrupt();

// 获取网卡MAC地址
const uint8_t* e1000_get_mac_address();
