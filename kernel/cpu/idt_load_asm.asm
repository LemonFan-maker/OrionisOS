[bits 64]
global idt_load
idt_load:
    lidt [rdi] ; 加载 IDT 指针
    ret