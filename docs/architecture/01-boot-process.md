# Boot Process

## Detailed Boot Flow for AArch64 Bare Metal OS

**Version:** 0.1.0
**Component:** Boot Chain (ATF + UEFI + Hypervisor + Kernel)

---

## Table of Contents

1. [Overview](#overview)
2. [Boot Stages](#boot-stages)
3. [Stage 1: BL1 (Boot ROM)](#stage-1-bl1-boot-rom)
4. [Stage 2: BL2 (Trusted Boot Firmware)](#stage-2-bl2-trusted-boot-firmware)
5. [Stage 3: BL31 (EL3 Runtime)](#stage-3-bl31-el3-runtime)
6. [Stage 4: BL33 (UEFI)](#stage-4-bl33-uefi)
7. [Stage 5: Hypervisor Initialization](#stage-5-hypervisor-initialization)
8. [Stage 6: Kernel Initialization](#stage-6-kernel-initialization)
9. [Exception Level Transitions](#exception-level-transitions)
10. [Memory State at Each Stage](#memory-state-at-each-stage)

---

## Overview

The boot process follows the ARM Trusted Firmware boot flow, extended with our hypervisor and kernel. The boot chain ensures:

- **Secure Boot**: Each stage verifies the next stage before execution
- **Platform Initialization**: Hardware is configured progressively
- **Exception Level Setup**: Proper EL configuration and transitions
- **Memory Management**: Memory is mapped and protected at each stage

### Boot Timeline

```
Time  Stage    Exception Level  Component
════════════════════════════════════════════════════════
 0ms  Reset    -                Hardware Reset
 1ms  BL1      EL3              Boot ROM (ATF)
10ms  BL2      Secure EL1       Trusted Boot Firmware (ATF)
25ms  BL31     EL3              EL3 Runtime (ATF)
30ms  BL33     EL2              UEFI (EDK2)
150ms HV       EL2              Hypervisor
160ms Kernel   EL1              OS Kernel
200ms Init     EL0              Init Process
```

---

## Boot Stages

### Complete Boot Flow Diagram

```
┌───────────────────────────────────────────────────────────┐
│                      POWER-ON RESET                       │
│            CPU starts at 0x0000_0000 (Flash)             │
└────────────────────┬──────────────────────────────────────┘
                     ↓
┌─────────────────────────────────────────────────────────────┐
│ STAGE 1: BL1 (Boot ROM)                                     │
│ Exception Level: EL3                                        │
│ Location: 0x0000_0000 (Flash)                              │
├─────────────────────────────────────────────────────────────┤
│ Tasks:                                                      │
│ 1. Initialize CPU core 0                                   │
│ 2. Setup minimal stack                                     │
│ 3. Initialize exception vectors (EL3)                      │
│ 4. Initialize architectural state (system registers)       │
│ 5. Setup translation tables for BL1                        │
│ 6. Initialize GIC (minimal)                                │
│ 7. Load BL2 from flash to RAM                             │
│ 8. Verify BL2 signature (secure boot)                     │
│ 9. Setup entry point for BL2                              │
│ 10. Jump to BL2 at Secure EL1                             │
└────────────────────┬────────────────────────────────────────┘
                     ↓
┌─────────────────────────────────────────────────────────────┐
│ STAGE 2: BL2 (Trusted Boot Firmware)                       │
│ Exception Level: Secure EL1                                 │
│ Location: RAM (loaded by BL1)                              │
├─────────────────────────────────────────────────────────────┤
│ Tasks:                                                      │
│ 1. Initialize UART for debug output                        │
│ 2. Print boot banner                                       │
│ 3. Detect RAM size and configuration                      │
│ 4. Initialize generic timer                               │
│ 5. Complete GIC initialization                            │
│ 6. Setup MMU for BL2 (identity mapping)                   │
│ 7. Detect CPU features (cache, FP, SIMD)                  │
│ 8. Initialize secondary CPUs (parking protocol)           │
│ 9. Load BL31 to RAM                                        │
│ 10. Load BL33 (UEFI) to RAM                               │
│ 11. Verify BL31 and BL33 signatures                       │
│ 12. Create entry point info for BL31                      │
│ 13. Jump to BL31 at EL3                                   │
└────────────────────┬────────────────────────────────────────┘
                     ↓
┌─────────────────────────────────────────────────────────────┐
│ STAGE 3: BL31 (EL3 Runtime Software)                       │
│ Exception Level: EL3 (STAYS RESIDENT)                       │
│ Location: RAM (loaded by BL2)                              │
├─────────────────────────────────────────────────────────────┤
│ Tasks:                                                      │
│ 1. Initialize EL3 exception vectors                        │
│ 2. Setup EL3 runtime services framework                    │
│ 3. Initialize PSCI (Power State Coordination Interface):   │
│    - CPU_ON, CPU_OFF handlers                             │
│    - CPU_SUSPEND, SYSTEM_RESET handlers                   │
│    - SYSTEM_OFF, MIGRATE handlers                         │
│ 4. Initialize Secure Monitor Call (SMC) handler           │
│ 5. Setup communication with Secure-EL1 payload (if any)   │
│ 6. Configure GIC for secure/non-secure interrupts         │
│ 7. Configure SCR_EL3 for non-secure world:                │
│    - Enable EL2 (HCE bit)                                 │
│    - Route to EL2 (RW bit for AArch64)                    │
│    - Non-secure bit (NS bit)                              │
│ 8. Initialize context management                          │
│ 9. Prepare BL33 entry point                               │
│ 10. ERET to BL33 at EL2 (non-secure)                      │
└────────────────────┬────────────────────────────────────────┘
                     ↓ (ERET to EL2, BL31 stays resident)
┌─────────────────────────────────────────────────────────────┐
│ STAGE 4: BL33 / UEFI (EDK2)                                │
│ Exception Level: EL2                                        │
│ Location: RAM (loaded by BL2)                              │
├─────────────────────────────────────────────────────────────┤
│ Phase 1: SEC (Security Phase) - Minimal Init               │
│   - Initialize temporary RAM/stack                         │
│   - CPU early initialization                              │
│                                                             │
│ Phase 2: PEI (Pre-EFI Initialization)                      │
│   - Permanent RAM initialization                           │
│   - Platform information HOBs (Hand-Off Blocks)            │
│   - Discover memory map                                    │
│   - CPU MP (Multi-Processor) initialization                │
│                                                             │
│ Phase 3: DXE (Driver Execution Environment)                │
│   - Load and execute DXE drivers:                          │
│     * GIC driver                                           │
│     * Timer driver                                         │
│     * UART driver                                          │
│     * ACPI table driver                                    │
│   - Setup UEFI Boot Services:                             │
│     * Memory allocation services                           │
│     * Protocol services                                    │
│     * Event services                                       │
│     * Image services                                       │
│   - Setup UEFI Runtime Services:                          │
│     * Variable services                                    │
│     * Time services                                        │
│     * Reset services                                       │
│   - Install ACPI tables                                    │
│                                                             │
│ Phase 4: BDS (Boot Device Selection)                       │
│   - Enumerate boot options                                 │
│   - Locate hypervisor binary                              │
│   - Load hypervisor into memory                           │
│   - Setup exit boot services callback                      │
│   - Call ExitBootServices()                               │
│   - Transfer control to hypervisor at EL2                 │
└────────────────────┬────────────────────────────────────────┘
                     ↓
┌─────────────────────────────────────────────────────────────┐
│ STAGE 5: Hypervisor Initialization                         │
│ Exception Level: EL2                                        │
│ Location: RAM (loaded by UEFI)                             │
├─────────────────────────────────────────────────────────────┤
│ Tasks:                                                      │
│ 1. Save UEFI-provided information:                         │
│    - Memory map                                            │
│    - ACPI tables location                                  │
│    - System configuration                                  │
│ 2. Initialize hypervisor-specific EL2 state:              │
│    - HCR_EL2: Hypervisor Configuration Register           │
│    - VTTBR_EL2: Stage-2 translation base                  │
│    - VTCR_EL2: Stage-2 translation control                │
│ 3. Setup EL2 exception vector table                        │
│ 4. Initialize Stage-2 page tables:                        │
│    - Identity map for VM 0 (kernel)                       │
│    - Memory protection                                     │
│ 5. Initialize vGIC (Virtual GIC):                         │
│    - List registers configuration                         │
│    - Virtual interrupt management                         │
│ 6. Setup hypervisor memory management:                     │
│    - Hypervisor heap                                       │
│    - VM control structures                                 │
│    - vCPU structures                                       │
│ 7. Initialize VM 0 (initial VM for kernel):               │
│    - Allocate VM control block                            │
│    - Allocate vCPU structures                             │
│    - Setup VM memory map                                   │
│ 8. Load kernel binary into VM 0 memory                    │
│ 9. Setup vCPU 0 initial state:                            │
│    - Entry point = kernel entry                           │
│    - Stack pointer                                         │
│    - Exception level = EL1                                │
│ 10. Initialize other vCPUs (for SMP)                      │
│ 11. Start vCPU 0 (enter VM)                               │
└────────────────────┬────────────────────────────────────────┘
                     ↓ (Enter VM at EL1)
┌─────────────────────────────────────────────────────────────┐
│ STAGE 6: Kernel Initialization                             │
│ Exception Level: EL1 (running in VM 0)                      │
│ Location: RAM (loaded by hypervisor)                       │
├─────────────────────────────────────────────────────────────┤
│ Early Init (kernel_entry):                                 │
│ 1. Save bootloader parameters (DTB/ACPI location)          │
│ 2. Clear BSS section                                       │
│ 3. Setup temporary stack                                   │
│ 4. Initialize UART for early debugging                     │
│ 5. Print kernel boot banner                               │
│                                                             │
│ Architecture Init:                                          │
│ 6. Initialize EL1 system registers:                        │
│    - SCTLR_EL1: System control                            │
│    - TCR_EL1: Translation control                         │
│    - MAIR_EL1: Memory attributes                          │
│ 7. Setup exception vector table (EL1)                      │
│ 8. Initialize page tables (Stage-1):                       │
│    - Kernel code/data mapping                             │
│    - Direct physical memory mapping                        │
│    - Device memory mapping                                 │
│ 9. Enable MMU and caches                                   │
│ 10. Relocate to virtual addresses                         │
│                                                             │
│ Core Subsystem Init:                                        │
│ 11. Initialize physical memory allocator                   │
│ 12. Initialize kernel heap                                │
│ 13. Initialize GICv3 driver                               │
│ 14. Initialize generic timer                              │
│ 15. Enable interrupts                                      │
│ 16. Parse ACPI tables / Device Tree                       │
│ 17. Initialize per-CPU data structures                     │
│                                                             │
│ Subsystem Init:                                             │
│ 18. Initialize scheduler                                  │
│ 19. Initialize VFS (if implemented)                       │
│ 20. Initialize device drivers                             │
│ 21. Initialize system call interface                       │
│                                                             │
│ SMP Init:                                                   │
│ 22. Bring up secondary CPUs via PSCI                      │
│ 23. Initialize per-CPU structures                          │
│ 24. Enable per-CPU interrupts                             │
│                                                             │
│ Final Init:                                                 │
│ 25. Create init process (PID 1)                           │
│ 26. Switch to init process at EL0                         │
│ 27. Kernel idle loop for CPU 0                            │
└─────────────────────────────────────────────────────────────┘
                     ↓
┌─────────────────────────────────────────────────────────────┐
│                     SYSTEM RUNNING                          │
│              Init process and user space                    │
└─────────────────────────────────────────────────────────────┘
```

---

## Stage 1: BL1 (Boot ROM)

### Entry Point

**Exception Level**: EL3
**Entry Address**: 0x0000_0000 (Flash ROM)
**Stack**: Minimal stack in on-chip SRAM or cache-as-RAM

### Execution Flow

```c
// Pseudo-code for BL1 execution

void bl1_main(void) {
    // 1. CPU initialization
    bl1_arch_setup();  // Initialize core registers

    // 2. Platform initialization
    bl1_platform_setup();  // Initialize peripherals needed for boot

    // 3. Setup exception vectors
    write_vbar_el3((uintptr_t)&bl1_exceptions);

    // 4. Setup initial page tables
    bl1_setup_page_tables();
    enable_mmu_el3();

    // 5. Initialize UART for debug
    uart_init();
    uart_print("BL1: Starting...\n");

    // 6. Load BL2 from flash to RAM
    bl1_load_bl2_image();

    // 7. Verify BL2 (secure boot)
    if (!bl1_verify_bl2()) {
        panic("BL2 verification failed");
    }

    // 8. Setup BL2 entry point
    entry_point_info_t ep_info;
    ep_info.pc = BL2_BASE;
    ep_info.spsr = SPSR_64(MODE_EL1, MODE_SP_ELX, DISABLE_ALL_EXCEPTIONS);

    // 9. Jump to BL2
    bl1_run_bl2(&ep_info);
}
```

### System Register Configuration

```asm
bl1_arch_setup:
    // Initialize SCTLR_EL3 (System Control Register)
    mrs  x0, sctlr_el3
    orr  x0, x0, #SCTLR_I_BIT      // Enable instruction cache
    orr  x0, x0, #SCTLR_A_BIT      // Enable alignment checking
    orr  x0, x0, #SCTLR_SA_BIT     // Enable stack alignment
    bic  x0, x0, #SCTLR_M_BIT      // Disable MMU (for now)
    bic  x0, x0, #SCTLR_C_BIT      // Disable data cache (for now)
    msr  sctlr_el3, x0
    isb

    // Initialize SCR_EL3 (Secure Configuration Register)
    mov  x0, #(SCR_RES1_BITS | SCR_RW_BIT | SCR_ST_BIT | SCR_HCE_BIT)
    msr  scr_el3, x0
    isb

    ret
```

### Memory Map at BL1

```
Physical Address    Description
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
0x0000_0000         BL1 code (Flash)
0x0400_0000         Devices (UART, GIC, etc.)
0x4000_0000         RAM (not yet fully configured)
```

---

## Stage 2: BL2 (Trusted Boot Firmware)

### Entry Point

**Exception Level**: Secure EL1
**Entry Address**: BL2_BASE (in RAM, typically 0x4001_0000)
**Stack**: BL2 stack in RAM

### Execution Flow

```c
void bl2_main(void) {
    // 1. Early platform setup
    bl2_early_platform_setup();

    // 2. Initialize console (UART)
    console_init();
    NOTICE("BL2: Trusted Boot Firmware\n");

    // 3. Architecture setup
    bl2_arch_setup();

    // 4. Platform setup
    bl2_platform_setup();  // Full hardware initialization

    // 5. Setup page tables and enable MMU
    bl2_setup_page_tables();
    enable_mmu_svc_mon(0);  // Secure EL1 MMU

    // 6. Load images
    bl2_load_images();
    /*
     * This loads:
     * - BL31: EL3 runtime firmware
     * - BL33: Non-secure firmware (UEFI)
     * - Optional: BL32 (Secure-EL1 Payload like OP-TEE)
     */

    // 7. Get entry point for BL31
    entry_point_info_t *bl31_ep_info = bl2_get_bl31_ep_info();

    // 8. Pass parameters to BL31
    bl31_params_t *bl31_params = bl2_create_bl31_params();
    bl31_params->bl33_ep_info = bl2_get_bl33_ep_info();

    // 9. Run BL31
    bl2_run_next_image(bl31_ep_info);
}
```

### Platform Initialization

```c
void bl2_platform_setup(void) {
    // 1. Memory detection
    detect_memory_size();

    // 2. Initialize GIC
    gic_driver_init();
    gic_distif_init();
    gic_cpuif_init();

    // 3. Initialize ARM Generic Timer
    generic_timer_init();

    // 4. Initialize multi-core support
    bl2_mp_init();

    // 5. Device tree / ACPI detection
    detect_platform_config();
}
```

### Image Loading

```c
void bl2_load_images(void) {
    // Load BL31
    load_auth_image(BL31_IMAGE_ID, &bl31_image_info);

    // Load BL33 (UEFI)
    load_auth_image(BL33_IMAGE_ID, &bl33_image_info);

    // Verify signatures
    verify_image_signature(BL31_IMAGE_ID);
    verify_image_signature(BL33_IMAGE_ID);
}
```

---

## Stage 3: BL31 (EL3 Runtime)

### Entry Point

**Exception Level**: EL3
**Entry Address**: BL31_BASE (typically 0x4002_0000)
**Stack**: BL31 stack (stays resident)

### Key Responsibility

BL31 is **RESIDENT** - it doesn't transfer control permanently. It provides runtime services via SMC.

### Execution Flow

```c
void bl31_main(void) {
    // 1. Early setup
    bl31_early_platform_setup2(u_register_t arg0, u_register_t arg1,
                               u_register_t arg2, u_register_t arg3);

    // 2. Architecture setup
    bl31_arch_setup();

    // 3. Platform setup
    bl31_platform_setup();

    // 4. Initialize PSCI
    psci_setup();

    // 5. Initialize runtime services framework
    runtime_svc_init();

    // 6. Setup exception vectors
    setup_el3_exception_vectors();

    // 7. Get BL33 entry point info
    entry_point_info_t *bl33_ep_info = bl31_plat_get_next_image_ep_info(NON_SECURE);

    // 8. Initialize EL2/EL1 state for BL33
    bl31_prepare_next_image_entry();

    // 9. ERET to BL33 at EL2
    console_flush();
    bl31_run_image(bl33_ep_info);
}
```

### PSCI Implementation

```c
// Power State Coordination Interface
static const psci_cpu_ops_t psci_ops = {
    .cpu_on         = psci_cpu_on,
    .cpu_off        = psci_cpu_off,
    .cpu_suspend    = psci_cpu_suspend,
    .affinity_info  = psci_affinity_info,
    .migrate        = psci_migrate,
    .system_reset   = psci_system_reset,
    .system_off     = psci_system_off,
};

// SMC handler for PSCI calls
uint64_t psci_smc_handler(uint32_t smc_fid, u_register_t x1,
                          u_register_t x2, u_register_t x3) {
    switch (smc_fid) {
    case PSCI_CPU_ON_AARCH64:
        return psci_cpu_on(x1, x2, x3);
    case PSCI_CPU_OFF:
        return psci_cpu_off();
    // ... other PSCI calls
    }
}
```

### Exception Level Transition (EL3 → EL2)

```asm
// Prepare to enter BL33 at EL2
bl31_run_image:
    // x0 = entry_point_info_t *

    // 1. Load entry point
    ldr  x1, [x0, #EP_PC_OFFSET]

    // 2. Load SPSR
    ldr  x2, [x0, #EP_SPSR_OFFSET]

    // 3. Setup SPSR_EL3 for EL2h (EL2 with dedicated stack)
    msr  spsr_el3, x2

    // 4. Setup ELR_EL3 (Exception Link Register - return address)
    msr  elr_el3, x1

    // 5. Setup arguments (x0-x3 from entry point info)
    ldp  x0, x1, [x0, #EP_ARGS_OFFSET]
    ldp  x2, x3, [x0, #EP_ARGS_OFFSET + 16]

    // 6. ERET (Exception Return - transitions to EL2)
    eret
```

---

## Stage 4: BL33 (UEFI)

### Entry Point

**Exception Level**: EL2
**Entry Address**: BL33_BASE (typically 0x6000_0000)
**Stack**: UEFI stack

### UEFI Boot Phases

#### SEC Phase (Security)

```c
// Minimal initialization
void SecMain(void) {
    // Initialize temporary RAM
    InitTempRam();

    // Setup stack
    SetupTemporaryStack();

    // Jump to PEI
    JumpToPeiCore();
}
```

#### PEI Phase (Pre-EFI Initialization)

```c
void PeiMain(void) {
    // 1. Initialize permanent memory
    MemoryInit();

    // 2. Create HOBs (Hand-Off Blocks)
    BuildResourceDescriptorHob();
    BuildMemoryAllocationHob();

    // 3. CPU init
    CpuInit();

    // 4. Install PPI (PEI-to-PEI Interfaces)
    InstallPeiMemoryPpi();

    // 5. Load DXE Core
    LoadDxeCore();
}
```

#### DXE Phase (Driver Execution Environment)

```c
void DxeMain(void) {
    // 1. Initialize core services
    InitializeMemoryServices();
    InitializeEventServices();
    InitializeTimerServices();

    // 2. Load drivers
    LoadGicDriver();
    LoadTimerDriver();
    LoadUartDriver();
    LoadAcpiDriver();

    // 3. Install protocols
    InstallBootServicesProtocols();
    InstallRuntimeServicesProtocols();

    // 4. Install ACPI tables
    InstallAcpiTables();

    // 5. Enter BDS
    BdsEntry();
}
```

#### BDS Phase (Boot Device Selection)

```c
void BdsEntry(void) {
    // 1. Enumerate boot options
    EnumerateBootOptions();

    // 2. Locate hypervisor/kernel
    LocateBootImage();

    // 3. Load image
    LoadImage(BootImageHandle);

    // 4. Exit boot services
    ExitBootServices(ImageHandle, MapKey);

    // 5. Start image (transfer to hypervisor)
    StartImage(ImageHandle);
}
```

### UEFI Boot Services

Key boot services provided:

```c
struct EFI_BOOT_SERVICES {
    // Memory services
    EFI_ALLOCATE_PAGES      AllocatePages;
    EFI_FREE_PAGES          FreePages;
    EFI_GET_MEMORY_MAP      GetMemoryMap;
    EFI_ALLOCATE_POOL       AllocatePool;
    EFI_FREE_POOL           FreePool;

    // Event & Timer services
    EFI_CREATE_EVENT        CreateEvent;
    EFI_SET_TIMER           SetTimer;
    EFI_WAIT_FOR_EVENT      WaitForEvent;

    // Protocol services
    EFI_INSTALL_PROTOCOL_INTERFACE    InstallProtocolInterface;
    EFI_HANDLE_PROTOCOL               HandleProtocol;

    // Image services
    EFI_LOAD_IMAGE          LoadImage;
    EFI_START_IMAGE         StartImage;
    EFI_EXIT                Exit;
    EFI_EXIT_BOOT_SERVICES  ExitBootServices;
};
```

---

## Stage 5: Hypervisor Initialization

### Entry Point

**Exception Level**: EL2
**Entry Address**: Provided by UEFI
**Arguments**:
- x0: UEFI Memory Map
- x1: ACPI Table pointer or DTB address

### Initialization Sequence

```c
void hypervisor_main(efi_memory_map_t *memory_map, void *acpi_rsdp) {
    // 1. Save UEFI info
    save_uefi_memory_map(memory_map);
    save_acpi_tables(acpi_rsdp);

    // 2. Initialize hypervisor EL2 state
    hv_el2_setup();

    // 3. Setup exception vectors
    write_vbar_el2((uint64_t)&hv_exception_vectors);

    // 4. Initialize Stage-2 page tables
    hv_init_stage2_tables();

    // 5. Initialize vGIC
    vgic_init();

    // 6. Initialize memory management
    hv_mm_init();

    // 7. Create VM 0 (host VM for kernel)
    vm_t *vm0 = hv_create_vm(VM_TYPE_HOST);

    // 8. Load kernel into VM 0
    hv_load_kernel(vm0, KERNEL_IMAGE_ADDR);

    // 9. Create vCPUs for VM 0
    for (int i = 0; i < num_cpus; i++) {
        vcpu_t *vcpu = hv_create_vcpu(vm0, i);
        if (i == 0) {
            vcpu_set_entry(vcpu, KERNEL_ENTRY, kernel_dtb_addr);
        }
    }

    // 10. Start VM 0
    hv_run_vcpu(vm0->vcpus[0]);  // Enter kernel at EL1
}
```

### EL2 Setup

```c
void hv_el2_setup(void) {
    uint64_t hcr;

    // Configure HCR_EL2 (Hypervisor Configuration Register)
    hcr = HCR_RW_BIT      |  // EL1 is AArch64
          HCR_TSC_BIT     |  // Trap SMC instructions
          HCR_VM_BIT      |  // Virtualization MMU enable
          HCR_IMO_BIT     |  // Route IRQs to EL2
          HCR_FMO_BIT     |  // Route FIQs to EL2
          HCR_AMO_BIT;       // Route SErrors to EL2
    write_hcr_el2(hcr);

    // Configure VTCR_EL2 (Stage-2 Translation Control)
    uint64_t vtcr = VTCR_PS(VTCR_PS_4GB)  |  // Physical address size
                    VTCR_TG0_4K           |  // 4KB granule
                    VTCR_SH0_INNER        |  // Inner shareable
                    VTCR_ORGN0_WBWA       |  // Outer write-back
                    VTCR_IRGN0_WBWA       |  // Inner write-back
                    VTCR_SL0_L1           |  // Start at level 1
                    VTCR_T0SZ(25);           // 2^39 address space
    write_vtcr_el2(vtcr);

    // Setup VTTBR_EL2 (Stage-2 translation table base)
    // Will be set per-VM when switching context
}
```

---

## Stage 6: Kernel Initialization

### Entry Point

**Exception Level**: EL1 (running as guest in VM 0)
**Entry Address**: KERNEL_ENTRY (typically 0xFFFF_0000_0000_0000)
**Arguments**:
- x0: Device Tree Blob address (or 0 for ACPI)
- x1: 0 (reserved)

### Early Entry Assembly

```asm
.section ".text.boot"
.global _start

_start:
    // x0 = DTB/ACPI pointer

    // 1. Save boot parameters
    mov  x19, x0            // Save DTB/ACPI pointer

    // 2. Check we're at EL1
    mrs  x0, CurrentEL
    cmp  x0, #(1 << 2)     // EL1 = 0b01 << 2
    b.ne halt               // Halt if not EL1

    // 3. Setup stack for CPU 0
    ldr  x0, =__stack_start
    mov  sp, x0

    // 4. Clear BSS
    ldr  x0, =__bss_start
    ldr  x1, =__bss_end
clear_bss:
    str  xzr, [x0], #8
    cmp  x0, x1
    b.lo clear_bss

    // 5. Jump to C code
    mov  x0, x19            // Restore DTB/ACPI pointer
    bl   kernel_main

halt:
    wfe
    b    halt
```

### Kernel Main

```c
void kernel_main(void *fdt_addr) {
    // 1. Early UART init
    uart_early_init();
    kprintf("AArch64 Kernel booting...\n");

    // 2. Parse boot parameters
    if (fdt_addr) {
        parse_device_tree(fdt_addr);
    } else {
        parse_acpi_tables();
    }

    // 3. Architecture init
    arch_init();
    /*
     * - Setup exception vectors
     * - Initialize system registers
     * - Setup page tables
     * - Enable MMU
     */

    // 4. Memory subsystem init
    mm_init();
    /*
     * - Initialize physical page allocator
     * - Initialize kernel heap
     * - Setup virtual memory management
     */

    // 5. Interrupt controller init
    gic_init();

    // 6. Timer init
    timer_init();

    // 7. Enable interrupts
    local_irq_enable();

    // 8. Scheduler init
    sched_init();

    // 9. Device drivers init
    drivers_init();

    // 10. SMP initialization (bring up other CPUs)
    smp_init();

    // 11. Create init process
    create_init_process();

    // 12. Start scheduler
    sched_start();  // Does not return
}
```

---

## Exception Level Transitions

### Summary of Transitions

```
Reset → EL3 (BL1)
  BL1 → Secure EL1 (BL2)      via exception return
  BL2 → EL3 (BL31)            via function call
  BL31 → EL2 (UEFI)           via ERET
  UEFI → EL2 (Hypervisor)     via function call
  Hypervisor → EL1 (Kernel)   via VM entry (ERET)
  Kernel → EL0 (User)         via ERET
```

### Transition Mechanisms

#### ERET (Exception Return)

Used for:
- EL3 → EL2 (BL31 to UEFI)
- EL2 → EL1 (Hypervisor to Kernel)
- EL1 → EL0 (Kernel to User)

```asm
// Setup SPSR for target exception level
mov  x0, #SPSR_EL1h    // EL1 with dedicated stack (EL1h)
orr  x0, x0, #(DAIF_FIQ_BIT | DAIF_IRQ_BIT)  // Mask interrupts
msr  spsr_el2, x0

// Setup entry point
ldr  x1, =target_address
msr  elr_el2, x1

// Setup arguments
mov  x0, arg0
mov  x1, arg1

// Exception return (drops to EL1)
eret
```

#### HVC (Hypervisor Call)

Used for: EL1 → EL2 (Guest VM to Hypervisor)

```asm
// In guest (EL1)
mov  x0, #HVC_SOME_SERVICE
hvc  #0              // Trap to EL2

// In hypervisor (EL2 exception handler)
hvc_handler:
    // Handle hypercall
    // ...
    eret             // Return to EL1
```

#### SMC (Secure Monitor Call)

Used for: EL2/EL1 → EL3 (Call PSCI or secure services)

```asm
// Request PSCI CPU_ON
mov  x0, #PSCI_CPU_ON_AARCH64
mov  x1, cpu_id
ldr  x2, =secondary_entry
mov  x3, context_id
smc  #0              // Trap to EL3

// In BL31 (EL3 SMC handler)
smc_handler:
    // Handle SMC
    // ...
    eret             // Return to caller
```

---

## Memory State at Each Stage

### BL1 Memory State

```
0x0000_0000  ┌─────────────┐
             │ BL1 (Flash) │ ← Executing here
0x0400_0000  ├─────────────┤
             │ Devices     │
0x4000_0000  ├─────────────┤
             │ RAM (empty) │
             └─────────────┘
```

### BL2 Memory State

```
0x0000_0000  ┌──────────────┐
             │ BL1 (Flash)  │
0x0400_0000  ├──────────────┤
             │ Devices      │
0x4000_0000  ├──────────────┤
             │ RAM:         │
             │ ┌──────────┐ │
0x4001_0000  │ │ BL2      │ │ ← Executing here
             │ ├──────────┤ │
             │ │ Stack    │ │
             │ └──────────┘ │
0x8000_0000  └──────────────┘
```

### BL31 Active Memory State

```
0x0000_0000  ┌──────────────┐
             │ BL1 (Flash)  │
0x0400_0000  ├──────────────┤
             │ Devices      │
0x4000_0000  ├──────────────┤
             │ RAM:         │
             │ ┌──────────┐ │
0x4001_0000  │ │ BL2 (old)│ │ (can be reclaimed)
             │ ├──────────┤ │
0x4002_0000  │ │ BL31     │ │ ← RESIDENT
             │ ├──────────┤ │
0x6000_0000  │ │ BL33/UEFI│ │ ← About to execute
             │ └──────────┘ │
0x8000_0000  └──────────────┘
```

### Hypervisor Active Memory State

```
0x0000_0000  ┌──────────────────┐
             │ Flash            │
0x0400_0000  ├──────────────────┤
             │ Devices (mapped) │
0x4000_0000  ├──────────────────┤
             │ RAM:             │
             │ ┌──────────────┐ │
0x4002_0000  │ │ BL31         │ │ ← RESIDENT (for SMC)
             │ ├──────────────┤ │
0x4010_0000  │ │ Hypervisor   │ │ ← Executing here
             │ ├──────────────┤ │
0x4020_0000  │ │ VM 0 Memory  │ │
             │ │ ┌──────────┐ │ │
             │ │ │ Kernel   │ │ │
             │ │ └──────────┘ │ │
             │ └──────────────┘ │
0x8000_0000  └──────────────────┘
```

---

## Next Steps

After successful boot, the system is ready for:

1. **User space applications** (Phase 4-6)
2. **Additional VMs** (Phase 7)
3. **Advanced power management** (Phase 8)
4. **Full ACPI functionality** (Phase 8)

---

**Next Document**: [Memory Management](02-memory-management.md)
