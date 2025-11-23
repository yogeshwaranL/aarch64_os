# ACPI 6.5 Implementation

## ACPI 6.5 Compliance with AML Interpreter

**Version:** 0.1.0
**Component:** ACPI Subsystem
**Implementation Phase:** Phase 8
**Specification:** ACPI 6.5

---

## Overview

Complete ACPI 6.5 implementation providing:
- **ACPI Tables**: RSDP, XSDT, MADT, GTDT, FADT, DSDT, SSDT
- **Full AML Interpreter**: Complete bytecode interpreter for ACPI Machine Language
- **ACPI Namespace**: Device tree management
- **Power Management**: C-states, P-states, S-states
- **Device Discovery**: Enumeration via ACPI namespace

---

## ACPI Table Structure

### Root System Description Pointer (RSDP)

```c
/**
 * @brief RSDP structure (ACPI 2.0+)
 */
struct acpi_rsdp {
    char signature[8];          // "RSD PTR "
    uint8_t checksum;           // Checksum of first 20 bytes
    char oem_id[6];             // OEM ID
    uint8_t revision;           // 2 for ACPI 2.0+
    uint32_t rsdt_address;      // 32-bit RSDT address (unused in ACPI 2.0+)

    // Extended fields (ACPI 2.0+)
    uint32_t length;            // Length of table
    uint64_t xsdt_address;      // 64-bit XSDT address
    uint8_t extended_checksum;  // Checksum of entire table
    uint8_t reserved[3];
} __packed;
```

### System Description Table Header

```c
/**
 * @brief Common ACPI table header
 */
struct acpi_table_header {
    char signature[4];          // Table signature (e.g., "XSDT", "MADT")
    uint32_t length;            // Total table length
    uint8_t revision;           // Table revision
    uint8_t checksum;           // Checksum of entire table
    char oem_id[6];             // OEM ID
    char oem_table_id[8];       // OEM Table ID
    uint32_t oem_revision;      // OEM Revision
    uint32_t creator_id;        // Creator ID
    uint32_t creator_revision;  // Creator Revision
} __packed;
```

### Extended System Description Table (XSDT)

```c
/**
 * @brief XSDT - contains 64-bit pointers to other tables
 */
struct acpi_xsdt {
    struct acpi_table_header header;
    uint64_t entry[];           // Array of 64-bit physical addresses
} __packed;
```

---

## Multiple APIC Description Table (MADT)

For ARM systems, describes GIC configuration.

```c
/**
 * @brief MADT structure
 */
struct acpi_madt {
    struct acpi_table_header header;
    uint32_t local_apic_address;    // Not used on ARM
    uint32_t flags;

    // Followed by variable-length interrupt controller structures
} __packed;

/**
 * @brief MADT entry types for ARM
 */
#define ACPI_MADT_TYPE_GIC_CPU          0x0B  // GIC CPU Interface
#define ACPI_MADT_TYPE_GIC_DISTRIBUTOR  0x0C  // GIC Distributor
#define ACPI_MADT_TYPE_GIC_MSI_FRAME    0x0D  // GIC MSI Frame
#define ACPI_MADT_TYPE_GIC_REDISTRIBUTOR 0x0E // GIC Redistributor
#define ACPI_MADT_TYPE_GIC_ITS          0x0F  // GIC Interrupt Translation Service

/**
 * @brief GIC CPU Interface structure (GICC)
 */
struct acpi_madt_gicc {
    uint8_t type;               // 0x0B
    uint8_t length;             // 80 bytes
    uint16_t reserved;
    uint32_t cpu_interface_number;
    uint32_t uid;               // ACPI processor UID
    uint32_t flags;             // Enabled, Performance Interrupt Mode
    uint32_t parking_version;   // Parking protocol version
    uint32_t performance_interrupt;
    uint64_t parked_address;    // Parking protocol mailbox
    uint64_t base_address;      // GIC CPU interface base address
    uint64_t gicv_base_address; // GIC virtual interface base
    uint64_t gich_base_address; // GIC virtual control base
    uint32_t vgic_interrupt;    // VGIC maintenance interrupt
    uint64_t gicr_base_address; // GICR base address
    uint64_t mpidr;             // ARM Multiprocessor Affinity Register
    uint8_t efficiency_class;   // Power efficiency class
    uint8_t reserved2;
    uint16_t spe_interrupt;     // Statistical Profiling Extension interrupt
} __packed;

/**
 * @brief GIC Distributor structure (GICD)
 */
struct acpi_madt_gicd {
    uint8_t type;               // 0x0C
    uint8_t length;             // 24 bytes
    uint16_t reserved;
    uint32_t gic_id;            // GIC ID
    uint64_t base_address;      // GIC Distributor base address
    uint32_t global_irq_base;   // Global system interrupt base
    uint8_t version;            // GIC version (3 or 4)
    uint8_t reserved2[3];
} __packed;
```

---

## Generic Timer Description Table (GTDT)

```c
/**
 * @brief GTDT - ARM Generic Timer information
 */
struct acpi_gtdt {
    struct acpi_table_header header;
    uint64_t counter_block_address; // Counter read base address
    uint32_t reserved;
    uint32_t secure_el1_interrupt;  // Secure EL1 timer GSIV
    uint32_t secure_el1_flags;      // Secure EL1 timer flags
    uint32_t non_secure_el1_interrupt; // Non-secure EL1 timer GSIV
    uint32_t non_secure_el1_flags;     // Non-secure EL1 timer flags
    uint32_t virtual_timer_interrupt;  // Virtual timer GSIV
    uint32_t virtual_timer_flags;      // Virtual timer flags
    uint32_t non_secure_el2_interrupt; // Non-secure EL2 timer GSIV
    uint32_t non_secure_el2_flags;     // Non-secure EL2 timer flags
    uint64_t counter_read_block_address;
    uint32_t platform_timer_count;     // Number of platform timers
    uint32_t platform_timer_offset;    // Offset to platform timer array
} __packed;
```

---

## Fixed ACPI Description Table (FADT)

```c
/**
 * @brief FADT - Fixed ACPI Description Table
 */
struct acpi_fadt {
    struct acpi_table_header header;
    uint32_t firmware_ctrl;     // 32-bit FACS address (legacy)
    uint32_t dsdt;              // 32-bit DSDT address (legacy)
    uint8_t reserved;
    uint8_t preferred_pm_profile; // Power management profile
    uint16_t sci_interrupt;     // SCI interrupt vector
    uint32_t smi_command;       // SMI command port
    uint8_t acpi_enable;        // Value to enable ACPI
    uint8_t acpi_disable;       // Value to disable ACPI
    // ... many more fields ...
    uint64_t x_firmware_ctrl;   // 64-bit FACS address
    uint64_t x_dsdt;            // 64-bit DSDT address
    // ... extended fields ...
} __packed;

#define ACPI_PM_PROFILE_UNSPECIFIED     0
#define ACPI_PM_PROFILE_DESKTOP         1
#define ACPI_PM_PROFILE_MOBILE          2
#define ACPI_PM_PROFILE_WORKSTATION     3
#define ACPI_PM_PROFILE_ENTERPRISE_SERVER 4
#define ACPI_PM_PROFILE_SOHO_SERVER     5
#define ACPI_PM_PROFILE_APPLIANCE_PC    6
#define ACPI_PM_PROFILE_PERFORMANCE_SERVER 7
#define ACPI_PM_PROFILE_TABLET          8
```

---

## Differentiated System Description Table (DSDT)

Contains AML bytecode describing system devices and configuration.

```c
/**
 * @brief DSDT structure
 */
struct acpi_dsdt {
    struct acpi_table_header header;
    uint8_t aml_code[];         // AML bytecode
} __packed;
```

---

## AML Interpreter

### AML Opcodes (Sample)

```c
/**
 * @brief AML Opcode definitions
 */
#define AML_NULL_OP             0x00
#define AML_ZERO_OP             0x00
#define AML_ONE_OP              0x01
#define AML_ALIAS_OP            0x06
#define AML_NAME_OP             0x08
#define AML_BYTE_PREFIX         0x0A
#define AML_WORD_PREFIX         0x0B
#define AML_DWORD_PREFIX        0x0C
#define AML_STRING_PREFIX       0x0D
#define AML_QWORD_PREFIX        0x0E
#define AML_SCOPE_OP            0x10
#define AML_BUFFER_OP           0x11
#define AML_PACKAGE_OP          0x12
#define AML_VAR_PACKAGE_OP      0x13
#define AML_METHOD_OP           0x14
#define AML_DUAL_NAME_PREFIX    0x2E
#define AML_MULTI_NAME_PREFIX   0x2F
#define AML_NAME_CHAR_A         0x41
#define AML_NAME_CHAR_Z         0x5A
#define AML_ROOT_PREFIX         0x5C
#define AML_PARENT_PREFIX       0x5E

// Arithmetic
#define AML_ADD_OP              0x72
#define AML_SUBTRACT_OP         0x74
#define AML_MULTIPLY_OP         0x77
#define AML_DIVIDE_OP           0x78
#define AML_MOD_OP              0x85
#define AML_INCREMENT_OP        0x75
#define AML_DECREMENT_OP        0x76

// Logical
#define AML_LAND_OP             0x90
#define AML_LOR_OP              0x91
#define AML_LNOT_OP             0x92
#define AML_LEQUAL_OP           0x93
#define AML_LGREATER_OP         0x94
#define AML_LLESS_OP            0x95

// Control flow
#define AML_IF_OP               0xA0
#define AML_ELSE_OP             0xA1
#define AML_WHILE_OP            0xA2
#define AML_RETURN_OP           0xA4
#define AML_BREAK_OP            0xA5

// Field access
#define AML_FIELD_OP            0x81
#define AML_DEVICE_OP           0x5B82
#define AML_PROCESSOR_OP        0x5B83
#define AML_POWER_RES_OP        0x5B84
#define AML_THERMAL_ZONE_OP     0x5B85
```

### AML Object Types

```c
/**
 * @brief ACPI object types
 */
enum acpi_object_type {
    ACPI_TYPE_ANY           = 0x00,
    ACPI_TYPE_INTEGER       = 0x01,
    ACPI_TYPE_STRING        = 0x02,
    ACPI_TYPE_BUFFER        = 0x03,
    ACPI_TYPE_PACKAGE       = 0x04,
    ACPI_TYPE_FIELD_UNIT    = 0x05,
    ACPI_TYPE_DEVICE        = 0x06,
    ACPI_TYPE_EVENT         = 0x07,
    ACPI_TYPE_METHOD        = 0x08,
    ACPI_TYPE_MUTEX         = 0x09,
    ACPI_TYPE_REGION        = 0x0A,
    ACPI_TYPE_POWER         = 0x0B,
    ACPI_TYPE_PROCESSOR     = 0x0C,
    ACPI_TYPE_THERMAL       = 0x0D,
    ACPI_TYPE_BUFFER_FIELD  = 0x0E,
};

/**
 * @brief ACPI object structure
 */
struct acpi_object {
    enum acpi_object_type type;
    union {
        uint64_t integer;
        struct {
            char *buffer;
            size_t length;
        } string;
        struct {
            uint8_t *buffer;
            size_t length;
        } buffer;
        struct {
            uint32_t count;
            struct acpi_object **elements;
        } package;
        struct {
            uint8_t *aml_code;
            uint32_t aml_length;
            uint8_t arg_count;
            uint8_t flags;
        } method;
        void *reference;
    };
};
```

### ACPI Namespace

```c
/**
 * @brief ACPI namespace node
 */
struct acpi_namespace_node {
    char name[4];                       // 4-character name
    enum acpi_object_type type;
    struct acpi_object *object;         // Associated object
    struct acpi_namespace_node *parent; // Parent node
    struct acpi_namespace_node *child;  // First child
    struct acpi_namespace_node *peer;   // Next sibling
    uint32_t flags;
};

/**
 * @brief Root of ACPI namespace
 */
extern struct acpi_namespace_node *acpi_root_node;

/**
 * @brief Lookup a name in the ACPI namespace
 * @param path Pathname (e.g., "\\_SB.CPU0._STA")
 * @return Namespace node, or NULL if not found
 */
struct acpi_namespace_node *acpi_ns_lookup(const char *path);

/**
 * @brief Add node to namespace
 */
int acpi_ns_add_node(struct acpi_namespace_node *parent,
                     const char *name,
                     enum acpi_object_type type,
                     struct acpi_object *object);
```

### AML Interpreter Core

```c
/**
 * @brief AML execution context
 */
struct aml_context {
    uint8_t *aml;               // Current AML code position
    uint8_t *aml_end;           // End of AML code
    struct acpi_namespace_node *scope; // Current scope
    struct acpi_object *locals[8];     // Local variables (Local0-Local7)
    struct acpi_object *args[7];       // Arguments (Arg0-Arg6)
    struct acpi_object *return_value;  // Return value
};

/**
 * @brief Execute AML code
 * @param aml Pointer to AML bytecode
 * @param length Length of AML code
 * @param scope Starting scope in namespace
 * @return Return object, or NULL
 */
struct acpi_object *aml_execute(uint8_t *aml, size_t length,
                                struct acpi_namespace_node *scope)
{
    struct aml_context ctx = {
        .aml = aml,
        .aml_end = aml + length,
        .scope = scope,
    };

    while (ctx.aml < ctx.aml_end) {
        uint8_t opcode = *ctx.aml++;

        switch (opcode) {
        case AML_NAME_OP:
            aml_exec_name(&ctx);
            break;

        case AML_SCOPE_OP:
            aml_exec_scope(&ctx);
            break;

        case AML_METHOD_OP:
            aml_exec_method_decl(&ctx);
            break;

        case AML_DEVICE_OP:
            aml_exec_device(&ctx);
            break;

        case AML_IF_OP:
            aml_exec_if(&ctx);
            break;

        case AML_RETURN_OP:
            return aml_exec_return(&ctx);

        case AML_ADD_OP:
            aml_exec_add(&ctx);
            break;

        // ... many more opcodes ...

        default:
            if (aml_is_name_char(opcode)) {
                aml_exec_name_reference(&ctx, opcode);
            } else {
                pr_err("Unknown AML opcode: 0x%02x\n", opcode);
                return NULL;
            }
        }
    }

    return ctx.return_value;
}

/**
 * @brief Execute NAME opcode
 * Syntax: Name(XXXX, Value)
 */
static void aml_exec_name(struct aml_context *ctx)
{
    char name[5];
    struct acpi_object *obj;

    // Parse name (4 characters)
    aml_parse_name(ctx, name);

    // Parse value
    obj = aml_parse_term_arg(ctx);

    // Add to namespace
    acpi_ns_add_node(ctx->scope, name, obj->type, obj);
}

/**
 * @brief Execute METHOD opcode
 */
static void aml_exec_method_decl(struct aml_context *ctx)
{
    char name[5];
    uint8_t *pkg_start = ctx->aml;
    uint32_t pkg_length;
    uint8_t flags;
    struct acpi_object *method;

    // Parse package length
    pkg_length = aml_parse_pkg_length(ctx);

    // Parse method name
    aml_parse_name(ctx, name);

    // Parse flags (arg count, serialize, etc.)
    flags = *ctx->aml++;

    // Create method object
    method = acpi_object_alloc(ACPI_TYPE_METHOD);
    method->method.aml_code = ctx->aml;
    method->method.aml_length = pkg_length - (ctx->aml - pkg_start);
    method->method.arg_count = flags & 0x07;
    method->method.flags = flags;

    // Add to namespace
    acpi_ns_add_node(ctx->scope, name, ACPI_TYPE_METHOD, method);

    // Skip method body
    ctx->aml += method->method.aml_length;
}

/**
 * @brief Call an ACPI method
 */
struct acpi_object *acpi_evaluate_method(const char *path,
                                         struct acpi_object **args,
                                         int arg_count)
{
    struct acpi_namespace_node *node;
    struct acpi_object *method;
    struct aml_context ctx = {0};

    // Lookup method
    node = acpi_ns_lookup(path);
    if (!node || node->type != ACPI_TYPE_METHOD)
        return NULL;

    method = node->object;

    // Setup execution context
    ctx.aml = method->method.aml_code;
    ctx.aml_end = ctx.aml + method->method.aml_length;
    ctx.scope = node->parent;

    // Copy arguments
    for (int i = 0; i < arg_count && i < method->method.arg_count; i++) {
        ctx.args[i] = args[i];
    }

    // Execute method
    return aml_execute(ctx.aml, method->method.aml_length, ctx.scope);
}
```

---

## Power Management

### C-States (CPU Idle States)

```c
/**
 * @brief C-State descriptor
 */
struct acpi_cstate {
    uint8_t type;               // C-state type (C1, C2, C3...)
    uint32_t latency;           // Entry latency (microseconds)
    uint32_t power;             // Power consumption (milliwatts)
    struct acpi_generic_address reg; // Register to write
    uint64_t value;             // Value to write to enter C-state
};

/**
 * @brief Enter C-state
 */
void acpi_enter_cstate(struct acpi_cstate *cstate)
{
    // Write to register to enter C-state
    acpi_write_register(&cstate->reg, cstate->value);

    // CPU will enter low-power state (WFI on ARM)
    wfi();
}
```

### P-States (CPU Performance States)

```c
/**
 * @brief P-State descriptor
 */
struct acpi_pstate {
    uint32_t frequency;         // Core frequency (MHz)
    uint32_t power;             // Power consumption (milliwatts)
    uint32_t latency;           // Transition latency (microseconds)
};

/**
 * @brief Set P-state
 */
int acpi_set_pstate(int cpu, int pstate_index);
```

---

## Device Discovery

```c
/**
 * @brief Discover devices via ACPI
 */
void acpi_scan_devices(void)
{
    struct acpi_namespace_node *sb_node;

    // Find \_SB (System Bus)
    sb_node = acpi_ns_lookup("\\_SB");
    if (!sb_node)
        return;

    // Walk namespace under \_SB
    acpi_walk_namespace(sb_node, acpi_device_callback, NULL);
}

/**
 * @brief Callback for each device found
 */
static int acpi_device_callback(struct acpi_namespace_node *node, void *context)
{
    struct acpi_object *sta;

    if (node->type != ACPI_TYPE_DEVICE)
        return 0;

    // Evaluate _STA (status) method
    sta = acpi_evaluate_method(acpi_get_full_path(node, "_STA"), NULL, 0);
    if (!sta)
        return 0;

    // Check if device is present and functional
    if ((sta->integer & 0x0F) == 0x0F) {
        pr_info("ACPI: Found device %s\n", node->name);
        // Register device with kernel
        register_acpi_device(node);
    }

    acpi_object_free(sta);
    return 0;
}
```

---

## Implementation Plan (Phase 8)

1. **Table parsing** (RSDP, XSDT, MADT, GTDT, FADT, DSDT)
2. **ACPI namespace** creation from DSDT
3. **AML interpreter core** (opcode execution)
4. **Method evaluation** (_STA, _INI, _HID, _CRS, etc.)
5. **Device enumeration**
6. **Power management** (C-states, P-states)
7. **GIC configuration** from MADT
8. **Timer configuration** from GTDT
9. **Full AML opcode support**
10. **ACPI events** and GPE handling

---

**Next Document**: [Build Guide](../guides/building.md)
