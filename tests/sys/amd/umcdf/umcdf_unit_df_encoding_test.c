/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * Author: ojanerif@amd.com
 *
 * [TC-UNIT-DFENC] — AMD DF event/unit mask encoding macro unit tests.
 *
 * Tests AMD_UMCDF_DF1_TO_EVENTMASK, AMD_UMCDF_DF1_TO_UNITMASK,
 * AMD_UMCDF_DF2_TO_EVENTMASK, and AMD_UMCDF_DF2_TO_UNITMASK from
 * amd_umcdf_decode.h.
 *
 * DF1 encoding (Zen 1 – Zen 3+):
 *   EventMask: bits[7:0]→[7:0], bits[11:8]→[35:32], bits[13:12]→[60:59]
 *   UnitMask:  bits[7:0]→[15:8]
 *
 * DF2 encoding (Zen 4+):
 *   EventMask: bits[7:0]→[7:0], bits[14:8]→[38:32]
 *   UnitMask:  bits[7:0]→[15:8], bits[11:8]→[27:24]
 *
 * No hardware access.  Runs on any architecture.
 *
 * Test IDs: TC-UNIT-DFENC-01 … TC-UNIT-DFENC-16
 */

#include <atf-c.h>

#include "amd_umcdf_decode.h"

/* TC-UNIT-DFENC-01: DF1 event low byte passes through unchanged */
ATF_TC_WITHOUT_HEAD(umcdf_unit_df1_event_low_byte);
ATF_TC_BODY(umcdf_unit_df1_event_low_byte, tc)
{
	ATF_CHECK_EQ(AMD_UMCDF_DF1_TO_EVENTMASK(0x55U), 0x55ULL);
}

/* TC-UNIT-DFENC-02: DF1 event zero input → zero output */
ATF_TC_WITHOUT_HEAD(umcdf_unit_df1_event_zero);
ATF_TC_BODY(umcdf_unit_df1_event_zero, tc)
{
	ATF_CHECK_EQ(AMD_UMCDF_DF1_TO_EVENTMASK(0x0U), 0x0ULL);
}

/* TC-UNIT-DFENC-03: DF1 event bits[11:8] map to result bits[35:32] */
ATF_TC_WITHOUT_HEAD(umcdf_unit_df1_event_nibble_shift);
ATF_TC_BODY(umcdf_unit_df1_event_nibble_shift, tc)
{
	/* event code 0x0100: bit 8 set → should appear at bit 32 */
	ATF_CHECK_EQ(AMD_UMCDF_DF1_TO_EVENTMASK(0x0100U), (1ULL << 32));
}

/* TC-UNIT-DFENC-04: DF1 event bits[13:12] map to result bits[60:59] */
ATF_TC_WITHOUT_HEAD(umcdf_unit_df1_event_ext_shift);
ATF_TC_BODY(umcdf_unit_df1_event_ext_shift, tc)
{
	/* event code 0x1000: bit 12 set → should appear at bit 59 */
	ATF_CHECK_EQ(AMD_UMCDF_DF1_TO_EVENTMASK(0x1000U), (1ULL << 59));
}

/* TC-UNIT-DFENC-05: DF1 unit mask low byte maps to result bits[15:8] */
ATF_TC_WITHOUT_HEAD(umcdf_unit_df1_unit_mask_byte);
ATF_TC_BODY(umcdf_unit_df1_unit_mask_byte, tc)
{
	ATF_CHECK_EQ(AMD_UMCDF_DF1_TO_UNITMASK(0x01U), (1ULL << 8));
	ATF_CHECK_EQ(AMD_UMCDF_DF1_TO_UNITMASK(0xFFU), 0xFF00ULL);
}

/* TC-UNIT-DFENC-06: DF1 unit mask zero → zero */
ATF_TC_WITHOUT_HEAD(umcdf_unit_df1_unit_zero);
ATF_TC_BODY(umcdf_unit_df1_unit_zero, tc)
{
	ATF_CHECK_EQ(AMD_UMCDF_DF1_TO_UNITMASK(0x0U), 0x0ULL);
}

/* TC-UNIT-DFENC-07: DF2 event low byte passes through unchanged */
ATF_TC_WITHOUT_HEAD(umcdf_unit_df2_event_low_byte);
ATF_TC_BODY(umcdf_unit_df2_event_low_byte, tc)
{
	ATF_CHECK_EQ(AMD_UMCDF_DF2_TO_EVENTMASK(0x55U), 0x55ULL);
}

/* TC-UNIT-DFENC-08: DF2 event zero input → zero output */
ATF_TC_WITHOUT_HEAD(umcdf_unit_df2_event_zero);
ATF_TC_BODY(umcdf_unit_df2_event_zero, tc)
{
	ATF_CHECK_EQ(AMD_UMCDF_DF2_TO_EVENTMASK(0x0U), 0x0ULL);
}

/* TC-UNIT-DFENC-09: DF2 event bits[14:8] map to result bits[38:32] */
ATF_TC_WITHOUT_HEAD(umcdf_unit_df2_event_7bit_shift);
ATF_TC_BODY(umcdf_unit_df2_event_7bit_shift, tc)
{
	/* event code 0x0100: bit 8 set → should appear at bit 32 */
	ATF_CHECK_EQ(AMD_UMCDF_DF2_TO_EVENTMASK(0x0100U), (1ULL << 32));
	/* event code 0x4000: bit 14 set → should appear at bit 38 */
	ATF_CHECK_EQ(AMD_UMCDF_DF2_TO_EVENTMASK(0x4000U), (1ULL << 38));
}

/* TC-UNIT-DFENC-10: DF2 unit mask low byte maps to bits[15:8] */
ATF_TC_WITHOUT_HEAD(umcdf_unit_df2_unit_low_byte);
ATF_TC_BODY(umcdf_unit_df2_unit_low_byte, tc)
{
	ATF_CHECK_EQ(AMD_UMCDF_DF2_TO_UNITMASK(0x01U), (1ULL << 8));
}

/* TC-UNIT-DFENC-11: DF2 unit mask bits[11:8] map to result bits[27:24] */
ATF_TC_WITHOUT_HEAD(umcdf_unit_df2_unit_high_nibble);
ATF_TC_BODY(umcdf_unit_df2_unit_high_nibble, tc)
{
	/* umask 0x0100: bit 8 set → should appear at bit 24 */
	ATF_CHECK_EQ(AMD_UMCDF_DF2_TO_UNITMASK(0x0100U), (1ULL << 24));
}

/* TC-UNIT-DFENC-12: DF1 and DF2 produce different results for high event bits */
ATF_TC_WITHOUT_HEAD(umcdf_unit_df1_df2_differ_high_event);
ATF_TC_BODY(umcdf_unit_df1_df2_differ_high_event, tc)
{
	/* event 0x0100 with umask 0: DF1 places at bit 32, DF2 also at bit 32
	 * but DF2 can address bit 38 (bit 14) while DF1 bit 13 goes to bit 60 */
	uint64_t df1 = AMD_UMCDF_DF1_TO_EVENTMASK(0x2000U);
	uint64_t df2 = AMD_UMCDF_DF2_TO_EVENTMASK(0x2000U);

	/* DF1: bit 13 (0x2000 & 0x3000 = 0x2000, shifted 47: bit 13+47=60) */
	ATF_CHECK_MSG(df1 != df2,
	    "DF1 and DF2 should encode high event bits differently; "
	    "df1=0x%016llx df2=0x%016llx",
	    (unsigned long long)df1, (unsigned long long)df2);
}

/* TC-UNIT-DFENC-13: DF1 combined event and unit for a known event */
ATF_TC_WITHOUT_HEAD(umcdf_unit_df1_combined);
ATF_TC_BODY(umcdf_unit_df1_combined, tc)
{
	/* event=0x07 (low byte), umask=0x38 → config=0x3807 */
	uint64_t cfg = AMD_UMCDF_DF1_TO_EVENTMASK(0x07U) |
	    AMD_UMCDF_DF1_TO_UNITMASK(0x38U);

	ATF_CHECK_EQ((cfg & 0xffULL), 0x07ULL);        /* event low byte */
	ATF_CHECK_EQ(((cfg >> 8) & 0xffULL), 0x38ULL); /* unit mask */
}

/* TC-UNIT-DFENC-14: DF2 combined event and unit for a known event */
ATF_TC_WITHOUT_HEAD(umcdf_unit_df2_combined);
ATF_TC_BODY(umcdf_unit_df2_combined, tc)
{
	/* event=0x0107 (low byte=0x07, ext=0x01), umask=0x38 */
	uint64_t cfg = AMD_UMCDF_DF2_TO_EVENTMASK(0x0107U) |
	    AMD_UMCDF_DF2_TO_UNITMASK(0x38U);

	ATF_CHECK_EQ((cfg & 0xffULL), 0x07ULL);         /* event low byte */
	ATF_CHECK_EQ(((cfg >> 8) & 0xffULL), 0x38ULL);  /* unit mask low byte */
	ATF_CHECK_EQ(((cfg >> 32) & 0x1ULL), 0x1ULL);   /* event ext bit 32 */
}

/* TC-UNIT-DFENC-15: event low byte is identical in DF1 and DF2 for low codes */
ATF_TC_WITHOUT_HEAD(umcdf_unit_df1_df2_same_low_byte);
ATF_TC_BODY(umcdf_unit_df1_df2_same_low_byte, tc)
{
	uint32_t code;

	for (code = 0; code <= 0xffU; code++) {
		uint64_t df1 = AMD_UMCDF_DF1_TO_EVENTMASK(code);
		uint64_t df2 = AMD_UMCDF_DF2_TO_EVENTMASK(code);

		ATF_CHECK_MSG((df1 & 0xffULL) == (df2 & 0xffULL),
		    "low byte mismatch for event_code=0x%02x: "
		    "df1=0x%016llx df2=0x%016llx",
		    code,
		    (unsigned long long)df1,
		    (unsigned long long)df2);
	}
}

/* TC-UNIT-DFENC-16: unit mask bits[15:8] are identical in DF1 and DF2 */
ATF_TC_WITHOUT_HEAD(umcdf_unit_df1_df2_same_unit_low);
ATF_TC_BODY(umcdf_unit_df1_df2_same_unit_low, tc)
{
	uint32_t umask;

	for (umask = 0; umask <= 0xffU; umask++) {
		uint64_t df1 = AMD_UMCDF_DF1_TO_UNITMASK(umask);
		uint64_t df2 = AMD_UMCDF_DF2_TO_UNITMASK(umask);

		ATF_CHECK_MSG(((df1 >> 8) & 0xffULL) == ((df2 >> 8) & 0xffULL),
		    "unit mask bits[15:8] mismatch for umask=0x%02x: "
		    "df1=0x%016llx df2=0x%016llx",
		    umask,
		    (unsigned long long)df1,
		    (unsigned long long)df2);
	}
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, umcdf_unit_df1_event_low_byte);
	ATF_TP_ADD_TC(tp, umcdf_unit_df1_event_zero);
	ATF_TP_ADD_TC(tp, umcdf_unit_df1_event_nibble_shift);
	ATF_TP_ADD_TC(tp, umcdf_unit_df1_event_ext_shift);
	ATF_TP_ADD_TC(tp, umcdf_unit_df1_unit_mask_byte);
	ATF_TP_ADD_TC(tp, umcdf_unit_df1_unit_zero);
	ATF_TP_ADD_TC(tp, umcdf_unit_df2_event_low_byte);
	ATF_TP_ADD_TC(tp, umcdf_unit_df2_event_zero);
	ATF_TP_ADD_TC(tp, umcdf_unit_df2_event_7bit_shift);
	ATF_TP_ADD_TC(tp, umcdf_unit_df2_unit_low_byte);
	ATF_TP_ADD_TC(tp, umcdf_unit_df2_unit_high_nibble);
	ATF_TP_ADD_TC(tp, umcdf_unit_df1_df2_differ_high_event);
	ATF_TP_ADD_TC(tp, umcdf_unit_df1_combined);
	ATF_TP_ADD_TC(tp, umcdf_unit_df2_combined);
	ATF_TP_ADD_TC(tp, umcdf_unit_df1_df2_same_low_byte);
	ATF_TP_ADD_TC(tp, umcdf_unit_df1_df2_same_unit_low);
	return (atf_no_error());
}
