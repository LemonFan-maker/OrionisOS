#pragma once
#include <stdint.h>
#include <stddef.h>

// VirtIO PCI 配置空间偏移
#define VIRTIO_PCI_CAP_COMMON_CFG 1 // Common configuration (virtio 1.0)
#define VIRTIO_PCI_CAP_NOTIFY_CFG 2 // Notification (virtio 1.0)
#define VIRTIO_PCI_CAP_ISR_CFG    3 // ISR Status (virtio 1.0)
#define VIRTIO_PCI_CAP_DEVICE_CFG 4 // Device specific configuration (virtio 1.0)
#define VIRTIO_PCI_CAP_PCI_CFG    5 // PCI configuration (virtio 1.0)

// VirtIO 状态寄存器位
#define VIRTIO_STATUS_RESET     0x00
#define VIRTIO_STATUS_ACKNOWLEDGE 0x01
#define VIRTIO_STATUS_DRIVER    0x02
#define VIRTIO_STATUS_DRIVER_OK 0x04
#define VIRTIO_STATUS_FEATURES_OK 0x08
#define VIRTIO_STATUS_DEVICE_NEEDS_RESET 0x40
#define VIRTIO_STATUS_FAILED    0x80

// VirtIO NET 设备特性 (Feature Bits)
#define VIRTIO_NET_F_CSUM      (1 << 0) // Host can do checksums in hardware.
#define VIRTIO_NET_F_MAC       (1 << 5) // Device has given MAC address.
#define NUM_RX_DESC 32
#define NUM_TX_DESC 32

#define VIRTQ_DESC_F_NEXT       1 // Chained to next descriptor
#define VIRTQ_DESC_F_WRITE      2 // Device writes to this descriptor (device-writable)
#define VIRTQ_DESC_F_INDIRECT   4 // Indirect descriptor table

// VirtIO 描述符 (Descriptor)
struct virtq_desc {
    uint64_t addr; // 缓冲区物理地址
    uint32_t len;  // 缓冲区长度
    uint16_t flags; // 标志位
    uint16_t next; // 下一个描述符的索引 (如果 flags 设置了 VIRTQ_DESC_F_NEXT)
};

// VirtIO 可用环 (Available Ring)
struct virtq_avail {
    uint16_t flags;
    uint16_t idx;  // 驱动程序更新的索引
    uint16_t ring[]; // 描述符索引数组
};

// VirtIO 已用环 (Used Ring)
struct virtq_used_elem {
    uint32_t id; // 描述符链的头部索引
    uint32_t len; // 描述符链的总长度
};
struct virtq_used {
    uint16_t flags;
    uint16_t idx;  // 设备更新的索引
    struct virtq_used_elem ring[]; // 已用描述符数组
};

// VirtIO 队列结构体 (新增 queue_notify_off 和 mmio_base_ptr)
struct virtq {
    struct virtq_desc  *desc;
    struct virtq_avail *avail;
    struct virtq_used  *used;
    uint16_t num;
    uint16_t queue_idx;
    uint16_t free_head;
    uint8_t *buffers[0];
    volatile uint8_t* mmio_base_ptr;
    uint32_t queue_notify_off;
    // --- 新增：用于跟踪已用环进度 ---
    uint16_t used_idx; 
};

// 初始化 virtio 网卡
void virtio_net_init(uint8_t pci_bus, uint8_t pci_device, uint8_t pci_function);

// 发送数据包
bool virtio_net_send_packet(const uint8_t* data, uint16_t len);

// 处理 virtio 网卡中断
void virtio_net_handle_interrupt();

// 获取 MAC 地址
const uint8_t* virtio_net_get_mac_address();