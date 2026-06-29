/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * Author: ojanerif@amd.com
 *
 * [TC-UNIT-UCAP] — AMD UMC/DF capability predicate unit tests.
 *
 * Tests amd_umcdf_has_df_feature(), amd_umcdf_has_freebsd_umc_json(),
 * amd_umcdf_has_freebsd_df_json(), and amd_umcdf_expected_df_config()
 * from amd_umcdf_decode.h.
 *
 * All functions operate on pre-populated structs — no hardware access.
 *
 * Test IDs: TC-UNIT-UCAP-01 … TC-UNIT-UCAP-22
 */

#include <string.h>

#include <atf-c.h>

#include "amd_umcdf_decode.h"

/* Helper: build a minimal amd_umcdf_cpu with the given zen and ecx */
static struct amd_umcdf_cpu
make_cpu(enum amd_umcdf_zen_generation zen, uint32_t ecx)
{
	struct amd_umcdf_cpu cpu;

	memset(&cpu, 0, sizeof(cpu));
	cpu.zen = zen;
	cpu.amd_feature2_ecx = ecx;
	cpu.is_amd = true;
	return (cpu);
}

/* TC-UNIT-UCAP-01: has_df_feature returns true when PNXC bit is set */
ATF_TC_WITHOUT_HEAD(umcdf_unit_cap_df_feature_true);
ATF_TC_BODY(umcdf_unit_cap_df_feature_true, tc)
{
	struct amd_umcdf_cpu cpu = make_cpu(AMD_UMCDF_ZEN_4, AMD_UMCDF_ID2_PNXC);

	ATF_CHECK_MSG(amd_umcdf_has_df_feature(&cpu) == true,
	    "has_df_feature must be true for Zen4 with PNXC bit set");
}

/* TC-UNIT-UCAP-02: has_df_feature returns false when PNXC bit is clear */
ATF_TC_WITHOUT_HEAD(umcdf_unit_cap_df_feature_false);
ATF_TC_BODY(umcdf_unit_cap_df_feature_false, tc)
{
	struct amd_umcdf_cpu cpu = make_cpu(AMD_UMCDF_ZEN_4, 0U);

	ATF_CHECK_MSG(amd_umcdf_has_df_feature(&cpu) == false,
	    "has_df_feature must be false for Zen4 with PNXC bit clear");
}

/* TC-UNIT-UCAP-03: AMD_UMCDF_ID2_PNXC is bit 24 of ECX */
ATF_TC_WITHOUT_HEAD(umcdf_unit_cap_pnxc_bit24);
ATF_TC_BODY(umcdf_unit_cap_pnxc_bit24, tc)
{
	ATF_CHECK_EQ_MSG(AMD_UMCDF_ID2_PNXC, (1U << 24),
	    "AMD_UMCDF_ID2_PNXC must be bit 24 (%#x), got %#x",
	    (1U << 24), AMD_UMCDF_ID2_PNXC);
}

/* TC-UNIT-UCAP-04: adjacent bit 23 does not trigger has_df_feature */
ATF_TC_WITHOUT_HEAD(umcdf_unit_cap_df_adjacent_bit_safe);
ATF_TC_BODY(umcdf_unit_cap_df_adjacent_bit_safe, tc)
{
	struct amd_umcdf_cpu cpu = make_cpu(AMD_UMCDF_ZEN_4, (1U << 23));

	ATF_CHECK_MSG(amd_umcdf_has_df_feature(&cpu) == false,
	    "has_df_feature must be false for Zen4 with only adjacent bit 23 set");
}

/* TC-UNIT-UCAP-05: has_freebsd_umc_json returns true for Zen 4 */
ATF_TC_WITHOUT_HEAD(umcdf_unit_cap_umc_json_zen4);
ATF_TC_BODY(umcdf_unit_cap_umc_json_zen4, tc)
{
	struct amd_umcdf_cpu cpu = make_cpu(AMD_UMCDF_ZEN_4, 0U);

	ATF_CHECK_MSG(amd_umcdf_has_freebsd_umc_json(&cpu) == true,
	    "has_freebsd_umc_json must be true for Zen4");
}

/* TC-UNIT-UCAP-06: has_freebsd_umc_json returns true for Zen 5 */
ATF_TC_WITHOUT_HEAD(umcdf_unit_cap_umc_json_zen5);
ATF_TC_BODY(umcdf_unit_cap_umc_json_zen5, tc)
{
	struct amd_umcdf_cpu cpu = make_cpu(AMD_UMCDF_ZEN_5, 0U);

	ATF_CHECK_MSG(amd_umcdf_has_freebsd_umc_json(&cpu) == true,
	    "has_freebsd_umc_json must be true for Zen5");
}

/* TC-UNIT-UCAP-07: has_freebsd_umc_json returns true for Zen 6 */
ATF_TC_WITHOUT_HEAD(umcdf_unit_cap_umc_json_zen6);
ATF_TC_BODY(umcdf_unit_cap_umc_json_zen6, tc)
{
	struct amd_umcdf_cpu cpu = make_cpu(AMD_UMCDF_ZEN_6, 0U);

	ATF_CHECK_MSG(amd_umcdf_has_freebsd_umc_json(&cpu) == true,
	    "has_freebsd_umc_json must be true for Zen6");
}

/* TC-UNIT-UCAP-08: has_freebsd_umc_json returns false for Zen 3 */
ATF_TC_WITHOUT_HEAD(umcdf_unit_cap_umc_json_zen3_false);
ATF_TC_BODY(umcdf_unit_cap_umc_json_zen3_false, tc)
{
	struct amd_umcdf_cpu cpu = make_cpu(AMD_UMCDF_ZEN_3, 0U);

	ATF_CHECK_MSG(amd_umcdf_has_freebsd_umc_json(&cpu) == false,
	    "has_freebsd_umc_json must be false for Zen3");
}

/* TC-UNIT-UCAP-09: has_freebsd_umc_json returns false for Zen 1 */
ATF_TC_WITHOUT_HEAD(umcdf_unit_cap_umc_json_zen1_false);
ATF_TC_BODY(umcdf_unit_cap_umc_json_zen1_false, tc)
{
	struct amd_umcdf_cpu cpu = make_cpu(AMD_UMCDF_ZEN_1, 0U);

	ATF_CHECK_MSG(amd_umcdf_has_freebsd_umc_json(&cpu) == false,
	    "has_freebsd_umc_json must be false for Zen1");
}

/* TC-UNIT-UCAP-10: has_freebsd_df_json returns true for Zen 1 */
ATF_TC_WITHOUT_HEAD(umcdf_unit_cap_df_json_zen1);
ATF_TC_BODY(umcdf_unit_cap_df_json_zen1, tc)
{
	struct amd_umcdf_cpu cpu = make_cpu(AMD_UMCDF_ZEN_1, 0U);

	ATF_CHECK_MSG(amd_umcdf_has_freebsd_df_json(&cpu) == true,
	    "has_freebsd_df_json must be true for Zen1");
}

/* TC-UNIT-UCAP-11: has_freebsd_df_json returns true for Zen+ */
ATF_TC_WITHOUT_HEAD(umcdf_unit_cap_df_json_zenplus);
ATF_TC_BODY(umcdf_unit_cap_df_json_zenplus, tc)
{
	struct amd_umcdf_cpu cpu = make_cpu(AMD_UMCDF_ZEN_PLUS, 0U);

	ATF_CHECK_MSG(amd_umcdf_has_freebsd_df_json(&cpu) == true,
	    "has_freebsd_df_json must be true for Zen+");
}

/* TC-UNIT-UCAP-12: has_freebsd_df_json returns true for Zen 2 */
ATF_TC_WITHOUT_HEAD(umcdf_unit_cap_df_json_zen2);
ATF_TC_BODY(umcdf_unit_cap_df_json_zen2, tc)
{
	struct amd_umcdf_cpu cpu = make_cpu(AMD_UMCDF_ZEN_2, 0U);

	ATF_CHECK_MSG(amd_umcdf_has_freebsd_df_json(&cpu) == true,
	    "has_freebsd_df_json must be true for Zen2");
}

/* TC-UNIT-UCAP-13: has_freebsd_df_json returns true for Zen 3 */
ATF_TC_WITHOUT_HEAD(umcdf_unit_cap_df_json_zen3);
ATF_TC_BODY(umcdf_unit_cap_df_json_zen3, tc)
{
	struct amd_umcdf_cpu cpu = make_cpu(AMD_UMCDF_ZEN_3, 0U);

	ATF_CHECK_MSG(amd_umcdf_has_freebsd_df_json(&cpu) == true,
	    "has_freebsd_df_json must be true for Zen3");
}

/* TC-UNIT-UCAP-14: has_freebsd_df_json returns true for Zen 3+ */
ATF_TC_WITHOUT_HEAD(umcdf_unit_cap_df_json_zen3plus);
ATF_TC_BODY(umcdf_unit_cap_df_json_zen3plus, tc)
{
	struct amd_umcdf_cpu cpu = make_cpu(AMD_UMCDF_ZEN_3_PLUS, 0U);

	ATF_CHECK_MSG(amd_umcdf_has_freebsd_df_json(&cpu) == true,
	    "has_freebsd_df_json must be true for Zen3+");
}

/* TC-UNIT-UCAP-15: has_freebsd_df_json returns true for Zen 4 */
ATF_TC_WITHOUT_HEAD(umcdf_unit_cap_df_json_zen4);
ATF_TC_BODY(umcdf_unit_cap_df_json_zen4, tc)
{
	struct amd_umcdf_cpu cpu = make_cpu(AMD_UMCDF_ZEN_4, 0U);

	ATF_CHECK_MSG(amd_umcdf_has_freebsd_df_json(&cpu) == true,
	    "has_freebsd_df_json must be true for Zen4");
}

/* TC-UNIT-UCAP-16: has_freebsd_df_json returns true for Zen 5 */
ATF_TC_WITHOUT_HEAD(umcdf_unit_cap_df_json_zen5);
ATF_TC_BODY(umcdf_unit_cap_df_json_zen5, tc)
{
	struct amd_umcdf_cpu cpu = make_cpu(AMD_UMCDF_ZEN_5, 0U);

	ATF_CHECK_MSG(amd_umcdf_has_freebsd_df_json(&cpu) == true,
	    "has_freebsd_df_json must be true for Zen5");
}

/* TC-UNIT-UCAP-17: has_freebsd_df_json returns false for Zen 6 */
ATF_TC_WITHOUT_HEAD(umcdf_unit_cap_df_json_zen6_false);
ATF_TC_BODY(umcdf_unit_cap_df_json_zen6_false, tc)
{
	struct amd_umcdf_cpu cpu = make_cpu(AMD_UMCDF_ZEN_6, 0U);

	ATF_CHECK_MSG(amd_umcdf_has_freebsd_df_json(&cpu) == false,
	    "has_freebsd_df_json must be false for Zen6");
}

/* TC-UNIT-UCAP-18: has_freebsd_df_json returns false for PRE_ZEN */
ATF_TC_WITHOUT_HEAD(umcdf_unit_cap_df_json_prezen_false);
ATF_TC_BODY(umcdf_unit_cap_df_json_prezen_false, tc)
{
	struct amd_umcdf_cpu cpu = make_cpu(AMD_UMCDF_ZEN_PRE_ZEN, 0U);

	ATF_CHECK_MSG(amd_umcdf_has_freebsd_df_json(&cpu) == false,
	    "has_freebsd_df_json must be false for PRE_ZEN");
}

/* TC-UNIT-UCAP-19: expected_df_config uses DF1 for Zen 3 */
ATF_TC_WITHOUT_HEAD(umcdf_unit_cap_df_config_zen3_is_df1);
ATF_TC_BODY(umcdf_unit_cap_df_config_zen3_is_df1, tc)
{
	struct amd_umcdf_cpu cpu = make_cpu(AMD_UMCDF_ZEN_3, 0U);
	struct amd_umcdf_event_candidate ev = {
		.name = "test", .reason = "", .event_code = 0x07U, .umask = 0x38U
	};
	uint64_t got = amd_umcdf_expected_df_config(&cpu, &ev);
	uint64_t want = AMD_UMCDF_DF1_TO_EVENTMASK(0x07U) |
	    AMD_UMCDF_DF1_TO_UNITMASK(0x38U);

	ATF_CHECK_EQ_MSG(got, want,
	    "df_config = %#llx, expected %#llx",
	    (unsigned long long)(got), (unsigned long long)(want));
}

/* TC-UNIT-UCAP-20: expected_df_config uses DF2 for Zen 4 */
ATF_TC_WITHOUT_HEAD(umcdf_unit_cap_df_config_zen4_is_df2);
ATF_TC_BODY(umcdf_unit_cap_df_config_zen4_is_df2, tc)
{
	struct amd_umcdf_cpu cpu = make_cpu(AMD_UMCDF_ZEN_4, 0U);
	struct amd_umcdf_event_candidate ev = {
		.name = "test", .reason = "", .event_code = 0x07U, .umask = 0x38U
	};
	uint64_t got = amd_umcdf_expected_df_config(&cpu, &ev);
	uint64_t want = AMD_UMCDF_DF2_TO_EVENTMASK(0x07U) |
	    AMD_UMCDF_DF2_TO_UNITMASK(0x38U);

	ATF_CHECK_EQ_MSG(got, want,
	    "df_config = %#llx, expected %#llx",
	    (unsigned long long)(got), (unsigned long long)(want));
}

/* TC-UNIT-UCAP-21: expected_df_config uses DF2 for Zen 5 */
ATF_TC_WITHOUT_HEAD(umcdf_unit_cap_df_config_zen5_is_df2);
ATF_TC_BODY(umcdf_unit_cap_df_config_zen5_is_df2, tc)
{
	struct amd_umcdf_cpu cpu = make_cpu(AMD_UMCDF_ZEN_5, 0U);
	struct amd_umcdf_event_candidate ev = {
		.name = "test", .reason = "", .event_code = 0x55U, .umask = 0xAAU
	};
	uint64_t got = amd_umcdf_expected_df_config(&cpu, &ev);
	uint64_t want = AMD_UMCDF_DF2_TO_EVENTMASK(0x55U) |
	    AMD_UMCDF_DF2_TO_UNITMASK(0xAAU);

	ATF_CHECK_EQ_MSG(got, want,
	    "df_config = %#llx, expected %#llx",
	    (unsigned long long)(got), (unsigned long long)(want));
}

/* TC-UNIT-UCAP-22: DF1 and DF2 configs differ for event with high bits set */
ATF_TC_WITHOUT_HEAD(umcdf_unit_cap_df1_df2_differ);
ATF_TC_BODY(umcdf_unit_cap_df1_df2_differ, tc)
{
	struct amd_umcdf_cpu cpu_z3 = make_cpu(AMD_UMCDF_ZEN_3, 0U);
	struct amd_umcdf_cpu cpu_z4 = make_cpu(AMD_UMCDF_ZEN_4, 0U);
	/* Event code with nibble in bits[11:8] to trigger divergence */
	struct amd_umcdf_event_candidate ev = {
		.name = "test", .reason = "", .event_code = 0x0107U, .umask = 0x01U
	};
	uint64_t df1_cfg = amd_umcdf_expected_df_config(&cpu_z3, &ev);
	uint64_t df2_cfg = amd_umcdf_expected_df_config(&cpu_z4, &ev);

	ATF_CHECK_MSG(df1_cfg != df2_cfg,
	    "DF1 and DF2 configs should differ for event 0x0107; "
	    "df1=0x%016llx df2=0x%016llx",
	    (unsigned long long)df1_cfg, (unsigned long long)df2_cfg);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, umcdf_unit_cap_df_feature_true);
	ATF_TP_ADD_TC(tp, umcdf_unit_cap_df_feature_false);
	ATF_TP_ADD_TC(tp, umcdf_unit_cap_pnxc_bit24);
	ATF_TP_ADD_TC(tp, umcdf_unit_cap_df_adjacent_bit_safe);
	ATF_TP_ADD_TC(tp, umcdf_unit_cap_umc_json_zen4);
	ATF_TP_ADD_TC(tp, umcdf_unit_cap_umc_json_zen5);
	ATF_TP_ADD_TC(tp, umcdf_unit_cap_umc_json_zen6);
	ATF_TP_ADD_TC(tp, umcdf_unit_cap_umc_json_zen3_false);
	ATF_TP_ADD_TC(tp, umcdf_unit_cap_umc_json_zen1_false);
	ATF_TP_ADD_TC(tp, umcdf_unit_cap_df_json_zen1);
	ATF_TP_ADD_TC(tp, umcdf_unit_cap_df_json_zenplus);
	ATF_TP_ADD_TC(tp, umcdf_unit_cap_df_json_zen2);
	ATF_TP_ADD_TC(tp, umcdf_unit_cap_df_json_zen3);
	ATF_TP_ADD_TC(tp, umcdf_unit_cap_df_json_zen3plus);
	ATF_TP_ADD_TC(tp, umcdf_unit_cap_df_json_zen4);
	ATF_TP_ADD_TC(tp, umcdf_unit_cap_df_json_zen5);
	ATF_TP_ADD_TC(tp, umcdf_unit_cap_df_json_zen6_false);
	ATF_TP_ADD_TC(tp, umcdf_unit_cap_df_json_prezen_false);
	ATF_TP_ADD_TC(tp, umcdf_unit_cap_df_config_zen3_is_df1);
	ATF_TP_ADD_TC(tp, umcdf_unit_cap_df_config_zen4_is_df2);
	ATF_TP_ADD_TC(tp, umcdf_unit_cap_df_config_zen5_is_df2);
	ATF_TP_ADD_TC(tp, umcdf_unit_cap_df1_df2_differ);
	return (atf_no_error());
}
