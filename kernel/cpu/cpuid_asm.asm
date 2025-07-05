[bits 64]
global cpuid_vendor

; void cpuid_vendor(char* buffer)
; 接收参数的寄存器: RDI (buffer)

cpuid_vendor:
    ; 保存 rbx，因为它会被 cpuid 修改
    push rbx
    
    ; 设置 cpuid 的功能号为 0
    xor eax, eax ; eax = 0, 这是比 mov eax, 0 更高效的方式

    ; 执行 cpuid 指令
    cpuid
    
    ; cpuid 返回后，制造商字符串在 ebx, edx, ecx
    ; 我们将它们直接写入 buffer 指向的内存地址
    mov [rdi], ebx     ; 将 32 位的 ebx 写入 buffer[0..3]
    mov [rdi + 4], edx ; 将 32 位的 edx 写入 buffer[4..7]
    mov [rdi + 8], ecx ; 将 32 位的 ecx 写入 buffer[8..11]
    
    ; 恢复 rbx
    pop rbx
    
    ret

global cpuid_brand_string
cpuid_brand_string:
    push rbx
    ; Brand string part 1
    mov eax, 0x80000002
    cpuid
    mov [rdi], eax
    mov [rdi + 4], ebx
    mov [rdi + 8], ecx
    mov [rdi + 12], edx
    ; Brand string part 2
    mov eax, 0x80000003
    cpuid
    mov [rdi + 16], eax
    mov [rdi + 20], ebx
    mov [rdi + 24], ecx
    mov [rdi + 28], edx
    ; Brand string part 3
    mov eax, 0x80000004
    cpuid
    mov [rdi + 32], eax
    mov [rdi + 36], ebx
    mov [rdi + 40], ecx
    mov [rdi + 44], edx
    pop rbx
    ret

global cpuid_features
cpuid_features:
    push rbx
    mov eax, 1
    cpuid
    mov [rdi], ecx
    mov [rsi], edx
    pop rbx
    ret
