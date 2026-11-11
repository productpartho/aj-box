/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 M-VAVE (groove_fm_unpack, from firmware/app/fm_library.c
 * at FM1 Open V14 4c7496d, unchanged).
 *
 * fm_library.c also holds the Flash voice library, which needs the device
 * SDK, so the desktop build takes only the VMEM unpacker from it. The factory
 * voices themselves come from firmware/app/fm_factory.c, compiled as is. */
#include "fm_patch.h"
#include <string.h>

static FM1_DSP_DATA const uint8_t op_max[21]={99,99,99,99,99,99,99,99,99,99,99,3,3,7,3,7,99,1,31,99,14};
static FM1_DSP_DATA const uint8_t global_max[19]={99,99,99,99,99,99,99,99,31,7,1,99,99,99,99,1,5,7,48};

FM1_DSP_CODE void groove_fm_unpack(uint8_t p[156], const uint8_t b[128])
{
    for (unsigned op=0;op<6;++op) {
        const uint8_t *v=b+op*17; uint8_t *q=p+op*21;
        memcpy(q,v,11); q[11]=v[11]&3U; q[12]=(v[11]>>2)&3U;
        q[13]=v[12]&7U; q[20]=(v[12]>>3)&15U;
        q[14]=v[13]&3U; q[15]=(v[13]>>2)&7U; q[16]=v[14];
        q[17]=v[15]&1U; q[18]=(v[15]>>1)&31U; q[19]=v[16];
    }
    memcpy(p+126,b+102,8); p[134]=b[110]&31U;
    p[135]=b[111]&7U; p[136]=(b[111]>>3)&1U;
    memcpy(p+137,b+112,4); p[141]=b[116]&1U;
    p[142]=(b[116]>>1)&7U; p[143]=(b[116]>>4)&7U;
    memcpy(p+144,b+117,11); p[155]=63;
    /* Some original V15 VMEM parameters are 127 (including rates/detune).
     * Preserve VMEM for round trips; never feed those values to DSP tables. */
    for (unsigned i=0;i<145;++i) {
        unsigned limit=i<126?op_max[i%21]:global_max[i-126];
        if (p[i]>limit) p[i]=(uint8_t)limit;
    }
}
