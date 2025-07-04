#include "idt.h"

// 外部汇编函数
extern "C" void idt_load(uint64_t idt_ptr_addr);

// 声明我们的256个汇编 ISR 存根
extern "C" {
    void isr0(); void isr1(); void isr2(); void isr3();
    void isr4(); void isr5(); void isr6(); void isr7();
    void isr8(); void isr9(); void isr10(); void isr11();
    void isr12(); void isr13(); void isr14(); void isr15();
    void isr16(); void isr17(); void isr18(); void isr19();
    void isr20(); void isr21(); void isr22(); void isr23();
    void isr24(); void isr25(); void isr26(); void isr27();
    void isr28(); void isr29(); void isr30(); void isr31();
    void isr32(); void isr33(); void isr42(); 
}

// 定义 IDT 数组和指针
idt_entry_t idt_entries[256];
idt_ptr_t   idt_ptr;

// 辅助函数，用于设置一个 IDT 门
static void idt_set_gate(uint8_t num, uint64_t base, uint16_t sel, uint8_t flags) {
    idt_entries[num].isr_low    = (base & 0xFFFF);
    idt_entries[num].isr_mid    = (base >> 16) & 0xFFFF;
    idt_entries[num].isr_high   = (base >> 32) & 0xFFFFFFFF;
    idt_entries[num].kernel_cs  = sel;
    idt_entries[num].ist        = 0;
    idt_entries[num].attributes = flags;
    idt_entries[num].reserved   = 0;
}

// IDT 初始化函数
void init_idt() {
    idt_ptr.base = (uint64_t)&idt_entries;
    idt_ptr.limit = sizeof(idt_entry_t) * 256 - 1;

    // 0x08 是我们的内核代码段选择子 (来自 GDT)
    // 0x8E 是标志：P=1, DPL=00, S=0, Type=E (64-bit Interrupt Gate)
    idt_set_gate(0, (uint64_t)isr0, 0x08, 0x8E);
    idt_set_gate(1, (uint64_t)isr1, 0x08, 0x8E);
    idt_set_gate(2, (uint64_t)isr2, 0x08, 0x8E);
    idt_set_gate(3, (uint64_t)isr3, 0x08, 0x8E);
    idt_set_gate(4, (uint64_t)isr4, 0x08, 0x8E);
    idt_set_gate(5, (uint64_t)isr5, 0x08, 0x8E);
    idt_set_gate(6, (uint64_t)isr6, 0x08, 0x8E);
    idt_set_gate(7, (uint64_t)isr7, 0x08, 0x8E);
    idt_set_gate(8, (uint64_t)isr8, 0x08, 0x8E);
    idt_set_gate(9, (uint64_t)isr9, 0x08, 0x8E);
    idt_set_gate(10, (uint64_t)isr10, 0x08, 0x8E);
    idt_set_gate(11, (uint64_t)isr11, 0x08, 0x8E);
    idt_set_gate(12, (uint64_t)isr12, 0x08, 0x8E);
    idt_set_gate(13, (uint64_t)isr13, 0x08, 0x8E);
    idt_set_gate(14, (uint64_t)isr14, 0x08, 0x8E);
    idt_set_gate(15, (uint64_t)isr15, 0x08, 0x8E);
    idt_set_gate(16, (uint64_t)isr16, 0x08, 0x8E);
    idt_set_gate(17, (uint64_t)isr17, 0x08, 0x8E);
    idt_set_gate(18, (uint64_t)isr18, 0x08, 0x8E);
    idt_set_gate(19, (uint64_t)isr19, 0x08, 0x8E);
    idt_set_gate(20, (uint64_t)isr20, 0x08, 0x8E);
    idt_set_gate(21, (uint64_t)isr21, 0x08, 0x8E);
    idt_set_gate(22, (uint64_t)isr22, 0x08, 0x8E);
    idt_set_gate(23, (uint64_t)isr23, 0x08, 0x8E);
    idt_set_gate(24, (uint64_t)isr24, 0x08, 0x8E);
    idt_set_gate(25, (uint64_t)isr25, 0x08, 0x8E);
    idt_set_gate(26, (uint64_t)isr26, 0x08, 0x8E);
    idt_set_gate(27, (uint64_t)isr27, 0x08, 0x8E);
    idt_set_gate(28, (uint64_t)isr28, 0x08, 0x8E);
    idt_set_gate(29, (uint64_t)isr29, 0x08, 0x8E);
    idt_set_gate(30, (uint64_t)isr30, 0x08, 0x8E);
    idt_set_gate(31, (uint64_t)isr31, 0x08, 0x8E);
    idt_set_gate(32, (uint64_t)isr32, 0x08, 0x8E);
    idt_set_gate(33, (uint64_t)isr33, 0x08, 0x8E);
    idt_set_gate(42, (uint64_t)isr42, 0x08, 0x8E); // E1000

    // 加载 IDT
    idt_load((uint64_t)&idt_ptr);
}