/*-
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Author: ojanerif@amd.com
 *
 * [TC-UNIT-LDLAT] — Load Latency field unit tests.
 *
 * Tests the IBS Load Latency fields in IBSOPCTL (MSR_IBS_OP_CTL):
 *
 *   IBS_LDLAT_THRSH  = bits 62:59  (4-bit threshold)
 *   IBS_LDLAT_EN     = bit  63     (enable bit)
 *
 * No hardware access.  Runs on any architecture.
 *
 * Test IDs: TC-UNIT-LDLAT-01 … TC-UNIT-LDLAT-15
 */

#include <atf-c.h>

#include "ibs_decode.h"

/* TC-UNIT-LDLAT-01: IBS_LDLAT_THRSH_SHIFT is 59 */
ATF_TC_WITHOUT_HEAD(ibs_unit_ldlat_shift_is_59);
ATF_TC_BODY(ibs_unit_ldlat_shift_is_59, tc)
{
	ATF_CHECK_EQ(IBS_LDLAT_THRSH_SHIFT, 59);
}

/* TC-UNIT-LDLAT-02: IBS_LDLAT_EN is bit 63 */
ATF_TC_WITHOUT_HEAD(ibs_unit_ldlat_en_bit63);
ATF_TC_BODY(ibs_unit_ldlat_en_bit63, tc)
{
	ATF_CHECK_EQ(IBS_LDLAT_EN, (1ULL << 63));
}

/* TC-UNIT-LDLAT-03: IBS_LDLAT_THRSH covers exactly bits 62:59 */
ATF_TC_WITHOUT_HEAD(ibs_unit_ldlat_thrsh_mask);
ATF_TC_BODY(ibs_unit_ldlat_thrsh_mask, tc)
{
	ATF_CHECK_EQ(IBS_LDLAT_THRSH, 0x7800000000000000ULL);
}

/* TC-UNIT-LDLAT-04: IBS_LDLAT_THRSH is exactly 4 bits wide */
ATF_TC_WITHOUT_HEAD(ibs_unit_ldlat_thrsh_width);
ATF_TC_BODY(ibs_unit_ldlat_thrsh_width, tc)
{
	uint64_t mask = IBS_LDLAT_THRSH;
	int bits = 0;

	while (mask != 0) {
		bits += (int)(mask & 1ULL);
		mask >>= 1;
	}
	ATF_CHECK_EQ(bits, 4);
}

/* TC-UNIT-LDLAT-05: IBS_LDLAT_THRSH == 0x0FULL << IBS_LDLAT_THRSH_SHIFT */
ATF_TC_WITHOUT_HEAD(ibs_unit_ldlat_thrsh_shift_selfcheck);
ATF_TC_BODY(ibs_unit_ldlat_thrsh_shift_selfcheck, tc)
{
	ATF_CHECK_EQ(IBS_LDLAT_THRSH,
	    (0x0FULL << IBS_LDLAT_THRSH_SHIFT));
}

/* TC-UNIT-LDLAT-06: IBS_LDLAT_EN does not overlap IBS_LDLAT_THRSH */
ATF_TC_WITHOUT_HEAD(ibs_unit_ldlat_en_thrsh_no_overlap);
ATF_TC_BODY(ibs_unit_ldlat_en_thrsh_no_overlap, tc)
{
	ATF_CHECK_EQ((IBS_LDLAT_EN & IBS_LDLAT_THRSH), 0ULL);
}

/* TC-UNIT-LDLAT-07: LDLAT_THRSH does not overlap IBS_OP_MAXCNT_EXT */
ATF_TC_WITHOUT_HEAD(ibs_unit_ldlat_no_overlap_ext);
ATF_TC_BODY(ibs_unit_ldlat_no_overlap_ext, tc)
{
	ATF_CHECK_EQ((IBS_LDLAT_THRSH & IBS_OP_MAXCNT_EXT), 0ULL);
}

/* TC-UNIT-LDLAT-08: LDLAT_THRSH does not overlap IBS_OP_CURCNT */
ATF_TC_WITHOUT_HEAD(ibs_unit_ldlat_no_overlap_curcnt);
ATF_TC_BODY(ibs_unit_ldlat_no_overlap_curcnt, tc)
{
	ATF_CHECK_EQ((IBS_LDLAT_THRSH & IBS_OP_CURCNT), 0ULL);
}

/* TC-UNIT-LDLAT-09: LDLAT_EN does not overlap IBS_OP_CURCNT */
ATF_TC_WITHOUT_HEAD(ibs_unit_ldlat_en_no_overlap_curcnt);
ATF_TC_BODY(ibs_unit_ldlat_en_no_overlap_curcnt, tc)
{
	ATF_CHECK_EQ((IBS_LDLAT_EN & IBS_OP_CURCNT), 0ULL);
}

/* TC-UNIT-LDLAT-10: LDLAT_EN does not overlap IBS_OP_MAXCNT */
ATF_TC_WITHOUT_HEAD(ibs_unit_ldlat_en_no_overlap_maxcnt);
ATF_TC_BODY(ibs_unit_ldlat_en_no_overlap_maxcnt, tc)
{
	ATF_CHECK_EQ((IBS_LDLAT_EN & IBS_OP_MAXCNT), 0ULL);
}

/* TC-UNIT-LDLAT-11: threshold level 0 — zero bits in field */
ATF_TC_WITHOUT_HEAD(ibs_unit_ldlat_thrsh_level0);
ATF_TC_BODY(ibs_unit_ldlat_thrsh_level0, tc)
{
	/* Construct opctl with threshold=0, EN=1 */
	uint64_t opctl = IBS_LDLAT_EN | (0ULL << IBS_LDLAT_THRSH_SHIFT);
	uint64_t thrsh = (opctl & IBS_LDLAT_THRSH) >> IBS_LDLAT_THRSH_SHIFT;

	ATF_CHECK_EQ(thrsh, 0ULL);
}

/* TC-UNIT-LDLAT-12: threshold level 1 — minimum non-zero */
ATF_TC_WITHOUT_HEAD(ibs_unit_ldlat_thrsh_level1);
ATF_TC_BODY(ibs_unit_ldlat_thrsh_level1, tc)
{
	uint64_t opctl = IBS_LDLAT_EN | (1ULL << IBS_LDLAT_THRSH_SHIFT);
	uint64_t thrsh = (opctl & IBS_LDLAT_THRSH) >> IBS_LDLAT_THRSH_SHIFT;

	ATF_CHECK_EQ(thrsh, 1ULL);
}

/* TC-UNIT-LDLAT-13: threshold level 15 — maximum 4-bit value */
ATF_TC_WITHOUT_HEAD(ibs_unit_ldlat_thrsh_level15);
ATF_TC_BODY(ibs_unit_ldlat_thrsh_level15, tc)
{
	uint64_t opctl = IBS_LDLAT_EN | (0x0fULL << IBS_LDLAT_THRSH_SHIFT);
	uint64_t thrsh = (opctl & IBS_LDLAT_THRSH) >> IBS_LDLAT_THRSH_SHIFT;

	ATF_CHECK_EQ(thrsh, 0x0fULL);
}

/* TC-UNIT-LDLAT-14: setting threshold preserves MaxCnt */
ATF_TC_WITHOUT_HEAD(ibs_unit_ldlat_preserves_maxcnt);
ATF_TC_BODY(ibs_unit_ldlat_preserves_maxcnt, tc)
{
	uint64_t opctl = ibs_op_set_full_maxcnt(0ULL, 0x1234ULL);
	/* add LDLAT fields */
	opctl |= IBS_LDLAT_EN | (7ULL << IBS_LDLAT_THRSH_SHIFT);

	ATF_CHECK_EQ(ibs_op_get_full_maxcnt(opctl), 0x1234ULL);
	ATF_CHECK_MSG((opctl & IBS_LDLAT_EN) != 0ULL,
	    "LDLAT_EN should be set");
}

/* TC-UNIT-LDLAT-15: round-trip threshold across all 4-bit values */
ATF_TC_WITHOUT_HEAD(ibs_unit_ldlat_thrsh_roundtrip);
ATF_TC_BODY(ibs_unit_ldlat_thrsh_roundtrip, tc)
{
	uint32_t t;

	for (t = 0; t <= 0xfU; t++) {
		uint64_t opctl = (uint64_t)t << IBS_LDLAT_THRSH_SHIFT;
		uint64_t got = (opctl & IBS_LDLAT_THRSH) >> IBS_LDLAT_THRSH_SHIFT;

		ATF_CHECK_MSG(got == (uint64_t)t,
		    "threshold round-trip failed for t=%u: got %llu",
		    t, (unsigned long long)got);
	}
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, ibs_unit_ldlat_shift_is_59);
	ATF_TP_ADD_TC(tp, ibs_unit_ldlat_en_bit63);
	ATF_TP_ADD_TC(tp, ibs_unit_ldlat_thrsh_mask);
	ATF_TP_ADD_TC(tp, ibs_unit_ldlat_thrsh_width);
	ATF_TP_ADD_TC(tp, ibs_unit_ldlat_thrsh_shift_selfcheck);
	ATF_TP_ADD_TC(tp, ibs_unit_ldlat_en_thrsh_no_overlap);
	ATF_TP_ADD_TC(tp, ibs_unit_ldlat_no_overlap_ext);
	ATF_TP_ADD_TC(tp, ibs_unit_ldlat_no_overlap_curcnt);
	ATF_TP_ADD_TC(tp, ibs_unit_ldlat_en_no_overlap_curcnt);
	ATF_TP_ADD_TC(tp, ibs_unit_ldlat_en_no_overlap_maxcnt);
	ATF_TP_ADD_TC(tp, ibs_unit_ldlat_thrsh_level0);
	ATF_TP_ADD_TC(tp, ibs_unit_ldlat_thrsh_level1);
	ATF_TP_ADD_TC(tp, ibs_unit_ldlat_thrsh_level15);
	ATF_TP_ADD_TC(tp, ibs_unit_ldlat_preserves_maxcnt);
	ATF_TP_ADD_TC(tp, ibs_unit_ldlat_thrsh_roundtrip);
	return (atf_no_error());
}
