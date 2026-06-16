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
#include "qemu/error-report.h"
#include "hw/pci/pci_device.h"
#include "exec/cpu-common.h"
#include "ati_3d.h"
#include "ati_regs.h"

void ati_3d_init(ATIVGAState *s)
{
    s->is_3d = true;
    warn_report("ati3d: Phase 1 capabilities activated (vgamem_mb=32)");

    s->regs.pm4_buffer_cntl         = 0;
    s->regs.pm4_buffer_wm_cntl      = 0;
    s->regs.pm4_buffer_dl_rptr_addr = 0;
    s->regs.pm4_buffer_dl_rptr      = 0;
    s->regs.pm4_buffer_dl_wptr      = 0;
    s->regs.pm4_micro_cntl          = 0;
    s->regs.scale_3d_cntl           = 0;
    s->regs.misc_3d_state_cntl      = 0;

    /*
     * Seed CRTC registers with a valid 1280×1024×32 geometry so that any
     * hardware-detection code that reads them before OpenBIOS programs the
     * VBE mode (which triggers the full sync in ati_mm_write) does not see
     * all-zeros and abort.  The VBE-enable write will overwrite these with
     * the actual display dimensions.
     */
    s->regs.crtc_gen_cntl      = CRTC2_EXT_DISP_EN | CRTC2_EN
                                  | CRTC_PIX_WIDTH_32BPP;
    s->regs.crtc_h_total_disp  = (uint32_t)((1280 / 8 - 1) << 16); /* 159 */
    s->regs.crtc_v_total_disp  = (uint32_t)((1024 - 1) << 16);     /* 1023 */
    s->regs.crtc_pitch         = (1280 * 4) / 8;                    /* 640 */
}

void ati_3d_pm4_sync(ATIVGAState *s)
{
    /*
     * Instantly complete all pending PM4 commands: advance RPTR to WPTR.
     * The guest driver will see the engine as idle on the next PM4_STAT
     * read or RPTR poll, unblocking its submission loop.
     */
    s->regs.pm4_buffer_dl_rptr = s->regs.pm4_buffer_dl_wptr;

    if (!s->regs.pm4_buffer_dl_rptr_addr) {
        return;
    }

    /*
     * Write the updated RPTR to the physical address the driver polls.
     * PM4_BUFFER_DL_RPTR_ADDR is a PCI bus address.  On real hardware the
     * GPU DMAs this value there; the driver may point it anywhere in the
     * machine's physical memory (VRAM via BAR0, or a DMA buffer it
     * allocated from system RAM).  Use cpu_physical_memory_write so we
     * reach any physical address without needing to know the region type.
     * Without this write the NDRV polls the location forever → hang.
     */
    uint32_t val = cpu_to_le32(s->regs.pm4_buffer_dl_rptr);
    cpu_physical_memory_write(s->regs.pm4_buffer_dl_rptr_addr, &val, 4);
}
