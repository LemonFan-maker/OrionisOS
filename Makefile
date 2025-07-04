# --- 工具链和程序 ---
CXX        = x86_64-elf-g++
LD         = x86_64-elf-ld
NASM       = nasm
LIMINE_EXE = ./limine/limine-install
LIMINE_BIN = ./limine/limine.bin

# --- 文件和目录 ---
SRC_DIRS = kernel kernel/cpu kernel/drivers kernel/mem kernel/drivers/ata command lib kernel/drivers/ethernet
CXX_SOURCES = $(foreach dir, $(SRC_DIRS), $(wildcard $(dir)/*.cpp))
ASM_SOURCES = $(foreach dir, $(SRC_DIRS), $(wildcard $(dir)/*.asm))
ALL_OBJS_WITH_DUPES = $(CXX_SOURCES:.cpp=.o) $(ASM_SOURCES:.asm=.o)
OBJS        = $(CXX_SOURCES:.cpp=.o) $(ASM_SOURCES:.asm=.o)

KERNEL_ELF = kernel.elf
KERNEL_HDD = disk.hdd
LIMINE_CFG = limine.cfg

# --- 编译和链接标志 ---
# 我们只告诉编译器去 limine/ 目录下找 stivale.h
# 其他所有头文件都将通过相对路径找到
CXXFLAGS = -std=c++17 -ffreestanding -fno-exceptions -fno-rtti -Wall -Wextra \
			-I. -Ilimine -Ikernel -Ikernel/drivers -Ikernel/cpu -mno-red-zone -mcmodel=kernel
NASMFLAGS = -f elf64
LDFLAGS = -nostdlib -static -no-pie -z max-page-size=0x1000 -T linker.ld

# --- QEMU 设置 ---
QEMU_CMD   = qemu-system-x86_64
QEMU_FLAGS = -m 2G \
    -device isa-debug-exit,iobase=0x8900,iosize=0x01 \
    -netdev user,id=net0 -device virtio-net-pci,netdev=net0 \
    -device piix3-ide,id=ide0 \
    -device ide-hd,bus=ide0.0,drive=disk0,unit=0,id=disk_a \
    -drive file=$(KERNEL_HDD),if=none,format=raw,id=disk0

# --- Makefile 规则---
.DEFAULT_GOAL := all
all: $(KERNEL_HDD)

$(KERNEL_HDD): $(KERNEL_ELF) $(LIMINE_CFG)
	@echo "==> Creating bootable HDD image..."
	# 确保 Limine 安装工具已编译
	@$(MAKE) -s -C limine limine-install

	# 清理旧的磁盘镜像
	@rm -f $(KERNEL_HDD)

	# 1. 创建一个 64MB 的空磁盘镜像文件
	@dd if=/dev/zero bs=1M count=0 seek=64 of=$(KERNEL_HDD) 2>/dev/null

	# 2. 使用 fdisk 创建 MBR 分区表和分区
	#    这里我们使用 fdisk 的脚本模式，并将其标记为可引导
	@echo -e "o\nn\np\n1\n\n\na\n1\nw\n" | fdisk $(KERNEL_HDD) 2>/dev/null

	# 3. 格式化第一个分区为 FAT32
	#    mformat -i @@@1M: 指定操作的是磁盘镜像的第一个分区 (通常从1MB偏移开始)
	@mformat -i $(KERNEL_HDD)@@1M -F 2>/dev/null

	# 4. 复制必要文件到 FAT32 分区中
	@mcopy -i $(KERNEL_HDD)@@1M $(KERNEL_ELF) ::/kernel.elf 2>/dev/null
	@mcopy -i $(KERNEL_HDD)@@1M $(LIMINE_CFG) ::/limine.cfg 2>/dev/null

	# 5. 使用 limine-install 安装引导程序到整个磁盘镜像上
	#    这会写入 MBR 和 VBR
	@$(LIMINE_EXE) $(LIMINE_BIN) $(KERNEL_HDD)

	@echo "==> HDD image created successfully: $(KERNEL_HDD)"

$(KERNEL_ELF): $(OBJS) linker.ld
	@echo "==> Linking kernel with objects: $(OBJS)"
	@$(LD) $(LDFLAGS) -o $@ $(OBJS)

%.o: %.cpp
	@echo "==> Compiling $<"
	@$(CXX) $(CXXFLAGS) -c $< -o $@

%.o: %.asm
	@echo "==> Assembling $<"
	@$(NASM) $(NASMFLAGS) $< -o $@

run: $(KERNEL_HDD)
	@$(QEMU_CMD) $(QEMU_FLAGS)

.PHONY: all run clean

clean:
	@echo "==> Cleaning up..."
	@rm -f $(KERNEL_ELF) $(KERNEL_HDD) $(foreach dir, $(SRC_DIRS), $(wildcard $(dir)/*.o))