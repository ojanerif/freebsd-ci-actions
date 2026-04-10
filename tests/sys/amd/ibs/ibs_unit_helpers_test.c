/*-
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Author: ojanerif@amd.com
 *
 * [TC-UNIT-HELP] — Pure helper function unit tests.
 *
 * Tests ibs_get_maxcnt(), ibs_set_maxcnt(), and ibs_maxcnt_to_period()
 * from ibs_decode.h.  No hardware access.  Runs on any architecture.
 *
 * Test IDs: TC-UNIT-HELP-01 … TC-UNIT-HELP-14
 */

#include <atf-c.h>

#include "ibs_decode.h"

/* TC-UNIT-HELP-01: ibs_get_maxcnt with zero input */
ATF_TC_WITHOUT_HEAD(ibs_unit_get_maxcnt_zero);
ATF_TC_BODY(ibs_unit_get_maxcnt_zero, tc)
{
	ATF_CHECK_EQ(ibs_get_maxcnt(0ULL), 0ULL);
}

/* TC-UNIT-HELP-02: ibs_get_maxcnt with max 16-bit value in low bits */
ATF_TC_WITHOUT_HEAD(ibs_unit_get_maxcnt_max);
ATF_TC_BODY(ibs_unit_get_maxcnt_max, tc)
{
	ATF_CHECK_EQ(ibs_get_maxcnt(0xFFFFULL), 0xFFFFULL);
}

/* TC-UNIT-HELP-03: upper bits must not bleed into the maxcnt result */
ATF_TC_WITHOUT_HEAD(ibs_unit_get_maxcnt_no_bleed);
ATF_TC_BODY(ibs_unit_get_maxcnt_no_bleed, tc)
{
	/* All upper bits set, lower 16 bits zero */
	ATF_CHECK_EQ(ibs_get_maxcnt(0xFFFFFFFFFFFF0000ULL), 0ULL);
}

/* TC-UNIT-HELP-04: only bits 15:0 are extracted */
ATF_TC_WITHOUT_HEAD(ibs_unit_get_maxcnt_mid);
ATF_TC_BODY(ibs_unit_get_maxcnt_mid, tc)
{
	ATF_CHECK_EQ(ibs_get_maxcnt(0x00DEADBEEF001234ULL), 0x1234ULL);
}

/* TC-UNIT-HELP-05: ibs_set_maxcnt basic — set on zero base */
ATF_TC_WITHOUT_HEAD(ibs_unit_set_maxcnt_basic);
ATF_TC_BODY(ibs_unit_set_maxcnt_basic, tc)
{
	ATF_CHECK_EQ(ibs_set_maxcnt(0ULL, 0x1000ULL), 0x1000ULL);
}

/* TC-UNIT-HELP-06: ibs_set_maxcnt preserves upper bits */
ATF_TC_WITHOUT_HEAD(ibs_unit_set_maxcnt_preserves_upper);
ATF_TC_BODY(ibs_unit_set_maxcnt_preserves_upper, tc)
{
	uint64_t base = 0xFFFFFFFFFFFF0000ULL;
	uint64_t result = ibs_set_maxcnt(base, 0x1234ULL);

	ATF_CHECK_EQ(result, 0xFFFFFFFFFFFF1234ULL);
}

/* TC-UNIT-HELP-07: ibs_set_maxcnt clears existing maxcnt when new value is 0 */
ATF_TC_WITHOUT_HEAD(ibs_unit_set_maxcnt_clears);
ATF_TC_BODY(ibs_unit_set_maxcnt_clears, tc)
{
	ATF_CHECK_EQ(ibs_set_maxcnt(0xFFFFULL, 0ULL), 0ULL);
}

/* TC-UNIT-HELP-08: ibs_set_maxcnt clamps to 16 bits */
ATF_TC_WITHOUT_HEAD(ibs_unit_set_maxcnt_clamps);
ATF_TC_BODY(ibs_unit_set_maxcnt_clamps, tc)
{
	/* 0x1FFFF has bit 16 set — that must be masked off */
	ATF_CHECK_EQ(ibs_set_maxcnt(0ULL, 0x1FFFFULL), 0xFFFFULL);
}

/* TC-UNIT-HELP-09: round-trip get(set(0, n)) == n for all 16-bit values */
ATF_TC_WITHOUT_HEAD(ibs_unit_roundtrip_all);
ATF_TC_BODY(ibs_unit_roundtrip_all, tc)
{
	uint32_t n;

	for (n = 0; n <= 0xFFFFU; n++) {
		uint64_t written = ibs_set_maxcnt(0ULL, (uint64_t)n);
		uint64_t read    = ibs_get_maxcnt(written);

		ATF_CHECK_MSG(read == (uint64_t)n,
		    "round-trip failed for n=0x%04x: got 0x%04llx",
		    n, (unsigned long long)read);
	}
}

/* TC-UNIT-HELP-10: ibs_maxcnt_to_period(1) == 16 */
ATF_TC_WITHOUT_HEAD(ibs_unit_period_min);
ATF_TC_BODY(ibs_unit_period_min, tc)
{
	ATF_CHECK_EQ(ibs_maxcnt_to_period(1ULL), 16ULL);
}

/* TC-UNIT-HELP-11: ibs_maxcnt_to_period(0xFFFF) == 0xFFFF0 */
ATF_TC_WITHOUT_HEAD(ibs_unit_period_max);
ATF_TC_BODY(ibs_unit_period_max, tc)
{
	ATF_CHECK_EQ(ibs_maxcnt_to_period(0xFFFFULL), 0xFFFF0ULL);
}

/* TC-UNIT-HELP-12: ibs_maxcnt_to_period(0) == 0 (disabled) */
ATF_TC_WITHOUT_HEAD(ibs_unit_period_zero);
ATF_TC_BODY(ibs_unit_period_zero, tc)
{
	ATF_CHECK_EQ(ibs_maxcnt_to_period(0ULL), 0ULL);
}

/* TC-UNIT-HELP-13: ibs_maxcnt_to_period(0x100) == 0x1000 */
ATF_TC_WITHOUT_HEAD(ibs_unit_period_mid);
ATF_TC_BODY(ibs_unit_period_mid, tc)
{
	ATF_CHECK_EQ(ibs_maxcnt_to_period(0x100ULL), 0x1000ULL);
}

/* TC-UNIT-HELP-14: ibs_maxcnt_to_period(n) == n << 4 for representative n */
ATF_TC_WITHOUT_HEAD(ibs_unit_period_shift);
ATF_TC_BODY(ibs_unit_period_shift, tc)
{
	const uint64_t values[] = { 1, 2, 4, 0x10, 0xFF, 0x1000, 0x8000, 0xFFFF };
	int i;

	for (i = 0; i < (int)(sizeof(values) / sizeof(values[0])); i++) {
		ATF_CHECK_MSG(ibs_maxcnt_to_period(values[i]) == (values[i] << 4),
		    "period mismatch for maxcnt=0x%llx",
		    (unsigned long long)values[i]);
	}
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, ibs_unit_get_maxcnt_zero);
	ATF_TP_ADD_TC(tp, ibs_unit_get_maxcnt_max);
	ATF_TP_ADD_TC(tp, ibs_unit_get_maxcnt_no_bleed);
	ATF_TP_ADD_TC(tp, ibs_unit_get_maxcnt_mid);
	ATF_TP_ADD_TC(tp, ibs_unit_set_maxcnt_basic);
	ATF_TP_ADD_TC(tp, ibs_unit_set_maxcnt_preserves_upper);
	ATF_TP_ADD_TC(tp, ibs_unit_set_maxcnt_clears);
	ATF_TP_ADD_TC(tp, ibs_unit_set_maxcnt_clamps);
	ATF_TP_ADD_TC(tp, ibs_unit_roundtrip_all);
	ATF_TP_ADD_TC(tp, ibs_unit_period_min);
	ATF_TP_ADD_TC(tp, ibs_unit_period_max);
	ATF_TP_ADD_TC(tp, ibs_unit_period_zero);
	ATF_TP_ADD_TC(tp, ibs_unit_period_mid);
	ATF_TP_ADD_TC(tp, ibs_unit_period_shift);
	return (atf_no_error());
}
