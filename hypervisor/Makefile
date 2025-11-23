# Hypervisor Makefile
# AArch64 Bare Metal OS - Type-1 Hypervisor (EL2)

# Configuration
HV_NAME := aarch64_hypervisor
BUILD_DIR ?= ../build/hypervisor

# Toolchain
CROSS_COMPILE ?= aarch64-linux-gnu-
CC := $(CROSS_COMPILE)gcc
AS := $(CROSS_COMPILE)as
LD := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump

# Directories
CORE_DIR := core
VM_DIR := vm
VCPU_DIR := vcpu
INCLUDE_DIR := include

# Output files
HV_ELF := $(BUILD_DIR)/hypervisor.elf
HV_BIN := $(BUILD_DIR)/hypervisor.bin
HV_MAP := $(BUILD_DIR)/hypervisor.map
HV_LST := $(BUILD_DIR)/hypervisor.lst

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
LDFLAGS := -nostdlib -Map=$(HV_MAP)

# Source files (to be populated in Phase 7)
CORE_SOURCES :=
VM_SOURCES :=
VCPU_SOURCES :=

# Object files
CORE_OBJS := $(patsubst %.S,$(BUILD_DIR)/%.o,$(patsubst %.c,$(BUILD_DIR)/%.o,$(CORE_SOURCES)))
VM_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(VM_SOURCES))
VCPU_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(VCPU_SOURCES))

ALL_OBJS := $(CORE_OBJS) $(VM_OBJS) $(VCPU_OBJS)

# Phony targets
.PHONY: all clean

# Default target
all: $(BUILD_DIR) $(HV_ELF) $(HV_BIN)
	@echo "Hypervisor build complete: $(HV_ELF)"

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Link hypervisor
$(HV_ELF): $(ALL_OBJS)
	@echo "Linking hypervisor..."
	@echo "Note: Hypervisor implementation starts in Phase 7"
	@echo "Creating placeholder hypervisor..."
	@mkdir -p $(BUILD_DIR)
	@touch $(HV_ELF)

# Create binary
$(HV_BIN): $(HV_ELF)
	@echo "Creating hypervisor binary..."
	@touch $(HV_BIN)

# Create disassembly listing
$(HV_LST): $(HV_ELF)
	$(OBJDUMP) -D $(HV_ELF) > $(HV_LST)

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
	@echo "Hypervisor build configuration:"
	@echo "  Target: $(HV_ELF)"
	@echo "  CFLAGS: $(CFLAGS)"
	@echo "  LDFLAGS: $(LDFLAGS)"
