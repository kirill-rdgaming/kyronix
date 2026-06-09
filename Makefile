# KyronixOS build system

TARGET     := kernel.elf
ISO        := kyronix.iso
LIMINE_DIR := limine-binary

ifneq (, $(shell which x86_64-elf-gcc 2>/dev/null))
    CC := x86_64-elf-gcc
    LD := x86_64-elf-ld
else
    CC := gcc
    LD := ld
endif

CFLAGS := \
    -std=c11           \
    -O2                \
    -Wall -Wextra      \
    -Wno-unused-parameter \
    -ffreestanding     \
    -fno-stack-protector \
    -fno-pic -fno-pie  \
    -m64 -march=x86-64 \
    -mno-80387         \
    -mno-mmx           \
    -mno-sse           \
    -mno-sse2          \
    -mno-red-zone      \
    -mcmodel=kernel    \
    -Ikernel           \
    -Ikernel/boot

LDFLAGS := \
    -T linker.ld       \
    -nostdlib          \
    -static            \
    -z max-page-size=0x1000

SRCS := \
    kernel/kernel.c         \
    kernel/drivers/serial.c \
    kernel/drivers/fb.c     \
    kernel/lib/string.c     \
    kernel/lib/printf.c

OBJS := $(SRCS:.c=.o)

.PHONY: all iso run clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(LIMINE_DIR)/limine: $(LIMINE_DIR)/limine.c
	$(MAKE) -C $(LIMINE_DIR)

iso: $(TARGET) $(LIMINE_DIR)/limine
	rm -rf iso_root
	mkdir -p iso_root/boot/limine
	mkdir -p iso_root/EFI/BOOT

	cp $(TARGET)              iso_root/boot/kernel.elf
	cp limine.conf            iso_root/boot/limine/limine.conf
	cp $(LIMINE_DIR)/limine-bios.sys    iso_root/boot/limine/
	cp $(LIMINE_DIR)/limine-bios-cd.bin iso_root/boot/limine/
	cp $(LIMINE_DIR)/limine-uefi-cd.bin iso_root/boot/limine/
	cp $(LIMINE_DIR)/BOOTX64.EFI        iso_root/EFI/BOOT/
	cp $(LIMINE_DIR)/BOOTIA32.EFI       iso_root/EFI/BOOT/

	xorriso -as mkisofs              \
	    -b boot/limine/limine-bios-cd.bin \
	    -no-emul-boot                \
	    -boot-load-size 4            \
	    -boot-info-table             \
	    --efi-boot boot/limine/limine-uefi-cd.bin \
	    -efi-boot-part               \
	    --efi-boot-image             \
	    --protective-msdos-label     \
	    iso_root -o $(ISO)

	./$(LIMINE_DIR)/limine bios-install $(ISO)
	@echo ""
	@echo "  Built: $(ISO)"

run: iso
	qemu-system-x86_64              \
	    -M q35                      \
	    -m 2G                       \
	    -cdrom $(ISO)               \
	    -boot d                     \
	    -serial stdio               \
	    -vga qxl                    \
	    -global qxl-vga.vgamem_mb=1024 \
	    -no-reboot                  \
	    -no-shutdown

run-serial: iso
	qemu-system-x86_64              \
	    -M q35                      \
	    -m 2G                       \
	    -cdrom $(ISO)               \
	    -boot d                     \
	    -display none               \
	    -serial stdio               \
	    -no-reboot                  \
	    -no-shutdown

OVMF ?= /usr/share/edk2/x64/OVMF.fd

run-uefi: iso
	qemu-system-x86_64              \
	    -M q35                      \
	    -m 2G                       \
	    -cdrom $(ISO)               \
	    -bios $(OVMF)               \
	    -boot d                     \
	    -serial stdio               \
	    -vga qxl                    \
	    -global qxl-vga.vgamem_mb=1024 \
	    -no-reboot                  \
	    -no-shutdown

clean:
	rm -f $(OBJS) $(TARGET) $(ISO)
	rm -rf iso_root
	$(MAKE) -C $(LIMINE_DIR) clean
