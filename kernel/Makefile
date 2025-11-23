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

# Source files (to be populated in Phase 4)
ARCH_SOURCES :=
CORE_SOURCES :=
MM_SOURCES :=
SCHED_SOURCES :=
DRIVER_SOURCES :=

# Object files
ARCH_OBJS := $(patsubst %.S,$(BUILD_DIR)/%.o,$(patsubst %.c,$(BUILD_DIR)/%.o,$(ARCH_SOURCES)))
CORE_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(CORE_SOURCES))
MM_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(MM_SOURCES))
SCHED_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SCHED_SOURCES))
DRIVER_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(DRIVER_SOURCES))

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
	@echo "Note: Kernel implementation starts in Phase 4"
	@echo "Creating placeholder kernel..."
	@mkdir -p $(BUILD_DIR)
	@touch $(KERNEL_ELF)

# Create binary
$(KERNEL_BIN): $(KERNEL_ELF)
	@echo "Creating kernel binary..."
	@touch $(KERNEL_BIN)

# Create disassembly listing
$(KERNEL_LST): $(KERNEL_ELF)
	$(OBJDUMP) -D $(KERNEL_ELF) > $(KERNEL_LST)

# Compile C sources
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile assembly sources
$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean
clean:
	rm -rf $(BUILD_DIR)

# Info
info:
	@echo "Kernel build configuration:"
	@echo "  Target: $(KERNEL_ELF)"
	@echo "  CFLAGS: $(CFLAGS)"
	@echo "  LDFLAGS: $(LDFLAGS)"
