#include "panic.h"
#include "drivers/tty.h"
#include "boot.h"

// 外部依赖
extern uint32_t current_bg_color;

const char* get_error_message(PanicCode code) {
    switch (code) {
        case PanicCode::DivisionByZero:               return "Division by zero";
        case PanicCode::DebugException:               return "Debug exception";
        case PanicCode::NonMaskableInterrupt:         return "Non-maskable interrupt";
        case PanicCode::Breakpoint:                   return "Breakpoint exception";
        case PanicCode::Overflow:                     return "Overflow exception";
        case PanicCode::BoundRangeExceeded:           return "Bound range exceeded";
        case PanicCode::InvalidOpcode:                return "Invalid opcode";
        case PanicCode::DeviceNotAvailable:           return "Device not available";
        case PanicCode::DoubleFault:                  return "Double fault";
        case PanicCode::CoprocessorSegmentOverrun:    return "Coprocessor segment overrun";
        case PanicCode::InvalidTSS:                   return "Invalid TSS";
        case PanicCode::SegmentNotPresent:            return "Segment not present";
        case PanicCode::StackSegmentFault:            return "Stack segment fault";
        case PanicCode::GeneralProtectionFault:       return "General protection fault";
        case PanicCode::PageFault:                    return "Page fault";
        case PanicCode::x87FloatingPointException:    return "x87 floating-point exception";
        case PanicCode::AlignmentCheck:               return "Alignment check";
        case PanicCode::MachineCheck:                 return "Machine check";
        case PanicCode::SIMD_FloatingPointException:  return "SIMD floating-point exception";
        case PanicCode::VirtualizationException:      return "Virtualization exception";
        case PanicCode::ControlProtectionException:   return "Control protection exception";
        case PanicCode::HypervisorInjectionException: return "Hypervisor injection exception";
        case PanicCode::VMMCommunicationException:    return "VMM communication exception";
        case PanicCode::SecurityException:            return "Security exception";
        default:                                      return "Unknown error";
    }
}

__attribute__((noreturn))
void kernel_panic(registers_t* regs, uint64_t int_no) {
    // 1. 关闭中断，防止进一步的混乱
    asm volatile("cli");

    const char* message = get_error_message(static_cast<PanicCode>(int_no));

    // 2. 用醒目的颜色清空屏幕
    current_bg_color = 0xAA0000; // 深红色
    tty_clear();

    // 3. 打印恐慌信息
    tty_print("\n*** KERNEL PANIC ***\n\n", 0xFFFFFF);
    tty_print("A fatal error has occurred and the system has been halted.\n", 0xFFFFFF);
    tty_print("Interrupt Number: ", 0xFFFFFF);
    print_hex(int_no, 0xFFFF00); // 红色
    tty_print("\n", 0xFFFFFF);
    tty_print("Reason: ", 0xFFFFFF);
    tty_print(message, 0xFFFF00); // 黄色

    // 4. 打印寄存器状态
    if (regs) {
        tty_print("\n\nRegister Dump:\n", 0xFFFFFF);
        tty_print("  RAX=", 0xFFFFFF); print_hex(regs->rax, 0x00FFFF);
        tty_print("  RBX=", 0xFFFFFF); print_hex(regs->rbx, 0x00FFFF);
        tty_print("\n  RIP=", 0xFFFFFF); print_hex(regs->rip, 0x00FFFF);
        tty_print("   CS=", 0xFFFFFF); print_hex(regs->cs, 0x00FFFF);
        tty_print(" RFLAGS=", 0xFFFFFF); print_hex(regs->rflags, 0x00FFFF);
    }
    
    // 5. 永久停机
    for (;;) {
        asm volatile("hlt");
    }
}