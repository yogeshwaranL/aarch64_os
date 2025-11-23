# ACPI Makefile
# AArch64 Bare Metal OS - ACPI 6.5 Implementation

# Configuration
ACPI_NAME := aarch64_acpi
BUILD_DIR ?= ../build/acpi

# Toolchain
CROSS_COMPILE ?= aarch64-linux-gnu-
CC := $(CROSS_COMPILE)gcc
AR := $(CROSS_COMPILE)ar

# Directories
INTERPRETER_DIR := interpreter
TABLES_DIR := tables

# Output files
ACPI_LIB := $(BUILD_DIR)/libacpi.a

# Compiler flags
ARCH_FLAGS := -march=armv8-a -mtune=cortex-a53
CFLAGS := -Wall -Wextra -Werror -std=gnu11 \
          -nostdlib -ffreestanding -fno-common \
          -fno-builtin -fno-stack-protector \
          $(ARCH_FLAGS)

# Debug vs Release
ifdef DEBUG
CFLAGS += -O0 -g3 -DDEBUG
else
CFLAGS += -O2 -DNDEBUG
endif

# Source files (to be populated in Phase 8)
INTERPRETER_SOURCES :=
TABLES_SOURCES :=

# Object files
INTERPRETER_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(INTERPRETER_SOURCES))
TABLES_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(TABLES_SOURCES))

ALL_OBJS := $(INTERPRETER_OBJS) $(TABLES_OBJS)

# Phony targets
.PHONY: all clean

# Default target
all: $(BUILD_DIR) $(ACPI_LIB)
	@echo "ACPI library build complete: $(ACPI_LIB)"

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Create library
$(ACPI_LIB): $(ALL_OBJS)
	@echo "Creating ACPI library..."
	@echo "Note: ACPI implementation starts in Phase 8"
	@echo "Creating placeholder ACPI library..."
	@mkdir -p $(BUILD_DIR)
	@touch $(ACPI_LIB)

# Compile C sources
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean
clean:
	rm -rf $(BUILD_DIR)

# Info
info:
	@echo "ACPI build configuration:"
	@echo "  Target: $(ACPI_LIB)"
	@echo "  CFLAGS: $(CFLAGS)"
