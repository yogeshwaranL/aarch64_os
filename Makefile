# AArch64 Bare Metal OS - Main Makefile
# Target: QEMU virt machine (Cortex-A53)

# Project configuration
PROJECT_NAME := aarch64_os
VERSION := 0.1.0

# Directories
BUILD_DIR := build
KERNEL_DIR := kernel
DOCS_DIR := docs
SCRIPTS_DIR := scripts

# Output directories
KERNEL_BUILD_DIR := $(BUILD_DIR)/kernel

# Toolchain
CROSS_COMPILE ?= aarch64-linux-gnu-
CC := $(CROSS_COMPILE)gcc
AS := $(CROSS_COMPILE)as
LD := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump
AR := $(CROSS_COMPILE)ar

# QEMU configuration
QEMU := qemu-system-aarch64
QEMU_MACHINE := virt,virtualization=on
QEMU_CPU := cortex-a53
QEMU_MEMORY := 1G
QEMU_SMP := 4

# Compiler flags
ARCH_FLAGS := -march=armv8-a -mtune=cortex-a53
COMMON_CFLAGS := -Wall -Wextra -Werror -std=gnu11 -nostdlib -ffreestanding \
                 -fno-common -fno-builtin -fno-stack-protector \
                 $(ARCH_FLAGS)

# Optimization flags
DEBUG_FLAGS := -O0 -g3 -DDEBUG
RELEASE_FLAGS := -O2 -DNDEBUG

# Default to debug build
CFLAGS ?= $(COMMON_CFLAGS) $(DEBUG_FLAGS)

# Output binaries
KERNEL_ELF := $(KERNEL_BUILD_DIR)/kernel.elf
KERNEL_BIN := $(KERNEL_BUILD_DIR)/kernel.bin

# Phony targets
.PHONY: all clean distclean help \
        kernel \
        run run-debug \
        docs docs-html \
        check-tools info

# Default target
all: check-tools kernel
	@echo "======================================"
	@echo "Build complete: $(PROJECT_NAME) v$(VERSION)"
	@echo "======================================"

# Help target
help:
	@echo "AArch64 Bare Metal OS - Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all              - Build kernel"
	@echo "  kernel           - Build OS kernel"
	@echo "  run              - Run in QEMU"
	@echo "  run-debug        - Run in QEMU with GDB stub"
	@echo "  docs             - Generate documentation"
	@echo "  docs-html        - Generate HTML documentation (Doxygen)"
	@echo "  clean            - Clean build artifacts"
	@echo "  distclean        - Deep clean all build files"
	@echo "  check-tools      - Check required tools are installed"
	@echo "  info             - Display build configuration"
	@echo ""
	@echo "Variables:"
	@echo "  CROSS_COMPILE    - Toolchain prefix (default: aarch64-linux-gnu-)"
	@echo "  DEBUG=1          - Build with debug symbols"
	@echo "  V=1              - Verbose build output"

# Check required tools
check-tools:
	@echo "Checking required tools..."
	@command -v $(CC) >/dev/null 2>&1 || \
		(echo "Error: $(CC) not found. Install aarch64 toolchain." && exit 1)
	@command -v $(QEMU) >/dev/null 2>&1 || \
		(echo "Error: $(QEMU) not found. Install QEMU." && exit 1)
	@command -v python3 >/dev/null 2>&1 || \
		(echo "Error: python3 not found." && exit 1)
	@command -v git >/dev/null 2>&1 || \
		(echo "Error: git not found." && exit 1)
	@echo "All required tools found."

# Create build directories
$(BUILD_DIR):
	mkdir -p $(KERNEL_BUILD_DIR)

# ==================== KERNEL ====================

kernel: $(BUILD_DIR)
	@echo "======================================"
	@echo "Building OS Kernel..."
	@echo "======================================"
	@$(MAKE) -C $(KERNEL_DIR) BUILD_DIR=../$(KERNEL_BUILD_DIR)
	@echo "Kernel build complete"

# ==================== QEMU ====================

# Run in QEMU
run: kernel
	@echo "======================================"
	@echo "Starting QEMU..."
	@echo "======================================"
	$(QEMU) \
		-machine $(QEMU_MACHINE) \
		-cpu $(QEMU_CPU) \
		-smp $(QEMU_SMP) \
		-m $(QEMU_MEMORY) \
		-nographic \
		-serial mon:stdio \
		-kernel $(KERNEL_ELF) \
		-nic none

# Run in QEMU with GDB debugging
run-debug: kernel
	@echo "======================================"
	@echo "Starting QEMU with GDB stub (port 1234)..."
	@echo "Connect with: gdb-multiarch $(KERNEL_ELF) -ex 'target remote :1234'"
	@echo "======================================"
	$(QEMU) \
		-machine $(QEMU_MACHINE) \
		-cpu $(QEMU_CPU) \
		-smp $(QEMU_SMP) \
		-m $(QEMU_MEMORY) \
		-nographic \
		-serial mon:stdio \
		-kernel $(KERNEL_ELF) \
		-nic none \
		-s -S

# ==================== DOCUMENTATION ====================

docs: docs-html
	@echo "Documentation generation complete"

docs-html:
	@echo "Generating HTML documentation with Doxygen..."
	@if [ -f "Doxyfile" ]; then \
		doxygen Doxyfile; \
	else \
		echo "Doxyfile not found."; \
	fi

# ==================== CLEAN ====================

clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(BUILD_DIR)
	@if [ -d "$(KERNEL_DIR)" ]; then $(MAKE) -C $(KERNEL_DIR) clean; fi
	@echo "Clean complete"

distclean: clean
	@echo "Performing deep clean..."
	rm -rf $(BUILD_DIR)
	find . -name "*.o" -delete
	find . -name "*.d" -delete
	find . -name "*.bin" -delete
	find . -name "*.elf" -delete
	@echo "Deep clean complete"

# ==================== INFO ====================

info:
	@echo "======================================"
	@echo "Project: $(PROJECT_NAME) v$(VERSION)"
	@echo "======================================"
	@echo "Toolchain:"
	@echo "  CC:       $(CC)"
	@echo "  AS:       $(AS)"
	@echo "  LD:       $(LD)"
	@echo ""
	@echo "QEMU Config:"
	@echo "  Machine:  $(QEMU_MACHINE)"
	@echo "  CPU:      $(QEMU_CPU)"
	@echo "  Memory:   $(QEMU_MEMORY)"
	@echo "  SMP:      $(QEMU_SMP) cores"
	@echo ""
	@echo "Build directories:"
	@echo "  Build:    $(BUILD_DIR)"
	@echo "  Kernel:   $(KERNEL_BUILD_DIR)"
	@echo "======================================"
