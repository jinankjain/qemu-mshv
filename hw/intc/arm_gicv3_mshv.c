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
#include "qapi/error.h"
#include "hw/intc/arm_gicv3_common.h"
#include "target/arm/cpu.h"
#include "qom/object.h"
#include "migration/blocker.h"

#define DEBUG_GICV3_MSHV 1

#ifdef DEBUG_GICV3_MSHV
#define DPRINTF(fmt, ...) \
        do { fprintf(stderr, "mshv_gicv3: " fmt, ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) \
        do { } while (0)
#endif

#define TYPE_MSHV_ARM_GICV3 "mshv-arm-gicv3-gicv2m"
typedef struct MSHVARMGICv3Class MSHVARMGICv3Class;

/* This is reusing the GICv3State typedef from ARMGICv3Common */
DECLARE_OBJ_CHECKERS(GICv3State, MSHVARMGICv3Class,
                     MSHV_ARM_GICV3, TYPE_MSHV_ARM_GICV3)

struct MSHVARMGICv3Class {
        ARMGICv3CommonClass parent_class;
        DeviceRealize parent_realize;
        ResettablePhases parent_phases;
        /* TODO: Add more field for MSHV GICv3 GICv2M */
};

static void mshv_arm_gicv3_get(GICv3State *s)
{
        /* TODO: Implement this function */
        g_assert_not_reached();
}

static void mshv_arm_gicv3_put(GICv3State *s)
{
        /* TODO: Implement this function */
        g_assert_not_reached();
}

static void mshv_arm_gicv3_reset_hold(Object *obj, ResetType type)
{
        GICv3State *s = ARM_GICV3_COMMON(obj);
        MSHVARMGICv3Class *mgcc = MSHV_ARM_GICV3_GET_CLASS(s);

        DPRINTF("Reset MSHV GIC\n");

        if (mgcc->parent_phases.hold) {
                mgcc->parent_phases.hold(obj, type);
        }

        if (s->migration_blocker) {
                DPRINTF("Cannot put kernel gic state, no kernel interface\n");
                return;
        }

        DPRINTF("Coming after hold phase\n");

        mshv_arm_gicv3_put(s);
}

static void mshv_arm_gicv3_set_irq(void *opaque, int irq, int level)
{
        GICv3State *s = (GICv3State *)opaque;
        uint32_t num_external_irq = s->num_irq;
        /* Meaning of the 'irq' parameter:
         *  [0..N-1] : SPI (device) interrupts
         *  [N..N+31] : SGI/PPI (internal) interrupts for CPU 0
         *  [N+32..N+63] : SGI/PPI (internal) interrupts for CPU 1
         *  ...
         * Convert this to MSHV's desired encoding. Architecturally, SGIs are
         * numbered 0-15 and PPIs are numbered 16-31. The SGIs and PPIs are banked
         * per CPU. SPIs are numbered 32-1019 and additionally 4096-5119 (if
         * supported by the GIC). WHP wants different encodings for SPIs and
         * SGIs/PPIs.
         */
        // uint32_t vector;
        // uint32_t destination;

        assert(num_external_irq > GIC_INTERNAL);

        if (irq < (num_external_irq - GIC_INTERNAL)) {
                /* SPI. The architectural interrupt number (32-1019) goes in
                 * vector, and destination must be 0. IRQ routing will be performed
                 * according to VM configuration (including GIC state).
                 */
                uint32_t arch_irq = irq + GIC_INTERNAL;
                DPRINTF("mshv_arm_gicv3_set_irq %d level %d\n", arch_irq, level);
                // vector = arch_irq;
                // destination = 0;
        } else {
                int banked_irq = irq - (num_external_irq - GIC_INTERNAL);
                int cpu = irq / GIC_INTERNAL;

                printf("mshv_arm_gicv3_set_irq unsupported irq %d for cpu %d level %d\n",
                       banked_irq, cpu, level);
                /* TODO: values for vector and destination are TBD */
                g_assert_not_reached();
        }
        // whpx_arm_set_irq(vector, destination, level);
}

static void mshv_arm_gicv3_realize(DeviceState *dev, Error **errp)
{
        GICv3State *s = MSHV_ARM_GICV3(dev);
        MSHVARMGICv3Class *mgcc = MSHV_ARM_GICV3_GET_CLASS(s);
        Error *local_err = NULL;
        // int i;

        DPRINTF("Realizing MSHV GIC\n");

        mgcc->parent_realize(dev, &local_err);
        if (local_err) {
                error_propagate(errp, local_err);
                return;
        }

        if (s->revision != 3) {
                error_setg(errp, "MSHV GICv3 only supports revision 3");
                return;
        }

        if (s->security_extn) {
                error_setg(errp, "MSHV GICv3 does not support security extensions");
                return;
        }

        if (s->nb_redist_regions > 1) {
                error_setg(errp, "Multiple VGICv3 redistributor regions are not "
                                 "supported by MSHV");
                error_append_hint(errp, "A maximum of %d VCPUs can be used",
                                  s->redist_region_count[0]);
                return;
        }

        /* TODO: Migration? */
        error_setg(&s->migration_blocker, "MSHV does not support VGICv3 migration");
        if (migrate_add_blocker(&s->migration_blocker, errp) < 0) {
                return;
        }

        gicv3_init_irqs_and_mmio(s, mshv_arm_gicv3_set_irq, NULL);
}

static void mshv_arm_gicv3_class_init(ObjectClass *klass, const void *data)
{
        DeviceClass *dc = DEVICE_CLASS(klass);
        ResettableClass *rc = RESETTABLE_CLASS(klass);
        ARMGICv3CommonClass *agcc = ARM_GICV3_COMMON_CLASS(klass);
        MSHVARMGICv3Class *mgcc = MSHV_ARM_GICV3_CLASS(klass);

        agcc->pre_save = mshv_arm_gicv3_get;
        agcc->post_load = mshv_arm_gicv3_put;

        device_class_set_parent_realize(dc, mshv_arm_gicv3_realize,
                                        &mgcc->parent_realize);

        resettable_class_set_parent_phases(rc, NULL, mshv_arm_gicv3_reset_hold,
                                           NULL, &mgcc->parent_phases);
}

static const TypeInfo mshv_arm_gicv3_info = {
        .name = TYPE_MSHV_ARM_GICV3,
        .parent = TYPE_ARM_GICV3_COMMON,
        .instance_size = sizeof(GICv3State),
        .class_init = mshv_arm_gicv3_class_init,
        .class_size = sizeof(MSHVARMGICv3Class),
};

static void mshv_arm_gicv3_register_types(void)
{
        type_register_static(&mshv_arm_gicv3_info);
}

type_init(mshv_arm_gicv3_register_types)