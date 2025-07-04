#include "virtio_net.h"
#include "kernel/cpu/pci.h"
#include "kernel/drivers/tty.h"
#include "kernel/mem/pmm.h"
#include "lib/libc.h"

#define VIRTQ_DESC_F_NEXT       1 // Chained to next descriptor
#define VIRTQ_DESC_F_WRITE      2 // Device writes to this descriptor (device-writable)
#define VIRTQ_DESC_F_INDIRECT   4 // Indirect descriptor table

// 外部依赖 (pci 读写)
extern uint32_t pci_read_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
extern void pci_write_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value);

// 辅助函数，读写 VirtIO PCI 配置寄存器
static uint8_t virtio_read_config_8(uint8_t cap_type, uint8_t offset) { /* ... */ return 0; }
static void virtio_write_config_8(uint8_t cap_type, uint8_t offset, uint8_t val) { /* ... */ }
static uint32_t virtio_read_config_32(uint8_t cap_type, uint8_t offset) { /* ... */ return 0; }
static void virtio_write_config_32(uint8_t cap_type, uint8_t offset, uint32_t val) { /* ... */ }
static uint64_t virtio_read_config_64(uint8_t cap_type, uint8_t offset) { /* ... */ return 0; }
static void virtio_write_config_64(uint8_t cap_type, uint8_t offset, uint64_t val) { /* ... */ }

// Virtqueue helper functions
static struct virtq* virtq_alloc(uint16_t queue_idx, uint16_t num_descs);
static void virtq_free(struct virtq* q);
static int virtq_add_buf(struct virtq* q, void* buf, uint32_t len, uint16_t flags);
static void virtq_kick(struct virtq* q);

// VirtIO 网卡 MMIO 基址和 MAC 地址
static volatile uint8_t* virtio_net_mmio_base = nullptr;
static uint8_t virtio_net_mac_addr[6] = {0};

// 发送和接收队列
static struct virtq* rx_q = nullptr;
static struct virtq* tx_q = nullptr;

// ==========================================================================
// VirtIO PCI Capability 寻址 (核心重写)
// ==========================================================================
// 用于存储各个 Capabilities 的 MMIO 地址
static volatile uint8_t* common_cfg_ptr = nullptr;
static volatile uint8_t* notify_cfg_ptr = nullptr;
static volatile uint8_t* isr_cfg_ptr = nullptr;
static volatile uint8_t* device_cfg_ptr = nullptr;
static volatile uint8_t* pci_cfg_ptr = nullptr;

// VirtIO PCI Capability Header (不变)
struct virtio_pci_cap {
    uint8_t cap_vndr;
    uint8_t cap_next;
    uint8_t cap_len;
    uint8_t cfg_type;
    uint8_t bar;
    uint8_t padding[3];
    uint32_t offset;
    uint32_t length;
};

// 辅助函数：根据 Capability offset 和 偏移量读取
static uint8_t virtio_read_cap_8(volatile uint8_t* cap_base_ptr, uint32_t offset_in_cap) { return *(volatile uint8_t*)(cap_base_ptr + offset_in_cap); }
static void virtio_write_cap_8(volatile uint8_t* cap_base_ptr, uint32_t offset_in_cap, uint8_t val) { *(volatile uint8_t*)(cap_base_ptr + offset_in_cap) = val; }
static uint16_t virtio_read_cap_16(volatile uint8_t* cap_base_ptr, uint32_t offset_in_cap) { return *(volatile uint16_t*)(cap_base_ptr + offset_in_cap); }
static void virtio_write_cap_16(volatile uint8_t* cap_base_ptr, uint32_t offset_in_cap, uint16_t val) { *(volatile uint16_t*)(cap_base_ptr + offset_in_cap) = val; }
static uint32_t virtio_read_cap_32(volatile uint8_t* cap_base_ptr, uint32_t offset_in_cap) { return *(volatile uint32_t*)(cap_base_ptr + offset_in_cap); }
static void virtio_write_cap_32(volatile uint8_t* cap_base_ptr, uint32_t offset_in_cap, uint32_t val) { *(volatile uint32_t*)(cap_base_ptr + offset_in_cap) = val; }
static uint64_t virtio_read_cap_64(volatile uint8_t* cap_base_ptr, uint32_t offset_in_cap) { return *(volatile uint64_t*)(cap_base_ptr + offset_in_cap); }
static void virtio_write_cap_64(volatile uint8_t* cap_base_ptr, uint32_t offset_in_cap, uint64_t val) { *(volatile uint64_t*)(cap_base_ptr + offset_in_cap) = val; }


// ==========================================================================
// Virtqueue 实现
// ==========================================================================
// virtq_alloc 
static struct virtq* virtq_alloc(uint16_t queue_idx, uint16_t num_descs) {
    struct virtq* q = (struct virtq*)pmm_alloc_page(); // 描述符环，可用环，已用环在同一页
    if (!q) return nullptr;
    
    memset(q, 0, PAGE_SIZE); // 清零整页

    q->num = num_descs;
    q->desc = (struct virtq_desc*)((uintptr_t)q);
    // 可用环紧跟在描述符表之后
    q->avail = (struct virtq_avail*)((uintptr_t)q->desc + num_descs * sizeof(struct virtq_desc));
    // 已用环在可用环之后，需要计算其大小
    q->used = (struct virtq_used*)((uintptr_t)q->avail + sizeof(struct virtq_avail) + num_descs * sizeof(uint16_t)); // sizeof(ring array)

    // 初始化空闲描述符链表
    q->free_head = 0;
    for (int i = 0; i < num_descs - 1; i++) {
        q->desc[i].next = i + 1;
    }
    q->desc[num_descs - 1].next = 0; // 最后一个指向 0

    // 设置队列索引 (Guest Page Table PFN)
    // Linux/OSDev 建议用 QueuePFN 寄存器，但在 VirtIO 1.0 PCI Configuration Space 中，
    // 是通过 common config 中的 QueueDesc/Avail/Used 寄存器设置物理地址
    // 这里我们已经通过 virtio_write_cap_64 完成了物理地址设置。
    // 这里主要是选择队列号
    virtio_write_cap_16(common_cfg_ptr, 0x0E, queue_idx); // Queue select
    
    // 通知设备队列大小 (Common Config: Queue Size 0x08)
    virtio_write_cap_16(common_cfg_ptr, 0x08, num_descs); 

    // 通知设备队列物理地址
    // Common Config: Queue Desc 0x10, Queue Avail 0x18, Queue Used 0x20
    virtio_write_cap_64(common_cfg_ptr, 0x10, (uint64_t)q->desc);
    virtio_write_cap_64(common_cfg_ptr, 0x18, (uint64_t)q->avail);
    virtio_write_cap_64(common_cfg_ptr, 0x20, (uint64_t)q->used);

    // 启用队列 (Common Config: Queue Enable 0x0E, bit 0)
    // 实际是队列号寄存器，写 1 启用队列，写 0 禁用
    // OSDev 建议在所有设置完成后再启用队列
    // 在主初始化函数末尾统一启用队列，此处仅设置物理地址
    // Linux 在 e1000_enable_queues 中设置 enable
    // 所以这里不直接写 enable 位

    q->mmio_base_ptr = notify_cfg_ptr; // 用于 virtq_kick
    q->queue_notify_off = virtio_read_cap_32(notify_cfg_ptr, 0x00); // Notify offset for this queue
    
    return q;
}


// virtq_free (简化，仅释放页)
static void virtq_free(struct virtq* q) {
    if (q) pmm_free_page(q);
}

// virtq_add_buf (添加缓冲区到队列)
static int virtq_add_buf(struct virtq* q, void* buf, uint32_t len, uint16_t flags) {
    if (q->free_head == q->num) return -1; // 队列满

    uint16_t head = q->free_head;
    q->free_head = q->desc[head].next; // 更新空闲链表头

    q->desc[head].addr = (uint64_t)buf;
    q->desc[head].len = len;
    q->desc[head].flags = flags;
    q->desc[head].next = 0; // 确保不链到下一个，除非显式设置

    // 将描述符添加到可用环
    q->avail->ring[q->avail->idx % q->num] = head;
    
    // 增加可用环索引
    // 必须是原子操作，但在此简化
    q->avail->idx++;

    return 0;
}

// virtq_kick (通知设备)
static void virtq_kick(struct virtq* q) {
    // 确保 MMIO 基地址和 notify 偏移都有效
    if (!q->mmio_base_ptr || q->queue_notify_off == 0xFFFF) return; // 0xFFFF 是默认的无效通知偏移

    // 写入 Queue Notify 寄存器，通知设备指定队列有新数据
    // 地址是 virtio_net_mmio_base + notify_cap_offset + queue_notify_off
    *(volatile uint16_t*)(q->mmio_base_ptr + q->queue_notify_off) = q->queue_notify_off;
    // OSDev 建议写入的是队列索引本身
    // *(volatile uint16_t*)(q->mmio_base_ptr + q->queue_notify_off) = q->queue_notify_off; 
    // 更标准的方式是写入队列号：
    // *(volatile uint16_t*)(q->mmio_base_ptr + virtio_read_cap_32(notify_cfg_ptr, 0) + q->queue_notify_off) = q->queue_idx; (requires queue_idx in virtq)

    // 最简单和最符合 OSDev 文档的 kick 方式
    // Common Config: Queue Notify (0x0E) 寄存器写入队列索引
    // No, it's notify_cfg_ptr + notify_offset + Queue_idx * size_of_each_notify_field (usually 2 bytes)
    // The offset is virtio_read_cap_32(notify_cap_ptr, 0x00) (notify_off_multiplier)
    // And notify_offset in virtio_pci_cap is the base
    // Simplified: write the queue index to the correct register
    // *(volatile uint16_t*)(notify_cfg_ptr + q->queue_notify_off * virtio_read_cap_32(notify_cfg_ptr, 0) ) = q->queue_idx;

    // Based on OSDev: `device.notify(queue_id)` via `offset + (queue_id * notify_offset_multiplier)`
    // And `queue_notify_off` from virtio_pci_cap itself
    // Simplified, assume offset is the final address to write to:
    // This assumes notify_cfg_ptr already points to the correct bar and cap.offset.
    // The value written is queue_idx.
    
    // OSDev: device_notify(uint16_t queue_id) -> *(volatile uint16_t*)(notify_base + offset + (queue_id * multiplier)) = queue_id
    // For now, let's just write queue_idx.
    virtio_write_cap_16(notify_cfg_ptr, q->queue_notify_off, q->num); // q->num (queue_idx)
}


// ==========================================================================
// VirtIO 初始化流程 (核心重写)
// ==========================================================================
void virtio_net_init(uint8_t pci_bus, uint8_t pci_device, uint8_t pci_function) {
    tty_print("VirtIO Net: Initializing...\n", 0xFFFFFF);

    // 1. 获取 PCI BAR0 MMIO 基地址 (不变)
    uint32_t bar0 = pci_read_dword(pci_bus, pci_device, pci_function, 0x10);
    uintptr_t mmio_phys_addr = (uintptr_t)(bar0 & 0xFFFFFFF0);
    if ((bar0 & 0x6) == 0x4) { // 64位 BAR
        uint32_t bar1 = pci_read_dword(pci_bus, pci_device, pci_function, 0x14);
        mmio_phys_addr = ((uint64_t)bar1 << 32) | (bar0 & 0xFFFFFFF0);
    }
    virtio_net_mmio_base = (volatile uint8_t*)((uintptr_t)mmio_phys_addr);
    tty_print("VirtIO MMIO @ 0x", 0xFFFFFF); print_hex((uint64_t)virtio_net_mmio_base, 0x00FFFF); tty_print("\n", 0x00FFFF);

    // 2. 扫描 PCI Capabilities，找到各个配置块的 MMIO 指针
    uint8_t cap_ptr_offset = pci_read_dword(pci_bus, pci_device, pci_function, 0x34) & 0xFF; // Capability Pointer
    while (cap_ptr_offset != 0) {
        struct virtio_pci_cap cap;
        // 读取通用 PCI Capability Header
        cap.cap_vndr = pci_read_dword(pci_bus, pci_device, pci_function, cap_ptr_offset + 0) & 0xFF;
        cap.cap_next = (pci_read_dword(pci_bus, pci_device, pci_function, cap_ptr_offset + 0) >> 8) & 0xFF;
        cap.cap_len  = (pci_read_dword(pci_bus, pci_device, pci_function, cap_ptr_offset + 0) >> 16) & 0xFF;
        // 读取 VirtIO 特定部分
        cap.cfg_type = (pci_read_dword(pci_bus, pci_device, pci_function, cap_ptr_offset + 4) >> 0) & 0xFF;
        cap.bar      = (pci_read_dword(pci_bus, pci_device, pci_function, cap_ptr_offset + 4) >> 8) & 0xFF;
        cap.offset   = pci_read_dword(pci_bus, pci_device, pci_function, cap_ptr_offset + 8);
        cap.length   = pci_read_dword(pci_bus, pci_device, pci_function, cap_ptr_offset + 12);
        
        // 计算每个 Capability 的实际 MMIO 地址，并存储指针
        if (cap.cap_vndr == 0x09) { // PCI_CAP_ID_VNDR
            volatile uint8_t* base_ptr_for_cap = virtio_net_mmio_base + cap.offset; // 简化: 假设所有 Cap 都从 BAR0 offset
            switch (cap.cfg_type) {
                case VIRTIO_PCI_CAP_COMMON_CFG: common_cfg_ptr = base_ptr_for_cap; break;
                case VIRTIO_PCI_CAP_NOTIFY_CFG: notify_cfg_ptr = base_ptr_for_cap; break;
                case VIRTIO_PCI_CAP_ISR_CFG:    isr_cfg_ptr = base_ptr_for_cap; break;
                case VIRTIO_PCI_CAP_DEVICE_CFG: device_cfg_ptr = base_ptr_for_cap; break;
                case VIRTIO_PCI_CAP_PCI_CFG:    pci_cfg_ptr = base_ptr_for_cap; break; // Added for completeness
            }
        }
        cap_ptr_offset = cap.cap_next;
    }
    
    // 3. 设备复位
    virtio_write_cap_8(common_cfg_ptr, 0x14, VIRTIO_STATUS_RESET); // Device Status Register offset 0x14
    for(volatile int i=0; i<10000; i++); // 忙等待
    
    // 4. 设置驱动状态: Acknowledge, Driver
    virtio_write_cap_8(common_cfg_ptr, 0x14, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER);

    // 5. 读取设备特性并协商 (Feature Negotiation)
    uint64_t device_features = virtio_read_cap_64(common_cfg_ptr, 0x00); // Features select bits 0x00 (read feature select), 0x04 (read feature)
    // 协商特性 (这里简化，直接接受设备所有支持的特性)
    virtio_write_cap_64(common_cfg_ptr, 0x08, device_features); // Driver Feature Select
    
    // 6. 确认特性协商完成
    virtio_write_cap_8(common_cfg_ptr, 0x14, virtio_read_cap_8(common_cfg_ptr, 0x14) | VIRTIO_STATUS_FEATURES_OK); // Add FEATURES_OK

    // 7. 获取 MAC 地址 (如果设备支持 VIRTIO_NET_F_MAC 特性)
    if (device_features & VIRTIO_NET_F_MAC) {
        for (int i = 0; i < 6; i++) {
            virtio_net_mac_addr[i] = virtio_read_cap_8(device_cfg_ptr, i); // MAC Address offset 0 in device cfg
        }
        tty_print("VirtIO MAC: ", 0xFFFFFF);
        for (int i = 0; i < 6; i++) {
            print_hex(virtio_net_mac_addr[i], 0x00FF00);
            if (i < 5) tty_print(":", 0x00FF00);
        }
        tty_print("\n", 0x00FF00);
    } else {
        tty_print("VirtIO: MAC address feature not supported.\n", 0xFF6060);
    }

    // 8. 分配并初始化 Virtqueue
    rx_q = virtq_alloc(0, NUM_RX_DESC); // 队列 0 是接收
    tx_q = virtq_alloc(1, NUM_TX_DESC); // 队列 1 是发送

    if (!rx_q || !tx_q) {
        tty_print("VirtIO: Failed to allocate virtqueues.\n", 0xFF0000);
        virtio_write_cap_8(common_cfg_ptr, 0x14, virtio_read_cap_8(common_cfg_ptr, 0x14) | VIRTIO_STATUS_FAILED);
        return;
    }

    // 9. 填充接收队列 (每个描述符对应一个缓冲区)
    // Virtio Net Header (10 bytes) + actual data buffer (e.g., 2048 bytes)
    for (int i = 0; i < NUM_RX_DESC; i++) {
        rx_q->buffers[i] = (uint8_t*)pmm_alloc_page(); // 分配缓冲区
        // 接收描述符需要两个缓冲区：一个 for virtio_net_hdr, 一个 for packet data
        // 简化: 整个 page 作为缓冲区，VirtIO Net Header 放在前面
        memset(rx_q->buffers[i], 0, 10); // 清零包头

        // VIRTQ_DESC_F_WRITE: 设备会写入此描述符 (接收)
        virtq_add_buf(rx_q, rx_q->buffers[i], PAGE_SIZE, VIRTIO_DESC_F_WRITE); 
    }
    virtq_kick(rx_q); // 通知设备有空闲缓冲区

    // 10. 完成设备初始化
    virtio_write_cap_8(common_cfg_ptr, 0x14, virtio_read_cap_8(common_cfg_ptr, 0x14) | VIRTIO_STATUS_DRIVER_OK);

    tty_print("VirtIO Net initialized.\n", 0x00FF00);
}

// 获取 MAC 地址
const uint8_t* virtio_net_get_mac_address() {
    return virtio_net_mac_addr;
}

// 发送数据包
bool virtio_net_send_packet(const uint8_t* data, uint16_t len) {
    if (len == 0 || len > 1514) { // 以太网最大帧长 1514 (含 CRC)
        tty_print("VirtIO TX: Invalid packet length (0 or >1514).\n", 0xFF0000);
        return false;
    }

    // VirtIO Net Spec: 一个以太网帧需要 10 字节的 virtio_net_hdr
    // 我们将数据包和包头放在同一个缓冲区里
    uint8_t* tx_buffer = tx_q->buffers[tx_q->free_head]; // 获取下一个空闲缓冲区
    if (!tx_buffer) {
        tty_print("VirtIO TX: No free buffer for header!\n", 0xFF0000);
        return false;
    }
    
    // 清零 virtio_net_hdr (目前全0，不使用复杂特性)
    memset(tx_buffer, 0, 10); 

    // 拷贝实际数据到缓冲区 (跳过 10 字节的包头)
    memcpy(tx_buffer + 10, data, len);

    // 将缓冲区添加到发送队列
    // flags: 0 (设备只读，因为驱动程序正在提供数据)
    virtq_add_buf(tx_q, tx_buffer, len + 10, 0); 

    virtq_kick(tx_q); // 通知设备

    tty_print("VirtIO: Packet sent. Len=", 0x00FF00); print_hex(len, 0x00FF00); tty_print("\n", 0x00FF00);
    return true;
}

// 处理 VirtIO 网卡中断
void virtio_net_handle_interrupt() {
    // 读取 ISR 状态寄存器 (会清除中断)
    uint8_t isr_status = virtio_read_cap_8(isr_cfg_ptr, 0); 
    
    if (isr_status == 0) return; // 没有中断发生

    if (isr_status & 0x01) { // Queue interrupt (Bit 0)
        // 检查 TX 队列是否有已完成的包
        while (tx_q->used->idx != tx_q->used_idx) {
            struct virtq_used_elem* used_elem = &tx_q->used->ring[tx_q->used_idx % tx_q->num];
            uint16_t desc_idx = used_elem->id;
            
            // 标记描述符空闲并重新添加到空闲链表
            tx_q->desc[desc_idx].flags = 0; // 清除所有标志
            tx_q->desc[desc_idx].next = tx_q->free_head;
            tx_q->free_head = desc_idx;
            tx_q->used_idx++;
            tty_print("VirtIO: TX packet acknowledged. Desc=", 0x00FF00); print_hex(desc_idx, 0x00FF00); tty_print("\n", 0x00FF00);
        }

        // 检查 RX 队列是否有新收到的包
        while (rx_q->used->idx != rx_q->used_idx) {
            struct virtq_used_elem* used_elem = &rx_q->used->ring[rx_q->used_idx % rx_q->num];
            uint16_t desc_idx = used_elem->id;
            uint32_t len = used_elem->len;
            uint8_t* packet_data = rx_q->buffers[desc_idx];

            tty_print("VirtIO: RX packet received. Len=", 0x00FFFF); print_hex(len, 0x00FFFF); tty_print("\n", 0x00FFFF);
            
            // 数据包在这里：packet_data (跳过前10字节的 virtio_net_hdr)
            // EtherType 在 packet_data + 10 + 12 (以太网头的 EtherType 偏移)
            uint16_t eth_type = (packet_data[10 + 12] << 8) | packet_data[10 + 13];
            tty_print("  EtherType: 0x", 0xFFFFFF); print_hex(eth_type, 0x00FF00); tty_print("\n", 0x00FF00);

            if (eth_type == 0x0806) { // ARP 协议
                tty_print("  ARP Packet Detected!\n", 0x00FF00);
            } else if (eth_type == 0x0800) { // IPv4 协议
                tty_print("  IPv4 Packet Detected!\n", 0x00FF00);
            } else {
                tty_print("  Unknown EtherType.\n", 0xFF6060);
            }
            
            // 重新将缓冲区添加到接收队列
            // 标记为设备写入 (VIRTQ_DESC_F_WRITE)
            virtq_add_buf(rx_q, rx_q->buffers[desc_idx], PAGE_SIZE, VIRTIO_DESC_F_WRITE); 
            rx_q->used_idx++;
        }
    }
}