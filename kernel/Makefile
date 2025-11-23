# Kernel Makefile
# AArch64 Bare Metal OS - Kernel

# Configuration
KERNEL_NAME := aarch64_kernel
BUILD_DIR ?= ../build/kernel

# Toolchain
CROSS_COMPILE ?= aarch64-linux-gnu-
CC := $(CROSS_COMPILE)gcc
AS := $(CROSS_COMPILE)as
LD := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump

# Directories
ARCH_DIR := arch/aarch64
CORE_DIR := core
MM_DIR := mm
SCHED_DIR := sched
DRIVERS_DIR := drivers
INCLUDE_DIR := include

# Output files
KERNEL_ELF := $(BUILD_DIR)/kernel.elf
KERNEL_BIN := $(BUILD_DIR)/kernel.bin
KERNEL_MAP := $(BUILD_DIR)/kernel.map
KERNEL_LST := $(BUILD_DIR)/kernel.lst

# Compiler flags
ARCH_FLAGS := -march=armv8-a -mtune=cortex-a53
CFLAGS := -Wall -Wextra -Werror -std=gnu11 \
          -nostdlib -ffreestanding -fno-common \
          -fno-builtin -fno-stack-protector \
          -I$(INCLUDE_DIR) \
          $(ARCH_FLAGS)

# Debug vs Release
ifdef DEBUG
CFLAGS += -O0 -g3 -DDEBUG
else
CFLAGS += -O2 -DNDEBUG
endif

# Linker flags
LDFLAGS := -nostdlib -T $(ARCH_DIR)/linker.ld -Map=$(KERNEL_MAP)

# Source files (Phase 4-7)
ARCH_SOURCES := $(ARCH_DIR)/entry.S
CORE_SOURCES := $(CORE_DIR)/main.c $(CORE_DIR)/string.c $(CORE_DIR)/irq.c $(CORE_DIR)/exception.c $(CORE_DIR)/syscall.c $(CORE_DIR)/hypervisor.c
MM_SOURCES := $(MM_DIR)/mmu.c $(MM_DIR)/pmm.c $(MM_DIR)/kmalloc.c
SCHED_SOURCES := $(SCHED_DIR)/sched.c $(SCHED_DIR)/switch.S
DRIVER_SOURCES := $(DRIVERS_DIR)/uart.c $(DRIVERS_DIR)/gic.c $(DRIVERS_DIR)/timer.c

# Object files
ARCH_OBJS := $(patsubst $(ARCH_DIR)/%.S,$(BUILD_DIR)/arch/%.o,$(ARCH_SOURCES))
CORE_OBJS := $(patsubst $(CORE_DIR)/%.c,$(BUILD_DIR)/core/%.o,$(CORE_SOURCES))
MM_OBJS := $(patsubst $(MM_DIR)/%.c,$(BUILD_DIR)/mm/%.o,$(MM_SOURCES))
SCHED_OBJS := $(patsubst $(SCHED_DIR)/%.c,$(BUILD_DIR)/sched/%.o,$(filter %.c,$(SCHED_SOURCES))) \
              $(patsubst $(SCHED_DIR)/%.S,$(BUILD_DIR)/sched/%.o,$(filter %.S,$(SCHED_SOURCES)))
DRIVER_OBJS := $(patsubst $(DRIVERS_DIR)/%.c,$(BUILD_DIR)/drivers/%.o,$(DRIVER_SOURCES))

ALL_OBJS := $(ARCH_OBJS) $(CORE_OBJS) $(MM_OBJS) $(SCHED_OBJS) $(DRIVER_OBJS)

# Phony targets
.PHONY: all clean

# Default target
all: $(BUILD_DIR) $(KERNEL_ELF) $(KERNEL_BIN)
	@echo "Kernel build complete: $(KERNEL_ELF)"

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Link kernel
$(KERNEL_ELF): $(ALL_OBJS)
	@echo "Linking kernel..."
	$(LD) $(LDFLAGS) $(ALL_OBJS) -o $@
	@echo "Kernel linked: $@"
	@ls -lh $@

# Create binary
$(KERNEL_BIN): $(KERNEL_ELF)
	@echo "Creating kernel binary..."
	$(OBJCOPY) -O binary $< $@
	@ls -lh $@

# Create disassembly listing
$(KERNEL_LST): $(KERNEL_ELF)
	$(OBJDUMP) -D $(KERNEL_ELF) > $(KERNEL_LST)

# Compile C sources
$(BUILD_DIR)/core/%.o: $(CORE_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "CC $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/drivers/%.o: $(DRIVERS_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "CC $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/mm/%.o: $(MM_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "CC $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/sched/%.o: $(SCHED_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "CC $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Compile assembly sources
$(BUILD_DIR)/arch/%.o: $(ARCH_DIR)/%.S
	@mkdir -p $(dir $@)
	@echo "AS $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/sched/%.o: $(SCHED_DIR)/%.S
	@mkdir -p $(dir $@)
	@echo "AS $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Clean
clean:
	rm -rf $(BUILD_DIR)

# Info
info:
	@echo "Kernel build configuration:"
	@echo "  Target: $(KERNEL_ELF)"
	@echo "  CFLAGS: $(CFLAGS)"
	@echo "  LDFLAGS: $(LDFLAGS)"
