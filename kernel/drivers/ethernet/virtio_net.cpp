#include "virtio_net.h"
#include "kernel/cpu/pci.h"

#include "kernel/drivers/tty.h"
#include "kernel/mem/pmm.h"
#include "kernel/cpu/pic.h"
#include "lib/libc.h"
#include "timer.h"

static uintptr_t pci_bars[6] = {0}; 
static uint8_t virtio_net_irq = 0;
static uint32_t virtio_notify_multiplier = 0;

// 外部依赖 (pci 读写)
extern uint32_t pci_read_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
extern void pci_write_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value);

// 辅助函数，读写 VirtIO PCI 配置寄存器
static uint8_t virtio_read_cap_8(volatile uint8_t* cap_base_ptr, uint32_t offset_in_cap);
static void virtio_write_cap_8(volatile uint8_t* cap_base_ptr, uint32_t offset_in_cap, uint8_t val);
static uint16_t virtio_read_cap_16(volatile uint8_t* cap_base_ptr, uint32_t offset_in_cap);
static void virtio_write_cap_16(volatile uint8_t* cap_base_ptr, uint32_t offset_in_cap, uint16_t val);
static uint32_t virtio_read_cap_32(volatile uint8_t* cap_base_ptr, uint32_t offset_in_cap);
static void virtio_write_cap_32(volatile uint8_t* cap_base_ptr, uint32_t offset_in_cap, uint32_t val);
static uint64_t virtio_read_cap_64(volatile uint8_t* cap_base_ptr, uint32_t offset_in_cap);
static void virtio_write_cap_64(volatile uint8_t* cap_base_ptr, uint32_t offset_in_cap, uint64_t val);

// Virtqueue helper functions
static struct virtq* virtq_alloc(uint16_t q_idx, uint16_t num_descs);
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

static volatile uint8_t* get_cap_mmio_ptr(uint8_t cap_type, uint32_t offset_in_cap) {
    volatile uint8_t* cap_base_ptr = nullptr;
    switch(cap_type) {
        case VIRTIO_PCI_CAP_COMMON_CFG: cap_base_ptr = common_cfg_ptr; break;
        case VIRTIO_PCI_CAP_NOTIFY_CFG: cap_base_ptr = notify_cfg_ptr; break;
        case VIRTIO_PCI_CAP_ISR_CFG:    cap_base_ptr = isr_cfg_ptr; break;
        case VIRTIO_PCI_CAP_DEVICE_CFG: cap_base_ptr = device_cfg_ptr; break;
        case VIRTIO_PCI_CAP_PCI_CFG:    cap_base_ptr = pci_cfg_ptr; break;
        default: return nullptr;
    }
    if (!cap_base_ptr) return nullptr;

    return (volatile uint8_t*)cap_base_ptr + offset_in_cap;
}


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
// virtq_alloc
// ==========================================================================
static struct virtq* virtq_alloc(uint16_t q_idx, uint16_t num_descs) {
    // --- 0. 为 virtq 结构体本身分配内存 ---
    // 如果你有 kmalloc，用它更好。如果没有，用 pmm_alloc_page() 也可以，只是有点浪费。
    struct virtq* q = (struct virtq*)pmm_alloc_page(); // 假设用页分配器
    if (!q) return nullptr;
    memset(q, 0, sizeof(struct virtq)); // 只清零结构体本身的大小

    // --- 1. **核心修改**：为 buffers 指针数组单独分配内存 ---
    // 同样，用 kmalloc 更好。这里我们再分配一个页来存放它。
    q->buffers = (uint8_t**)pmm_alloc_page();
    if (!q->buffers) {
        pmm_free_page(q);
        return nullptr;
    }
    memset(q->buffers, 0, num_descs * sizeof(uint8_t*)); // 清零指针数组

    // --- 2. 为描述符环、可用环、已用环分配内存 (现在它们需要自己的页) ---
    q->desc = (struct virtq_desc*)pmm_alloc_page();
    if (!q->desc) { /* ... 错误处理，释放已分配的内存 ... */ return nullptr; }
    
    // 计算可用环和已用环的地址，它们可以共享一页
    uintptr_t ring_page = (uintptr_t)pmm_alloc_page();
    if (!ring_page) { /* ... 错误处理 ... */ return nullptr; }
    q->avail = (struct virtq_avail*)ring_page;
    q->used = (struct virtq_used*)(ring_page + 1024); // 简单地将它们放在一页的不同位置

    // 清零这些新分配的页
    memset(q->desc, 0, PAGE_SIZE);
    memset((void*)ring_page, 0, PAGE_SIZE);

    // --- 3. 初始化结构体和队列 (其余逻辑不变) ---
    q->num = num_descs;
    q->queue_idx = q_idx;
    // ... (初始化 free_head, next 指针等) ...
    for (int i = 0; i < num_descs - 1; i++) {
        q->desc[i].next = i + 1;
    }

    // --- 1. 选择队列 ---
    virtio_write_cap_16(common_cfg_ptr, 0x16 /* queue_select */, q_idx);

    // --- 2. 检查队列是否已被启用 (正确检查点) ---
    if (virtio_read_cap_16(common_cfg_ptr, 0x1C /* queue_enable */)) {
        tty_print("VirtIO FATAL: Queue #", 0xFF0000); print_hex(q_idx, 0xFF0000);
        tty_print(" is already enabled before setup!\n", 0xFF0000);
        pmm_free_page(q);
        return nullptr;
    }

    // --- 3. 协商队列大小 ---
    uint16_t max_size = virtio_read_cap_16(common_cfg_ptr, 0x18 /* queue_size (read as max_size) */);
    if (num_descs > max_size && max_size > 0) {
        num_descs = max_size; // 自动使用设备支持的最大值
        q->num = num_descs;
    }

    // --- 4. 【关键】设置最终使用的队列大小 ---
    virtio_write_cap_16(common_cfg_ptr, 0x18 /* queue_size (write as final size) */, num_descs);

    // --- 5. 【关键】设置队列的物理地址 ---
    virtio_write_cap_64(common_cfg_ptr, 0x20 /* queue_desc */, (uint64_t)q->desc);
    virtio_write_cap_64(common_cfg_ptr, 0x28 /* queue_driver */, (uint64_t)q->avail);
    virtio_write_cap_64(common_cfg_ptr, 0x30 /* queue_device */, (uint64_t)q->used);

    // --- 6. 获取通知信息 ---
    q->queue_notify_off = virtio_read_cap_16(common_cfg_ptr, 0x1E /* queue_notify_off */);
    q->mmio_base_ptr = notify_cfg_ptr;
    q->notify_off_multiplier = virtio_notify_multiplier; 

    tty_print("VirtIO: Successfully allocated Queue #", 0x00FF00); print_hex(q_idx, 0x00FF00); tty_print("\n", 0x00FF00);
    return q;
}

// virtq_free (简化，仅释放页)
static void virtq_free(struct virtq* q) {
    if (q) pmm_free_page(q);
}

// virtq_add_buf (添加缓冲区到队列)
static int virtq_add_buf(struct virtq* q, void* buf, uint32_t len, uint16_t flags) {
    // --- 1. 从空闲链表中获取一个描述符 ---
    uint16_t head_idx = q->free_head;

    // --- 2. 填充描述符 ---
    struct virtq_desc* desc = &q->desc[head_idx];
    desc->addr = (uint64_t)buf;
    desc->len = len;
    desc->flags = flags;

    // --- 3. 更新空闲链表头，指向下一个空闲描述符 ---
    q->free_head = desc->next;

    // --- 4. 将这个描述符的索引添加到可用环中 ---
    q->avail->ring[q->avail->idx % q->num] = head_idx;
    
    // --- 5. 增加可用环的索引，让设备看到这个新添加的条目 ---
    q->avail->idx++;

    return 0;
}

// virtq_kick (通知设备)
// static void virtq_kick(struct virtq* q) {
//     // 确保 MMIO 基地址和 notify 偏移都有效
//     if (!q->mmio_base_ptr || q->queue_notify_off == 0xFFFF) return; // 0xFFFF 是默认的无效通知偏移
//     // if (!notify_cfg_ptr) return;

//     // 写入 Queue Notify 寄存器，通知设备指定队列有新数据
//     // 地址是 virtio_net_mmio_base + notify_cap_offset + queue_notify_off
//     *(volatile uint16_t*)(q->mmio_base_ptr + q->queue_notify_off) = q->queue_notify_off;
//     // OSDev 建议写入的是队列索引本身
//     // *(volatile uint16_t*)(q->mmio_base_ptr + q->queue_notify_off) = q->queue_notify_off; 
//     // 更标准的方式是写入队列号：
//     // *(volatile uint16_t*)(q->mmio_base_ptr + virtio_read_cap_32(notify_cfg_ptr, 0) + q->queue_notify_off) = q->queue_idx; (requires queue_idx in virtq)

//     virtio_write_cap_16(notify_cfg_ptr, q->queue_notify_off, q->queue_idx); // q->num (queue_idx)
// };

static void virtq_kick(struct virtq* q) {
    // --- 核心修正：使用乘数计算正确的字节偏移量 ---
    uint32_t final_byte_offset = q->queue_notify_off * q->notify_off_multiplier;

    // (可以保留之前的日志用于验证)
    tty_print("VirtIO Kick: Notifying Queue #", 0x00FFFF);
    print_hex(q->queue_idx, 0x00FFFF);
    tty_print(" at MMIO address 0x", 0x00FFFF);
    print_hex((uint64_t)(q->mmio_base_ptr + final_byte_offset), 0x00FFFF);
    tty_print("\n", 0x00FFFF);
    
    // 向正确计算出的地址写入队列索引
    virtio_write_cap_16(notify_cfg_ptr, final_byte_offset, q->queue_idx);
};


static inline void io_delay_us(uint32_t us) {
    // 经验值：QEMU/KVM环境下约3000次循环=1ms
    // 调整系数：us * 3 = 微秒数对应的循环次数
    for (volatile uint32_t i = 0; i < us * 3; i++) {
        asm volatile("outb %%al, $0x80" : : "a"(0)); // 使用端口0x80制造延迟
    }
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

        for (int i = 0; i < 6; i++) {
        uint32_t bar_val = pci_read_dword(pci_bus, pci_device, pci_function, 0x10 + i * 4);
        if ((bar_val & 0x4) == 0x4) { // 64-bit BAR
            uint32_t next_bar_val = pci_read_dword(pci_bus, pci_device, pci_function, 0x10 + (i + 1) * 4);
            pci_bars[i] = ((uint64_t)next_bar_val << 32) | (bar_val & 0xFFFFFFF0);
            i++; // Consume next BAR slot
        } else { // 32-bit BAR
            pci_bars[i] = (uintptr_t)(bar_val & 0xFFFFFFF0);
        }
    }

    virtio_net_mmio_base = (volatile uint8_t*)((uintptr_t)pci_bars[0]); 
    tty_print("VirtIO MMIO @ 0x", 0xFFFFFF); print_hex((uint64_t)virtio_net_mmio_base, 0x00FFFF); tty_print("\n", 0x00FFFF);

    // 2. 扫描 PCI Capabilities，找到各个配置块的 MMIO 指针
    uint8_t cap_ptr_offset = pci_read_dword(pci_bus, pci_device, pci_function, 0x34) & 0xFF; // Capability Pointer
    while (cap_ptr_offset != 0) {
        // 读取 Capability Header 的前 4 个字节 (ID, next, len, type)
        uint32_t cap_header = pci_read_dword(pci_bus, pci_device, pci_function, cap_ptr_offset);
        uint8_t cap_vndr = cap_header & 0xFF;
        uint8_t cap_next = (cap_header >> 8) & 0xFF;

        if (cap_vndr == 0x09) { // 确认是 Vendor Specific Capability (VirtIO 使用 0x09)
            // 读取 Capability 结构的其余部分
            uint32_t dword_bar_padding = pci_read_dword(pci_bus, pci_device, pci_function, cap_ptr_offset + 4);
            uint32_t dword_offset      = pci_read_dword(pci_bus, pci_device, pci_function, cap_ptr_offset + 8);
            
            // --- 核心修正：从正确的位置提取字段 ---
            // cfg_type 在 offset +3，是 cap_header 的最高字节
            uint8_t cfg_type = (cap_header >> 24) & 0xFF; 
            // bar 在 offset +4，是 dword_bar_padding 的最低字节
            uint8_t cap_bar_idx = dword_bar_padding & 0xFF;
            // offset 在 offset +8，就是 dword_offset
            uint32_t cap_offset_in_bar = dword_offset;
            
            // 确保 BAR 索引有效且 BAR 地址已读取
            if (cap_bar_idx < 6 && pci_bars[cap_bar_idx] != 0) {
                volatile uint8_t* base_ptr_for_cap = (volatile uint8_t*)(pci_bars[cap_bar_idx] + cap_offset_in_bar);

                switch (cfg_type) {
                    case VIRTIO_PCI_CAP_COMMON_CFG: tty_print("  Found Common Cfg @ 0x", 0xFFFFFF); print_hex((uint64_t)base_ptr_for_cap, 0x00FFFF); tty_print("\n", 0x00FFFF); common_cfg_ptr = base_ptr_for_cap; break;
                    case VIRTIO_PCI_CAP_NOTIFY_CFG:
                        tty_print("  Found Notify Cfg @ 0x", 0xFFFFFF);
                        print_hex((uint64_t)base_ptr_for_cap, 0x00FFFF);
                        tty_print("\n", 0x00FFFF); 
                        notify_cfg_ptr = base_ptr_for_cap;
                        virtio_notify_multiplier = pci_read_dword(pci_bus, pci_device, pci_function, cap_ptr_offset + 16);
                    break;  
                    case VIRTIO_PCI_CAP_ISR_CFG:    tty_print("  Found ISR Cfg @ 0x", 0xFFFFFF); print_hex((uint64_t)base_ptr_for_cap, 0x00FFFF); tty_print("\n", 0x00FFFF); isr_cfg_ptr = base_ptr_for_cap; break;
                    case VIRTIO_PCI_CAP_DEVICE_CFG: tty_print("  Found Device Cfg @ 0x", 0xFFFFFF); print_hex((uint64_t)base_ptr_for_cap, 0x00FFFF); tty_print("\n", 0x00FFFF); device_cfg_ptr = base_ptr_for_cap; break;
                    case VIRTIO_PCI_CAP_PCI_CFG:    tty_print("  Found PCI Cfg @ 0x", 0xFFFFFF); print_hex((uint64_t)base_ptr_for_cap, 0x00FFFF); tty_print("\n", 0x00FFFF); pci_cfg_ptr = base_ptr_for_cap; break;
                }
            }
        }
        cap_ptr_offset = cap_next;
    }
    
    if (!common_cfg_ptr || !notify_cfg_ptr || !isr_cfg_ptr || !device_cfg_ptr) {
        tty_print("VirtIO: Missing crucial capabilities!\n", 0xFF0000);
        return;
    }
    
    // 3. 设备复位 (通过 common_cfg_ptr 访问)
    tty_print("VirtIO: Resetting device...\n", 0xFFFFFF);
    virtio_write_cap_8(common_cfg_ptr, 0x14 /* device_status */, 0);
    int timeout = 1000; 
    while (virtio_read_cap_8(common_cfg_ptr, 0x14 /* device_status */) != 0 && timeout-- > 0) {
        io_delay_us(1000);
    }
    if (timeout <= 0) {
        tty_print("VirtIO: Device reset timed out!\n", 0xFF0000);
        return;
    }
    tty_print("VirtIO: Device reset complete.\n", 0x00FF00);
    
    // 4. 设置驱动状态: Acknowledge, Driver
    uint8_t status = 0;
    status |= VIRTIO_STATUS_ACKNOWLEDGE;
    virtio_write_cap_8(common_cfg_ptr, 0x14 /* device_status */, status);
    tty_print("VirtIO: Set ACKNOWLEDGE. Status=0x", 0xFFFFFF);
    print_hex(virtio_read_cap_8(common_cfg_ptr, 0x14), 0xFFFFFF);
    tty_print("\n", 0xFFFFFF);
    
    status |= VIRTIO_STATUS_DRIVER;
    virtio_write_cap_8(common_cfg_ptr, 0x14 /* device_status */, status);
    tty_print("VirtIO: Set DRIVER. Status=0x", 0xFFFFFF);
    print_hex(virtio_read_cap_8(common_cfg_ptr, 0x14), 0xFFFFFF);
    tty_print("\n", 0xFFFFFF);

    // 5. 读取设备特性并协商 (Feature Negotiation)
    virtio_write_cap_32(common_cfg_ptr, 0x00 /* device_feature_select */, 0); // 选择低 32 位
    uint64_t device_features = virtio_read_cap_32(common_cfg_ptr, 0x04 /* device_feature */);
    virtio_write_cap_32(common_cfg_ptr, 0x00 /* device_feature_select */, 1); // 选择高 32 位
    device_features |= ((uint64_t)virtio_read_cap_32(common_cfg_ptr, 0x04 /* device_feature */)) << 32;


    // 为了简单，我们告诉设备我们支持它提供的所有特性
    uint64_t driver_features = device_features; 

    // 将我们选择的特性写回给设备
    virtio_write_cap_32(common_cfg_ptr, 0x08 /* driver_feature_select */, 0);
    virtio_write_cap_32(common_cfg_ptr, 0x0C /* driver_feature */, (uint32_t)driver_features);
    virtio_write_cap_32(common_cfg_ptr, 0x08 /* driver_feature_select */, 1);
    virtio_write_cap_32(common_cfg_ptr, 0x0C /* driver_feature */, (uint32_t)(driver_features >> 32));
    
    // 6. 确认特性协商完成
    status |= VIRTIO_STATUS_FEATURES_OK;
    virtio_write_cap_8(common_cfg_ptr, 0x14 /* device_status */, status);

    // 再次检查，确认设备接受了我们的特性
    status |= VIRTIO_STATUS_FEATURES_OK;
    virtio_write_cap_8(common_cfg_ptr, 0x14 /* device_status */, status);
    tty_print("VirtIO: Set FEATURES_OK. Status=0x", 0xFFFFFF); 
    print_hex(virtio_read_cap_8(common_cfg_ptr, 0x14), 0xFFFFFF);
    tty_print("\n", 0xFFFFFF);

    if (!(virtio_read_cap_8(common_cfg_ptr, 0x14 /* device_status */) & VIRTIO_STATUS_FEATURES_OK)) {
        tty_print("VirtIO: Features *NOT* accepted by device! Device cleared the bit.\n", 0xFF0000);
        return;
    }
    tty_print("VirtIO: Feature negotiation complete.\n", 0x00FF00);

    // 7. 获取 MAC 地址 (通过 device_cfg_ptr 访问)
    if (driver_features & (1ULL << 5) /* VIRTIO_NET_F_MAC */) {
        for (int i = 0; i < 6; i++) {
            virtio_net_mac_addr[i] = virtio_read_cap_8(device_cfg_ptr, i);
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
    //    这里需要将 common_cfg_ptr 和 notify_cfg_ptr 传递给 
    
    rx_q = virtq_alloc(0, NUM_RX_DESC);
    tx_q = virtq_alloc(1, NUM_TX_DESC);

    if (!rx_q || !tx_q) {
        tty_print("VirtIO: Failed to allocate virtqueues.\n", 0xFF0000);
        virtio_write_cap_8(common_cfg_ptr, 0x14, VIRTIO_STATUS_FAILED);
        return;
    }

    // 9. 填充接收队列
    for (int i = 0; i < NUM_RX_DESC; i++) {
        rx_q->buffers[i] = (uint8_t*)pmm_alloc_page(); // 分配缓冲区
        memset(rx_q->buffers[i], 0, 10); // 清零 virtio_net_hdr
        virtq_add_buf(rx_q, rx_q->buffers[i], PAGE_SIZE, VIRTQ_DESC_F_WRITE); 
    }
    virtq_kick(rx_q); // 通知设备有空闲缓冲区

    // 10. 启用队列
    // Common Config: Queue Enable 0x0E
    virtio_write_cap_16(common_cfg_ptr, 0x16 /* queue_select */, 0);
    virtio_write_cap_16(common_cfg_ptr, 0x1C /* queue_enable */, 1);
    tty_print("VirtIO: RX Queue #0 enabled.\n", 0x00FF00);

    // 启用 TX 队列 (Queue 1)
    virtio_write_cap_16(common_cfg_ptr, 0x16 /* queue_select */, 1);
    virtio_write_cap_16(common_cfg_ptr, 0x1C /* queue_enable */, 1);
    tty_print("VirtIO: TX Queue #1 enabled.\n", 0x00FF00);

    // 11. 完成设备初始化
    status |= VIRTIO_STATUS_DRIVER_OK;
    virtio_write_cap_8(common_cfg_ptr, 0x14 /* device_status */, status);
    tty_print("VirtIO: Set DRIVER_OK. Final Status=0x", 0x00FF00); 
    print_hex(virtio_read_cap_8(common_cfg_ptr, 0x14), 0x00FF00); 
    tty_print("\n", 0x00FF00);

    // 12. 读取并保存 IRQ 中断号
    // PCI 配置空间的 0x3C 偏移量是 Interrupt Line 寄存器
    uint32_t pci_irq_pin = pci_read_dword(pci_bus, pci_device, pci_function, 0x3C);
    virtio_net_irq = (uint8_t)(pci_irq_pin & 0xFF);
    tty_print("VirtIO: IRQ Line = ", 0xFFFFFF); 
    print_hex(virtio_net_irq, 0xFFFFFF);
    tty_print("\n", 0xFFFFFF);

    tty_print("VirtIO Net initialized successfully!\n", 0x00FF00);

    pic_unmask_irq(virtio_net_irq);
    tty_print("VirtIO: Unmasked IRQ ", 0xFFFFFF); print_hex(virtio_net_irq, 0xFFFFFF); tty_print(" in PIC.\n", 0xFFFFFF);

}

// 获取 MAC 地址
const uint8_t* virtio_net_get_mac_address() {
    return virtio_net_mac_addr;
}

// 发送数据包
bool virtio_net_send_packet(const uint8_t* data, uint16_t len) {
    if (len == 0 || len > 1514) {
        return false;
    }

    // 1. **核心修正**：为这次发送动态分配一个缓冲区。
    //    这个缓冲区需要包含 VirtIO Net Header (10 bytes) 和数据包本身。
    uint16_t total_len = 10 + len;
    uint8_t* tx_buffer = (uint8_t*)pmm_alloc_page(); // 或者用 kmalloc(total_len)
    if (!tx_buffer) {
        tty_print("VirtIO TX: Failed to allocate buffer for sending!\n", 0xFF0000);
        return false;
    }
    
    // 2. 准备缓冲区内容
    memset(tx_buffer, 0, 10); // 清零 virtio_net_hdr
    memcpy(tx_buffer + 10, data, len); // 拷贝用户数据

    // 3. 将这个**新分配的**缓冲区添加到发送队列
    //    这里的 flags 必须是 0 (设备只读)
    if (virtq_add_buf(tx_q, tx_buffer, total_len, 0) != 0) {
        tty_print("VirtIO TX: Failed to add buffer to virtqueue. Queue full?\n", 0xFF0000);
        pmm_free_page(tx_buffer); // 释放我们分配的内存
        return false;
    }

    // 4. 通知设备
    virtq_kick(tx_q);

    // **重要**：这个 tx_buffer 现在由设备拥有。我们必须在中断处理函数中，
    // 当设备确认发送完成后，再调用 pmm_free_page(tx_buffer) 来释放它。
    // 这需要在 virtq_desc 和中断处理中添加一些逻辑来跟踪和释放它。
    
    // 为了简化第一次测试，我们可以暂时“泄露”这块内存，以后再来完善释放逻辑。

    tty_print("VirtIO: Packet sent. Len=", 0x00FF00); print_hex(len, 0x00FF00); tty_print("\n", 0x00FF00);
    return true;
}


// 处理 VirtIO 网卡中断
// void virtio_net_handle_interrupt() {
//     // 读取 ISR 状态寄存器 (会清除中断)
//     uint8_t isr_status = virtio_read_cap_8(isr_cfg_ptr, 0);
//     tty_print("\n--- VirtIO IRQ HIT! ---\n", 0xFFFF00);
//     tty_print("  ISR Status Register: 0b", 0xFFFFFF);
//     for (int i = 7; i >= 0; i--) { tty_print((isr_status & (1 << i)) ? "1" : "0", 0xFFFFFF); }
//     tty_print("\n", 0xFFFFFF);
    
//     if (isr_status == 0) return; // 没有中断发生

//     if (isr_status & 0x01) { // VIRTIO_PCI_ISR_QUEUE
//         // 3. 在处理任何事情之前，先打印两个队列的“设备索引”
//         uint16_t tx_device_idx = tx_q->used->idx;
//         uint16_t rx_device_idx = rx_q->used->idx;
//         tty_print("  Device state before processing:\n", 0xFFFFFF);
//         tty_print("    TX used->idx: ", 0xFFFFFF); print_hex(tx_device_idx, 0xFFFFFF); 
//         tty_print(" (Driver expects: ", 0xFFFFFF); print_hex(tx_q->used_idx, 0xFFFFFF); tty_print(")\n", 0xFFFFFF);
//         tty_print("    RX used->idx: ", 0xFFFFFF); print_hex(rx_device_idx, 0xFFFFFF);
//         tty_print(" (Driver expects: ", 0xFFFFFF); print_hex(rx_q->used_idx, 0xFFFFFF); tty_print(")\n", 0xFFFFFF);
//         }

//         // 检查 RX 队列是否有新收到的包
//         while (rx_q->used->idx != rx_q->used_idx) {
//             struct virtq_used_elem* used_elem = &rx_q->used->ring[rx_q->used_idx % rx_q->num];
//             uint16_t desc_idx = used_elem->id;
//             uint32_t len = used_elem->len;
//             uint8_t* packet_data = rx_q->buffers[desc_idx];

//             tty_print("VirtIO: RX packet received. Len=", 0x00FFFF); print_hex(len, 0x00FFFF); tty_print("\n", 0x00FFFF);
            
//             // 数据包在这里：packet_data (跳过前10字节的 virtio_net_hdr)
//             // EtherType 在 packet_data + 10 + 12 (以太网头的 EtherType 偏移)
//             uint16_t eth_type = (packet_data[10 + 12] << 8) | packet_data[10 + 13];
//             tty_print("  EtherType: 0x", 0xFFFFFF); print_hex(eth_type, 0x00FF00); tty_print("\n", 0x00FF00);

//             if (eth_type == 0x0806) { // ARP 协议
//                 tty_print("  ARP Packet Detected!\n", 0x00FF00);
//             } else if (eth_type == 0x0800) { // IPv4 协议
//                 tty_print("  IPv4 Packet Detected!\n", 0x00FF00);
//             } else {
//                 tty_print("  Unknown EtherType.\n", 0xFF6060);
//             }
            
//             // 重新将缓冲区添加到接收队列
//             // 标记为设备写入 (VIRTQ_DESC_F_WRITE)
//             virtq_add_buf(rx_q, rx_q->buffers[desc_idx], PAGE_SIZE, VIRTQ_DESC_F_WRITE); 
//             rx_q->used_idx++;
//         }
//     }
// }
void virtio_net_handle_interrupt() {
    // 1. 读取并立即打印 ISR 状态寄存器 (这会清除设备的中断状态)
    uint8_t isr_status = virtio_read_cap_8(isr_cfg_ptr, 0);
    tty_print("\n--- VirtIO IRQ HIT! ---\n", 0xFFFF00);
    tty_print("  ISR Status Register: 0b", 0xFFFFFF);
    for (int i = 7; i >= 0; i--) { tty_print((isr_status & (1 << i)) ? "1" : "0", 0xFFFFFF); }
    tty_print("\n", 0xFFFFFF);

    // 2. 只有在“队列更新”位被设置时才继续
    if (isr_status & 0x01) { // VIRTIO_PCI_ISR_QUEUE
        // 3. 在处理任何事情之前，先打印两个队列的“设备索引”
        uint16_t tx_device_idx = tx_q->used->idx;
        uint16_t rx_device_idx = rx_q->used->idx;
        tty_print("  Device state before processing:\n", 0xFFFFFF);
        tty_print("    TX used->idx: ", 0xFFFFFF); print_hex(tx_device_idx, 0xFFFFFF); 
        tty_print(" (Driver expects: ", 0xFFFFFF); print_hex(tx_q->used_idx, 0xFFFFFF); tty_print(")\n", 0xFFFFFF);
        tty_print("    RX used->idx: ", 0xFFFFFF); print_hex(rx_device_idx, 0xFFFFFF);
        tty_print(" (Driver expects: ", 0xFFFFFF); print_hex(rx_q->used_idx, 0xFFFFFF); tty_print(")\n", 0xFFFFFF);

        // 4. 处理发送完成的队列 (TX)
        while (tx_q->used_idx != tx_device_idx) {
            struct virtq_used_elem* used_elem = &tx_q->used->ring[tx_q->used_idx % tx_q->num];
            uint16_t desc_idx = used_elem->id;
            
            tty_print("  > VirtIO: TX packet acknowledged. Desc=", 0x00FF00); print_hex(desc_idx, 0x00FF00); tty_print("\n", 0x00FF00);
            
            // 重要：在这里释放为发送而分配的缓冲区
            // pmm_free_page( (void*)tx_q->desc[desc_idx].addr ); // 暂缓实现

            // 将描述符重新加入空闲链表
            tx_q->desc[desc_idx].next = tx_q->free_head;
            tx_q->free_head = desc_idx;
            tx_q->used_idx++;
        }

        // 5. 处理已接收数据的队列 (RX)
        while (rx_q->used_idx != rx_device_idx) {
            struct virtq_used_elem* used_elem = &rx_q->used->ring[rx_q->used_idx % rx_q->num];
            uint16_t desc_idx = used_elem->id;
            uint32_t len = used_elem->len;
            uint8_t* packet_data = rx_q->buffers[desc_idx];
            
            tty_print("  > VirtIO: RX packet received! Desc=", 0x00FFFF); print_hex(desc_idx, 0x00FFFF); 
            tty_print(" Len=", 0x00FFFF); print_hex(len, 0x00FFFF); tty_print("\n", 0x00FFFF);

            int16_t eth_type = (packet_data[10 + 12] << 8) | packet_data[10 + 13];
            tty_print("  EtherType: 0x", 0xFFFFFF); print_hex(eth_type, 0x00FF00); tty_print("\n", 0x00FF00);
            
            // 将这个刚刚用完的缓冲区，重新放回接收队列，以便接收下一个包
            virtq_add_buf(rx_q, rx_q->buffers[desc_idx], PAGE_SIZE, VIRTQ_DESC_F_WRITE);
            rx_q->used_idx++;
        }
    }
}
