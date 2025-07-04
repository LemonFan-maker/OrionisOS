[bits 64]

; 声明我们将要调用的 C++ 通用处理器
extern isr_handler

; --- 宏定义：用于生成 ISR 存根 ---
; ISR 无错误码版本
%macro ISR_NO_ERR_CODE 1
[global isr%1]
isr%1:
    cli          ; 关中断，防止嵌套
    push byte 0  ; 压入一个假的错误码，保持堆栈对齐
    push byte %1 ; 压入中断号
    jmp isr_common_stub
%endmacro

; ISR 有错误码版本 (CPU会自动压入一个错误码)
%macro ISR_ERR_CODE 1
[global isr%1]
isr%1:
    cli
    push byte %1 ; 只需压入中断号
    jmp isr_common_stub
%endmacro

; --- 通用存根 ---
; 所有 ISR 都会跳转到这里
isr_common_stub:
    ; 保存所有通用寄存器
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, rsp ; 将堆栈指针 (指向所有保存的寄存器) 作为参数传给 C++ 函数
    call isr_handler

    ; 恢复所有通用寄存器
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    
    add rsp, 16 ; 清理中断号和错误码
    iretq        ; 中断返回 (重要！不是 ret)

; --- 生成前32个 ISR (用于 CPU 异常) ---
ISR_NO_ERR_CODE 0
ISR_NO_ERR_CODE 1
ISR_NO_ERR_CODE 2
ISR_NO_ERR_CODE 3
ISR_NO_ERR_CODE 4
ISR_NO_ERR_CODE 5
ISR_NO_ERR_CODE 6
ISR_NO_ERR_CODE 7
ISR_ERR_CODE    8
ISR_NO_ERR_CODE 9
ISR_ERR_CODE    10
ISR_ERR_CODE    11
ISR_ERR_CODE    12
ISR_ERR_CODE    13
ISR_ERR_CODE    14
ISR_NO_ERR_CODE 15
ISR_NO_ERR_CODE 16
ISR_ERR_CODE    17
ISR_NO_ERR_CODE 18
ISR_NO_ERR_CODE 19
ISR_NO_ERR_CODE 20
; ... 为了简洁，我们先只定义前 32 个异常处理
; 剩下的 21-31 都是无错误码的
ISR_NO_ERR_CODE 21
ISR_NO_ERR_CODE 22
ISR_NO_ERR_CODE 23
ISR_NO_ERR_CODE 24
ISR_NO_ERR_CODE 25
ISR_NO_ERR_CODE 26
ISR_NO_ERR_CODE 27
ISR_NO_ERR_CODE 28
ISR_NO_ERR_CODE 29
ISR_NO_ERR_CODE 30
ISR_NO_ERR_CODE 31
ISR_NO_ERR_CODE 32  ; IRQ0: Programmable Interval Timer
ISR_NO_ERR_CODE 33 ; IRQ1: Keyboard
ISR_NO_ERR_CODE 42  ; IRQ10: E1000 Network Card
ISR_NO_ERR_CODE 43  ; IRQ11: VirtIO Network Card
