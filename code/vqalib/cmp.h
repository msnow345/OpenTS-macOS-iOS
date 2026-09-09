/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2025 Electronic Arts Inc.
 * Copyright 2026 OpenTS contributors
 *
 * Contains material derived from Electronic Arts source code.
 * Modified by OpenTS contributors, 2026.
 * EA's GPLv3 Section 7 additional terms and supplemental warranty
 * disclaimers apply; see LICENSE.md.
 ******************************************************************************/

#ifndef VQACMP_H
#define VQACMP_H

#pragma once

#include <cstddef>
#include <cstdint>

// The ADPCM decoder state is packed on every compiler rather than only on the two the
// original build used, so one build cannot disagree with another about its layout.
#pragma pack(push,1)

struct _VQA_SOS_COMPRESS_INFO

{
	int32_t dwPredicted;
	short wIndex;
	int32_t dwPredicted2;
	short wIndex2;
};

typedef _VQA_SOS_COMPRESS_INFO VQASOS;

static_assert(sizeof(VQASOS) == 12, "ADPCM decoder state layout changed");
static_assert(offsetof(VQASOS, dwPredicted2) == 6, "ADPCM decoder state layout changed");

extern "C" {
void __cdecl VQA_sosCODECInitStream(_VQA_SOS_COMPRESS_INFO *);
void __cdecl VQA_sosCODECDecompressData(void *src, void *dst, unsigned short wBitSize, unsigned short wChannels, uint32_t dwUnCompSize, _VQA_SOS_COMPRESS_INFO *sosinfo);
}

//#define VQA_sosCODECDecompressData sosCODECDecompressData

#pragma pack(pop)

#endif //VQACMP_H
