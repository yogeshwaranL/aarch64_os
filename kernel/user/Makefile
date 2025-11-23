# User Programs Makefile
# Builds simple EL0 user programs and embeds them as binary data

# Toolchain
CROSS_COMPILE ?= aarch64-linux-gnu-
AS := $(CROSS_COMPILE)as
LD := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy

# Build directory
BUILD_DIR ?= ../../build/kernel/user

# User programs
USER_PROGS := user_hello user_counter

# Output files (binary and object for embedding)
USER_BINS := $(addprefix $(BUILD_DIR)/,$(addsuffix .bin,$(USER_PROGS)))
USER_OBJS := $(addprefix $(BUILD_DIR)/,$(addsuffix .o,$(USER_PROGS)))

# Linker script for user programs (position-independent, flat binary)
LDSCRIPT := user.ld

# Assembly flags
ASFLAGS := -march=armv8-a

# Linker flags
LDFLAGS := -T $(LDSCRIPT) -nostdlib

.PHONY: all clean

all: $(BUILD_DIR) $(USER_BINS) $(USER_OBJS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Assemble user program
$(BUILD_DIR)/%.s.o: %.S
	@echo "AS $<"
	@$(AS) $(ASFLAGS) -o $@ $<

# Link user program as ELF
$(BUILD_DIR)/%.elf: $(BUILD_DIR)/%.s.o
	@echo "LD $@"
	@$(LD) $(LDFLAGS) -o $@ $<

# Convert to flat binary
$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf
	@echo "OBJCOPY $@"
	@$(OBJCOPY) -O binary $< $@
	@ls -lh $@

# Create object file for embedding in kernel
$(BUILD_DIR)/%.o: $(BUILD_DIR)/%.bin
	@echo "Creating embeddable object for $<"
	@cd $(BUILD_DIR) && \
	$(LD) -r -b binary -o $(notdir $@) $(notdir $<)

clean:
	rm -f $(BUILD_DIR)/*.o $(BUILD_DIR)/*.elf $(BUILD_DIR)/*.bin
