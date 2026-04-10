/*-
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Author: ojanerif@amd.com
 *
 * [TC-UNIT-FEAT] — IBS CPUID feature flag bit position and accessor tests.
 *
 * Verifies the IBS_CPUID_* bit positions in CPUID 0x8000001B EAX and tests
 * the ibs_feat_*() boolean accessor functions from ibs_decode.h.
 *
 * No hardware access.  Runs on any architecture.
 *
 * Test IDs: TC-UNIT-FEAT-01 … TC-UNIT-FEAT-10
 */

#include <atf-c.h>

#include "ibs_decode.h"

/* TC-UNIT-FEAT-01: IbsFetchSam = bit 0 */
ATF_TC_WITHOUT_HEAD(ibs_unit_feat_fetch_sam_bit);
ATF_TC_BODY(ibs_unit_feat_fetch_sam_bit, tc)
{
	ATF_CHECK_EQ(IBS_CPUID_FETCH_SAMPLING, (1U << 0));
}

/* TC-UNIT-FEAT-02: IbsOpSam = bit 1 */
ATF_TC_WITHOUT_HEAD(ibs_unit_feat_op_sam_bit);
ATF_TC_BODY(ibs_unit_feat_op_sam_bit, tc)
{
	ATF_CHECK_EQ(IBS_CPUID_OP_SAMPLING, (1U << 1));
}

/* TC-UNIT-FEAT-03: RdWrOpCnt = bit 2 */
ATF_TC_WITHOUT_HEAD(ibs_unit_feat_rdwropcnt_bit);
ATF_TC_BODY(ibs_unit_feat_rdwropcnt_bit, tc)
{
	ATF_CHECK_EQ(IBS_CPUID_RDWROPCNT, (1U << 2));
}

/* TC-UNIT-FEAT-04: OpCnt = bit 3 */
ATF_TC_WITHOUT_HEAD(ibs_unit_feat_opcnt_bit);
ATF_TC_BODY(ibs_unit_feat_opcnt_bit, tc)
{
	ATF_CHECK_EQ(IBS_CPUID_OPCNT, (1U << 3));
}

/* TC-UNIT-FEAT-05: BrnTrgt = bit 4 */
ATF_TC_WITHOUT_HEAD(ibs_unit_feat_brntarget_bit);
ATF_TC_BODY(ibs_unit_feat_brntarget_bit, tc)
{
	ATF_CHECK_EQ(IBS_CPUID_BRANCH_TARGET_ADDR, (1U << 4));
}

/* TC-UNIT-FEAT-06: IbsOpData4 = bit 5 */
ATF_TC_WITHOUT_HEAD(ibs_unit_feat_opdata4_bit);
ATF_TC_BODY(ibs_unit_feat_opdata4_bit, tc)
{
	ATF_CHECK_EQ(IBS_CPUID_OP_DATA_4, (1U << 5));
}

/* TC-UNIT-FEAT-07: Zen4IbsExt = bit 6 */
ATF_TC_WITHOUT_HEAD(ibs_unit_feat_zen4_bit);
ATF_TC_BODY(ibs_unit_feat_zen4_bit, tc)
{
	ATF_CHECK_EQ(IBS_CPUID_ZEN4_IBS, (1U << 6));
}

/* TC-UNIT-FEAT-08: ibs_feat_zen4() returns true when bit 6 is set */
ATF_TC_WITHOUT_HEAD(ibs_unit_feat_parse_zen4);
ATF_TC_BODY(ibs_unit_feat_parse_zen4, tc)
{
	/* 0x7F = bits 0-6 all set — all features present including Zen4 */
	ATF_CHECK(ibs_feat_zen4(0x7FU) == true);
	/* 0x3F = bits 0-5 set, bit 6 clear — no Zen4 */
	ATF_CHECK(ibs_feat_zen4(0x3FU) == false);
}

/* TC-UNIT-FEAT-09: ibs_feat_fetch_sampling() returns false when EAX is 0 */
ATF_TC_WITHOUT_HEAD(ibs_unit_feat_parse_none);
ATF_TC_BODY(ibs_unit_feat_parse_none, tc)
{
	ATF_CHECK(ibs_feat_fetch_sampling(0U) == false);
	ATF_CHECK(ibs_feat_op_sampling(0U) == false);
	ATF_CHECK(ibs_feat_zen4(0U) == false);
}

/* TC-UNIT-FEAT-10: all six feature bits are independent (no aliases) */
ATF_TC_WITHOUT_HEAD(ibs_unit_feat_independent_bits);
ATF_TC_BODY(ibs_unit_feat_independent_bits, tc)
{
	const uint32_t bits[] = {
		IBS_CPUID_FETCH_SAMPLING,
		IBS_CPUID_OP_SAMPLING,
		IBS_CPUID_RDWROPCNT,
		IBS_CPUID_OPCNT,
		IBS_CPUID_BRANCH_TARGET_ADDR,
		IBS_CPUID_OP_DATA_4,
		IBS_CPUID_ZEN4_IBS,
	};
	const int n = (int)(sizeof(bits) / sizeof(bits[0]));
	int i, j;

	for (i = 0; i < n; i++) {
		for (j = i + 1; j < n; j++) {
			ATF_CHECK_MSG((bits[i] & bits[j]) == 0U,
			    "feature bits[%d]=0x%02x and bits[%d]=0x%02x "
			    "are not independent",
			    i, bits[i], j, bits[j]);
		}
	}
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, ibs_unit_feat_fetch_sam_bit);
	ATF_TP_ADD_TC(tp, ibs_unit_feat_op_sam_bit);
	ATF_TP_ADD_TC(tp, ibs_unit_feat_rdwropcnt_bit);
	ATF_TP_ADD_TC(tp, ibs_unit_feat_opcnt_bit);
	ATF_TP_ADD_TC(tp, ibs_unit_feat_brntarget_bit);
	ATF_TP_ADD_TC(tp, ibs_unit_feat_opdata4_bit);
	ATF_TP_ADD_TC(tp, ibs_unit_feat_zen4_bit);
	ATF_TP_ADD_TC(tp, ibs_unit_feat_parse_zen4);
	ATF_TP_ADD_TC(tp, ibs_unit_feat_parse_none);
	ATF_TP_ADD_TC(tp, ibs_unit_feat_independent_bits);
	return (atf_no_error());
}
