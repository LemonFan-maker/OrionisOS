#pragma once
#include "cpu/isr.h" // 需要 registers_t 结构

enum class PanicCode {
    DivisionByZero = 0,           // 除零错误
    DebugException = 1,           // 调试异常
    NonMaskableInterrupt = 2,     // 非屏蔽中断
    Breakpoint = 3,               // 断点异常
    Overflow = 4,                 // 溢出异常
    BoundRangeExceeded = 5,       // 范围超出错误
    InvalidOpcode = 6,            // 无效操作码
    DeviceNotAvailable = 7,       // 设备不可用
    DoubleFault = 8,              // 双重故障
    CoprocessorSegmentOverrun = 9,// 协处理器段溢出
    InvalidTSS = 10,              // 无效TSS
    SegmentNotPresent = 11,       // 段不存在
    StackSegmentFault = 12,       // 堆栈段错误
    GeneralProtectionFault = 13,  // 一般保护错误
    PageFault = 14,               // 页面错误
    Reserved1 = 15,               // 保留
    x87FloatingPointException = 16, // x87浮点异常
    AlignmentCheck = 17,          // 对齐检查
    MachineCheck = 18,            // 机器检查
    SIMD_FloatingPointException = 19, // SIMD浮点异常
    VirtualizationException = 20, // 虚拟化异常
    ControlProtectionException = 21, // 控制保护异常
    Reserved2 = 22,               // 保留
    HypervisorInjectionException = 28, // Hypervisor注入异常
    VMMCommunicationException = 29, // VMM通信异常
    SecurityException = 30,       // 安全异常
    Reserved3 = 31,               // 保留
};

const char* get_error_message(PanicCode code);

// __attribute__((noreturn)) 告诉编译器这个函数永远不会返回
void kernel_panic(registers_t* regs, uint64_t int_no);
