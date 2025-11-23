# Building the AArch64 Bare Metal OS

## Complete Build Instructions

**Version:** 0.1.0

---

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Setting Up the Environment](#setting-up-the-environment)
3. [Obtaining the Source Code](#obtaining-the-source-code)
4. [Building Components](#building-components)
5. [Running in QEMU](#running-in-qemu)
6. [Debugging](#debugging)
7. [Troubleshooting](#troubleshooting)

---

## Prerequisites

### Required Tools

| Tool | Minimum Version | Purpose |
|------|----------------|---------|
| GCC (aarch64) | 9.0+ | Cross-compiler for AArch64 |
| GNU Make | 4.0+ | Build automation |
| Python 3 | 3.8+ | EDK2 build scripts |
| Git | 2.20+ | Version control and submodules |
| QEMU | 6.0+ | AArch64 emulation |
| Device Tree Compiler (dtc) | 1.6+ | Device tree compilation |
| Doxygen | 1.8+ | Documentation generation |

### Optional Tools

| Tool | Purpose |
|------|---------|
| gdb-multiarch | Debugging |
| tmux | Terminal multiplexing |
| bear | Generate compile_commands.json |
| clangd | LSP for IDE integration |

---

## Setting Up the Environment

### Ubuntu/Debian

```bash
# Update package list
sudo apt-get update

# Install build essentials
sudo apt-get install -y \
    build-essential \
    gcc-aarch64-linux-gnu \
    g++-aarch64-linux-gnu \
    binutils-aarch64-linux-gnu \
    qemu-system-arm \
    qemu-efi-aarch64 \
    device-tree-compiler \
    python3 \
    python3-pip \
    git \
    make \
    doxygen \
    graphviz \
    gdb-multiarch \
    tmux

# Install Python dependencies (for EDK2)
pip3 install --user lcov
```

### Fedora/RHEL

```bash
sudo dnf install -y \
    gcc-aarch64-linux-gnu \
    binutils-aarch64-linux-gnu \
    qemu-system-aarch64 \
    python3 \
    python3-pip \
    git \
    make \
    device-tree-compiler \
    doxygen \
    graphviz \
    gdb \
    tmux
```

### macOS (with Homebrew)

```bash
# Install Homebrew if not already installed
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install tools
brew install \
    aarch64-elf-gcc \
    qemu \
    python3 \
    git \
    make \
    dtc \
    doxygen \
    graphviz

# Install ARM GCC toolchain
brew tap messense/macos-cross-toolchains
brew install aarch64-unknown-linux-gnu
```

### Verify Installation

```bash
# Check cross-compiler
aarch64-linux-gnu-gcc --version

# Check QEMU
qemu-system-aarch64 --version

# Check Python
python3 --version

# Check Git
git --version
```

---

## Obtaining the Source Code

### Clone Repository

```bash
# Clone the repository
git clone <repository-url> aarch64_os
cd aarch64_os

# Initialize submodules (ATF and EDK2)
git submodule update --init --recursive
```

This will take some time as ATF and EDK2 are large projects.

### Repository Structure

```
aarch64_os/
├── firmware/
│   ├── atf/        (submodule: ARM Trusted Firmware)
│   └── edk2/       (submodule: EDK2)
├── kernel/         (Our kernel code)
├── hypervisor/     (Our hypervisor code)
├── acpi/           (ACPI implementation)
├── docs/           (Documentation)
├── scripts/        (Build and run scripts)
├── Makefile        (Main build file)
└── README.md
```

---

## Building Components

### Quick Build (Everything)

```bash
# Build all components
make all

# This will build:
# 1. ARM Trusted Firmware (ATF)
# 2. UEFI EDK2
# 3. Kernel
# 4. Hypervisor
# 5. ACPI components
```

### Build Individual Components

#### 1. ARM Trusted Firmware

```bash
make atf

# Or manually:
cd firmware/atf
make PLAT=qemu ARCH=aarch64 \
     DEBUG=1 \
     CROSS_COMPILE=aarch64-linux-gnu- \
     bl1 bl2 bl31
```

**Output files:**
- `firmware/atf/build/qemu/debug/bl1.bin`
- `firmware/atf/build/qemu/debug/bl2.bin`
- `firmware/atf/build/qemu/debug/bl31.bin`

#### 2. UEFI EDK2

```bash
make edk2

# Or manually:
cd firmware/edk2
export GCC5_AARCH64_PREFIX=aarch64-linux-gnu-
source edksetup.sh
build -a AARCH64 -t GCC5 -p ArmVirtPkg/ArmVirtQemu.dsc -b DEBUG
```

**Output files:**
- `firmware/edk2/Build/ArmVirtQemu-AARCH64/DEBUG_GCC5/FV/QEMU_EFI.fd`

#### 3. Kernel

```bash
make kernel

# Or manually:
cd kernel
make DEBUG=1 CROSS_COMPILE=aarch64-linux-gnu-
```

**Output files:**
- `build/kernel/kernel.elf`
- `build/kernel/kernel.bin`

#### 4. Hypervisor

```bash
make hypervisor

# Or manually:
cd hypervisor
make DEBUG=1 CROSS_COMPILE=aarch64-linux-gnu-
```

**Output files:**
- `build/hypervisor/hypervisor.elf`
- `build/hypervisor/hypervisor.bin`

#### 5. ACPI Components

```bash
make acpi

# Or manually:
cd acpi
make DEBUG=1 CROSS_COMPILE=aarch64-linux-gnu-
```

**Output files:**
- `build/acpi/libacpi.a`

### Build Configuration

#### Debug Build (Default)

```bash
make all
# or
make DEBUG=1
```

Features:
- Debug symbols (-g3)
- No optimization (-O0)
- Debug assertions enabled
- Verbose logging

#### Release Build

```bash
make DEBUG=0
# or
make RELEASE=1
```

Features:
- No debug symbols
- Optimizations (-O2)
- Assertions disabled
- Minimal logging

#### Verbose Build

```bash
make V=1
```

Shows full compiler commands.

---

## Running in QEMU

### Basic Run

```bash
make run
```

This will:
1. Build all components (if needed)
2. Launch QEMU with the kernel

### QEMU Configuration

Default QEMU settings:
- **Machine**: virt (ARM Virtual Machine)
- **CPU**: cortex-a53
- **Cores**: 4
- **Memory**: 1GB
- **Serial**: Redirected to stdio

### Manual QEMU Invocation

```bash
qemu-system-aarch64 \
    -machine virt \
    -cpu cortex-a53 \
    -smp 4 \
    -m 1G \
    -nographic \
    -serial mon:stdio \
    -kernel build/kernel/kernel.elf
```

### QEMU with Full Firmware Stack

```bash
qemu-system-aarch64 \
    -machine virt,secure=on \
    -cpu cortex-a53 \
    -smp 4 \
    -m 1G \
    -nographic \
    -serial mon:stdio \
    -bios build/firmware/bl1.bin \
    -device loader,file=build/firmware/bl2.bin,addr=0x4001000 \
    -device loader,file=build/firmware/bl31.bin,addr=0x4002000 \
    -device loader,file=build/firmware/bl33.bin,addr=0x6000000 \
    -device loader,file=build/kernel/kernel.bin,addr=0x4020000
```

### Exiting QEMU

Press `Ctrl-A` then `X` to exit QEMU.

Or from the QEMU monitor: `quit`

---

## Debugging

### GDB Debugging

#### Start QEMU with GDB Stub

```bash
make run-debug

# Or manually:
qemu-system-aarch64 \
    -machine virt \
    -cpu cortex-a53 \
    -smp 4 \
    -m 1G \
    -nographic \
    -serial mon:stdio \
    -kernel build/kernel/kernel.elf \
    -s -S
```

- `-s`: Start GDB server on port 1234
- `-S`: Freeze CPU at startup (wait for GDB)

#### Connect GDB

In another terminal:

```bash
gdb-multiarch build/kernel/kernel.elf

# In GDB:
(gdb) target remote :1234
(gdb) break kernel_main
(gdb) continue
```

#### Useful GDB Commands

```gdb
# View current location
(gdb) list

# Set breakpoint
(gdb) break function_name
(gdb) break file.c:123

# Step through code
(gdb) step      # Step into
(gdb) next      # Step over
(gdb) finish    # Step out

# View registers
(gdb) info registers
(gdb) info all-registers

# View memory
(gdb) x/16x 0x40000000

# View backtrace
(gdb) backtrace

# Continue execution
(gdb) continue
```

### QEMU Monitor

Access QEMU monitor: `Ctrl-A` then `C`

Useful commands:
```
(qemu) info registers     # View CPU registers
(qemu) info mem           # View memory mappings
(qemu) info qtree         # View device tree
(qemu) x /16x 0x40000000  # Examine memory
(qemu) quit               # Exit QEMU
```

Switch back to serial console: `Ctrl-A` then `C` again

---

## Building Documentation

### Generate Doxygen API Docs

```bash
make docs

# Output: docs/api/html/index.html
```

Open in browser:
```bash
firefox docs/api/html/index.html
# or
open docs/api/html/index.html  # macOS
```

### View Markdown Documentation

```bash
# Install a markdown viewer (optional)
pip3 install --user grip

# View README
grip README.md
# Open http://localhost:6419 in browser

# Or use any markdown-compatible editor/viewer
```

---

## Cleaning

### Clean Build Artifacts

```bash
# Clean kernel, hypervisor, and ACPI builds
make clean
```

### Deep Clean (Including Firmware)

```bash
# Clean everything including ATF and EDK2
make distclean
```

---

## Troubleshooting

### Common Issues

#### 1. Cross-Compiler Not Found

**Error:**
```
aarch64-linux-gnu-gcc: command not found
```

**Solution:**
```bash
# Ubuntu/Debian
sudo apt-get install gcc-aarch64-linux-gnu

# Fedora
sudo dnf install gcc-aarch64-linux-gnu

# macOS
brew install aarch64-elf-gcc
```

#### 2. QEMU Not Found

**Error:**
```
qemu-system-aarch64: command not found
```

**Solution:**
```bash
# Ubuntu/Debian
sudo apt-get install qemu-system-arm

# Fedora
sudo dnf install qemu-system-aarch64

# macOS
brew install qemu
```

#### 3. Submodules Not Initialized

**Error:**
```
firmware/atf directory is empty
```

**Solution:**
```bash
git submodule update --init --recursive
```

#### 4. Python Dependencies Missing (EDK2)

**Error:**
```
ModuleNotFoundError: No module named 'lcov'
```

**Solution:**
```bash
pip3 install --user lcov
```

#### 5. Build Fails with Permission Errors

**Solution:**
```bash
# Make sure you're not running as root
# Check file permissions
ls -la

# Fix permissions if needed
chmod -R u+rw .
```

### Getting Help

- **Documentation**: Check `docs/` directory
- **Build Issues**: Run `make clean && make V=1` for verbose output
- **QEMU Issues**: Check QEMU log with `-D qemu.log -d int,cpu_reset`

---

## Build System Details

### Makefile Targets

| Target | Description |
|--------|-------------|
| `all` | Build everything |
| `firmware` | Build ATF and EDK2 |
| `atf` | Build ARM Trusted Firmware |
| `edk2` | Build UEFI EDK2 |
| `kernel` | Build kernel |
| `hypervisor` | Build hypervisor |
| `acpi` | Build ACPI components |
| `run` | Build and run in QEMU |
| `run-debug` | Run in QEMU with GDB stub |
| `docs` | Generate documentation |
| `clean` | Clean build artifacts |
| `distclean` | Deep clean including firmware |
| `help` | Show help message |
| `info` | Show build configuration |

### Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `CROSS_COMPILE` | `aarch64-linux-gnu-` | Toolchain prefix |
| `DEBUG` | `1` | Enable debug build |
| `V` | `0` | Verbose build (1=verbose) |
| `QEMU_MACHINE` | `virt` | QEMU machine type |
| `QEMU_CPU` | `cortex-a53` | QEMU CPU type |
| `QEMU_MEMORY` | `1G` | QEMU memory size |
| `QEMU_SMP` | `4` | Number of CPU cores |

### Example Custom Build

```bash
# Build with different toolchain
make CROSS_COMPILE=aarch64-none-elf- all

# Build release version
make DEBUG=0 all

# Run with 8 cores and 2GB RAM
make QEMU_SMP=8 QEMU_MEMORY=2G run

# Verbose build
make V=1 all
```

---

## Next Steps

After successful build:

1. **Run in QEMU**: `make run`
2. **Read Architecture Docs**: `docs/architecture/`
3. **Start Development**: See `docs/guides/development.md`
4. **Contribute**: See `CONTRIBUTING.md`

---

**Happy Building!** 🚀
