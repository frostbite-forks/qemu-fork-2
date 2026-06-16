/*
 * ATI Rage 128 Pro — Phase 1: Fake 3D capability stubs
 *
 * Activated when vgamem_mb == 32. Makes Mac OS 9 RAVE detect hardware
 * acceleration by responding correctly to PM4/CCE engine queries.
 *
 * Copyright (c) 2026 brunocastello
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef ATI_3D_H
#define ATI_3D_H

#include "ati_int.h"

/* Called from ati_realize() when vga.vram_size_mb == 32 */
void ati_3d_init(ATIVGAState *s);

/*
 * Called on every PM4_BUFFER_DL_WPTR write.
 * Phase 1: instantly advances RPTR = WPTR and writes it to the VRAM
 * poll location so the guest driver sees all commands as completed.
 * Phase 2 will intercept the ring buffer here and dispatch to Metal.
 */
void ati_3d_pm4_sync(ATIVGAState *s);

#endif /* ATI_3D_H */
