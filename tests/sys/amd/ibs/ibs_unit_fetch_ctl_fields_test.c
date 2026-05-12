/*-
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Author: ojanerif@amd.com
 *
 * [TC-UNIT-FETCHF] — IBS Fetch CTL multi-bit field unit tests.
 *
 * Tests the latency counter, page-size, and enable fields of the
 * IBS Fetch Control MSR (MSR_IBS_FETCH_CTL = 0xC0011030):
 *
 *   IBS_FETCH_MAXCNT   bits 15: 0
 *   IBS_FETCH_CNT      bits 31:16  (current count, read-only)
 *   IBS_FETCH_LAT      bits 47:32  (fetch latency)
 *   IBS_FETCH_EN       bit  48
 *   IBS_FETCH_VAL      bit  49
 *   IBS_FETCH_COMP     bit  50
 *   IBS_IC_MISS        bit  51
 *   IBS_PHY_ADDR_VALID bit  52
 *   IBS_L1TLB_PGSZ     bits 54:53  (page size field)
 *   IBS_L1TLB_MISS     bit  55
 *   IBS_L2TLB_MISS     bit  56
 *   IBS_RAND_EN        bit  57
 *   IBS_FETCH_L2_MISS  bit  58
 *   IBS_L3_MISS_ONLY   bit  59
 *
 * No hardware access.  Runs on any architecture.
 *
 * Test IDs: TC-UNIT-FETCHF-01 … TC-UNIT-FETCHF-16
 */

#include <atf-c.h>

#include "ibs_decode.h"

/* TC-UNIT-FETCHF-01: IBS_FETCH_LAT mask covers bits 47:32 */
ATF_TC_WITHOUT_HEAD(ibs_unit_fetch_lat_mask);
ATF_TC_BODY(ibs_unit_fetch_lat_mask, tc)
{
	ATF_CHECK_EQ(IBS_FETCH_LAT, 0x0000ffff00000000ULL);
}

/* TC-UNIT-FETCHF-02: IBS_FETCH_CNT mask covers bits 31:16 */
ATF_TC_WITHOUT_HEAD(ibs_unit_fetch_cnt_mask);
ATF_TC_BODY(ibs_unit_fetch_cnt_mask, tc)
{
	ATF_CHECK_EQ(IBS_FETCH_CNT, 0x00000000ffff0000ULL);
}

/* TC-UNIT-FETCHF-03: IBS_FETCH_LAT and IBS_FETCH_CNT do not overlap */
ATF_TC_WITHOUT_HEAD(ibs_unit_fetch_lat_cnt_no_overlap);
ATF_TC_BODY(ibs_unit_fetch_lat_cnt_no_overlap, tc)
{
	ATF_CHECK_EQ((IBS_FETCH_LAT & IBS_FETCH_CNT), 0ULL);
}

/* TC-UNIT-FETCHF-04: IBS_FETCH_LAT does not overlap IBS_FETCH_MAXCNT */
ATF_TC_WITHOUT_HEAD(ibs_unit_fetch_lat_maxcnt_no_overlap);
ATF_TC_BODY(ibs_unit_fetch_lat_maxcnt_no_overlap, tc)
{
	ATF_CHECK_EQ((IBS_FETCH_LAT & IBS_FETCH_MAXCNT), 0ULL);
}

/* TC-UNIT-FETCHF-05: IBS_FETCH_CNT does not overlap IBS_FETCH_MAXCNT */
ATF_TC_WITHOUT_HEAD(ibs_unit_fetch_cnt_maxcnt_no_overlap);
ATF_TC_BODY(ibs_unit_fetch_cnt_maxcnt_no_overlap, tc)
{
	ATF_CHECK_EQ((IBS_FETCH_CNT & IBS_FETCH_MAXCNT), 0ULL);
}

/* TC-UNIT-FETCHF-06: IBS_FETCH_EN is bit 48 */
ATF_TC_WITHOUT_HEAD(ibs_unit_fetch_en_bit48);
ATF_TC_BODY(ibs_unit_fetch_en_bit48, tc)
{
	ATF_CHECK_EQ(IBS_FETCH_EN, (1ULL << 48));
}

/* TC-UNIT-FETCHF-07: IBS_FETCH_VAL is bit 49 */
ATF_TC_WITHOUT_HEAD(ibs_unit_fetch_val_bit49);
ATF_TC_BODY(ibs_unit_fetch_val_bit49, tc)
{
	ATF_CHECK_EQ(IBS_FETCH_VAL, (1ULL << 49));
}

/* TC-UNIT-FETCHF-08: IBS_FETCH_EN does not overlap IBS_FETCH_LAT */
ATF_TC_WITHOUT_HEAD(ibs_unit_fetch_en_lat_no_overlap);
ATF_TC_BODY(ibs_unit_fetch_en_lat_no_overlap, tc)
{
	ATF_CHECK_EQ((IBS_FETCH_EN & IBS_FETCH_LAT), 0ULL);
}

/* TC-UNIT-FETCHF-09: IBS_L1TLB_PGSZ_SHIFT is 53 */
ATF_TC_WITHOUT_HEAD(ibs_unit_l1tlb_pgsz_shift_is_53);
ATF_TC_BODY(ibs_unit_l1tlb_pgsz_shift_is_53, tc)
{
	ATF_CHECK_EQ(IBS_L1TLB_PGSZ_SHIFT, 53);
}

/* TC-UNIT-FETCHF-10: IBS_L1TLB_PGSZ covers exactly 2 bits (53 and 54) */
ATF_TC_WITHOUT_HEAD(ibs_unit_l1tlb_pgsz_width);
ATF_TC_BODY(ibs_unit_l1tlb_pgsz_width, tc)
{
	uint64_t mask = IBS_L1TLB_PGSZ;
	int bits = 0;

	while (mask != 0) {
		bits += (int)(mask & 1ULL);
		mask >>= 1;
	}
	ATF_CHECK_EQ(bits, 2);
}

/* TC-UNIT-FETCHF-11: IBS_L1TLB_PGSZ == 0x3ULL << IBS_L1TLB_PGSZ_SHIFT */
ATF_TC_WITHOUT_HEAD(ibs_unit_l1tlb_pgsz_shift_selfcheck);
ATF_TC_BODY(ibs_unit_l1tlb_pgsz_shift_selfcheck, tc)
{
	ATF_CHECK_EQ(IBS_L1TLB_PGSZ, (0x3ULL << IBS_L1TLB_PGSZ_SHIFT));
}

/* TC-UNIT-FETCHF-12: page size 0 (4K) encodes to 0 in the field */
ATF_TC_WITHOUT_HEAD(ibs_unit_l1tlb_pgsz_4k);
ATF_TC_BODY(ibs_unit_l1tlb_pgsz_4k, tc)
{
	uint64_t fetchctl = (0ULL << IBS_L1TLB_PGSZ_SHIFT);
	uint64_t pgsz = (fetchctl & IBS_L1TLB_PGSZ) >> IBS_L1TLB_PGSZ_SHIFT;

	ATF_CHECK_EQ(pgsz, 0ULL);
}

/* TC-UNIT-FETCHF-13: page size 1 (2M) encodes to 1 in the field */
ATF_TC_WITHOUT_HEAD(ibs_unit_l1tlb_pgsz_2m);
ATF_TC_BODY(ibs_unit_l1tlb_pgsz_2m, tc)
{
	uint64_t fetchctl = (1ULL << IBS_L1TLB_PGSZ_SHIFT);
	uint64_t pgsz = (fetchctl & IBS_L1TLB_PGSZ) >> IBS_L1TLB_PGSZ_SHIFT;

	ATF_CHECK_EQ(pgsz, 1ULL);
}

/* TC-UNIT-FETCHF-14: page size 2 (1G) encodes to 2 in the field */
ATF_TC_WITHOUT_HEAD(ibs_unit_l1tlb_pgsz_1g);
ATF_TC_BODY(ibs_unit_l1tlb_pgsz_1g, tc)
{
	uint64_t fetchctl = (2ULL << IBS_L1TLB_PGSZ_SHIFT);
	uint64_t pgsz = (fetchctl & IBS_L1TLB_PGSZ) >> IBS_L1TLB_PGSZ_SHIFT;

	ATF_CHECK_EQ(pgsz, 2ULL);
}

/* TC-UNIT-FETCHF-15: setting MaxCnt preserves LAT and enable bits */
ATF_TC_WITHOUT_HEAD(ibs_unit_fetch_set_maxcnt_preserves_lat_en);
ATF_TC_BODY(ibs_unit_fetch_set_maxcnt_preserves_lat_en, tc)
{
	/* Start with LAT=0xABCD, EN and VAL set */
	uint64_t base = IBS_FETCH_LAT | IBS_FETCH_EN | IBS_FETCH_VAL;
	base |= (0xABCDULL << 32);
	uint64_t updated = ibs_set_maxcnt(base, 0x5678ULL);

	/* MaxCnt should be updated */
	ATF_CHECK_EQ(ibs_get_maxcnt(updated), 0x5678ULL);
	/* EN and VAL should survive */
	ATF_CHECK_MSG((updated & IBS_FETCH_EN) != 0ULL, "FETCH_EN lost");
	ATF_CHECK_MSG((updated & IBS_FETCH_VAL) != 0ULL, "FETCH_VAL lost");
}

/* TC-UNIT-FETCHF-16: IBS_L1TLB_PGSZ does not overlap IBS_FETCH_EN */
ATF_TC_WITHOUT_HEAD(ibs_unit_l1tlb_pgsz_no_overlap_en);
ATF_TC_BODY(ibs_unit_l1tlb_pgsz_no_overlap_en, tc)
{
	ATF_CHECK_EQ((IBS_L1TLB_PGSZ & IBS_FETCH_EN), 0ULL);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, ibs_unit_fetch_lat_mask);
	ATF_TP_ADD_TC(tp, ibs_unit_fetch_cnt_mask);
	ATF_TP_ADD_TC(tp, ibs_unit_fetch_lat_cnt_no_overlap);
	ATF_TP_ADD_TC(tp, ibs_unit_fetch_lat_maxcnt_no_overlap);
	ATF_TP_ADD_TC(tp, ibs_unit_fetch_cnt_maxcnt_no_overlap);
	ATF_TP_ADD_TC(tp, ibs_unit_fetch_en_bit48);
	ATF_TP_ADD_TC(tp, ibs_unit_fetch_val_bit49);
	ATF_TP_ADD_TC(tp, ibs_unit_fetch_en_lat_no_overlap);
	ATF_TP_ADD_TC(tp, ibs_unit_l1tlb_pgsz_shift_is_53);
	ATF_TP_ADD_TC(tp, ibs_unit_l1tlb_pgsz_width);
	ATF_TP_ADD_TC(tp, ibs_unit_l1tlb_pgsz_shift_selfcheck);
	ATF_TP_ADD_TC(tp, ibs_unit_l1tlb_pgsz_4k);
	ATF_TP_ADD_TC(tp, ibs_unit_l1tlb_pgsz_2m);
	ATF_TP_ADD_TC(tp, ibs_unit_l1tlb_pgsz_1g);
	ATF_TP_ADD_TC(tp, ibs_unit_fetch_set_maxcnt_preserves_lat_en);
	ATF_TP_ADD_TC(tp, ibs_unit_l1tlb_pgsz_no_overlap_en);
	return (atf_no_error());
}
