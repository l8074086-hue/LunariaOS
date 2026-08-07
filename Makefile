ASM = nasm
CC = cc
LD = ld
OBJCOPY = objcopy
BUILD_DIR = bin
SRC_DIR = src
KERNEL_DIR = $(SRC_DIR)/kernel

.DEFAULT_GOAL := all

CFLAGS = -m32 -ffreestanding -nostdlib -nostartfiles -Wall -Wextra -std=c99 -I$(SRC_DIR)/headers -I$(SRC_DIR)/lib -I$(SRC_DIR)/home/lib -fno-stack-protector -fno-pic -fno-builtin -fno-asynchronous-unwind-tables -mno-sse -mno-mmx -mno-80387 -MMD -MP
LDFLAGS = -m elf_i386 -T $(KERNEL_DIR)/linker.ld
ASMFLAGS_BIN = -f bin
ASMFLAGS_ELF = -f elf32

DISK_IMG = $(BUILD_DIR)/disk.img
DISK_SIZE_MB = 16

DEPS := $(wildcard $(BUILD_DIR)/*.d)
-include $(DEPS)

PROG_SRCS = $(wildcard src/home/*.c)
PROGS = $(patsubst src/home/%.c,$(BUILD_DIR)/prog_%.bin,$(PROG_SRCS))

all: $(BUILD_DIR)/OS.bin $(DISK_IMG)

$(BUILD_DIR):
	@mkdir -p $@

$(BUILD_DIR)/OS.bin: $(BUILD_DIR)/boot.bin $(BUILD_DIR)/kernel.bin | $(BUILD_DIR)
	cat $^ > $@

$(BUILD_DIR)/fs_seeder: tools/fs_seeder.c $(PROGS) | $(BUILD_DIR)
	$(CC) -Wall -Wextra -o $@ $<

$(DISK_IMG): $(BUILD_DIR)/boot.bin $(BUILD_DIR)/kernel.bin $(BUILD_DIR)/fs_seeder $(PROGS) | $(BUILD_DIR)
	@dd if=/dev/zero of=$@ bs=1M count=$(DISK_SIZE_MB) status=none
	$(BUILD_DIR)/fs_seeder $(PROGS)
	dd if=$(BUILD_DIR)/boot.bin of=$@ conv=notrunc status=none
	dd if=$(BUILD_DIR)/kernel.bin of=$@ bs=512 seek=1 conv=notrunc status=none

KERNEL_SECTORS = $(shell echo $$(( $$(stat -c %s $(BUILD_DIR)/kernel.bin 2>/dev/null || echo 512) / 512 )))

$(BUILD_DIR)/boot.bin: $(SRC_DIR)/boot/boot.asm $(BUILD_DIR)/kernel.bin | $(BUILD_DIR)
	$(ASM) $(ASMFLAGS_BIN) -DKERNEL_SECTORS=$(KERNEL_SECTORS) $< -o $@

$(BUILD_DIR)/kernel.bin: $(BUILD_DIR)/kernel.elf | $(BUILD_DIR)
	$(OBJCOPY) -O binary $< $@
	truncate -s %512 $@

$(BUILD_DIR)/prog_%.o: src/home/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/prog_%.elf: $(BUILD_DIR)/prog_%.o src/home/prog.ld | $(BUILD_DIR)
	$(LD) -m elf_i386 -T src/home/prog.ld -o $@ $<

$(BUILD_DIR)/prog_%.bin: $(BUILD_DIR)/prog_%.elf | $(BUILD_DIR)
	$(OBJCOPY) -O binary $< $@

$(BUILD_DIR)/kernel.elf: $(BUILD_DIR)/entry.o $(BUILD_DIR)/kernel.o $(BUILD_DIR)/vga.o $(BUILD_DIR)/keyboard.o $(BUILD_DIR)/ata.o $(BUILD_DIR)/idt.o $(BUILD_DIR)/isr.o $(BUILD_DIR)/shell.o $(BUILD_DIR)/wm.o $(BUILD_DIR)/fs.o $(BUILD_DIR)/gdt.o $(BUILD_DIR)/syscall.o | $(BUILD_DIR)
	$(LD) $(LDFLAGS) -o $@ $^

$(BUILD_DIR)/entry.o: $(KERNEL_DIR)/entry.asm | $(BUILD_DIR)
	$(ASM) $(ASMFLAGS_ELF) $< -o $@

$(BUILD_DIR)/isr.o: $(KERNEL_DIR)/isr.asm | $(BUILD_DIR)
	$(ASM) $(ASMFLAGS_ELF) $< -o $@

$(BUILD_DIR)/kernel.o: $(KERNEL_DIR)/kernel.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/idt.o: $(KERNEL_DIR)/idt.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/shell.o: $(KERNEL_DIR)/shell.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/wm.o: $(KERNEL_DIR)/wm.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/vga.o: $(SRC_DIR)/drivers/vga.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/keyboard.o: $(SRC_DIR)/drivers/keyboard.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/ata.o: $(SRC_DIR)/drivers/ata.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/fs.o: $(SRC_DIR)/drivers/fs.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/gdt.o: $(KERNEL_DIR)/gdt.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/syscall.o: $(KERNEL_DIR)/syscall.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

run: $(DISK_IMG)
	-pkill qemu 2>/dev/null
	qemu-system-x86_64 -no-reboot -drive format=raw,file=$(DISK_IMG)

headless: $(DISK_IMG)
	-pkill qemu 2>/dev/null
	qemu-system-x86_64 -display curses -monitor none -no-reboot -drive format=raw,file=$(DISK_IMG)

release: $(DISK_IMG) $(BUILD_DIR)/OS.bin
	@mkdir -p release
	@cp $(DISK_IMG) $(BUILD_DIR)/OS.bin run.sh release/
	@chmod +x release/run.sh
	@tar -czf lunariaos-$(shell git describe --tags --always 2>/dev/null || echo dev).tar.gz -C release disk.img OS.bin run.sh
	@rm -rf release
	@echo "created lunariaos-*.tar.gz"

clean:
	rm -rf $(BUILD_DIR)
