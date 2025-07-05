#include "e1000.h"
#include "ports.h"
#include "kernel/cpu/pci.h"
#include "kernel/drivers/tty.h"
#include "kernel/mem/pmm.h" // 需要 pmm_alloc_page
#include "lib/libc.h" // 需要 memcpy
#include "idt.h"

#ifdef __cplusplus
extern "C" {
    #endif
    void isr42(); // E1000 网卡中断的汇编入口
    #ifdef __cplusplus
}
#endif

// E1000 MMIO 基地址 (虚拟地址)
static volatile uint32_t* e1000_mmio_base = nullptr;
static uint8_t e1000_mac_addr[6];

// 接收/发送环形缓冲区
#define NUM_RX_DESC 32
#define NUM_TX_DESC 32

static struct e1000_rx_desc* rx_descs[NUM_RX_DESC];
static uint8_t* rx_buffers[NUM_RX_DESC];
static struct e1000_tx_desc* tx_descs[NUM_TX_DESC];
static uint8_t* tx_buffers[NUM_TX_DESC];

static uint16_t rx_cur;
static uint16_t tx_cur;

// MAC地址
const uint8_t* e1000_get_mac_address() {
    return e1000_mac_addr;
}

// 读取 E1000 寄存器
static uint32_t e1000_read_reg(uint32_t reg) {
    return e1000_mmio_base[reg / 4]; // MMIO 访问是 DWORD (4字节) 对齐的
}

// 写入 E1000 寄存器
static void e1000_write_reg(uint32_t reg, uint32_t val) {
    e1000_mmio_base[reg / 4] = val;
}

// 初始化 E1000 网卡
void e1000_init(uint8_t pci_bus, uint8_t pci_device, uint8_t pci_function) {
    // 1. 读取 PCI 配置空间信息和 MMIO 基地址
    uint32_t bar0 = pci_read_dword(pci_bus, pci_device, pci_function, 0x10);
    uintptr_t mmio_phys_addr = (uintptr_t)(bar0 & 0xFFFFFFF0);
    if ((bar0 & 0x6) == 0x4) { // 64位 BAR
        uint32_t bar1 = pci_read_dword(pci_bus, pci_device, pci_function, 0x14);
        mmio_phys_addr = ((uint64_t)bar1 << 32) | (bar0 & 0xFFFFFFF0);
    }
    e1000_mmio_base = (volatile uint32_t*)((uintptr_t)mmio_phys_addr);

    // 2. 开启 PCI Master Enable 位
    uint32_t pci_command = pci_read_dword(pci_bus, pci_device, pci_function, 0x04);
    pci_command |= (1 << 2); // Bus Master Enable
    pci_command |= (1 << 0); // IO Space Enable
    pci_command |= (1 << 1); // Memory Space Enable
    pci_write_dword(pci_bus, pci_device, pci_function, 0x04, pci_command);

    // 3. 设置 PCI Interrupt Line 寄存器 (关键)
    pci_write_dword(pci_bus, pci_device, pci_function, 0x3C, 10); // IRQ 10

    // 调试打印
    tty_print("E1000 MMIO @ 0x", 0xFFFFFF); print_hex((uint64_t)e1000_mmio_base, 0x00FFFF); tty_print("\n", 0x00FFFF);
    tty_print("E1000 PCI Interrupt Line set to: 0x", 0xFFFFFF); print_hex(pci_read_dword(pci_bus, pci_device, pci_function, 0x3C), 0x00FFFF); tty_print("\n", 0x00FFFF);

    // 4. E1000 网卡复位 (更彻底的复位)
    e1000_write_reg(E1000_REG_CTRL, 0x04000000); // RST bit (Bit 26)
    for(volatile int i = 0; i < 1000000; i++); // 忙等待
    e1000_write_reg(E1000_REG_CTRL, 0); // Clear RST bit
    e1000_read_reg(E1000_REG_CTRL); // 再次读取确保生效

    // 5. 禁用所有中断，并清除所有挂起的中断
    e1000_write_reg(E1000_REG_IMS, 0); 
    e1000_read_reg(E1000_REG_ICR); 

    // 6. 读取 MAC 地址 (从 RA 寄存器)
    uint32_t ra_low = e1000_read_reg(E1000_REG_RA);
    uint32_t ra_high = e1000_read_reg(E1000_REG_RA + 4);
    e1000_mac_addr[0] = (ra_low >> 0) & 0xFF; e1000_mac_addr[1] = (ra_low >> 8) & 0xFF;
    e1000_mac_addr[2] = (ra_low >> 16) & 0xFF; e1000_mac_addr[3] = (ra_low >> 24) & 0xFF;
    e1000_mac_addr[4] = (ra_high >> 0) & 0xFF; e1000_mac_addr[5] = (ra_high >> 8) & 0xFF;
    tty_print("E1000 MAC: ", 0xFFFFFF);
    for (int i = 0; i < 6; i++) { print_hex(e1000_mac_addr[i], 0x00FF00); if (i < 5) tty_print(":", 0x00FF00); }
    tty_print("\n", 0x00FF00);

    // 7. 配置接收地址寄存器 (RAR[0]) 和 MAC 地址过滤器
    // 这是 E1000 接收数据包的基础：它必须知道自己的 MAC 地址
    e1000_write_reg(E1000_REG_RA, (e1000_mac_addr[0] | (e1000_mac_addr[1] << 8) | (e1000_mac_addr[2] << 16) | (e1000_mac_addr[3] << 24)));
    e1000_write_reg(E1000_REG_RA + 4, (e1000_mac_addr[4] | (e1000_mac_addr[5] << 8) | (1 << 31))); // 最后一位置 1 启用

    // 8. 初始化接收环形缓冲区
    rx_cur = 0;
    uintptr_t rx_ring_phys = (uintptr_t)pmm_alloc_page();
    tty_print("RX Ring @ 0x", 0xFFFFFF); print_hex(rx_ring_phys, 0x00FFFF); tty_print("\n", 0x00FFFF);
    for (int i = 0; i < NUM_RX_DESC; i++) {
        rx_descs[i] = (struct e1000_rx_desc*)((uintptr_t)rx_ring_phys + i * sizeof(struct e1000_rx_desc));
        rx_buffers[i] = (uint8_t*)pmm_alloc_page();
        rx_descs[i]->addr = (uint64_t)rx_buffers[i];
        rx_descs[i]->status = 0;
        rx_descs[i]->length = 0;
    }
    e1000_write_reg(E1000_REG_RDBAL, (uint32_t)(rx_ring_phys & 0xFFFFFFFF));
    e1000_write_reg(E1000_REG_RDBAH, (uint32_t)(rx_ring_phys >> 32));
    e1000_write_reg(E1000_REG_RDLEN, NUM_RX_DESC * sizeof(struct e1000_rx_desc));
    e1000_write_reg(E1000_REG_RDH, 0);
    e1000_write_reg(E1000_REG_RDT, NUM_RX_DESC - 1); 

    // 9. 初始化发送环形缓冲区 (类似接收)
    tx_cur = 0;
    uintptr_t tx_ring_phys = (uintptr_t)pmm_alloc_page();
    tty_print("TX Ring @ 0x", 0xFFFFFF); print_hex(tx_ring_phys, 0x00FFFF); tty_print("\n", 0x00FFFF);
    for (int i = 0; i < NUM_TX_DESC; i++) {
        tx_descs[i] = (struct e1000_tx_desc*)((uintptr_t)tx_ring_phys + i * sizeof(struct e1000_tx_desc));
        tx_buffers[i] = (uint8_t*)pmm_alloc_page();
        tx_descs[i]->addr = (uint64_t)tx_buffers[i];
        tx_descs[i]->cmd = 0;
        tx_descs[i]->status = (1 << 0); // DD (Descriptor Done) bit 0
        tx_descs[i]->length = 0;
    }
    e1000_write_reg(E1000_REG_TDBAL, (uint32_t)(tx_ring_phys & 0xFFFFFFFF));
    e1000_write_reg(E1000_REG_TDBAH, (uint32_t)(tx_ring_phys >> 32));
    e1000_write_reg(E1000_REG_TDLEN, NUM_TX_DESC * sizeof(struct e1000_tx_desc));
    e1000_write_reg(E1000_REG_TDH, 0);
    e1000_write_reg(E1000_REG_TDT, 0);

    // 10. 配置传输控制寄存器 (TCTL) 和 IPG (Inter Packet Gap)
    // TCTL: Enable (TxEn) = 1 (bit 1), PAD (Pad short packets) = 1 (bit 8), CRC (Append CRC) = 1 (bit 9),
    // PSP (Preamble Short Packet) = 1 (bit 3), CT (Collision Threshold) = 0x0F (bits 4-7),
    // COLDA (Collision Distance) = 0x3F (bits 12-21)
    e1000_write_reg(E1000_REG_TCTL, (1 << 1) | (1 << 3) | (0x0F << 4) | (0x3F << 12) | (1 << 8) | (1 << 9));
    e1000_write_reg(E1000_REG_TIPG, 0x0060200A);

    // 11. 配置接收控制寄存器 (RCTL)
    // RCTL: EN (Enable) = 1 (bit 1), BAM (Broadcast Accept Mode) = 1 (bit 15),
    // BSIZE (Buffer Size) = 00b (2048 bytes), BDM (Broadcast Descriptor Mode) = 1 (bit 25)
    e1000_write_reg(E1000_REG_RCTL, 
        (1 << 1)  |   // EN (Enable Receiver)
        (1 << 15) |   // BAM (Broadcast Accept Mode)
        (1 << 26) |   // LPE (Long Packet Enable) - 允许接收大包
        (1 << 24) |   // RDMTS (Receive Descriptor Minimum Threshold Size) - 1/2
        (0 << 2)  |   // BSIZE (Buffer Size) - 00b = 2048 bytes (Rx Buffer Size)
        (0 << 0)    // RDM (Receive Descriptor Min Threshold) - 00b = 1/2 threshold
    );

    // 12. 开启中断掩码 (只开启必要的，例如接收和发送完成)
    e1000_write_reg(E1000_REG_IMS, 
        (1 << 0) | // RXT: Receiver Timer Interrupt (new packet)
        (1 << 1) | // RXDMT0: Receiver Descriptor Minimum Threshold Interrupt
        (1 << 6) | // LSC: Link Status Change
        (1 << 7) | // RXO: Receive Overrun
        (1 << 4) | // TXQE: Transmit Queue Empty
        (1 << 5)   // TXDW: Transmit Descriptor Write Back (packet sent)
    );

    // 13. 再次清除所有挂起的中断
    e1000_read_reg(E1000_REG_ICR); 

    tty_print("E1000 initialized.\n", 0x00FF00);
}

bool e1000_send_packet(const uint8_t* data, uint16_t len) {
    if (len == 0 || len > 2048) { // E1000 缓冲区通常最大 2KB
        tty_print("E1000: Invalid packet length (0 or >2048).\n", 0xFF0000);
        return false;
    }

    // 找到当前头指针指向的描述符
    struct e1000_tx_desc* desc = tx_descs[tx_cur];
    
    // 检查描述符是否空闲 (DD 位是否设置，表示设备已处理完)
    if (!(desc->status & (1 << 0))) { // Descriptor Done (DD) is bit 0
        tty_print("E1000: TX descriptor not ready!\n", 0xFF6060);
        return false;
    }

    // 拷贝数据到描述符的缓冲区
    memcpy(tx_buffers[tx_cur], data, len);

    // 填充描述符
    desc->length = len;
    // Cmd 寄存器：EOP (End of Packet) | RS (Report Status)
    desc->cmd = (1 << 0) | (1 << 3); 
    desc->status = 0; // 清除状态位，表示设备可以处理了

    // 更新发送环的头指针，通知网卡有新的数据包要发送
    tx_cur = (tx_cur + 1) % NUM_TX_DESC;
    e1000_write_reg(E1000_REG_TDH, tx_cur);

    tty_print("E1000: Packet sent. Len=", 0x00FF00); print_hex(len, 0x00FF00); tty_print("\n", 0x00FF00);
    return true;
}

void e1000_handle_interrupt() {
    // 1. 读取 ICR 寄存器，获取中断原因。
    //    读取 ICR 会清除一些中断位。
    uint32_t icr = e1000_read_reg(E1000_REG_ICR);
    
    // 如果没有中断原因，直接返回
    if (icr == 0) return;

    tty_print("E1000 Interrupt hit! ICR = 0x", 0xFF00FF); print_hex(icr, 0xFF00FF); tty_print("\n", 0xFF00FF); // 关键调试信息

    // 2. 处理接收中断 (RXT 或 RXDMT0)
    if (icr & ((1 << 0) | (1 << 1))) { // RXT (bit 0) or RXDMT0 (bit 1)
        tty_print("  E1000 Handler: RX Interrupts active.\n", 0x00FF00); // 调试信息
        
        // 核心：处理所有挂起的接收描述符
        // RDH 指向网卡当前正在处理的描述符，RDT 是我们驱动的尾指针
        // 当 RDH 追上 RDT 时，说明所有待处理的包都处理完了
        uint16_t current_rdh = e1000_read_reg(E1000_REG_RDH); // 硬件的头指针

        // 循环处理，直到我们的软件尾指针追上硬件头指针
        while (rx_cur != current_rdh) {
            struct e1000_rx_desc* desc = rx_descs[rx_cur];
            
            // 检查描述符的 DD (Descriptor Done) 位
            if (!(desc->status & (1 << 0))) { // DD bit is 0, not yet done by hardware
                break; // 如果这个描述符还没处理完，就停止
            }
            
            uint16_t len = desc->length;
            uint8_t* packet_data = rx_buffers[rx_cur];

            tty_print("  Received Packet Len: ", 0x00FFFF); print_hex(len, 0x00FFFF); tty_print("\n", 0x00FFFF);
            
            //  识别以太网帧类型 (EtherType) 
            uint16_t eth_type = (packet_data[12] << 8) | packet_data[13];
            tty_print("  EtherType: 0x", 0xFFFFFF); print_hex(eth_type, 0x00FF00); tty_print("\n", 0x00FF00);

            if (eth_type == 0x0806) { // ARP 协议
                tty_print("  ARP Packet Detected!\n", 0x00FF00);
                tty_print("  Sender MAC: ", 0xFFFFFF);
                for(int i=0; i<6; i++) { print_hex(packet_data[22+i], 0x00FF00); if(i<5) tty_print(":", 0x00FF00); } tty_print("\n", 0x00FF00);
                tty_print("  Sender IP:  ", 0xFFFFFF);
                for(int i=0; i<4; i++) { print_hex(packet_data[28+i], 0x00FF00); if(i<3) tty_print(".", 0x00FF00); } tty_print("\n", 0x00FF00);

                uint16_t arp_opcode = (packet_data[20] << 8) | packet_data[21];
                if (arp_opcode == 0x0002) {
                    tty_print("  ARP Response from Gateway!\n", 0x00FF00);
                }
            } else if (eth_type == 0x0800) { // IPv4 协议
                tty_print("  IPv4 Packet Detected!\n", 0x00FF00);
            } else {
                tty_print("  Unknown EtherType.\n", 0xFF6060);
            }

            // 清除描述符状态，准备好再次接收
            desc->status = 0; // 清除描述符的 DD 位
            rx_cur = (rx_cur + 1) % NUM_RX_DESC;
            // 每次处理一个描述符就更新硬件 RDT 寄存器
            e1000_write_reg(E1000_REG_RDT, rx_cur); 
        }
        tty_print("  E1000 Handler: RX Processing finished.\n", 0x00FF00);
    }
    
    // 3. 处理发送中断 (TXQE 或 TXDW)
    if (icr & ((1 << 4) | (1 << 5))) { // TXQE (bit 4) or TXDW (bit 5)
        tty_print("E1000: TX Interrupts active.\n", 0x00FF00);
        // 通常这里不需要特别处理，只是确认发送完成，描述符的 DD 位会在发送包时被检查
    }

    // 4. 处理链路状态改变中断 (LSC)
    if (icr & (1 << 6)) { // LSC: Link Status Change
        tty_print("E1000: Link status changed.\n", 0xFFFFFF);
    }
    
    // 5. 再次读取 ICR 确保清除所有挂起的中断
    // 某些 E1000 型号需要再次读取 ICR 来确认中断清除
    e1000_read_reg(E1000_REG_ICR); 
}

// PCI 写入 DWORD (pci.cpp 中缺少)
// 外部声明，让 e1000.cpp 能用
extern void pci_write_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value);