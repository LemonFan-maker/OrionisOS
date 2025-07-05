#pragma once
#include <stdint.h>
#include <stddef.h> // for size_t

// ATA 端口定义 (PIO 模式)
#define ATA_MASTER_DATA_PORT    0x1F0
#define ATA_MASTER_ERROR_PORT   0x1F1
#define ATA_MASTER_SECTOR_COUNT 0x1F2
#define ATA_MASTER_LBA_LOW      0x1F3
#define ATA_MASTER_LBA_MID      0x1F4
#define ATA_MASTER_LBA_HIGH     0x1F5
#define ATA_MASTER_DRIVE_SEL    0x1F6
#define ATA_MASTER_COMMAND_PORT 0x1F7
#define ATA_MASTER_STATUS_PORT  0x1F7

// ATA 命令
#define ATA_CMD_READ_PIO    0x20
#define ATA_CMD_IDENTIFY    0xEC

// ATA 状态位
#define ATA_SR_BSY  0x80 // Busy
#define ATA_SR_DRDY 0x40 // Drive Ready
#define ATA_SR_DF   0x20 // Drive Write Fault
#define ATA_SR_ERR  0x01 // Error
#define ATA_SR_DRQ  0x08

// 初始化 ATA 驱动
void ata_init();

// 读取扇区
bool ata_read_sectors(uint8_t drive, uint64_t lba, uint8_t num_sectors, void* buffer);
