#include "ata.h"
#include "kernel/cpu/ports.h"
#include "kernel/drivers/tty.h"
#include "lib/libc.h"

//  辅助函数 

// 等待 BSY 位清零，带有超时机制
static int ata_wait_bsy() {
    for(int i = 0; i < 100000; i++) {
        if (!(inb(ATA_MASTER_STATUS_PORT) & ATA_SR_BSY))
            return 0; // Not busy
    }
    return 1; // Timed out
}

// 等待 DRQ 位置位，带有超时机制
static int ata_wait_drq() {
    for(int i = 0; i < 100000; i++) {
        if (inb(ATA_MASTER_STATUS_PORT) & ATA_SR_DRQ)
            return 0; // Data request is ready
    }
    return 1; // Timed out
}

static bool ata_read_buffer(uint16_t* buffer, size_t words) {
    for (size_t i = 0; i < words; i++) {
        // 在读取每一个字之前，都等待 DRQ
        if (ata_wait_drq()) {
            tty_print("ATA: DRQ timeout during data transfer!", 0xFF0000);
            return false;
        }
        buffer[i] = inw(ATA_MASTER_DATA_PORT);
    }
    return true;
}

//  公共接口函数 

void ata_init() {
    // 我们可以尝试识别一下硬盘
    outb(ATA_MASTER_DRIVE_SEL, 0xA0); // Select master drive
    outb(ATA_MASTER_COMMAND_PORT, ATA_CMD_IDENTIFY);
    if (inb(ATA_MASTER_STATUS_PORT) == 0) {
        tty_print("\nATA: No drive found.", 0xFF6060);
        return;
    }
    tty_print("\nATA driver initialized.", 0x4EC9B0);
}

bool ata_read_sectors(uint8_t drive, uint64_t lba, uint8_t num_sectors, void* buffer) {
    if (drive > 1) return false;

    if (ata_wait_bsy()) {
        tty_print("ATA: Drive busy timeout!", 0xFF0000);
        return false;
    }

    outb(ATA_MASTER_DRIVE_SEL, 0xE0);
    outb(ATA_MASTER_SECTOR_COUNT, num_sectors);
    outb(ATA_MASTER_LBA_LOW, (uint8_t)lba);
    outb(ATA_MASTER_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_MASTER_LBA_HIGH, (uint8_t)(lba >> 16));
    outb(ATA_MASTER_COMMAND_PORT, ATA_CMD_READ_PIO);

    for (uint8_t i = 0; i < num_sectors; i++) {
        if (ata_wait_bsy()) {
            tty_print("ATA: Drive busy timeout before read!", 0xFF0000);
            return false;
        }
        
        if (inb(ATA_MASTER_STATUS_PORT) & ATA_SR_ERR) {
            tty_print("ATA Read Error!", 0xFF0000);
            return false;
        }

        // 调用新的、健壮的 ata_read_buffer
        if (!ata_read_buffer((uint16_t*)((uintptr_t)buffer + i * 512), 256)) {
            return false; // 如果读取失败，直接返回
        }
    }

    return true;
}