/*-
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Author: ojanerif@amd.com
 *
 * [TC-UNIT-DSRC] — DataSrc field extraction unit tests.
 *
 * Tests ibs_get_data_src() from ibs_decode.h.  The function extracts the
 * 5-bit combined DataSrc field from MSR_IBS_OP_DATA2:
 *
 *   DataSrcLo = bits[2:0]  (IBS_DATA_SRC_LO)
 *   DataSrcHi = bits[7:6]  (IBS_DATA_SRC_HI)
 *   result    = (DataSrcHi << 3) | DataSrcLo
 *
 * Key regression: DataSrcHi sits at bits[7:6] of the register, so the
 * extraction must shift right by 6 (not 3) before shifting left by 3 to
 * place it into the high bits of the result.  An earlier bug used shift-3.
 *
 * Test IDs: TC-UNIT-DSRC-01 … TC-UNIT-DSRC-10
 */

#include <atf-c.h>

#include "ibs_decode.h"

/* TC-UNIT-DSRC-01: local L1 cache hit — all DataSrc bits zero */
ATF_TC_WITHOUT_HEAD(ibs_unit_datasrc_l1);
ATF_TC_BODY(ibs_unit_datasrc_l1, tc)
{
	ATF_CHECK_EQ(ibs_get_data_src(0x00ULL), 0U);
}

/* TC-UNIT-DSRC-02: local L2 cache hit — DataSrcLo = 1 */
ATF_TC_WITHOUT_HEAD(ibs_unit_datasrc_l2);
ATF_TC_BODY(ibs_unit_datasrc_l2, tc)
{
	ATF_CHECK_EQ(ibs_get_data_src(0x01ULL), 1U);
}

/* TC-UNIT-DSRC-03: local DRAM — DataSrcLo = 3 */
ATF_TC_WITHOUT_HEAD(ibs_unit_datasrc_dram);
ATF_TC_BODY(ibs_unit_datasrc_dram, tc)
{
	ATF_CHECK_EQ(ibs_get_data_src(0x03ULL), 3U);
}

/* TC-UNIT-DSRC-04: remote DRAM — DataSrcLo = 4 */
ATF_TC_WITHOUT_HEAD(ibs_unit_datasrc_remote_dram);
ATF_TC_BODY(ibs_unit_datasrc_remote_dram, tc)
{
	ATF_CHECK_EQ(ibs_get_data_src(0x04ULL), 4U);
}

/* TC-UNIT-DSRC-05: high bits only — DataSrcHi[0]=1 → result 0x08 */
ATF_TC_WITHOUT_HEAD(ibs_unit_datasrc_hi_shift);
ATF_TC_BODY(ibs_unit_datasrc_hi_shift, tc)
{
	/* bit 6 set (= DataSrcHi[0] = 1, DataSrcLo = 0)
	 * expected: (1 << 3) | 0 = 8 = 0x08 */
	ATF_CHECK_EQ(ibs_get_data_src(0x40ULL), 0x08U);
}

/* TC-UNIT-DSRC-06: extended encoding 0x08 (non-DRAM extended memory) */
ATF_TC_WITHOUT_HEAD(ibs_unit_datasrc_ext_8);
ATF_TC_BODY(ibs_unit_datasrc_ext_8, tc)
{
	/* Same as datasrc_hi_shift; also verifies the canonical 0x08 path */
	ATF_CHECK_EQ(ibs_get_data_src(0x40ULL), 0x08U);
}

/* TC-UNIT-DSRC-07: extended encoding 0x0C (peer agent / cross-die) */
ATF_TC_WITHOUT_HEAD(ibs_unit_datasrc_ext_c);
ATF_TC_BODY(ibs_unit_datasrc_ext_c, tc)
{
	/* bits[7:6]=1, bits[2:0]=4  =>  0x40 | 0x04 = 0x44
	 * expected: (1 << 3) | 4 = 12 = 0x0C */
	ATF_CHECK_EQ(ibs_get_data_src(0x44ULL), 0x0CU);
}

/* TC-UNIT-DSRC-08: extended encoding 0x0D (long-latency DRAM) */
ATF_TC_WITHOUT_HEAD(ibs_unit_datasrc_ext_d);
ATF_TC_BODY(ibs_unit_datasrc_ext_d, tc)
{
	/* bits[7:6]=1, bits[2:0]=5  =>  0x45
	 * expected: (1 << 3) | 5 = 13 = 0x0D */
	ATF_CHECK_EQ(ibs_get_data_src(0x45ULL), 0x0DU);
}

/* TC-UNIT-DSRC-09: bits outside [7:6] and [2:0] must not affect the result */
ATF_TC_WITHOUT_HEAD(ibs_unit_datasrc_no_bleed);
ATF_TC_BODY(ibs_unit_datasrc_no_bleed, tc)
{
	/* 0xFFFFFFFFFFFFFF18: bits 3 and 4 set, all upper bytes FF —
	 * none of those land in DataSrcHi[7:6] or DataSrcLo[2:0] */
	uint32_t r1 = ibs_get_data_src(0xFFFFFFFFFFFFFF18ULL);
	uint32_t r2 = ibs_get_data_src(0x18ULL);

	ATF_CHECK_EQ(r1, r2);
	ATF_CHECK_EQ(r1, 0U);
}

/* TC-UNIT-DSRC-10: regression — shift must be 6, not 3
 * The old broken implementation was:
 *   hi = (op_data2 & IBS_DATA_SRC_HI) >> 3;  (wrong)
 * vs the correct:
 *   hi = (op_data2 & IBS_DATA_SRC_HI) >> 6;  (correct)
 *
 * For input 0x40 (bit 6 set):
 *   correct:  (0x40 >> 6) << 3 = 1 << 3 = 8 = 0x08
 *   wrong:    (0x40 >> 3) << 3 = 8 << 3 = 64 = 0x40  */
ATF_TC_WITHOUT_HEAD(ibs_unit_datasrc_regression_shift6);
ATF_TC_BODY(ibs_unit_datasrc_regression_shift6, tc)
{
	uint32_t result = ibs_get_data_src(0x40ULL);

	ATF_CHECK_MSG(result == 0x08U,
	    "expected 0x08 (shift-6), got 0x%02x — "
	    "possible shift-3 regression", result);
	/* Explicitly reject the result the old broken code would produce */
	ATF_CHECK_MSG(result != 0x40U,
	    "got 0x40 — shift-3 regression confirmed");
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, ibs_unit_datasrc_l1);
	ATF_TP_ADD_TC(tp, ibs_unit_datasrc_l2);
	ATF_TP_ADD_TC(tp, ibs_unit_datasrc_dram);
	ATF_TP_ADD_TC(tp, ibs_unit_datasrc_remote_dram);
	ATF_TP_ADD_TC(tp, ibs_unit_datasrc_hi_shift);
	ATF_TP_ADD_TC(tp, ibs_unit_datasrc_ext_8);
	ATF_TP_ADD_TC(tp, ibs_unit_datasrc_ext_c);
	ATF_TP_ADD_TC(tp, ibs_unit_datasrc_ext_d);
	ATF_TP_ADD_TC(tp, ibs_unit_datasrc_no_bleed);
	ATF_TP_ADD_TC(tp, ibs_unit_datasrc_regression_shift6);
	return (atf_no_error());
}
