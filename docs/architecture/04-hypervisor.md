# Hypervisor (Type-1)

## AArch64 Type-1 Hypervisor Implementation

**Version:** 0.1.0
**Component:** Hypervisor (EL2)
**Implementation Phase:** Phase 7

---

## Overview

A full Type-1 hypervisor providing:
- **VM lifecycle management** (create, start, pause, stop, destroy)
- **Virtual CPU scheduling** and management
- **Stage-2 memory translation** (IPA → PA)
- **Virtual interrupt controller** (vGIC)
- **VM isolation** and resource partitioning
- **Inter-VM communication** mechanisms

---

## Hypervisor Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    Hypervisor (EL2)                     │
├──────────────┬──────────────┬──────────────┬───────────┤
│ VM Manager   │ vCPU Sched   │ Stage-2 MMU  │   vGIC    │
└──────┬───────┴──────┬───────┴──────┬───────┴───────┬───┘
       │              │              │               │
       ↓              ↓              ↓               ↓
┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌──────────┐
│ VM 0 (Host) │  │ VM 1 (Guest)│  │ VM 2 (Guest)│  │ VM 3 ... │
│  ┌───────┐  │  │  ┌───────┐  │  │  ┌───────┐  │  │          │
│  │Kernel │  │  │  │Guest  │  │  │  │Guest  │  │  │          │
│  │ (EL1) │  │  │  │OS(EL1)│  │  │  │OS(EL1)│  │  │          │
│  └───────┘  │  │  └───────┘  │  │  └───────┘  │  │          │
└─────────────┘  └─────────────┘  └─────────────┘  └──────────┘
```

---

## VM Control Block

```c
/**
 * @brief VM types
 */
enum vm_type {
    VM_TYPE_HOST,       // Host VM (privileged)
    VM_TYPE_GUEST,      // Normal guest VM
    VM_TYPE_TRUSTED,    // Trusted VM (future: TEE support)
};

/**
 * @brief VM states
 */
enum vm_state {
    VM_STATE_CREATED,   // Created but not started
    VM_STATE_RUNNING,   // Running
    VM_STATE_PAUSED,    // Paused
    VM_STATE_STOPPED,   // Stopped
    VM_STATE_CRASHED,   // Crashed
};

/**
 * @brief Virtual Machine Control Block
 */
struct vm {
    // Identification
    uint32_t vmid;              // VM ID (for VTTBR_EL2.VMID)
    char name[64];              // VM name
    enum vm_type type;          // VM type
    enum vm_state state;        // Current state

    // vCPUs
    uint32_t nr_vcpus;          // Number of vCPUs
    struct vcpu *vcpus[MAX_VCPUS]; // vCPU array

    // Memory management
    struct mm_struct *mm;       // VM memory descriptor
    pgd_t *pgd_stage2;          // Stage-2 page table base
    uint64_t ipa_start;         // IPA (Intermediate Physical Address) start
    uint64_t ipa_size;          // IPA size
    uint64_t pa_start;          // Actual physical address start
    uint64_t pa_size;           // Physical memory size

    // Virtual devices
    struct vgic *vgic;          // Virtual GIC state
    struct vtimer *vtimer;      // Virtual timer state
    struct list_head vdev_list; // Virtual device list

    // Inter-VM communication
    struct list_head shared_mem_list; // Shared memory regions
    struct vm_channel *channels[MAX_VM_CHANNELS];

    // Statistics
    uint64_t total_runtime;     // Total runtime
    uint64_t nr_hypercalls;     // Number of hypercalls
    uint64_t nr_traps;          // Number of traps

    // Lock
    spinlock_t lock;            // Protects VM state

    // List linkage
    struct list_head vm_list;   // Global VM list
};
```

---

## Virtual CPU (vCPU)

```c
/**
 * @brief vCPU states
 */
enum vcpu_state {
    VCPU_STATE_INVALID,     // Not initialized
    VCPU_STATE_RUNNABLE,    // Ready to run
    VCPU_STATE_RUNNING,     // Currently running
    VCPU_STATE_BLOCKED,     // Blocked (waiting for event)
    VCPU_STATE_OFFLINE,     // Offline
};

/**
 * @brief Virtual CPU structure
 */
struct vcpu {
    // Identification
    uint32_t vcpu_id;           // vCPU ID (within VM)
    struct vm *vm;              // Parent VM
    enum vcpu_state state;      // vCPU state

    // Physical CPU affinity
    int pcpu;                   // Current physical CPU (-1 if not running)
    unsigned long pcpus_allowed; // Physical CPU affinity mask

    // CPU context (guest registers)
    struct vcpu_context {
        // General purpose registers
        uint64_t regs[31];      // x0-x30
        uint64_t sp_el0;        // User stack pointer
        uint64_t sp_el1;        // Kernel stack pointer
        uint64_t elr_el1;       // Exception Link Register
        uint64_t spsr_el1;      // Saved Program Status Register

        // System registers (partial list)
        uint64_t sctlr_el1;     // System Control Register
        uint64_t ttbr0_el1;     // Translation Table Base Register 0
        uint64_t ttbr1_el1;     // Translation Table Base Register 1
        uint64_t tcr_el1;       // Translation Control Register
        uint64_t mair_el1;      // Memory Attribute Indirection Register
        uint64_t vbar_el1;      // Vector Base Address Register
        uint64_t esr_el1;       // Exception Syndrome Register
        uint64_t far_el1;       // Fault Address Register
        uint64_t par_el1;       // Physical Address Register

        // GIC registers
        uint64_t icc_sre_el1;   // ICC System Register Enable
        uint64_t icc_ctlr_el1;  // ICC Control Register

        // Timer registers
        uint64_t cntv_ctl_el0;  // Counter-timer Virtual Control
        uint64_t cntv_cval_el0; // Counter-timer Virtual CompareValue

        // FP/SIMD state
        uint128_t vregs[32];    // V0-V31
        uint32_t fpcr;          // Floating-Point Control Register
        uint32_t fpsr;          // Floating-Point Status Register
    } context;

    // Scheduling
    int priority;               // Scheduling priority
    uint64_t runtime;           // Total runtime
    uint64_t time_slice;        // Remaining time slice
    struct list_head sched_list; // Scheduler list linkage

    // Events and blocking
    wait_queue_head_t wq;       // Wait queue for blocking

    // Statistics
    uint64_t nr_exits;          // Number of VM exits
    uint64_t nr_irqs;           // Number of virtual IRQs injected

    // vGIC state (per-vCPU)
    struct vgic_cpu vgic_cpu;   // vGIC per-CPU state
};
```

---

## Stage-2 Page Tables

### Address Translation

```
Guest Virtual Address (GVA)
         ↓
    ┌─────────┐
    │Stage-1  │  (Controlled by guest OS in EL1)
    │VA → IPA │
    └────┬────┘
         ↓
Intermediate Physical Address (IPA)
         ↓
    ┌─────────┐
    │Stage-2  │  (Controlled by hypervisor in EL2)
    │IPA → PA │
    └────┬────┘
         ↓
Physical Address (PA)
```

### Stage-2 Page Table Structure

```c
/**
 * @brief Setup Stage-2 page tables for a VM
 * @param vm Virtual machine
 * @return 0 on success, negative on error
 */
int vm_setup_stage2(struct vm *vm)
{
    pgd_t *pgd;
    uint64_t ipa, pa;

    // Allocate Stage-2 page table
    pgd = (pgd_t *)alloc_pages(PAGE_SIZE, GFP_KERNEL | __GFP_ZERO);
    if (!pgd)
        return -ENOMEM;

    vm->pgd_stage2 = pgd;

    // Map VM's physical memory (IPA == PA for simplicity, or offset)
    for (ipa = vm->ipa_start, pa = vm->pa_start;
         ipa < vm->ipa_start + vm->ipa_size;
         ipa += PAGE_SIZE, pa += PAGE_SIZE) {

        // Map IPA to PA
        stage2_map_page(vm, ipa, pa, S2_PROT_NORMAL_RWX);
    }

    // Map device regions (if needed)
    // For host VM, map GIC, UART, etc.
    if (vm->type == VM_TYPE_HOST) {
        stage2_map_device(vm, GIC_BASE, GIC_SIZE, S2_PROT_DEVICE_RW);
        stage2_map_device(vm, UART_BASE, UART_SIZE, S2_PROT_DEVICE_RW);
    }

    return 0;
}

/**
 * @brief Map a page in Stage-2 page tables
 */
int stage2_map_page(struct vm *vm, uint64_t ipa, uint64_t pa, uint64_t prot)
{
    pgd_t *pgd;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *pte;

    // Walk/create page table hierarchy (similar to Stage-1)
    pgd = pgd_offset_stage2(vm->pgd_stage2, ipa);
    if (pgd_none(*pgd)) {
        pud = pud_alloc_stage2(vm, pgd, ipa);
    } else {
        pud = pud_offset(pgd, ipa);
    }

    if (pud_none(*pud)) {
        pmd = pmd_alloc_stage2(vm, pud, ipa);
    } else {
        pmd = pmd_offset(pud, ipa);
    }

    if (pmd_none(*pmd)) {
        pte = pte_alloc_stage2(vm, pmd, ipa);
    } else {
        pte = pte_offset(pmd, ipa);
    }

    // Set PTE
    set_pte(pte, pfn_pte(pa >> PAGE_SHIFT, prot));

    return 0;
}
```

### Loading VTTBR_EL2

```c
/**
 * @brief Load VM's Stage-2 page table
 */
static inline void load_stage2_pgd(struct vm *vm)
{
    uint64_t vttbr;

    vttbr = ((uint64_t)vm->vmid << 48) | virt_to_phys(vm->pgd_stage2);
    write_sysreg(vttbr, vttbr_el2);
    isb();
}
```

---

## VM Entry and Exit

### VM Entry

```c
/**
 * @brief Enter a VM (run vCPU)
 * @param vcpu Virtual CPU to run
 */
void vm_enter(struct vcpu *vcpu)
{
    struct vm *vm = vcpu->vm;

    // Load VM's Stage-2 page table
    load_stage2_pgd(vm);

    // Configure HCR_EL2 for this VM
    configure_hcr_el2(vm);

    // Load vCPU context
    load_vcpu_context(vcpu);

    // Load vGIC state
    vgic_restore_state(&vcpu->vgic_cpu);

    vcpu->state = VCPU_STATE_RUNNING;
    vcpu->pcpu = smp_processor_id();

    // Enter guest (ERET to EL1)
    __vm_enter_guest(&vcpu->context);

    // --- Guest runs here until VM exit ---

    // VM exit occurred
    vcpu->state = VCPU_STATE_RUNNABLE;
    vcpu->pcpu = -1;

    // Save vCPU context
    save_vcpu_context(vcpu);

    // Save vGIC state
    vgic_save_state(&vcpu->vgic_cpu);
}
```

### VM Exit Handling

```asm
/**
 * @brief Low-level VM entry
 * x0 = vcpu_context pointer
 */
.global __vm_enter_guest
__vm_enter_guest:
    // Restore guest general purpose registers
    ldp     x0, x1,   [x0, #VCPU_CTX_REGS + 0]
    ldp     x2, x3,   [x0, #VCPU_CTX_REGS + 16]
    // ... (restore x0-x30)

    // Restore guest SP and PC
    ldr     x9,  [x0, #VCPU_CTX_SP_EL1]
    ldr     x10, [x0, #VCPU_CTX_ELR_EL1]
    ldr     x11, [x0, #VCPU_CTX_SPSR_EL1]

    msr     sp_el1, x9
    msr     elr_el2, x10
    msr     spsr_el2, x11

    // ERET to guest at EL1
    eret

/**
 * @brief VM exit handler (exception vector entry)
 */
vm_exit_handler:
    // Save guest context to vcpu->context
    // (This is the EL2 exception vector)

    // Save guest registers
    stp     x0, x1,   [x19, #VCPU_CTX_REGS + 0]
    stp     x2, x3,   [x19, #VCPU_CTX_REGS + 16]
    // ... (save x0-x30)

    // Save SP, ELR, SPSR
    mrs     x9, sp_el1
    mrs     x10, elr_el2
    mrs     x11, spsr_el2
    str     x9,  [x19, #VCPU_CTX_SP_EL1]
    str     x10, [x19, #VCPU_CTX_ELR_EL1]
    str     x11, [x19, #VCPU_CTX_SPSR_EL1]

    // Read ESR_EL2 (Exception Syndrome Register)
    mrs     x0, esr_el2
    lsr     x1, x0, #ESR_ELx_EC_SHIFT   // Extract exception class

    // Jump to C handler
    bl      handle_vm_exit

    // Return path continues in vm_enter()
    ret
```

### VM Exit Causes

```c
/**
 * @brief Handle VM exit
 * @param vcpu vCPU that exited
 * @param esr Exception Syndrome Register value
 */
void handle_vm_exit(struct vcpu *vcpu, uint64_t esr)
{
    uint32_t ec = ESR_ELx_EC(esr);  // Exception class

    vcpu->nr_exits++;

    switch (ec) {
    case ESR_ELx_EC_HVC64:
        // Hypercall
        handle_hypercall(vcpu);
        break;

    case ESR_ELx_EC_DABT_LOW:
        // Data abort (MMIO access)
        handle_mmio(vcpu, esr);
        break;

    case ESR_ELx_EC_IABT_LOW:
        // Instruction abort
        handle_inst_abort(vcpu, esr);
        break;

    case ESR_ELx_EC_SYS64:
        // System register access
        handle_sysreg_access(vcpu, esr);
        break;

    case ESR_ELx_EC_WFx:
        // WFI/WFE instruction
        handle_wfx(vcpu);
        break;

    case ESR_ELx_EC_SMC64:
        // SMC (forward to EL3)
        handle_smc(vcpu);
        break;

    default:
        pr_err("Unknown VM exit: EC=0x%x\n", ec);
        vm_crash(vcpu->vm);
        break;
    }
}
```

---

## Virtual GIC (vGIC)

```c
/**
 * @brief Virtual GIC distributor state
 */
struct vgic_dist {
    bool enabled;
    spinlock_t lock;

    // Interrupt configuration
    uint32_t nr_spis;           // Number of SPIs
    struct vgic_irq *interrupts[1024]; // All interrupts

    // List registers
    uint32_t nr_lr;             // Number of list registers
};

/**
 * @brief Virtual GIC per-CPU state
 */
struct vgic_cpu {
    bool enabled;

    // List registers (hardware-backed)
    struct vgic_lr {
        uint32_t state;         // Pending/Active
        uint32_t hwirq;         // Hardware IRQ number
        uint32_t vintid;        // Virtual interrupt ID
        uint32_t priority;
    } lr[MAX_LR];

    // Pending interrupts (software list)
    struct list_head pending_irqs;
};

/**
 * @brief Inject virtual interrupt
 */
void vgic_inject_irq(struct vcpu *vcpu, uint32_t intid, uint32_t priority)
{
    struct vgic_cpu *vgic_cpu = &vcpu->vgic_cpu;
    struct vgic_lr *lr;

    // Find free list register
    lr = vgic_find_free_lr(vgic_cpu);
    if (!lr) {
        // No free LR, add to pending list
        vgic_queue_irq(vcpu, intid, priority);
        return;
    }

    // Program list register
    lr->vintid = intid;
    lr->priority = priority;
    lr->state = VGIC_LR_STATE_PENDING;

    // Write to hardware LR
    write_gich_lr(lr_index, encode_lr(lr));

    // Trigger virtual IRQ
    write_sysreg(1, ICH_HCR_EL2);  // Assert maintenance interrupt if needed
}
```

---

## Hypercall Interface

```c
/**
 * @brief Hypercall numbers
 */
#define HV_CONSOLE_PUTC     0
#define HV_CONSOLE_GETC     1
#define HV_YIELD            2
#define HV_SHUTDOWN         3
#define HV_REBOOT           4
#define HV_GET_TIME         5
#define HV_MAP_SHARED_MEM   6
#define HV_UNMAP_SHARED_MEM 7

/**
 * @brief Handle hypercall from guest
 */
void handle_hypercall(struct vcpu *vcpu)
{
    uint64_t call_nr = vcpu->context.regs[0];
    uint64_t arg0 = vcpu->context.regs[1];
    uint64_t arg1 = vcpu->context.regs[2];
    uint64_t arg2 = vcpu->context.regs[3];
    uint64_t ret = 0;

    switch (call_nr) {
    case HV_CONSOLE_PUTC:
        uart_putc((char)arg0);
        break;

    case HV_YIELD:
        vcpu->state = VCPU_STATE_RUNNABLE;
        schedule();  // Yield to other vCPU
        break;

    case HV_SHUTDOWN:
        vm_shutdown(vcpu->vm, arg0);
        break;

    case HV_GET_TIME:
        ret = get_system_time();
        break;

    default:
        ret = -ENOSYS;
        break;
    }

    vcpu->context.regs[0] = ret;  // Return value
    vcpu->context.elr_el1 += 4;   // Skip HVC instruction
}
```

---

## Implementation Plan (Phase 7)

1. **Core hypervisor initialization** at EL2
2. **VM lifecycle management** (create, start, stop, destroy)
3. **vCPU creation and scheduling**
4. **Stage-2 page tables** setup and management
5. **VM entry/exit** infrastructure
6. **Virtual GIC** implementation
7. **Hypercall interface**
8. **Inter-VM shared memory**
9. **VM monitoring and debugging** tools

---

**Next Document**: [ACPI 6.5](05-acpi.md)
