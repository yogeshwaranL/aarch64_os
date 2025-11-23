# AArch64 Bare Metal OS - Main Makefile
# Target: QEMU virt machine (Cortex-A53)

# Project configuration
PROJECT_NAME := aarch64_os
VERSION := 0.1.0

# Directories
BUILD_DIR := build
FIRMWARE_DIR := firmware
KERNEL_DIR := kernel
HYPERVISOR_DIR := hypervisor
ACPI_DIR := acpi
DOCS_DIR := docs
SCRIPTS_DIR := scripts

# Output directories
FW_BUILD_DIR := $(BUILD_DIR)/firmware
KERNEL_BUILD_DIR := $(BUILD_DIR)/kernel
HYPERVISOR_BUILD_DIR := $(BUILD_DIR)/hypervisor
ACPI_BUILD_DIR := $(BUILD_DIR)/acpi

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
QEMU_MACHINE := virt
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

# Firmware paths (ATF and EDK2)
ATF_DIR := $(FIRMWARE_DIR)/atf
ATF_BUILD := $(ATF_DIR)/build/qemu/release
EDK2_DIR := $(FIRMWARE_DIR)/edk2
EDK2_BUILD := $(EDK2_DIR)/Build

# Output binaries
BL1_BIN := $(FW_BUILD_DIR)/bl1.bin
BL2_BIN := $(FW_BUILD_DIR)/bl2.bin
BL31_BIN := $(FW_BUILD_DIR)/bl31.bin
BL33_BIN := $(FW_BUILD_DIR)/bl33.bin
KERNEL_ELF := $(KERNEL_BUILD_DIR)/kernel.elf
KERNEL_BIN := $(KERNEL_BUILD_DIR)/kernel.bin
HYPERVISOR_ELF := $(HYPERVISOR_BUILD_DIR)/hypervisor.elf
HYPERVISOR_BIN := $(HYPERVISOR_BUILD_DIR)/hypervisor.bin

# Flash image
FLASH_IMG := $(BUILD_DIR)/flash.img

# Phony targets
.PHONY: all clean distclean help \
        firmware atf edk2 \
        kernel hypervisor acpi \
        run run-debug \
        docs docs-html docs-pdf \
        init-submodules \
        check-tools

# Default target
all: check-tools firmware kernel hypervisor
	@echo "======================================"
	@echo "Build complete: $(PROJECT_NAME) v$(VERSION)"
	@echo "======================================"

# Help target
help:
	@echo "AArch64 Bare Metal OS - Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all              - Build everything (firmware, kernel, hypervisor)"
	@echo "  firmware         - Build all firmware (ATF + EDK2)"
	@echo "  atf              - Build ARM Trusted Firmware"
	@echo "  edk2             - Build UEFI EDK2"
	@echo "  kernel           - Build OS kernel"
	@echo "  hypervisor       - Build hypervisor"
	@echo "  acpi             - Build ACPI components"
	@echo "  run              - Run in QEMU"
	@echo "  run-debug        - Run in QEMU with GDB stub"
	@echo "  docs             - Generate all documentation"
	@echo "  docs-html        - Generate HTML documentation (Doxygen)"
	@echo "  clean            - Clean build artifacts"
	@echo "  distclean        - Clean everything including firmware"
	@echo "  init-submodules  - Initialize git submodules (ATF, EDK2)"
	@echo "  check-tools      - Check required tools are installed"
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

# Initialize git submodules
init-submodules:
	@echo "Initializing git submodules..."
	git submodule update --init --recursive

# Create build directories
$(BUILD_DIR):
	mkdir -p $(FW_BUILD_DIR) $(KERNEL_BUILD_DIR) $(HYPERVISOR_BUILD_DIR) $(ACPI_BUILD_DIR)

# ==================== FIRMWARE ====================

firmware: atf edk2
	@echo "Firmware build complete"

# ARM Trusted Firmware
atf: $(BUILD_DIR)
	@echo "======================================"
	@echo "Building ARM Trusted Firmware (ATF)..."
	@echo "======================================"
	@if [ ! -d "$(ATF_DIR)" ]; then \
		echo "Error: ATF directory not found. Run 'make init-submodules'"; \
		exit 1; \
	fi
	@$(MAKE) -C $(ATF_DIR) \
		PLAT=qemu \
		ARCH=aarch64 \
		CROSS_COMPILE=$(CROSS_COMPILE) \
		DEBUG=1 \
		bl1 bl2 bl31
	@mkdir -p $(FW_BUILD_DIR)
	@cp $(ATF_DIR)/build/qemu/debug/bl1.bin $(FW_BUILD_DIR)/
	@cp $(ATF_DIR)/build/qemu/debug/bl2.bin $(FW_BUILD_DIR)/
	@cp $(ATF_DIR)/build/qemu/debug/bl31.bin $(FW_BUILD_DIR)/
	@echo "ATF binaries copied to $(FW_BUILD_DIR)"

# UEFI EDK2
edk2: $(BUILD_DIR)
	@echo "======================================"
	@echo "Building UEFI EDK2..."
	@echo "======================================"
	@if [ ! -d "$(EDK2_DIR)" ]; then \
		echo "Error: EDK2 directory not found. Run 'make init-submodules'"; \
		exit 1; \
	fi
	@echo "EDK2 will be built in Phase 3"
	@echo "Placeholder: EDK2 build"
	@mkdir -p $(FW_BUILD_DIR)
	@touch $(FW_BUILD_DIR)/edk2-placeholder.txt

# ==================== KERNEL ====================

kernel: $(BUILD_DIR)
	@echo "======================================"
	@echo "Building OS Kernel..."
	@echo "======================================"
	@$(MAKE) -C $(KERNEL_DIR) BUILD_DIR=../$(KERNEL_BUILD_DIR)
	@echo "Kernel build complete"

# ==================== HYPERVISOR ====================

hypervisor: $(BUILD_DIR)
	@echo "======================================"
	@echo "Building Hypervisor..."
	@echo "======================================"
	@$(MAKE) -C $(HYPERVISOR_DIR) BUILD_DIR=../$(HYPERVISOR_BUILD_DIR)
	@echo "Hypervisor build complete"

# ==================== ACPI ====================

acpi: $(BUILD_DIR)
	@echo "======================================"
	@echo "Building ACPI Components..."
	@echo "======================================"
	@$(MAKE) -C $(ACPI_DIR) BUILD_DIR=../$(ACPI_BUILD_DIR)
	@echo "ACPI build complete"

# ==================== QEMU ====================

# Run in QEMU
run: all
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
		-kernel $(KERNEL_ELF)

# Run in QEMU with GDB debugging
run-debug: all
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
		-s -S

# ==================== DOCUMENTATION ====================

docs: docs-html
	@echo "Documentation generation complete"

docs-html:
	@echo "Generating HTML documentation with Doxygen..."
	@if [ -f "Doxyfile" ]; then \
		doxygen Doxyfile; \
	else \
		echo "Doxyfile not found. Will be created in Phase 1."; \
	fi

docs-pdf:
	@echo "Generating PDF documentation..."
	@cd $(DOCS_DIR) && $(MAKE) pdf

# ==================== CLEAN ====================

clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(BUILD_DIR)
	@if [ -d "$(KERNEL_DIR)" ]; then $(MAKE) -C $(KERNEL_DIR) clean; fi
	@if [ -d "$(HYPERVISOR_DIR)" ]; then $(MAKE) -C $(HYPERVISOR_DIR) clean; fi
	@if [ -d "$(ACPI_DIR)" ]; then $(MAKE) -C $(ACPI_DIR) clean; fi
	@echo "Clean complete"

distclean: clean
	@echo "Performing deep clean..."
	@if [ -d "$(ATF_DIR)" ]; then $(MAKE) -C $(ATF_DIR) distclean 2>/dev/null || true; fi
	@if [ -d "$(EDK2_DIR)" ]; then rm -rf $(EDK2_BUILD) 2>/dev/null || true; fi
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
	@echo "  Firmware: $(FW_BUILD_DIR)"
	@echo "  Kernel:   $(KERNEL_BUILD_DIR)"
	@echo "  HV:       $(HYPERVISOR_BUILD_DIR)"
	@echo "======================================"
