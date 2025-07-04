#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <lib/acpi.h>
#include <sys/cpu.h>
#include <lib/blib.h>
#include <lib/print.h>
#include <sys/smp.h>
#include <sys/lapic.h>
#include <mm/vmm.h>
#include <mm/pmm.h>

struct madt {
    struct sdt;
    uint32_t local_controller_addr;
    uint32_t flags;
    char     madt_entries_begin[];
} __attribute__((packed));

struct madt_header {
    uint8_t type;
    uint8_t length;
} __attribute__((packed));

struct madt_lapic {
    struct madt_header;
    uint8_t  acpi_processor_uid;
    uint8_t  lapic_id;
    uint32_t flags;
} __attribute__((packed));

struct madt_x2apic {
    struct madt_header;
    uint8_t  reserved[2];
    uint32_t x2apic_id;
    uint32_t flags;
    uint32_t acpi_processor_uid;
} __attribute__((packed));

struct gdtr {
    uint16_t limit;
    uint32_t ptr;
} __attribute__((packed));

static void delay(uint32_t cycles) {
    for (uint32_t i = 0; i < cycles; i++)
        inb(0x80);
}

void     smp_trampoline(void);
extern   struct gdtr smp_tpl_gdt;
struct   smp_information *smp_tpl_info_struct;
uint8_t  smp_tpl_booted_flag;
uint32_t smp_tpl_pagemap;
uint8_t  smp_tpl_target_mode;

static bool smp_start_ap(uint32_t lapic_id, struct gdtr *gdtr,
                         struct smp_information *info_struct,
                         bool longmode, bool lv5, uint32_t pagemap,
                         bool x2apic) {
    // Prepare the trampoline
    smp_tpl_info_struct = info_struct;
    smp_tpl_booted_flag = 0;
    smp_tpl_pagemap     = pagemap;
    smp_tpl_target_mode = ((uint32_t)x2apic << 2)
                        | ((uint32_t)lv5 << 1)
                        | (uint32_t)longmode;
    smp_tpl_gdt         = *gdtr;

    // Send the INIT IPI
    if (x2apic) {
        x2apic_write(LAPIC_REG_ICR0, ((uint64_t)lapic_id << 32) | 0x500);
    } else {
        lapic_write(LAPIC_REG_ICR1, lapic_id << 24);
        lapic_write(LAPIC_REG_ICR0, 0x500);
    }
    delay(5000);

    // Send the Startup IPI
    if (x2apic) {
        x2apic_write(LAPIC_REG_ICR0, ((uint64_t)lapic_id << 32) |
                                     ((size_t)smp_trampoline / 4096) | 0x600);
    } else {
        lapic_write(LAPIC_REG_ICR1, lapic_id << 24);
        lapic_write(LAPIC_REG_ICR0, ((size_t)smp_trampoline / 4096) | 0x600);
    }

    for (int i = 0; i < 100; i++) {
        if (locked_read(&smp_tpl_booted_flag) == 1) {
            return true;
        }
        delay(10000);
    }

    return false;
}

struct smp_information *init_smp(size_t   *cpu_count,
                                 bool      longmode,
                                 bool      lv5,
                                 pagemap_t pagemap,
                                 bool      x2apic) {
    // Search for MADT table
    struct madt *madt = acpi_get_table("APIC", 0);

    if (madt == NULL)
        return NULL;

    struct gdtr gdtr;
    asm volatile ("sgdt %0" :: "m"(gdtr) : "memory");

    struct smp_information *ret = conv_mem_alloc_aligned(0, 1);
    *cpu_count = 0;

    x2apic = x2apic && x2apic_enable();

    // Parse the MADT entries
    for (uint8_t *madt_ptr = (uint8_t *)madt->madt_entries_begin;
      (uintptr_t)madt_ptr < (uintptr_t)madt + madt->length;
      madt_ptr += *(madt_ptr + 1)) {
        switch (*madt_ptr) {
            case 0: {
                // Processor local xAPIC
                struct madt_lapic *lapic = (void *)madt_ptr;

                // Check if we can actually try to start the AP
                if (!((lapic->flags & 1) ^ ((lapic->flags >> 1) & 1)))
                    continue;

                struct smp_information *info_struct =
                        conv_mem_alloc_aligned(sizeof(struct smp_information), 1);

                info_struct->acpi_processor_uid = lapic->acpi_processor_uid;
                info_struct->lapic_id           = lapic->lapic_id;

                // Do not try to restart the BSP
                if (lapic->lapic_id == 0) {
                    (*cpu_count)++;
                    continue;
                }

                print("smp: [xAPIC] Found candidate AP for bring-up. LAPIC ID: %u\n", lapic->lapic_id);

                // Try to start the AP
                if (!smp_start_ap(lapic->lapic_id, &gdtr, info_struct,
                                  longmode, lv5, (uint32_t)pagemap.top_level,
                                  x2apic)) {
                    print("smp: FAILED to bring-up AP\n");
                    conv_mem_rewind(sizeof(struct smp_information));
                    continue;
                }

                print("smp: Successfully brought up AP\n");

                (*cpu_count)++;
                continue;
            }
            case 9: {
                // Processor local x2APIC
                if (!x2apic)
                    continue;

                struct madt_x2apic *x2apic = (void *)madt_ptr;

                // Check if we can actually try to start the AP
                if (!((x2apic->flags & 1) ^ ((x2apic->flags >> 1) & 1)))
                    continue;

                struct smp_information *info_struct =
                        conv_mem_alloc_aligned(sizeof(struct smp_information), 1);

                info_struct->acpi_processor_uid = x2apic->acpi_processor_uid;
                info_struct->lapic_id           = x2apic->x2apic_id;

                // Do not try to restart the BSP
                if (x2apic->x2apic_id == 0) {
                    (*cpu_count)++;
                    continue;
                }

                print("smp: [x2APIC] Found candidate AP for bring-up. LAPIC ID: %u\n", x2apic->x2apic_id);

                // Try to start the AP
                if (!smp_start_ap(x2apic->x2apic_id, &gdtr, info_struct,
                                  longmode, lv5, (uint32_t)pagemap.top_level,
                                  true)) {
                    print("smp: FAILED to bring-up AP\n");
                    conv_mem_rewind(sizeof(struct smp_information));
                    continue;
                }

                print("smp: Successfully brought up AP\n");

                (*cpu_count)++;
                continue;
            }
        }
    }

    return ret;
}
