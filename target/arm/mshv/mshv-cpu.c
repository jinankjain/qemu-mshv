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
#include "system/mshv.h"
#include "hw/hyperv/linux-mshv.h"
#include <sys/ioctl.h>

int mshv_set_generic_regs(int cpu_fd, hv_register_assoc *assocs, size_t n_regs)
{
    struct mshv_vp_registers input = {
        .count = n_regs,
        .regs = assocs,
    };

    return ioctl(cpu_fd, MSHV_SET_VP_REGISTERS, &input);
}

int mshv_load_regs(CPUState *cpu)
{
    return -1;
}

int mshv_arch_put_registers(const CPUState *cpu)
{
    return -1;
}

void mshv_arch_init_vcpu(CPUState *cpu)
{
    return;
}

void mshv_arch_destroy_vcpu(CPUState *cpu)
{
    return;
}

int mshv_run_vcpu(int vm_fd, CPUState *cpu, hv_message *msg, MshvVmExit *exit)
{
    return -1;
}

int mshv_create_vcpu(int vm_fd, uint8_t vp_index, int *cpu_fd)
{
    return -1;
}

void mshv_remove_vcpu(int vm_fd, int cpu_fd)
{
    return;
}

void mshv_init_cpu_logic(void)
{
    return;
}