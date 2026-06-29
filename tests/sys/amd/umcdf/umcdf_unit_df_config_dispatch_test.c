/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * Author: ojanerif@amd.com
 *
 * [TC-UNIT-DFDIS] — amd_umcdf_expected_df_config() dispatch unit tests.
 *
 * Exhaustively checks that amd_umcdf_expected_df_config() chooses the
 * correct encoding (DF1 vs DF2) based on the Zen generation in the cpu
 * struct, and that the resulting 64-bit value matches the expected
 * encoding for each generation tier.
 *
 * Dispatch rule from amd_umcdf_decode.h:
 *   zen >= AMD_UMCDF_ZEN_4  →  DF2 encoding
 *   zen <  AMD_UMCDF_ZEN_4  →  DF1 encoding
 *
 * No hardware access.  Runs on any architecture.
 *
 * Test IDs: TC-UNIT-DFDIS-01 … TC-UNIT-DFDIS-17
 */

#include <string.h>

#include <atf-c.h>

#include "amd_umcdf_decode.h"

/* A representative event with both low-byte and high-nibble bits set so that
 * DF1 and DF2 encode it differently. */
#define TEST_EVENT_CODE	0x0107U
#define TEST_UMASK	0x38U

static struct amd_umcdf_cpu
make_cpu_zen(enum amd_umcdf_zen_generation zen)
{
	struct amd_umcdf_cpu cpu;

	memset(&cpu, 0, sizeof(cpu));
	cpu.zen = zen;
	cpu.is_amd = true;
	return (cpu);
}

static struct amd_umcdf_event_candidate
make_event(uint32_t code, uint32_t umask)
{
	struct amd_umcdf_event_candidate ev;

	memset(&ev, 0, sizeof(ev));
	ev.name = "test";
	ev.reason = "";
	ev.event_code = code;
	ev.umask = umask;
	return (ev);
}

/* TC-UNIT-DFDIS-01: PRE_ZEN uses DF1 encoding */
ATF_TC_WITHOUT_HEAD(umcdf_unit_dfdis_prezen_df1);
ATF_TC_BODY(umcdf_unit_dfdis_prezen_df1, tc)
{
	struct amd_umcdf_cpu cpu = make_cpu_zen(AMD_UMCDF_ZEN_PRE_ZEN);
	struct amd_umcdf_event_candidate ev = make_event(TEST_EVENT_CODE, TEST_UMASK);
	uint64_t want = AMD_UMCDF_DF1_TO_EVENTMASK(TEST_EVENT_CODE) |
	    AMD_UMCDF_DF1_TO_UNITMASK(TEST_UMASK);

	ATF_CHECK_EQ_MSG(amd_umcdf_expected_df_config(&cpu, &ev), want,
	    "expected_df_config = %#llx, want %#llx",
	    (unsigned long long)amd_umcdf_expected_df_config(&cpu, &ev),
	    (unsigned long long)(want));
}

/* TC-UNIT-DFDIS-02: Zen 1 uses DF1 encoding */
ATF_TC_WITHOUT_HEAD(umcdf_unit_dfdis_zen1_df1);
ATF_TC_BODY(umcdf_unit_dfdis_zen1_df1, tc)
{
	struct amd_umcdf_cpu cpu = make_cpu_zen(AMD_UMCDF_ZEN_1);
	struct amd_umcdf_event_candidate ev = make_event(TEST_EVENT_CODE, TEST_UMASK);
	uint64_t want = AMD_UMCDF_DF1_TO_EVENTMASK(TEST_EVENT_CODE) |
	    AMD_UMCDF_DF1_TO_UNITMASK(TEST_UMASK);

	ATF_CHECK_EQ_MSG(amd_umcdf_expected_df_config(&cpu, &ev), want,
	    "expected_df_config = %#llx, want %#llx",
	    (unsigned long long)amd_umcdf_expected_df_config(&cpu, &ev),
	    (unsigned long long)(want));
}

/* TC-UNIT-DFDIS-03: Zen+ uses DF1 encoding */
ATF_TC_WITHOUT_HEAD(umcdf_unit_dfdis_zenplus_df1);
ATF_TC_BODY(umcdf_unit_dfdis_zenplus_df1, tc)
{
	struct amd_umcdf_cpu cpu = make_cpu_zen(AMD_UMCDF_ZEN_PLUS);
	struct amd_umcdf_event_candidate ev = make_event(TEST_EVENT_CODE, TEST_UMASK);
	uint64_t want = AMD_UMCDF_DF1_TO_EVENTMASK(TEST_EVENT_CODE) |
	    AMD_UMCDF_DF1_TO_UNITMASK(TEST_UMASK);

	ATF_CHECK_EQ_MSG(amd_umcdf_expected_df_config(&cpu, &ev), want,
	    "expected_df_config = %#llx, want %#llx",
	    (unsigned long long)amd_umcdf_expected_df_config(&cpu, &ev),
	    (unsigned long long)(want));
}

/* TC-UNIT-DFDIS-04: Zen 2 uses DF1 encoding */
ATF_TC_WITHOUT_HEAD(umcdf_unit_dfdis_zen2_df1);
ATF_TC_BODY(umcdf_unit_dfdis_zen2_df1, tc)
{
	struct amd_umcdf_cpu cpu = make_cpu_zen(AMD_UMCDF_ZEN_2);
	struct amd_umcdf_event_candidate ev = make_event(TEST_EVENT_CODE, TEST_UMASK);
	uint64_t want = AMD_UMCDF_DF1_TO_EVENTMASK(TEST_EVENT_CODE) |
	    AMD_UMCDF_DF1_TO_UNITMASK(TEST_UMASK);

	ATF_CHECK_EQ_MSG(amd_umcdf_expected_df_config(&cpu, &ev), want,
	    "expected_df_config = %#llx, want %#llx",
	    (unsigned long long)amd_umcdf_expected_df_config(&cpu, &ev),
	    (unsigned long long)(want));
}

/* TC-UNIT-DFDIS-05: Zen 3 uses DF1 encoding */
ATF_TC_WITHOUT_HEAD(umcdf_unit_dfdis_zen3_df1);
ATF_TC_BODY(umcdf_unit_dfdis_zen3_df1, tc)
{
	struct amd_umcdf_cpu cpu = make_cpu_zen(AMD_UMCDF_ZEN_3);
	struct amd_umcdf_event_candidate ev = make_event(TEST_EVENT_CODE, TEST_UMASK);
	uint64_t want = AMD_UMCDF_DF1_TO_EVENTMASK(TEST_EVENT_CODE) |
	    AMD_UMCDF_DF1_TO_UNITMASK(TEST_UMASK);

	ATF_CHECK_EQ_MSG(amd_umcdf_expected_df_config(&cpu, &ev), want,
	    "expected_df_config = %#llx, want %#llx",
	    (unsigned long long)amd_umcdf_expected_df_config(&cpu, &ev),
	    (unsigned long long)(want));
}

/* TC-UNIT-DFDIS-06: Zen 3+ uses DF1 encoding */
ATF_TC_WITHOUT_HEAD(umcdf_unit_dfdis_zen3plus_df1);
ATF_TC_BODY(umcdf_unit_dfdis_zen3plus_df1, tc)
{
	struct amd_umcdf_cpu cpu = make_cpu_zen(AMD_UMCDF_ZEN_3_PLUS);
	struct amd_umcdf_event_candidate ev = make_event(TEST_EVENT_CODE, TEST_UMASK);
	uint64_t want = AMD_UMCDF_DF1_TO_EVENTMASK(TEST_EVENT_CODE) |
	    AMD_UMCDF_DF1_TO_UNITMASK(TEST_UMASK);

	ATF_CHECK_EQ_MSG(amd_umcdf_expected_df_config(&cpu, &ev), want,
	    "expected_df_config = %#llx, want %#llx",
	    (unsigned long long)amd_umcdf_expected_df_config(&cpu, &ev),
	    (unsigned long long)(want));
}

/* TC-UNIT-DFDIS-07: Zen 4 uses DF2 encoding */
ATF_TC_WITHOUT_HEAD(umcdf_unit_dfdis_zen4_df2);
ATF_TC_BODY(umcdf_unit_dfdis_zen4_df2, tc)
{
	struct amd_umcdf_cpu cpu = make_cpu_zen(AMD_UMCDF_ZEN_4);
	struct amd_umcdf_event_candidate ev = make_event(TEST_EVENT_CODE, TEST_UMASK);
	uint64_t want = AMD_UMCDF_DF2_TO_EVENTMASK(TEST_EVENT_CODE) |
	    AMD_UMCDF_DF2_TO_UNITMASK(TEST_UMASK);

	ATF_CHECK_EQ_MSG(amd_umcdf_expected_df_config(&cpu, &ev), want,
	    "expected_df_config = %#llx, want %#llx",
	    (unsigned long long)amd_umcdf_expected_df_config(&cpu, &ev),
	    (unsigned long long)(want));
}

/* TC-UNIT-DFDIS-08: Zen 5 uses DF2 encoding */
ATF_TC_WITHOUT_HEAD(umcdf_unit_dfdis_zen5_df2);
ATF_TC_BODY(umcdf_unit_dfdis_zen5_df2, tc)
{
	struct amd_umcdf_cpu cpu = make_cpu_zen(AMD_UMCDF_ZEN_5);
	struct amd_umcdf_event_candidate ev = make_event(TEST_EVENT_CODE, TEST_UMASK);
	uint64_t want = AMD_UMCDF_DF2_TO_EVENTMASK(TEST_EVENT_CODE) |
	    AMD_UMCDF_DF2_TO_UNITMASK(TEST_UMASK);

	ATF_CHECK_EQ_MSG(amd_umcdf_expected_df_config(&cpu, &ev), want,
	    "expected_df_config = %#llx, want %#llx",
	    (unsigned long long)amd_umcdf_expected_df_config(&cpu, &ev),
	    (unsigned long long)(want));
}

/* TC-UNIT-DFDIS-09: Zen 6 uses DF2 encoding */
ATF_TC_WITHOUT_HEAD(umcdf_unit_dfdis_zen6_df2);
ATF_TC_BODY(umcdf_unit_dfdis_zen6_df2, tc)
{
	struct amd_umcdf_cpu cpu = make_cpu_zen(AMD_UMCDF_ZEN_6);
	struct amd_umcdf_event_candidate ev = make_event(TEST_EVENT_CODE, TEST_UMASK);
	uint64_t want = AMD_UMCDF_DF2_TO_EVENTMASK(TEST_EVENT_CODE) |
	    AMD_UMCDF_DF2_TO_UNITMASK(TEST_UMASK);

	ATF_CHECK_EQ_MSG(amd_umcdf_expected_df_config(&cpu, &ev), want,
	    "expected_df_config = %#llx, want %#llx",
	    (unsigned long long)amd_umcdf_expected_df_config(&cpu, &ev),
	    (unsigned long long)(want));
}

/* TC-UNIT-DFDIS-10: DF1 and DF2 produce different values for high-bit event */
ATF_TC_WITHOUT_HEAD(umcdf_unit_dfdis_df1_df2_differ);
ATF_TC_BODY(umcdf_unit_dfdis_df1_df2_differ, tc)
{
	struct amd_umcdf_cpu z3 = make_cpu_zen(AMD_UMCDF_ZEN_3);
	struct amd_umcdf_cpu z4 = make_cpu_zen(AMD_UMCDF_ZEN_4);
	struct amd_umcdf_event_candidate ev = make_event(TEST_EVENT_CODE, TEST_UMASK);
	uint64_t cfg_z3 = amd_umcdf_expected_df_config(&z3, &ev);
	uint64_t cfg_z4 = amd_umcdf_expected_df_config(&z4, &ev);

	ATF_CHECK_MSG(cfg_z3 != cfg_z4,
	    "Zen3 (DF1) and Zen4 (DF2) configs should differ for "
	    "event=0x%04x umask=0x%02x; got the same 0x%016llx",
	    TEST_EVENT_CODE, TEST_UMASK, (unsigned long long)cfg_z3);
}

/* TC-UNIT-DFDIS-11: event code 0 + umask 0 → 0 for all generations */
ATF_TC_WITHOUT_HEAD(umcdf_unit_dfdis_zero_event_zero_config);
ATF_TC_BODY(umcdf_unit_dfdis_zero_event_zero_config, tc)
{
	const enum amd_umcdf_zen_generation gens[] = {
		AMD_UMCDF_ZEN_1, AMD_UMCDF_ZEN_2, AMD_UMCDF_ZEN_3,
		AMD_UMCDF_ZEN_4, AMD_UMCDF_ZEN_5, AMD_UMCDF_ZEN_6,
	};
	struct amd_umcdf_event_candidate ev = make_event(0U, 0U);
	int i;

	for (i = 0; i < (int)(sizeof(gens) / sizeof(gens[0])); i++) {
		struct amd_umcdf_cpu cpu = make_cpu_zen(gens[i]);
		uint64_t cfg = amd_umcdf_expected_df_config(&cpu, &ev);

		ATF_CHECK_MSG(cfg == 0ULL,
		    "expected zero config for event=0/umask=0 on %s, got 0x%016llx",
		    amd_umcdf_zen_name(gens[i]), (unsigned long long)cfg);
	}
}

/* TC-UNIT-DFDIS-12: event low byte is preserved in DF1 result */
ATF_TC_WITHOUT_HEAD(umcdf_unit_dfdis_df1_low_byte_preserved);
ATF_TC_BODY(umcdf_unit_dfdis_df1_low_byte_preserved, tc)
{
	struct amd_umcdf_cpu cpu = make_cpu_zen(AMD_UMCDF_ZEN_3);
	struct amd_umcdf_event_candidate ev = make_event(0x42U, 0U);
	uint64_t cfg = amd_umcdf_expected_df_config(&cpu, &ev);

	ATF_CHECK_EQ_MSG((cfg & 0xffULL), 0x42ULL,
	    "eventmask bits = %#llx, expected %#llx",
	    (unsigned long long)(cfg & 0xffULL), (unsigned long long)0x42ULL);
}

/* TC-UNIT-DFDIS-13: event low byte is preserved in DF2 result */
ATF_TC_WITHOUT_HEAD(umcdf_unit_dfdis_df2_low_byte_preserved);
ATF_TC_BODY(umcdf_unit_dfdis_df2_low_byte_preserved, tc)
{
	struct amd_umcdf_cpu cpu = make_cpu_zen(AMD_UMCDF_ZEN_4);
	struct amd_umcdf_event_candidate ev = make_event(0x42U, 0U);
	uint64_t cfg = amd_umcdf_expected_df_config(&cpu, &ev);

	ATF_CHECK_EQ_MSG((cfg & 0xffULL), 0x42ULL,
	    "eventmask bits = %#llx, expected %#llx",
	    (unsigned long long)(cfg & 0xffULL), (unsigned long long)0x42ULL);
}

/* TC-UNIT-DFDIS-14: unit mask low byte lands at bits[15:8] in DF1 */
ATF_TC_WITHOUT_HEAD(umcdf_unit_dfdis_df1_unit_position);
ATF_TC_BODY(umcdf_unit_dfdis_df1_unit_position, tc)
{
	struct amd_umcdf_cpu cpu = make_cpu_zen(AMD_UMCDF_ZEN_3);
	struct amd_umcdf_event_candidate ev = make_event(0U, 0x3CU);
	uint64_t cfg = amd_umcdf_expected_df_config(&cpu, &ev);

	ATF_CHECK_EQ_MSG(((cfg >> 8) & 0xffULL), 0x3CULL,
	    "unitmask byte = %#llx, expected %#llx",
	    (unsigned long long)((cfg >> 8) & 0xffULL), (unsigned long long)0x3CULL);
}

/* TC-UNIT-DFDIS-15: unit mask low byte lands at bits[15:8] in DF2 */
ATF_TC_WITHOUT_HEAD(umcdf_unit_dfdis_df2_unit_position);
ATF_TC_BODY(umcdf_unit_dfdis_df2_unit_position, tc)
{
	struct amd_umcdf_cpu cpu = make_cpu_zen(AMD_UMCDF_ZEN_4);
	struct amd_umcdf_event_candidate ev = make_event(0U, 0x3CU);
	uint64_t cfg = amd_umcdf_expected_df_config(&cpu, &ev);

	ATF_CHECK_EQ_MSG(((cfg >> 8) & 0xffULL), 0x3CULL,
	    "unitmask byte = %#llx, expected %#llx",
	    (unsigned long long)((cfg >> 8) & 0xffULL), (unsigned long long)0x3CULL);
}

/* TC-UNIT-DFDIS-16: DF2 high umask nibble lands at bits[27:24] */
ATF_TC_WITHOUT_HEAD(umcdf_unit_dfdis_df2_unit_high_nibble);
ATF_TC_BODY(umcdf_unit_dfdis_df2_unit_high_nibble, tc)
{
	struct amd_umcdf_cpu cpu = make_cpu_zen(AMD_UMCDF_ZEN_5);
	struct amd_umcdf_event_candidate ev = make_event(0U, 0x0100U);
	uint64_t cfg = amd_umcdf_expected_df_config(&cpu, &ev);

	/* bit 8 of umask → bit 24 of result */
	ATF_CHECK_EQ_MSG(((cfg >> 24) & 0x1ULL), 0x1ULL,
	    "enable/high bit = %#llx, expected %#llx",
	    (unsigned long long)((cfg >> 24) & 0x1ULL), (unsigned long long)0x1ULL);
}

/* TC-UNIT-DFDIS-17: DF1 high-extension umask is NOT present in the result */
ATF_TC_WITHOUT_HEAD(umcdf_unit_dfdis_df1_no_high_umask);
ATF_TC_BODY(umcdf_unit_dfdis_df1_no_high_umask, tc)
{
	struct amd_umcdf_cpu cpu = make_cpu_zen(AMD_UMCDF_ZEN_3);
	struct amd_umcdf_event_candidate ev = make_event(0U, 0x0100U);
	uint64_t cfg = amd_umcdf_expected_df_config(&cpu, &ev);

	/* DF1 only uses bits[7:0] of umask.  0x0100 has no low byte, so
	 * the entire config should be 0. */
	ATF_CHECK_EQ_MSG(cfg, 0ULL,
	    "full config = %#llx, expected %#llx",
	    (unsigned long long)cfg, (unsigned long long)0ULL);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, umcdf_unit_dfdis_prezen_df1);
	ATF_TP_ADD_TC(tp, umcdf_unit_dfdis_zen1_df1);
	ATF_TP_ADD_TC(tp, umcdf_unit_dfdis_zenplus_df1);
	ATF_TP_ADD_TC(tp, umcdf_unit_dfdis_zen2_df1);
	ATF_TP_ADD_TC(tp, umcdf_unit_dfdis_zen3_df1);
	ATF_TP_ADD_TC(tp, umcdf_unit_dfdis_zen3plus_df1);
	ATF_TP_ADD_TC(tp, umcdf_unit_dfdis_zen4_df2);
	ATF_TP_ADD_TC(tp, umcdf_unit_dfdis_zen5_df2);
	ATF_TP_ADD_TC(tp, umcdf_unit_dfdis_zen6_df2);
	ATF_TP_ADD_TC(tp, umcdf_unit_dfdis_df1_df2_differ);
	ATF_TP_ADD_TC(tp, umcdf_unit_dfdis_zero_event_zero_config);
	ATF_TP_ADD_TC(tp, umcdf_unit_dfdis_df1_low_byte_preserved);
	ATF_TP_ADD_TC(tp, umcdf_unit_dfdis_df2_low_byte_preserved);
	ATF_TP_ADD_TC(tp, umcdf_unit_dfdis_df1_unit_position);
	ATF_TP_ADD_TC(tp, umcdf_unit_dfdis_df2_unit_position);
	ATF_TP_ADD_TC(tp, umcdf_unit_dfdis_df2_unit_high_nibble);
	ATF_TP_ADD_TC(tp, umcdf_unit_dfdis_df1_no_high_umask);
	return (atf_no_error());
}
