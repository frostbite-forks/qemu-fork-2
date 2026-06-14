/*
 * ATI Rage 128 Pro — Phase 1: Fake 3D capability stubs
 *
 * Responds to PM4/CCE engine register accesses so Mac OS 9 RAVE
 * detects hardware 3D acceleration and enables the hardware pipeline.
 *
 * The CCE (Concurrent Command Engine) ring buffer protocol:
 *   1. Guest writes PM4_BUFFER_DL_RPTR_ADDR with a VRAM address.
 *   2. Guest writes PM4_BUFFER_CNTL to configure and enable the ring.
 *   3. Guest submits PM4 packets by writing to WPTR.
 *   4. Guest polls the VRAM location (or PM4_BUFFER_DL_RPTR register)
 *      to detect when the engine has consumed the commands.
 *   5. Guest checks PM4_STAT == 0 for engine-idle confirmation.
 *
 * Phase 1: All submitted commands are "completed" instantly by setting
 * RPTR = WPTR. Phase 2 will intercept the ring buffer contents here
 * and translate RAVE PM4 packets to Metal command buffers.
 *
 * Copyright (c) 2026 brunocastello
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/bswap.h"
#include "hw/pci/pci_device.h"
#include "ati_3d.h"
#include "ati_regs.h"

void ati_3d_init(ATIVGAState *s)
{
    s->is_3d = true;

    s->regs.pm4_buffer_cntl         = 0;
    s->regs.pm4_buffer_wm_cntl      = 0;
    s->regs.pm4_buffer_dl_rptr_addr = 0;
    s->regs.pm4_buffer_dl_rptr      = 0;
    s->regs.pm4_buffer_dl_wptr      = 0;
    s->regs.pm4_micro_cntl          = 0;
    s->regs.scale_3d_cntl           = 0;
    s->regs.misc_3d_state_cntl      = 0;
}

void ati_3d_pm4_sync(ATIVGAState *s)
{
    /*
     * Instantly complete all pending PM4 commands: advance RPTR to WPTR.
     * The guest driver will see the engine as idle on the next PM4_STAT
     * read or RPTR poll, unblocking its submission loop.
     */
    s->regs.pm4_buffer_dl_rptr = s->regs.pm4_buffer_dl_wptr;

    /*
     * Write the updated RPTR to the VRAM location the driver polls.
     * PM4_BUFFER_DL_RPTR_ADDR holds the GPU bus address; we convert it
     * to a VRAM offset by subtracting BAR 0's assigned PCI base address.
     */
    if (s->regs.pm4_buffer_dl_rptr_addr) {
        uint32_t bar0 = pci_get_long(s->dev.config + PCI_BASE_ADDRESS_0)
                        & ~0xfU;
        if (bar0) {
            uint32_t offset = s->regs.pm4_buffer_dl_rptr_addr - bar0;
            if (offset + 4 <= s->vga.vram_size) {
                stl_le_p(s->vga.vram_ptr + offset,
                         s->regs.pm4_buffer_dl_rptr);
            }
        }
    }
}
