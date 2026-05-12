/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * Author: ojanerif@amd.com
 *
 * [TC-UNIT-UZEN] — amd_umcdf_map_zen() unit tests.
 *
 * Tests the pure amd_umcdf_map_zen(family, model) function from
 * amd_umcdf_decode.h against the validated AMD family/model table.
 *
 * No hardware access.  Runs on any architecture.
 *
 * Test IDs: TC-UNIT-UZEN-01 … TC-UNIT-UZEN-22
 */

#include <atf-c.h>

#include "amd_umcdf_decode.h"

/* TC-UNIT-UZEN-01: pre-Zen — family below 0x17 */
ATF_TC_WITHOUT_HEAD(umcdf_unit_zen_pre_zen_family);
ATF_TC_BODY(umcdf_unit_zen_pre_zen_family, tc)
{
	ATF_CHECK_EQ(amd_umcdf_map_zen(0x16, 0x00), AMD_UMCDF_ZEN_PRE_ZEN);
	ATF_CHECK_EQ(amd_umcdf_map_zen(0x00, 0x00), AMD_UMCDF_ZEN_PRE_ZEN);
}

/* TC-UNIT-UZEN-02: future — family above 0x1a */
ATF_TC_WITHOUT_HEAD(umcdf_unit_zen_future_family);
ATF_TC_BODY(umcdf_unit_zen_future_family, tc)
{
	ATF_CHECK_EQ(amd_umcdf_map_zen(0x1b, 0x00), AMD_UMCDF_ZEN_FUTURE);
	ATF_CHECK_EQ(amd_umcdf_map_zen(0xff, 0x00), AMD_UMCDF_ZEN_FUTURE);
}

/* TC-UNIT-UZEN-03: family 0x17 model 0x01 → Zen 1 */
ATF_TC_WITHOUT_HEAD(umcdf_unit_zen_17_01_zen1);
ATF_TC_BODY(umcdf_unit_zen_17_01_zen1, tc)
{
	ATF_CHECK_EQ(amd_umcdf_map_zen(0x17, 0x01), AMD_UMCDF_ZEN_1);
}

/* TC-UNIT-UZEN-04: family 0x17 model 0x07 → Zen 1 (upper edge of range) */
ATF_TC_WITHOUT_HEAD(umcdf_unit_zen_17_07_zen1);
ATF_TC_BODY(umcdf_unit_zen_17_07_zen1, tc)
{
	ATF_CHECK_EQ(amd_umcdf_map_zen(0x17, 0x07), AMD_UMCDF_ZEN_1);
}

/* TC-UNIT-UZEN-05: family 0x17 model 0x11 → Zen 1 (second Zen 1 sub-range) */
ATF_TC_WITHOUT_HEAD(umcdf_unit_zen_17_11_zen1);
ATF_TC_BODY(umcdf_unit_zen_17_11_zen1, tc)
{
	ATF_CHECK_EQ(amd_umcdf_map_zen(0x17, 0x11), AMD_UMCDF_ZEN_1);
}

/* TC-UNIT-UZEN-06: family 0x17 model 0x08 → Zen+ */
ATF_TC_WITHOUT_HEAD(umcdf_unit_zen_17_08_zenplus);
ATF_TC_BODY(umcdf_unit_zen_17_08_zenplus, tc)
{
	ATF_CHECK_EQ(amd_umcdf_map_zen(0x17, 0x08), AMD_UMCDF_ZEN_PLUS);
}

/* TC-UNIT-UZEN-07: family 0x17 model 0x18 → Zen+ (second Zen+ sub-range) */
ATF_TC_WITHOUT_HEAD(umcdf_unit_zen_17_18_zenplus);
ATF_TC_BODY(umcdf_unit_zen_17_18_zenplus, tc)
{
	ATF_CHECK_EQ(amd_umcdf_map_zen(0x17, 0x18), AMD_UMCDF_ZEN_PLUS);
}

/* TC-UNIT-UZEN-08: family 0x17 model 0x31 → Zen 2 */
ATF_TC_WITHOUT_HEAD(umcdf_unit_zen_17_31_zen2);
ATF_TC_BODY(umcdf_unit_zen_17_31_zen2, tc)
{
	ATF_CHECK_EQ(amd_umcdf_map_zen(0x17, 0x31), AMD_UMCDF_ZEN_2);
}

/* TC-UNIT-UZEN-09: family 0x17 model 0x71 → Zen 2 */
ATF_TC_WITHOUT_HEAD(umcdf_unit_zen_17_71_zen2);
ATF_TC_BODY(umcdf_unit_zen_17_71_zen2, tc)
{
	ATF_CHECK_EQ(amd_umcdf_map_zen(0x17, 0x71), AMD_UMCDF_ZEN_2);
}

/* TC-UNIT-UZEN-10: family 0x17 model 0x00 → unknown (not in any range) */
ATF_TC_WITHOUT_HEAD(umcdf_unit_zen_17_00_unknown);
ATF_TC_BODY(umcdf_unit_zen_17_00_unknown, tc)
{
	ATF_CHECK_EQ(amd_umcdf_map_zen(0x17, 0x00), AMD_UMCDF_ZEN_UNKNOWN);
}

/* TC-UNIT-UZEN-11: family 0x19 model 0x00 → Zen 3 */
ATF_TC_WITHOUT_HEAD(umcdf_unit_zen_19_00_zen3);
ATF_TC_BODY(umcdf_unit_zen_19_00_zen3, tc)
{
	ATF_CHECK_EQ(amd_umcdf_map_zen(0x19, 0x00), AMD_UMCDF_ZEN_3);
}

/* TC-UNIT-UZEN-12: family 0x19 model 0x21 → Zen 3 */
ATF_TC_WITHOUT_HEAD(umcdf_unit_zen_19_21_zen3);
ATF_TC_BODY(umcdf_unit_zen_19_21_zen3, tc)
{
	ATF_CHECK_EQ(amd_umcdf_map_zen(0x19, 0x21), AMD_UMCDF_ZEN_3);
}

/* TC-UNIT-UZEN-13: family 0x19 model 0x40 → Zen 3+ */
ATF_TC_WITHOUT_HEAD(umcdf_unit_zen_19_40_zen3plus);
ATF_TC_BODY(umcdf_unit_zen_19_40_zen3plus, tc)
{
	ATF_CHECK_EQ(amd_umcdf_map_zen(0x19, 0x40), AMD_UMCDF_ZEN_3_PLUS);
}

/* TC-UNIT-UZEN-14: family 0x19 model 0x10 → Zen 4 */
ATF_TC_WITHOUT_HEAD(umcdf_unit_zen_19_10_zen4);
ATF_TC_BODY(umcdf_unit_zen_19_10_zen4, tc)
{
	ATF_CHECK_EQ(amd_umcdf_map_zen(0x19, 0x10), AMD_UMCDF_ZEN_4);
}

/* TC-UNIT-UZEN-15: family 0x19 model 0x61 → Zen 4 */
ATF_TC_WITHOUT_HEAD(umcdf_unit_zen_19_61_zen4);
ATF_TC_BODY(umcdf_unit_zen_19_61_zen4, tc)
{
	ATF_CHECK_EQ(amd_umcdf_map_zen(0x19, 0x61), AMD_UMCDF_ZEN_4);
}

/* TC-UNIT-UZEN-16: family 0x19 model 0xa0 → Zen 4 */
ATF_TC_WITHOUT_HEAD(umcdf_unit_zen_19_a0_zen4);
ATF_TC_BODY(umcdf_unit_zen_19_a0_zen4, tc)
{
	ATF_CHECK_EQ(amd_umcdf_map_zen(0x19, 0xa0), AMD_UMCDF_ZEN_4);
}

/* TC-UNIT-UZEN-17: family 0x1a model 0x00 → Zen 5 */
ATF_TC_WITHOUT_HEAD(umcdf_unit_zen_1a_00_zen5);
ATF_TC_BODY(umcdf_unit_zen_1a_00_zen5, tc)
{
	ATF_CHECK_EQ(amd_umcdf_map_zen(0x1a, 0x00), AMD_UMCDF_ZEN_5);
}

/* TC-UNIT-UZEN-18: family 0x1a model 0x44 → Zen 5 */
ATF_TC_WITHOUT_HEAD(umcdf_unit_zen_1a_44_zen5);
ATF_TC_BODY(umcdf_unit_zen_1a_44_zen5, tc)
{
	ATF_CHECK_EQ(amd_umcdf_map_zen(0x1a, 0x44), AMD_UMCDF_ZEN_5);
}

/* TC-UNIT-UZEN-19: family 0x1a model 0x50 → Zen 6 */
ATF_TC_WITHOUT_HEAD(umcdf_unit_zen_1a_50_zen6);
ATF_TC_BODY(umcdf_unit_zen_1a_50_zen6, tc)
{
	ATF_CHECK_EQ(amd_umcdf_map_zen(0x1a, 0x50), AMD_UMCDF_ZEN_6);
}

/* TC-UNIT-UZEN-20: family 0x1a model 0x80 → Zen 6 */
ATF_TC_WITHOUT_HEAD(umcdf_unit_zen_1a_80_zen6);
ATF_TC_BODY(umcdf_unit_zen_1a_80_zen6, tc)
{
	ATF_CHECK_EQ(amd_umcdf_map_zen(0x1a, 0x80), AMD_UMCDF_ZEN_6);
}

/* TC-UNIT-UZEN-21: family 0x1a model 0xc0 → Zen 6 */
ATF_TC_WITHOUT_HEAD(umcdf_unit_zen_1a_c0_zen6);
ATF_TC_BODY(umcdf_unit_zen_1a_c0_zen6, tc)
{
	ATF_CHECK_EQ(amd_umcdf_map_zen(0x1a, 0xc0), AMD_UMCDF_ZEN_6);
}

/* TC-UNIT-UZEN-22: enum order guarantees zen >= ZEN_4 selects DF2 encoding */
ATF_TC_WITHOUT_HEAD(umcdf_unit_zen_ordering);
ATF_TC_BODY(umcdf_unit_zen_ordering, tc)
{
	/* Generations below Zen 4 must have enum values < AMD_UMCDF_ZEN_4 */
	ATF_CHECK_MSG(AMD_UMCDF_ZEN_PRE_ZEN < AMD_UMCDF_ZEN_4,
	    "PRE_ZEN should be < ZEN_4");
	ATF_CHECK_MSG(AMD_UMCDF_ZEN_1 < AMD_UMCDF_ZEN_4,
	    "ZEN_1 should be < ZEN_4");
	ATF_CHECK_MSG(AMD_UMCDF_ZEN_3_PLUS < AMD_UMCDF_ZEN_4,
	    "ZEN_3_PLUS should be < ZEN_4");
	/* Generations from Zen 4 onward must have enum values >= AMD_UMCDF_ZEN_4 */
	ATF_CHECK_MSG(AMD_UMCDF_ZEN_4 >= AMD_UMCDF_ZEN_4,
	    "ZEN_4 should be >= ZEN_4");
	ATF_CHECK_MSG(AMD_UMCDF_ZEN_5 >= AMD_UMCDF_ZEN_4,
	    "ZEN_5 should be >= ZEN_4");
	ATF_CHECK_MSG(AMD_UMCDF_ZEN_6 >= AMD_UMCDF_ZEN_4,
	    "ZEN_6 should be >= ZEN_4");
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, umcdf_unit_zen_pre_zen_family);
	ATF_TP_ADD_TC(tp, umcdf_unit_zen_future_family);
	ATF_TP_ADD_TC(tp, umcdf_unit_zen_17_01_zen1);
	ATF_TP_ADD_TC(tp, umcdf_unit_zen_17_07_zen1);
	ATF_TP_ADD_TC(tp, umcdf_unit_zen_17_11_zen1);
	ATF_TP_ADD_TC(tp, umcdf_unit_zen_17_08_zenplus);
	ATF_TP_ADD_TC(tp, umcdf_unit_zen_17_18_zenplus);
	ATF_TP_ADD_TC(tp, umcdf_unit_zen_17_31_zen2);
	ATF_TP_ADD_TC(tp, umcdf_unit_zen_17_71_zen2);
	ATF_TP_ADD_TC(tp, umcdf_unit_zen_17_00_unknown);
	ATF_TP_ADD_TC(tp, umcdf_unit_zen_19_00_zen3);
	ATF_TP_ADD_TC(tp, umcdf_unit_zen_19_21_zen3);
	ATF_TP_ADD_TC(tp, umcdf_unit_zen_19_40_zen3plus);
	ATF_TP_ADD_TC(tp, umcdf_unit_zen_19_10_zen4);
	ATF_TP_ADD_TC(tp, umcdf_unit_zen_19_61_zen4);
	ATF_TP_ADD_TC(tp, umcdf_unit_zen_19_a0_zen4);
	ATF_TP_ADD_TC(tp, umcdf_unit_zen_1a_00_zen5);
	ATF_TP_ADD_TC(tp, umcdf_unit_zen_1a_44_zen5);
	ATF_TP_ADD_TC(tp, umcdf_unit_zen_1a_50_zen6);
	ATF_TP_ADD_TC(tp, umcdf_unit_zen_1a_80_zen6);
	ATF_TP_ADD_TC(tp, umcdf_unit_zen_1a_c0_zen6);
	ATF_TP_ADD_TC(tp, umcdf_unit_zen_ordering);
	return (atf_no_error());
}
