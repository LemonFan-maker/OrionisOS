# OrionisOS - A Custom x86_64 Operating System

![OrionisOS Logo Placeholder](https://img.shields.io/badge/OrionisOS-OSDev-blue?style=for-the-badge)
![Build Status](https://img.shields.io/badge/Build-Passing-brightgreen?style=for-the-badge)
![License](https://img.shields.io/github/license/LemonFan-maker/OrionisOS?style=for-the-badge)

## 🌌 简介 (Introduction)

**OrionisOS** 是一个从零开始构建的 64 位操作系统，运行在 x86_64 架构上，并使用 [Limine](https://github.com/limine-bootloader/limine) 作为其引导加载程序。这个项目不仅仅是一个操作系统，它更是一次深入探索计算机底层原理的实践之旅，涵盖了从 CPU 引导、内存管理到硬件驱动和用户交互界面的方方面面。

在 OrionisOS 中，我们注重模块化设计，将不同功能（如 CPU 控制、设备驱动、内存管理、命令行解释器）分离到独立的模块中，以实现代码的清晰和可维护性。

## ✨ 主要特性 (Key Features)

OrionisOS 已经具备了作为现代操作系统核心的诸多功能，以下是当前版本已实现的亮点：

### 🚀 核心引导与系统初始化 (Core Boot & System Initialization)

**Limine 引导**：通过 Limine 引导加载程序启动内核，并接收来自其的引导信息。

**64 位长模式运行**：内核在 x86_64 架构的 64 位长模式下运行，充分利用现代处理器的能力。

**自定义 `stivale_header`**：符合 `stivale` 引导协议的内核头部，与 Limine 完美握手。

**精确的链接器脚本**：严格控制内核在内存中的布局和虚拟地址映射。

**GDT (全局描述符表) 初始化**：接管 CPU 的分段机制，为未来的内存保护和用户模式奠定基础。

**IDT (中断描述符表) 初始化**：设置中断向量表，能够捕获并响应各种 CPU 异常。

**PIT (可编程间隔计时器) 支持**：配置系统时钟中断，赋予内核“时间”的概念。

**PIC (可编程中断控制器) 重映射**：将硬件中断重映射到安全的向量范围，避免与 CPU 异常冲突。

**全面启用中断 (STI)**：系统准备好接收并处理来自硬件的所有中断信号。

### 🖥️ 显示与交互 (Display & Interaction)

**Framebuffer 图形模式**：直接操作帧缓冲显存，在图形模式下进行像素级绘制。

**自定义 9x16 像素字体**：提供清晰且美观的文本渲染效果。

**多色文本输出**：支持 32 位 RGB 颜色，输出多彩的日志和命令行信息。

**平滑的屏幕滚动**：实现了基于高效 `memcpy` 的屏幕内容平滑滚动功能。

**键盘驱动**：通过中断捕获键盘事件，支持扫描码到 ASCII 的转换，处理 Shift、Caps Lock、Backspace、Enter 等按键。

### 🧠 资源管理 (Resource Management)

**PMM (物理内存管理器)**：解析 Limine 提供的内存映射表，使用位图算法管理物理内存页的分配和释放。这是实现虚拟内存和动态内存分配的基础。

### 🌐 网络能力 (Networking) - (基础已具备，未来可扩展)

**PCI 总线扫描**：能够枚举并识别系统上连接的所有 PCI 设备。

**Intel E1000 网卡识别与基础初始化**：成功识别 QEMU 模拟的 E1000 网卡，读取其 MMIO 地址、MAC 地址，并设置发送/接收 DMA 环形缓冲区。

**以太网帧发送**：能够构建并发送原始的以太网帧，开启了与外部世界通信的大门。

### 🐚 命令行 Shell (Command Line Interface)

**模块化设计**：Shell 逻辑被清晰地分离到独立的 `command/` 模块，易于扩展和维护。

**命令缓冲区管理**：支持命令输入、回显和精确的退格操作。

**丰富的内置命令**：

`help`：显示所有可用命令的列表。

`clear`：清空屏幕内容，并重置光标到顶部。

`echo <text>`：将输入的文本回显到屏幕。

`shutdown`：关闭 QEMU 虚拟机。

`meminfo`：显示当前物理内存的使用情况（总量、已用、空闲，以 MiB/KiB 显示）。

`cpuinfo`：显示 CPU 制造商信息（通过 `CPUID` 指令获取）。

`time`：显示当前的系统时间（从 RTC 芯片读取）。

`panic`：主动触发一次内核恐慌，用于测试异常处理和调试。

`version`：显示内核和 Shell 的版本信息。

`nettest`：执行网络测试，尝试发送一个 ARP 请求。

`lspci`：查看目前所有的PCI设备。

`reboot`：重启系统。

**调试模式**：通过 `debug` / `undebug` 命令，动态开启/关闭命令解析的详细调试信息。

## 🛠️ 技术栈 (Technology Stack)

**编程语言**：C++17 (所有内核代码。~~胡说！在我看来全是C!~~) 和 NASM 汇编 (底层引导、GDT、IDT、CPUID、memcpy)。

**架构**：x86_64。

**引导加载程序**：Limine。

**开发环境**：Linux / macOS (需要 `x86_64-elf-gcc` 交叉编译工具链)。

**虚拟机**：QEMU。

**构建系统**：GNU Make。

**外部工具**：`fdisk`, `mtools` (用于操作 FAT32 镜像)。

## 🚀 构建与运行 (Building & Running)

### 前提条件 (Prerequisites)

1. **x86_64-elf 交叉编译器**：你需要安装一个针对 `x86_64-elf` 目标的 GCC 工具链。在 Linux 上，可以手动编译 [GNU Toolchain for x86_64-elf](https://wiki.osdev.org/GCC_Cross-Compiler)，或使用像 `limine-bootloader/limine` 项目提供的 `toolchain/make_toolchain.sh` 脚本。 
2. **QEMU**：安装 `qemu-system-x86_64`。 
3. **其他工具**：`make`, `fdisk` (或 `parted`), `mtools` (例如 `mformat`, `mcopy`)。

**在 Debian/Ubuntu 上安装**:
```bash
sudo apt update
sudo apt install build-essential qemu-system-x86 fdisk mtools parted
# 安装交叉编译器需要手动编译或查找预构建版本，也可以寻找预编译版本
```

**在 Arch Linux 上安装**:
```bash
sudo pacman -S base-devel qemu gptfdisk mtools parted yay
# 安装交叉编译器
sudo yay -S x86_64-elf-gcc x86_64-elf-binutils
```

### 构建步骤 (Build Steps)

在项目根目录下打开终端：
```bash
# 清理所有之前编译生成的文件
make clean

# 编译整个操作系统
# -j 参数会根据你的 CPU 核心数进行并行编译，加快速度
make -j
```

如果构建成功，你将在项目根目录下找到 `kernel.elf` (内核可执行文件) 和 `disk.hdd` (可引导的磁盘镜像)。

### 运行操作系统 (Running OrionisOS)

```bash
make run
```
这将在 QEMU 虚拟机中启动 OrionisOS。

## ⌨️ 使用 OrionisOS (Usage)

当 OrionisOS 在 QEMU 中启动后，你将看到初始化信息，然后是 Shell 提示符 `> `。
你可以尝试的命令参考上文`命令行 (Shell)`

## 🗺️ 未来展望 (Future Plans / Roadmap)

OrionisOS 仍处于早期开发阶段，但已经具备了坚实的基础。未来的发展方向包括：

**动态内存分配 (Heap Manager)**：实现 `kmalloc` 和 `kfree`，支持任意大小的内存分配。

**虚拟内存与分页 (Virtual Memory & Paging)**：为每个进程提供独立的虚拟地址空间，实现内存保护。

**多任务与进程调度**：支持同时运行多个程序。

**ELF 加载器**：能够加载并执行磁盘上的用户程序。

**文件系统增强**：实现完整的 FAT32 读写支持，甚至探索 EXT2/EXT4。

**更完善的设备驱动**：硬盘驱动，USB 驱动，计时器增强 (HPET)。

**网络协议栈**：实现 ARP、IP、ICMP、UDP、TCP 等协议。

**图形用户界面 (GUI)**：在 Framebuffer 模式下绘制窗口、鼠标指针等。

**移植**：在闲得无聊的时候可以想象一下移植到 aarch64 上该有多难。

## 🙏 鸣谢 (Acknowledgements)

**Limine Bootloader**：感谢其强大的引导加载程序和提供的基础库。

**OSDev Wiki**：操作系统开发者的宝贵知识库。

**Linux Kernel Developers**：特别是 `e1000` 和 `Virtio Ethernet`驱动的开发者，他们的开源代码是学习和理解底层硬件交互的无价之宝。(有可能是我太废物，导致到现在都没研究明白E1000是如何接收 ARP 请求QAQ)

**BruhOS**：感谢其在 GDT 和启动流程方面提供的宝贵参考。

## 核心贡献者

| 贡献者 | 功能 | 影响 |
|:------:|:----:|:----:|
| [@aba2222](https://github.com/aba2222) | Buddy内存分配器 | 提升内存分配性能 |

## 📄 许可证 (License)

本项目采用 GPL 3.0 许可证。详见 `LICENSE` 文件。

---
**Happy Hacking!**
