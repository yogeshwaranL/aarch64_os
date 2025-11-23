#!/bin/bash
# QEMU run script for AArch64 Bare Metal OS

# Configuration
QEMU_BIN="qemu-system-aarch64"
MACHINE="virt"
CPU="cortex-a53"
SMP="${QEMU_SMP:-4}"
MEMORY="${QEMU_MEMORY:-1G}"
KERNEL="build/kernel/kernel.elf"
UEFI_FW="build/firmware/QEMU_EFI_PADDED.fd"
UEFI_VARS="build/firmware/QEMU_VARS_PADDED.fd"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if QEMU is installed
if ! command -v $QEMU_BIN &> /dev/null; then
    echo -e "${RED}Error: $QEMU_BIN not found${NC}"
    echo "Please install QEMU: sudo apt-get install qemu-system-arm"
    exit 1
fi

# Check if kernel exists
if [ ! -f "$KERNEL" ]; then
    echo -e "${YELLOW}Warning: Kernel not found at $KERNEL${NC}"
    echo "Building kernel first..."
    make kernel
    if [ $? -ne 0 ]; then
        echo -e "${RED}Error: Kernel build failed${NC}"
        exit 1
    fi
fi

# Parse arguments
DEBUG_MODE=0
UEFI_MODE=0
GDB_PORT=1234
EXTRA_ARGS=""

while [[ $# -gt 0 ]]; do
    case $1 in
        -d|--debug)
            DEBUG_MODE=1
            shift
            ;;
        -u|--uefi)
            UEFI_MODE=1
            shift
            ;;
        -g|--gdb-port)
            GDB_PORT="$2"
            shift 2
            ;;
        -s|--smp)
            SMP="$2"
            shift 2
            ;;
        -m|--memory)
            MEMORY="$2"
            shift 2
            ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  -d, --debug          Enable debug mode (GDB stub)"
            echo "  -u, --uefi           Boot with UEFI firmware"
            echo "  -g, --gdb-port PORT  GDB server port (default: 1234)"
            echo "  -s, --smp CORES      Number of CPU cores (default: 4)"
            echo "  -m, --memory SIZE    Memory size (default: 1G)"
            echo "  -h, --help           Show this help message"
            echo ""
            echo "Examples:"
            echo "  $0                   # Normal run (direct kernel boot)"
            echo "  $0 --uefi            # Boot with UEFI firmware"
            echo "  $0 --debug           # Debug mode"
            echo "  $0 -s 8 -m 2G        # 8 cores, 2GB RAM"
            exit 0
            ;;
        *)
            EXTRA_ARGS="$EXTRA_ARGS $1"
            shift
            ;;
    esac
done

# Build QEMU command based on boot mode
if [ $UEFI_MODE -eq 1 ]; then
    # UEFI boot mode
    if [ ! -f "$UEFI_FW" ]; then
        echo -e "${RED}Error: UEFI firmware not found at $UEFI_FW${NC}"
        echo "Build firmware first with: make firmware"
        exit 1
    fi

    # Create a temporary copy of VARS (pflash requires writable file)
    VARS_TEMP="/tmp/QEMU_VARS_$(date +%s).fd"
    cp "$UEFI_VARS" "$VARS_TEMP"

    QEMU_CMD="$QEMU_BIN \
        -machine $MACHINE \
        -cpu $CPU \
        -smp $SMP \
        -m $MEMORY \
        -nographic \
        -serial mon:stdio \
        -drive if=pflash,format=raw,file=$UEFI_FW,readonly=on \
        -drive if=pflash,format=raw,file=$VARS_TEMP"
else
    # Direct kernel boot mode
    if [ ! -f "$KERNEL" ]; then
        echo -e "${YELLOW}Warning: Kernel not found at $KERNEL${NC}"
        echo "Building kernel first..."
        make kernel
        if [ $? -ne 0 ]; then
            echo -e "${RED}Error: Kernel build failed${NC}"
            exit 1
        fi
    fi

    QEMU_CMD="$QEMU_BIN \
        -machine $MACHINE \
        -cpu $CPU \
        -smp $SMP \
        -m $MEMORY \
        -nographic \
        -serial mon:stdio \
        -kernel $KERNEL \
        -nic none"
fi

if [ $DEBUG_MODE -eq 1 ]; then
    QEMU_CMD="$QEMU_CMD -s -S"
    echo -e "${GREEN}Starting QEMU in debug mode (GDB port: $GDB_PORT)${NC}"
    if [ $UEFI_MODE -eq 0 ]; then
        echo -e "${YELLOW}Connect GDB with: gdb-multiarch $KERNEL -ex 'target remote :$GDB_PORT'${NC}"
    else
        echo -e "${YELLOW}Connect GDB with: gdb-multiarch -ex 'target remote :$GDB_PORT'${NC}"
    fi
else
    echo -e "${GREEN}Starting QEMU...${NC}"
fi

# Add extra arguments
if [ -n "$EXTRA_ARGS" ]; then
    QEMU_CMD="$QEMU_CMD $EXTRA_ARGS"
fi

# Print configuration
echo "Configuration:"
echo "  Machine: $MACHINE"
echo "  CPU:     $CPU"
echo "  Cores:   $SMP"
echo "  Memory:  $MEMORY"
if [ $UEFI_MODE -eq 1 ]; then
    echo "  Mode:    UEFI Boot"
    echo "  UEFI FW: $UEFI_FW"
else
    echo "  Mode:    Direct Kernel Boot"
    echo "  Kernel:  $KERNEL"
fi
echo ""
echo "Press Ctrl-A then X to exit QEMU"
echo "Press Ctrl-A then C to access QEMU monitor"
echo ""
echo "-------------------------------------------"
echo ""

# Run QEMU
$QEMU_CMD

# Cleanup temporary files
if [ $UEFI_MODE -eq 1 ] && [ -n "$VARS_TEMP" ] && [ -f "$VARS_TEMP" ]; then
    rm -f "$VARS_TEMP"
fi

echo ""
echo "QEMU exited"
