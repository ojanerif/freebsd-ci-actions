/*-
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Author: ojanerif@amd.com
 *
 * [TC-UNIT-EXT] — Zen 2+ extended Op MaxCnt field unit tests.
 *
 * Tests ibs_op_get_full_maxcnt() and ibs_op_set_full_maxcnt() from
 * ibs_decode.h.
 *
 * On Zen 2 and later, IBS Op supports a 23-bit sampling period encoded
 * across two non-contiguous fields in IBSOPCTL:
 *   base[15:0]  = IBSOPCTL bits 15:0   (IBS_OP_MAXCNT)
 *   ext[6:0]    = IBSOPCTL bits 26:20  (IBS_OP_MAXCNT_EXT)
 *   full_maxcnt = (ext << 16) | base
 *
 * No hardware access.  Runs on any architecture.
 *
 * Test IDs: TC-UNIT-EXT-01 … TC-UNIT-EXT-07
 */

#include <atf-c.h>

#include "ibs_decode.h"

/* TC-UNIT-EXT-01: base-only value (ext bits zero) */
ATF_TC_WITHOUT_HEAD(ibs_unit_ext_maxcnt_base_only);
ATF_TC_BODY(ibs_unit_ext_maxcnt_base_only, tc)
{
	ATF_CHECK_EQ(ibs_op_get_full_maxcnt(0x1234ULL), 0x1234ULL);
}

/* TC-UNIT-EXT-02: ext-only value (base bits zero) */
ATF_TC_WITHOUT_HEAD(ibs_unit_ext_maxcnt_ext_only);
ATF_TC_BODY(ibs_unit_ext_maxcnt_ext_only, tc)
{
	/* opctl = 1 << IBS_OP_MAXCNT_EXT_SHIFT = 1 << 20 = 0x100000
	 * ext = 1, base = 0 → full = (1 << 16) | 0 = 0x10000 */
	ATF_CHECK_EQ(ibs_op_get_full_maxcnt(0x100000ULL), 0x10000ULL);
}

/* TC-UNIT-EXT-03: combined base and ext */
ATF_TC_WITHOUT_HEAD(ibs_unit_ext_maxcnt_combined);
ATF_TC_BODY(ibs_unit_ext_maxcnt_combined, tc)
{
	/* opctl = 0x101234: ext=1 (bit 20), base=0x1234
	 * full = (1 << 16) | 0x1234 = 0x11234 */
	ATF_CHECK_EQ(ibs_op_get_full_maxcnt(0x101234ULL), 0x11234ULL);
}

/* TC-UNIT-EXT-04: maximum 23-bit value (ext=0x7F, base=0xFFFF) */
ATF_TC_WITHOUT_HEAD(ibs_unit_ext_maxcnt_max);
ATF_TC_BODY(ibs_unit_ext_maxcnt_max, tc)
{
	/* opctl = (0x7F << 20) | 0xFFFF = 0x07F0FFFF */
	uint64_t opctl = (0x7FULL << IBS_OP_MAXCNT_EXT_SHIFT) | 0xFFFFULL;
	ATF_CHECK_EQ(ibs_op_get_full_maxcnt(opctl), 0x7FFFFFULL);
}

/* TC-UNIT-EXT-05: set/get round-trip for a representative set of values */
ATF_TC_WITHOUT_HEAD(ibs_unit_ext_set_roundtrip);
ATF_TC_BODY(ibs_unit_ext_set_roundtrip, tc)
{
	const uint64_t values[] = { 0, 1, 0xFFFF, 0x10000, 0x7FFFFF };
	int i;

	for (i = 0; i < (int)(sizeof(values) / sizeof(values[0])); i++) {
		uint64_t opctl  = ibs_op_set_full_maxcnt(0ULL, values[i]);
		uint64_t result = ibs_op_get_full_maxcnt(opctl);

		ATF_CHECK_MSG(result == values[i],
		    "round-trip failed for maxcnt=0x%llx: got 0x%llx",
		    (unsigned long long)values[i],
		    (unsigned long long)result);
	}
}

/* TC-UNIT-EXT-06: IBS_OP_MAXCNT_EXT_SHIFT == 20 */
ATF_TC_WITHOUT_HEAD(ibs_unit_ext_shift_is_20);
ATF_TC_BODY(ibs_unit_ext_shift_is_20, tc)
{
	ATF_CHECK_EQ(IBS_OP_MAXCNT_EXT_SHIFT, 20);
}

/* TC-UNIT-EXT-07: IBS_OP_MAXCNT_EXT covers exactly 7 bits (bits 20–26) */
ATF_TC_WITHOUT_HEAD(ibs_unit_ext_mask_7bits);
ATF_TC_BODY(ibs_unit_ext_mask_7bits, tc)
{
	uint64_t mask = IBS_OP_MAXCNT_EXT;
	int bits = 0;

	while (mask != 0) {
		bits += (int)(mask & 1ULL);
		mask >>= 1;
	}
	ATF_CHECK_EQ(bits, 7);
	/* Verify it occupies bits 26:20 specifically */
	ATF_CHECK_EQ(IBS_OP_MAXCNT_EXT, 0x7FULL << IBS_OP_MAXCNT_EXT_SHIFT);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, ibs_unit_ext_maxcnt_base_only);
	ATF_TP_ADD_TC(tp, ibs_unit_ext_maxcnt_ext_only);
	ATF_TP_ADD_TC(tp, ibs_unit_ext_maxcnt_combined);
	ATF_TP_ADD_TC(tp, ibs_unit_ext_maxcnt_max);
	ATF_TP_ADD_TC(tp, ibs_unit_ext_set_roundtrip);
	ATF_TP_ADD_TC(tp, ibs_unit_ext_shift_is_20);
	ATF_TP_ADD_TC(tp, ibs_unit_ext_mask_7bits);
	return (atf_no_error());
}
