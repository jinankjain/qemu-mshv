/*
 * QEMU MSHV support
 *
 * Copyright Microsoft, Corp. 2025
 *
 * Authors:
 *  Jinank Jain       <jinankjain@microsoft.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#include "qemu/osdep.h"
#include "qemu/error-report.h"
#include "qemu/lockable.h"
#include "system/mshv.h"
#include "hw/hyperv/linux-mshv.h"
#include <sys/ioctl.h>
#include "system/cpus.h"
#include "target/arm/cpu.h"

static QemuMutex *cpu_guards_lock;
static GHashTable *cpu_guards;

static enum hv_register_name STANDARD_REGISTER_NAMES[32] = {
    HV_ARM64_REGISTER_X0,
    HV_ARM64_REGISTER_X1,
    HV_ARM64_REGISTER_X2,
    HV_ARM64_REGISTER_X3,
    HV_ARM64_REGISTER_X4,
    HV_ARM64_REGISTER_X5,
    HV_ARM64_REGISTER_X6,
    HV_ARM64_REGISTER_X7,
    HV_ARM64_REGISTER_X8,
    HV_ARM64_REGISTER_X9,
    HV_ARM64_REGISTER_X10,
    HV_ARM64_REGISTER_X11,
    HV_ARM64_REGISTER_X12,
    HV_ARM64_REGISTER_X13,
    HV_ARM64_REGISTER_X14,
    HV_ARM64_REGISTER_X15,
    HV_ARM64_REGISTER_X16,
    HV_ARM64_REGISTER_X17,
    HV_ARM64_REGISTER_X18,
    HV_ARM64_REGISTER_X19,
    HV_ARM64_REGISTER_X20,
    HV_ARM64_REGISTER_X21,
    HV_ARM64_REGISTER_X22,
    HV_ARM64_REGISTER_X23,
    HV_ARM64_REGISTER_X24,
    HV_ARM64_REGISTER_X25,
    HV_ARM64_REGISTER_X26,
    HV_ARM64_REGISTER_X27,
    HV_ARM64_REGISTER_X28,
    HV_ARM64_REGISTER_FP,
    HV_ARM64_REGISTER_LR,
    HV_ARM64_REGISTER_PC,
};

int mshv_set_generic_regs(int cpu_fd, hv_register_assoc *assocs, size_t n_regs)
{
    struct mshv_vp_registers input = {
        .count = n_regs,
        .regs = assocs,
    };

    return ioctl(cpu_fd, MSHV_SET_VP_REGISTERS, &input);
}

static int get_generic_regs(int cpu_fd, struct hv_register_assoc *assocs,
                            size_t n_regs)
{
    struct mshv_vp_registers input = {
        .count = n_regs,
        .regs = assocs,
    };

    return ioctl(cpu_fd, MSHV_GET_VP_REGISTERS, &input);
}

static void populate_standard_regs(const hv_register_assoc *assocs,
                                   CPUARMState *env)
{
    env->xregs[0] = assocs[0].value.reg64;
    env->xregs[1] = assocs[1].value.reg64;
    env->xregs[2] = assocs[2].value.reg64;
    env->xregs[3] = assocs[3].value.reg64;
    env->xregs[4] = assocs[4].value.reg64;
    env->xregs[5] = assocs[5].value.reg64;
    env->xregs[6] = assocs[6].value.reg64;
    env->xregs[7] = assocs[7].value.reg64;
    env->xregs[8] = assocs[8].value.reg64;
    env->xregs[9] = assocs[9].value.reg64;
    env->xregs[10] = assocs[10].value.reg64;
    env->xregs[11] = assocs[11].value.reg64;
    env->xregs[12] = assocs[12].value.reg64;
    env->xregs[13] = assocs[13].value.reg64;
    env->xregs[14] = assocs[14].value.reg64;
    env->xregs[15] = assocs[15].value.reg64;
    env->xregs[16] = assocs[16].value.reg64;
    env->xregs[17] = assocs[17].value.reg64;
    env->xregs[18] = assocs[18].value.reg64;
    env->xregs[19] = assocs[19].value.reg64;
    env->xregs[20] = assocs[20].value.reg64;
    env->xregs[21] = assocs[21].value.reg64;
    env->xregs[22] = assocs[22].value.reg64;
    env->xregs[23] = assocs[23].value.reg64;
    env->xregs[24] = assocs[24].value.reg64;
    env->xregs[25] = assocs[25].value.reg64;
    env->xregs[26] = assocs[26].value.reg64;
    env->xregs[27] = assocs[27].value.reg64;
    env->xregs[28] = assocs[28].value.reg64;
    env->xregs[29] = assocs[29].value.reg64;
    env->xregs[30] = assocs[30].value.reg64;
    env->pc = assocs[31].value.reg64;
}

int mshv_get_standard_regs(CPUState *cpu)
{
    size_t n_regs = sizeof(STANDARD_REGISTER_NAMES) / sizeof(hv_register_name);
    struct hv_register_assoc *assocs;
    int ret;
    ARMCPU *arm_cpu = ARM_CPU(cpu);
    CPUARMState *env = &arm_cpu->env;
    int cpu_fd = mshv_vcpufd(cpu);

    assocs = g_new0(hv_register_assoc, n_regs);
    for (size_t i = 0; i < n_regs; i++)
    {
        assocs[i].name = STANDARD_REGISTER_NAMES[i];
    }
    ret = get_generic_regs(cpu_fd, assocs, n_regs);
    if (ret < 0)
    {
        error_report("failed to get standard registers");
        g_free(assocs);
        return -1;
    }

    populate_standard_regs(assocs, env);

    g_free(assocs);
    return 0;
}

int mshv_load_regs(CPUState *cpu)
{
    int ret;

    ret = mshv_get_standard_regs(cpu);
    if (ret < 0)
    {
        error_report("Failed to load standard registers");
        return -1;
    }

    return 0;
}

static int set_standard_regs(const CPUState *cpu)
{
    size_t n_regs = sizeof(STANDARD_REGISTER_NAMES) / sizeof(hv_register_name);
    struct hv_register_assoc *assocs;
    int ret;
    ARMCPU *arm_cpu = ARM_CPU(cpu);
    CPUARMState *env = &arm_cpu->env;
    int cpu_fd = mshv_vcpufd(cpu);

    assocs = g_new0(hv_register_assoc, n_regs);
    for (size_t i = 0; i < n_regs; i++)
    {
        assocs[i].name = STANDARD_REGISTER_NAMES[i];
    }

    assocs[0].value.reg64 = env->xregs[0];
    assocs[1].value.reg64 = env->xregs[1];
    assocs[2].value.reg64 = env->xregs[2];
    assocs[3].value.reg64 = env->xregs[3];
    assocs[4].value.reg64 = env->xregs[4];
    assocs[5].value.reg64 = env->xregs[5];
    assocs[6].value.reg64 = env->xregs[6];
    assocs[7].value.reg64 = env->xregs[7];
    assocs[8].value.reg64 = env->xregs[8];
    assocs[9].value.reg64 = env->xregs[9];
    assocs[10].value.reg64 = env->xregs[10];
    assocs[11].value.reg64 = env->xregs[11];
    assocs[12].value.reg64 = env->xregs[12];
    assocs[13].value.reg64 = env->xregs[13];
    assocs[14].value.reg64 = env->xregs[14];
    assocs[15].value.reg64 = env->xregs[15];
    assocs[16].value.reg64 = env->xregs[16];
    assocs[17].value.reg64 = env->xregs[17];
    assocs[18].value.reg64 = env->xregs[18];
    assocs[19].value.reg64 = env->xregs[19];
    assocs[20].value.reg64 = env->xregs[20];
    assocs[21].value.reg64 = env->xregs[21];
    assocs[22].value.reg64 = env->xregs[22];
    assocs[23].value.reg64 = env->xregs[23];
    assocs[24].value.reg64 = env->xregs[24];
    assocs[25].value.reg64 = env->xregs[25];
    assocs[26].value.reg64 = env->xregs[26];
    assocs[27].value.reg64 = env->xregs[27];
    assocs[28].value.reg64 = env->xregs[28];
    assocs[29].value.reg64 = env->xregs[29];
    assocs[30].value.reg64 = env->xregs[30];
    assocs[31].value.reg64 = env->pc;

    // printf("set_standard_regs: pc: %lx\n", env->pc);

    ret = mshv_set_generic_regs(cpu_fd, assocs, n_regs);
    if (ret < 0)
    {
        error_report("failed to set standard registers");
        g_free(assocs);
        return -1;
    }

    g_free(assocs);

    return 0;
}

static int set_cpu_state(const CPUState *cpu)
{
    int ret;

    ret = set_standard_regs(cpu);
    if (ret < 0)
    {
        return ret;
    }

    return 0;
}

static int mshv_configure_arm_vcpu(const CPUState *cpu)
{
    int ret;

    ret = set_cpu_state(cpu);
    if (ret < 0)
    {
        error_report("failed to set cpu state");
        return -1;
    }

    return 0;
}

static int put_regs(const CPUState *cpu)
{
    int ret;

    ret = mshv_configure_arm_vcpu(cpu);
    if (ret < 0)
    {
        error_report("failed to configure vcpu");
        return ret;
    }

    return 0;
}

int mshv_arch_put_registers(const CPUState *cpu)
{
    int ret;

    ret = put_regs(cpu);
    if (ret < 0)
    {
        error_report("Failed to put registers");
        return -1;
    }

    return 0;
}

void mshv_arch_init_vcpu(CPUState *cpu)
{
    return;
}

void mshv_arch_destroy_vcpu(CPUState *cpu)
{
    return;
}

static int set_memory_info(const struct hyperv_message *msg,
                           struct hv_arm64_memory_intercept_message *info)
{
    if (msg->header.message_type != HVMSG_GPA_INTERCEPT
            && msg->header.message_type != HVMSG_UNMAPPED_GPA
            && msg->header.message_type != HVMSG_UNACCEPTED_GPA) {
        error_report("invalid message type");
        return -1;
    }
    memcpy(info, msg->payload, sizeof(*info));

    return 0;
}

typedef union {
    uint64_t raw;
    struct {
        uint32_t iss: 25;
        uint32_t il: 1;
        uint32_t ec: 6;
        uint32_t iss2: 5;
        uint32_t _rsvd: 27;
    } __attribute__((packed));
} EsrEl2;

typedef union {
    uint32_t raw;
    struct {
        uint32_t dfsc: 6;
        uint32_t wnr: 1;
        uint32_t s1ptw: 1;
        uint32_t cm: 1;
        uint32_t ea: 1;
        uint32_t fnv: 1;
        uint32_t set: 2;
        uint32_t vncr: 1;
        uint32_t ar: 1;
        uint32_t sf: 1;
        uint32_t srt: 5;
        uint32_t sse: 1;
        uint32_t sas: 2;
        uint32_t isv: 1;
        uint32_t _unused: 7;
    } __attribute__((packed));
} IssDataAbort;

typedef enum {
    data_abort_lower = 36,
    data_abort = 37,
} ExceptionClass;

int mshv_store_regs(CPUState *cpu)
{
    int ret;

    ret = set_standard_regs(cpu);
    if (ret < 0) {
        error_report("Failed to store standard registers");
        return -1;
    }

    /* TODO: should store special registers? the equivalent hvf code doesn't */

    return 0;
}

static int emulate_with_syndrome(CPUState *cpu,
                                 struct hv_arm64_memory_intercept_message *info)
{
    ARMCPU *arm_cpu = ARM_CPU(cpu);
    CPUARMState *env = &arm_cpu->env;
    int ret;
    EsrEl2 syndrome = { 0 };
    syndrome.raw = info->syndrome;
    int cpu_fd = mshv_vcpufd(cpu);
    QemuMutex *guard;

    guard = g_hash_table_lookup(cpu_guards, GUINT_TO_POINTER(cpu_fd));
    if (!guard) {
        error_report("failed to get cpu guard");
        return -1;
    }

    uint8_t ec = syndrome.ec;
    uint64_t gpa = info->guest_physical_address;

    if (!(ec == data_abort_lower || ec == data_abort)) {
        error_report("Unknown exception class 0x%x\n", ec);
        return -1;
    }

    IssDataAbort iss = { 0 };
    iss.raw = syndrome.iss;
    if (!iss.isv) {
        error_report("Invalid ESR EL2 ISV field\n");
        return -1;
    }

    uint64_t len = 1ULL << iss.sas;
    bool sign_extend = iss.sse;
    uint64_t reg_index = iss.srt;

    WITH_QEMU_LOCK_GUARD(guard) {
        // Load the regs from MSHV
        ret = mshv_load_regs(cpu);
        if (ret < 0) {
            error_report("Failed to load registers");
            return -1;
        }

        if (iss.wnr) {
            uint8_t data[8];
            uint64_t val = reg_index < 31 ? env->xregs[reg_index] : 0ULL;
            val = cpu_to_le64(val);

            memcpy(data, &val, sizeof(val));
            ret = mshv_guest_mem_write(gpa, data, len, false);
            if (ret < 0) {
                error_report("Failed to write guest memory");
                return -1;
            }
        } else {
            uint8_t data[8] = { 0 };
            ret = mshv_guest_mem_read(gpa, data, len, false, false);
            if (ret < 0) {
                error_report("Failed to read guest memory");
                return -1;
            }

            uint64_t val;
            memcpy(&val, data, sizeof(val));

            val = le64_to_cpu(val);

            if (sign_extend) {
                uint64_t shift = 64 - (len * 8);
                val = (((int64_t)val << shift) >> shift);
                if (!iss.sf) {
                    val &= 0xffffffff;
                }
            }

            env->xregs[reg_index] = val;
        }

        env->pc += (syndrome.il == 1) ? 4 : 2;

        ret = mshv_store_regs(cpu);
        if (ret < 0) {
            error_report("failed to store registers");
            return -1;
        }
    }

    return 0;
}

static int handle_unmapped_mem(int vm_fd, CPUState *cpu,
                               const struct hyperv_message *msg,
                               MshvVmExit *exit_reason)
{
    struct hv_arm64_memory_intercept_message info = { 0 };
    int ret;

    ret = set_memory_info(msg, &info);
    if (ret < 0) {
        error_report("failed to convert message to memory info");
        return -1;
    }

    ret = emulate_with_syndrome(cpu, &info);
    if (ret < 0) {
        error_report("Failed to emulate with syndrome");
        return -1;
    }

    return 0;
}

int mshv_run_vcpu(int vm_fd, CPUState *cpu, hv_message *msg, MshvVmExit *exit)
{
    int ret;
    hv_message exit_msg = {0};
    enum MshvVmExit exit_reason = {0};
    int cpu_fd = mshv_vcpufd(cpu);

    // printf("mshv_run_vcpu %d\n", cpu_fd);

    ret = ioctl(cpu_fd, MSHV_RUN_VP, &exit_msg);
    // printf("mshv_run_vcpu1 %d\n", cpu_fd);

    if (ret < 0)
    {
        error_report("failed to run vcpu: %s", strerror(errno));
        return MshvVmExitShutdown;
    }

    // printf("exit_msg: %d\n", exit_msg.header.message_type);

    switch (exit_msg.header.message_type) {
    case HVMSG_UNMAPPED_GPA:
        ret = handle_unmapped_mem(vm_fd, cpu, &exit_msg, &exit_reason);
        if (ret < 0) {
            error_report("failed to handle unmapped memory");
            return -1;
        }
        return exit_reason;
    default:
        printf("exit_msg: %d\n", exit_msg.header.message_type);
        msg = &exit_msg;
    }

    *exit = MshvVmExitIgnore;
    return 0;
}

static void add_cpu_guard(int cpu_fd)
{
    QemuMutex *guard;

    WITH_QEMU_LOCK_GUARD(cpu_guards_lock)
    {
        guard = g_new0(QemuMutex, 1);
        qemu_mutex_init(guard);
        g_hash_table_insert(cpu_guards, GUINT_TO_POINTER(cpu_fd), guard);
    }
}

int mshv_create_vcpu(int vm_fd, uint8_t vp_index, int *cpu_fd)
{
    int ret;
    struct mshv_create_vp vp_arg = {
        .vp_index = vp_index,
    };

    ret = ioctl(vm_fd, MSHV_CREATE_VP, &vp_arg);
    if (ret < 0)
    {
        error_report("failed to create mshv vcpu: %s", strerror(errno));
        return -1;
    }

    add_cpu_guard(ret);
    *cpu_fd = ret;

    return 0;
}

void mshv_remove_vcpu(int vm_fd, int cpu_fd)
{
    return;
}

void mshv_init_cpu_logic(void)
{
    cpu_guards_lock = g_new0(QemuMutex, 1);
    qemu_mutex_init(cpu_guards_lock);
    cpu_guards = g_hash_table_new(g_direct_hash, g_direct_equal);
}

void mshv_arch_amend_proc_features(union hv_partition_synthetic_processor_features *features)
{
    /* No-op for now */
    return;
}

int mshv_arch_post_init_vm(int vm_fd)
{
    return 0;
}