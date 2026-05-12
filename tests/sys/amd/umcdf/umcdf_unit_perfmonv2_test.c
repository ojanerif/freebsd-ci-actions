/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * Author: ojanerif@amd.com
 *
 * [TC-UNIT-PMV2] — CPUID Fn80000022 PerfMonV2 parsing macro unit tests.
 *
 * Tests AMD_UMCDF_EXTPERFMON_CORE_PMCS(), AMD_UMCDF_EXTPERFMON_LBR_V2_DEPTH(),
 * and AMD_UMCDF_EXTPERFMON_DF_PMCS() from amd_umcdf_decode.h.
 *
 * These macros parse the EBX register of CPUID leaf Fn80000022:
 *
 *   bits  3:0   CorePmcNum   → CORE_PMCS
 *   bits  9:4   LbrStackSize → LBR_V2_DEPTH
 *   bits 15:10  DfPmcNum     → DF_PMCS
 *
 * No hardware access.  Runs on any architecture.
 *
 * Test IDs: TC-UNIT-PMV2-01 … TC-UNIT-PMV2-15
 */

#include <atf-c.h>

#include "amd_umcdf_decode.h"

/* TC-UNIT-PMV2-01: AMD_UMCDF_EXTPERFMON_PRESENT is bit 0 of EAX */
ATF_TC_WITHOUT_HEAD(umcdf_unit_pmv2_present_bit);
ATF_TC_BODY(umcdf_unit_pmv2_present_bit, tc)
{
	ATF_CHECK_EQ(AMD_UMCDF_EXTPERFMON_PRESENT, 0x1U);
}

/* TC-UNIT-PMV2-02: CORE_PMCS extracts bits 3:0 */
ATF_TC_WITHOUT_HEAD(umcdf_unit_pmv2_core_pmcs_basic);
ATF_TC_BODY(umcdf_unit_pmv2_core_pmcs_basic, tc)
{
	ATF_CHECK_EQ(AMD_UMCDF_EXTPERFMON_CORE_PMCS(0x06U), 6U);
}

/* TC-UNIT-PMV2-03: CORE_PMCS with full 4-bit value */
ATF_TC_WITHOUT_HEAD(umcdf_unit_pmv2_core_pmcs_max);
ATF_TC_BODY(umcdf_unit_pmv2_core_pmcs_max, tc)
{
	ATF_CHECK_EQ(AMD_UMCDF_EXTPERFMON_CORE_PMCS(0x0FU), 0x0FU);
}

/* TC-UNIT-PMV2-04: CORE_PMCS does not bleed from bits above 3 */
ATF_TC_WITHOUT_HEAD(umcdf_unit_pmv2_core_pmcs_no_bleed);
ATF_TC_BODY(umcdf_unit_pmv2_core_pmcs_no_bleed, tc)
{
	/* All bits except 3:0 set */
	ATF_CHECK_EQ(AMD_UMCDF_EXTPERFMON_CORE_PMCS(0xFFFFFFF0U), 0U);
}

/* TC-UNIT-PMV2-05: LBR_V2_DEPTH extracts bits 9:4 */
ATF_TC_WITHOUT_HEAD(umcdf_unit_pmv2_lbr_depth_basic);
ATF_TC_BODY(umcdf_unit_pmv2_lbr_depth_basic, tc)
{
	/* EBX = 0x10: bit 4 set → depth = 1 */
	ATF_CHECK_EQ(AMD_UMCDF_EXTPERFMON_LBR_V2_DEPTH(0x10U), 1U);
}

/* TC-UNIT-PMV2-06: LBR_V2_DEPTH maximum 6-bit value */
ATF_TC_WITHOUT_HEAD(umcdf_unit_pmv2_lbr_depth_max);
ATF_TC_BODY(umcdf_unit_pmv2_lbr_depth_max, tc)
{
	/* Bits 9:4 all set = 0x3F0 → depth = 0x3F */
	ATF_CHECK_EQ(AMD_UMCDF_EXTPERFMON_LBR_V2_DEPTH(0x3F0U), 0x3FU);
}

/* TC-UNIT-PMV2-07: LBR_V2_DEPTH does not bleed from bits 3:0 */
ATF_TC_WITHOUT_HEAD(umcdf_unit_pmv2_lbr_depth_no_low_bleed);
ATF_TC_BODY(umcdf_unit_pmv2_lbr_depth_no_low_bleed, tc)
{
	ATF_CHECK_EQ(AMD_UMCDF_EXTPERFMON_LBR_V2_DEPTH(0x0FU), 0U);
}

/* TC-UNIT-PMV2-08: LBR_V2_DEPTH does not bleed from bits above 9 */
ATF_TC_WITHOUT_HEAD(umcdf_unit_pmv2_lbr_depth_no_high_bleed);
ATF_TC_BODY(umcdf_unit_pmv2_lbr_depth_no_high_bleed, tc)
{
	ATF_CHECK_EQ(AMD_UMCDF_EXTPERFMON_LBR_V2_DEPTH(0xFFFFFC00U), 0U);
}

/* TC-UNIT-PMV2-09: DF_PMCS extracts bits 15:10 */
ATF_TC_WITHOUT_HEAD(umcdf_unit_pmv2_df_pmcs_basic);
ATF_TC_BODY(umcdf_unit_pmv2_df_pmcs_basic, tc)
{
	/* EBX = 0x0400: bit 10 set → df_pmcs = 1 */
	ATF_CHECK_EQ(AMD_UMCDF_EXTPERFMON_DF_PMCS(0x0400U), 1U);
}

/* TC-UNIT-PMV2-10: DF_PMCS maximum 6-bit value */
ATF_TC_WITHOUT_HEAD(umcdf_unit_pmv2_df_pmcs_max);
ATF_TC_BODY(umcdf_unit_pmv2_df_pmcs_max, tc)
{
	/* Bits 15:10 all set = 0xFC00 → df_pmcs = 0x3F */
	ATF_CHECK_EQ(AMD_UMCDF_EXTPERFMON_DF_PMCS(0xFC00U), 0x3FU);
}

/* TC-UNIT-PMV2-11: DF_PMCS does not bleed from bits below 10 */
ATF_TC_WITHOUT_HEAD(umcdf_unit_pmv2_df_pmcs_no_low_bleed);
ATF_TC_BODY(umcdf_unit_pmv2_df_pmcs_no_low_bleed, tc)
{
	ATF_CHECK_EQ(AMD_UMCDF_EXTPERFMON_DF_PMCS(0x03FFU), 0U);
}

/* TC-UNIT-PMV2-12: AMD_UMCDF_DF_DEFAULT_PMCS is 4 */
ATF_TC_WITHOUT_HEAD(umcdf_unit_pmv2_df_default_4);
ATF_TC_BODY(umcdf_unit_pmv2_df_default_4, tc)
{
	ATF_CHECK_EQ(AMD_UMCDF_DF_DEFAULT_PMCS, 4U);
}

/* TC-UNIT-PMV2-13: AMD_UMCDF_MAX_DF_PMCS is 64 */
ATF_TC_WITHOUT_HEAD(umcdf_unit_pmv2_df_max_64);
ATF_TC_BODY(umcdf_unit_pmv2_df_max_64, tc)
{
	ATF_CHECK_EQ(AMD_UMCDF_MAX_DF_PMCS, 64U);
}

/* TC-UNIT-PMV2-14: AMD_UMCDF_MAX_DF_PMCS exceeds the 6-bit field maximum */
ATF_TC_WITHOUT_HEAD(umcdf_unit_pmv2_max_exceeds_6bit);
ATF_TC_BODY(umcdf_unit_pmv2_max_exceeds_6bit, tc)
{
	/* The hardware DF_PMCS field is 6 bits (max 63).  The software cap is
	 * 64 to allow treating the count as a one-indexed limit. */
	ATF_CHECK_MSG(AMD_UMCDF_MAX_DF_PMCS > 0x3FU,
	    "MAX_DF_PMCS (%u) should exceed the 6-bit hardware field max (63)",
	    AMD_UMCDF_MAX_DF_PMCS);
}

/* TC-UNIT-PMV2-15: three EBX fields are non-overlapping for a composite value */
ATF_TC_WITHOUT_HEAD(umcdf_unit_pmv2_fields_independent);
ATF_TC_BODY(umcdf_unit_pmv2_fields_independent, tc)
{
	/* Encode: core_pmcs=3, lbr_depth=5, df_pmcs=7 */
	uint32_t ebx = (3U) | (5U << 4) | (7U << 10);

	ATF_CHECK_EQ(AMD_UMCDF_EXTPERFMON_CORE_PMCS(ebx), 3U);
	ATF_CHECK_EQ(AMD_UMCDF_EXTPERFMON_LBR_V2_DEPTH(ebx), 5U);
	ATF_CHECK_EQ(AMD_UMCDF_EXTPERFMON_DF_PMCS(ebx), 7U);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, umcdf_unit_pmv2_present_bit);
	ATF_TP_ADD_TC(tp, umcdf_unit_pmv2_core_pmcs_basic);
	ATF_TP_ADD_TC(tp, umcdf_unit_pmv2_core_pmcs_max);
	ATF_TP_ADD_TC(tp, umcdf_unit_pmv2_core_pmcs_no_bleed);
	ATF_TP_ADD_TC(tp, umcdf_unit_pmv2_lbr_depth_basic);
	ATF_TP_ADD_TC(tp, umcdf_unit_pmv2_lbr_depth_max);
	ATF_TP_ADD_TC(tp, umcdf_unit_pmv2_lbr_depth_no_low_bleed);
	ATF_TP_ADD_TC(tp, umcdf_unit_pmv2_lbr_depth_no_high_bleed);
	ATF_TP_ADD_TC(tp, umcdf_unit_pmv2_df_pmcs_basic);
	ATF_TP_ADD_TC(tp, umcdf_unit_pmv2_df_pmcs_max);
	ATF_TP_ADD_TC(tp, umcdf_unit_pmv2_df_pmcs_no_low_bleed);
	ATF_TP_ADD_TC(tp, umcdf_unit_pmv2_df_default_4);
	ATF_TP_ADD_TC(tp, umcdf_unit_pmv2_df_max_64);
	ATF_TP_ADD_TC(tp, umcdf_unit_pmv2_max_exceeds_6bit);
	ATF_TP_ADD_TC(tp, umcdf_unit_pmv2_fields_independent);
	return (atf_no_error());
}
