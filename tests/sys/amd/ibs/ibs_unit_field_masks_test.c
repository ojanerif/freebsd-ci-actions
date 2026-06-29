/*-
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Author: ojanerif@amd.com
 *
 * [TC-UNIT-MASK] — Field mask constant verification.
 *
 * Verifies every bit-mask and address constant in ibs_decode.h against the
 * AMD Processor Programming Reference (PPR).  No hardware access, no OS
 * calls.  These tests run on any architecture and do not require root.
 *
 * Test IDs: TC-UNIT-MASK-01 … TC-UNIT-MASK-09
 */

#include <atf-c.h>

#include "ibs_decode.h"

/* -----------------------------------------------------------------------
 * TC-UNIT-MASK-01: IBS Fetch CTL mask values
 * ----------------------------------------------------------------------- */
ATF_TC_WITHOUT_HEAD(ibs_unit_fetch_ctl_mask_values);
ATF_TC_BODY(ibs_unit_fetch_ctl_mask_values, tc)
{
	/* AMD PPR: IbsFetchMaxCnt = bits 15:0 */
	ATF_CHECK_EQ_MSG(IBS_FETCH_MAXCNT, 0x000000000000ffffULL,
	    "IBS_FETCH_MAXCNT = %#llx, expected %#llx",
	    (unsigned long long)(IBS_FETCH_MAXCNT),
	    (unsigned long long)(0x000000000000ffffULL));
	/* IbsFetchEn = bit 48 */
	ATF_CHECK_EQ_MSG(IBS_FETCH_EN, (1ULL << 48),
	    "IBS_FETCH_EN = %#llx, expected %#llx",
	    (unsigned long long)(IBS_FETCH_EN),
	    (unsigned long long)((1ULL << 48)));
	/* IbsFetchVal = bit 49 */
	ATF_CHECK_EQ_MSG(IBS_FETCH_VAL, (1ULL << 49),
	    "IBS_FETCH_VAL = %#llx, expected %#llx",
	    (unsigned long long)(IBS_FETCH_VAL),
	    (unsigned long long)((1ULL << 49)));
	/* IbsFetchComp = bit 50 */
	ATF_CHECK_EQ_MSG(IBS_FETCH_COMP, (1ULL << 50),
	    "IBS_FETCH_COMP = %#llx, expected %#llx",
	    (unsigned long long)(IBS_FETCH_COMP),
	    (unsigned long long)((1ULL << 50)));
	/* IbsFetchL3MissOnly = bit 59 */
	ATF_CHECK_EQ_MSG(IBS_L3_MISS_ONLY, (1ULL << 59),
	    "IBS_L3_MISS_ONLY = %#llx, expected %#llx",
	    (unsigned long long)(IBS_L3_MISS_ONLY),
	    (unsigned long long)((1ULL << 59)));
	/* IbsRandEn = bit 57 */
	ATF_CHECK_EQ_MSG(IBS_RAND_EN, (1ULL << 57),
	    "IBS_RAND_EN = %#llx, expected %#llx",
	    (unsigned long long)(IBS_RAND_EN),
	    (unsigned long long)((1ULL << 57)));
}

/* -----------------------------------------------------------------------
 * TC-UNIT-MASK-02: IBS Op CTL mask values
 * ----------------------------------------------------------------------- */
ATF_TC_WITHOUT_HEAD(ibs_unit_op_ctl_mask_values);
ATF_TC_BODY(ibs_unit_op_ctl_mask_values, tc)
{
	/* IbsOpMaxCnt = bits 15:0 */
	ATF_CHECK_EQ_MSG(IBS_OP_MAXCNT, 0x000000000000ffffULL,
	    "IBS_OP_MAXCNT = %#llx, expected %#llx",
	    (unsigned long long)(IBS_OP_MAXCNT),
	    (unsigned long long)(0x000000000000ffffULL));
	/* IbsOpEn = bit 17 */
	ATF_CHECK_EQ_MSG(IBS_OP_EN, (1ULL << 17),
	    "IBS_OP_EN = %#llx, expected %#llx",
	    (unsigned long long)(IBS_OP_EN),
	    (unsigned long long)((1ULL << 17)));
	/* IbsOpVal = bit 18 */
	ATF_CHECK_EQ_MSG(IBS_OP_VAL, (1ULL << 18),
	    "IBS_OP_VAL = %#llx, expected %#llx",
	    (unsigned long long)(IBS_OP_VAL),
	    (unsigned long long)((1ULL << 18)));
	/* IbsCntCtl = bit 19 */
	ATF_CHECK_EQ_MSG(IBS_CNT_CTL, (1ULL << 19),
	    "IBS_CNT_CTL = %#llx, expected %#llx",
	    (unsigned long long)(IBS_CNT_CTL),
	    (unsigned long long)((1ULL << 19)));
	/* IbsOpMaxCnt[22:16] = bits 26:20 */
	ATF_CHECK_EQ_MSG(IBS_OP_MAXCNT_EXT, 0x0000000007f00000ULL,
	    "IBS_OP_MAXCNT_EXT = %#llx, expected %#llx",
	    (unsigned long long)(IBS_OP_MAXCNT_EXT),
	    (unsigned long long)(0x0000000007f00000ULL));
	/* IbsOpL3MissOnly = bit 16 */
	ATF_CHECK_EQ_MSG(IBS_OP_L3_MISS_ONLY, (1ULL << 16),
	    "IBS_OP_L3_MISS_ONLY = %#llx, expected %#llx",
	    (unsigned long long)(IBS_OP_L3_MISS_ONLY),
	    (unsigned long long)((1ULL << 16)));
}

/* -----------------------------------------------------------------------
 * TC-UNIT-MASK-03: IBS Op Data 1 mask values
 * ----------------------------------------------------------------------- */
ATF_TC_WITHOUT_HEAD(ibs_unit_op_data1_mask_values);
ATF_TC_BODY(ibs_unit_op_data1_mask_values, tc)
{
	/* IbsCompToRetCtr = bits 15:0 */
	ATF_CHECK_EQ_MSG(IBS_COMP_TO_RET_CTR, 0x000000000000ffffULL,
	    "IBS_COMP_TO_RET_CTR = %#llx, expected %#llx",
	    (unsigned long long)(IBS_COMP_TO_RET_CTR),
	    (unsigned long long)(0x000000000000ffffULL));
	/* IbsTagToRetCtr = bits 31:16 */
	ATF_CHECK_EQ_MSG(IBS_TAG_TO_RET_CTR, 0x00000000ffff0000ULL,
	    "IBS_TAG_TO_RET_CTR = %#llx, expected %#llx",
	    (unsigned long long)(IBS_TAG_TO_RET_CTR),
	    (unsigned long long)(0x00000000ffff0000ULL));
	/* IbsOpReturn = bit 34 */
	ATF_CHECK_EQ_MSG(IBS_OP_RETURN, (1ULL << 34),
	    "IBS_OP_RETURN = %#llx, expected %#llx",
	    (unsigned long long)(IBS_OP_RETURN),
	    (unsigned long long)((1ULL << 34)));
	/* IbsOpBrnTaken = bit 35 */
	ATF_CHECK_EQ_MSG(IBS_OP_BRN_TAKEN, (1ULL << 35),
	    "IBS_OP_BRN_TAKEN = %#llx, expected %#llx",
	    (unsigned long long)(IBS_OP_BRN_TAKEN),
	    (unsigned long long)((1ULL << 35)));
	/* IbsOpBrnMisp = bit 36 */
	ATF_CHECK_EQ_MSG(IBS_OP_BRN_MISP, (1ULL << 36),
	    "IBS_OP_BRN_MISP = %#llx, expected %#llx",
	    (unsigned long long)(IBS_OP_BRN_MISP),
	    (unsigned long long)((1ULL << 36)));
	/* IbsOpBrnRet = bit 37 */
	ATF_CHECK_EQ_MSG(IBS_OP_BRN_RET, (1ULL << 37),
	    "IBS_OP_BRN_RET = %#llx, expected %#llx",
	    (unsigned long long)(IBS_OP_BRN_RET),
	    (unsigned long long)((1ULL << 37)));
}

/* -----------------------------------------------------------------------
 * TC-UNIT-MASK-04: IBS Op Data 2 mask values (DataSrc)
 * ----------------------------------------------------------------------- */
ATF_TC_WITHOUT_HEAD(ibs_unit_op_data2_mask_values);
ATF_TC_BODY(ibs_unit_op_data2_mask_values, tc)
{
	/* DataSrcLo = bits 2:0 */
	ATF_CHECK_EQ_MSG(IBS_DATA_SRC_LO, 0x0000000000000007ULL,
	    "IBS_DATA_SRC_LO = %#llx, expected %#llx",
	    (unsigned long long)(IBS_DATA_SRC_LO),
	    (unsigned long long)(0x0000000000000007ULL));
	/* RmtNode = bit 4 */
	ATF_CHECK_EQ_MSG(IBS_RMT_NODE, (1ULL << 4),
	    "IBS_RMT_NODE = %#llx, expected %#llx",
	    (unsigned long long)(IBS_RMT_NODE),
	    (unsigned long long)((1ULL << 4)));
	/* CacheHitSt = bit 5 */
	ATF_CHECK_EQ_MSG(IBS_CACHE_HIT_ST, (1ULL << 5),
	    "IBS_CACHE_HIT_ST = %#llx, expected %#llx",
	    (unsigned long long)(IBS_CACHE_HIT_ST),
	    (unsigned long long)((1ULL << 5)));
	/* DataSrcHi = bits 7:6 */
	ATF_CHECK_EQ_MSG(IBS_DATA_SRC_HI, 0x00000000000000c0ULL,
	    "IBS_DATA_SRC_HI = %#llx, expected %#llx",
	    (unsigned long long)(IBS_DATA_SRC_HI),
	    (unsigned long long)(0x00000000000000c0ULL));
}

/* -----------------------------------------------------------------------
 * TC-UNIT-MASK-05: IBS Op Data 3 mask values
 * ----------------------------------------------------------------------- */
ATF_TC_WITHOUT_HEAD(ibs_unit_op_data3_mask_values);
ATF_TC_BODY(ibs_unit_op_data3_mask_values, tc)
{
	/* IbsLdOp = bit 0 */
	ATF_CHECK_EQ_MSG(IBS_LD_OP, (1ULL << 0),
	    "IBS_LD_OP = %#llx, expected %#llx",
	    (unsigned long long)(IBS_LD_OP),
	    (unsigned long long)((1ULL << 0)));
	/* IbsStOp = bit 1 */
	ATF_CHECK_EQ_MSG(IBS_ST_OP, (1ULL << 1),
	    "IBS_ST_OP = %#llx, expected %#llx",
	    (unsigned long long)(IBS_ST_OP),
	    (unsigned long long)((1ULL << 1)));
	/* IbsDcMiss = bit 7 */
	ATF_CHECK_EQ_MSG(IBS_DC_MISS, (1ULL << 7),
	    "IBS_DC_MISS = %#llx, expected %#llx",
	    (unsigned long long)(IBS_DC_MISS),
	    (unsigned long long)((1ULL << 7)));
	/* IbsDcLinAddrValid = bit 17 */
	ATF_CHECK_EQ_MSG(IBS_DC_LIN_ADDR_VALID, (1ULL << 17),
	    "IBS_DC_LIN_ADDR_VALID = %#llx, expected %#llx",
	    (unsigned long long)(IBS_DC_LIN_ADDR_VALID),
	    (unsigned long long)((1ULL << 17)));
	/* IbsDcPhyAddrValid = bit 18 */
	ATF_CHECK_EQ_MSG(IBS_DC_PHY_ADDR_VALID, (1ULL << 18),
	    "IBS_DC_PHY_ADDR_VALID = %#llx, expected %#llx",
	    (unsigned long long)(IBS_DC_PHY_ADDR_VALID),
	    (unsigned long long)((1ULL << 18)));
	/* IbsDcMissLat = bits 47:32 */
	ATF_CHECK_EQ_MSG(IBS_DC_MISS_LAT, 0x0000ffff00000000ULL,
	    "IBS_DC_MISS_LAT = %#llx, expected %#llx",
	    (unsigned long long)(IBS_DC_MISS_LAT),
	    (unsigned long long)(0x0000ffff00000000ULL));
}

/* -----------------------------------------------------------------------
 * TC-UNIT-MASK-06: MSR address map
 * ----------------------------------------------------------------------- */
ATF_TC_WITHOUT_HEAD(ibs_unit_msr_address_map);
ATF_TC_BODY(ibs_unit_msr_address_map, tc)
{
	/* AMD PPR table 2-63: IBS MSR address range */
	ATF_CHECK_EQ_MSG(IBS_MSR_FETCH_CTL,    0xC0011030U,
	    "IBS_MSR_FETCH_CTL = %#llx, expected %#llx",
	    (unsigned long long)(IBS_MSR_FETCH_CTL),
	    (unsigned long long)(0xC0011030U));
	ATF_CHECK_EQ_MSG(IBS_MSR_FETCH_LINADDR, 0xC0011031U,
	    "IBS_MSR_FETCH_LINADDR = %#llx, expected %#llx",
	    (unsigned long long)(IBS_MSR_FETCH_LINADDR),
	    (unsigned long long)(0xC0011031U));
	ATF_CHECK_EQ_MSG(IBS_MSR_FETCH_PHYSADDR, 0xC0011032U,
	    "IBS_MSR_FETCH_PHYSADDR = %#llx, expected %#llx",
	    (unsigned long long)(IBS_MSR_FETCH_PHYSADDR),
	    (unsigned long long)(0xC0011032U));
	ATF_CHECK_EQ_MSG(IBS_MSR_OP_CTL,       0xC0011033U,
	    "IBS_MSR_OP_CTL = %#llx, expected %#llx",
	    (unsigned long long)(IBS_MSR_OP_CTL),
	    (unsigned long long)(0xC0011033U));
	ATF_CHECK_EQ_MSG(IBS_MSR_OP_RIP,       0xC0011034U,
	    "IBS_MSR_OP_RIP = %#llx, expected %#llx",
	    (unsigned long long)(IBS_MSR_OP_RIP),
	    (unsigned long long)(0xC0011034U));
	ATF_CHECK_EQ_MSG(IBS_MSR_OP_DATA1,     0xC0011035U,
	    "IBS_MSR_OP_DATA1 = %#llx, expected %#llx",
	    (unsigned long long)(IBS_MSR_OP_DATA1),
	    (unsigned long long)(0xC0011035U));
	ATF_CHECK_EQ_MSG(IBS_MSR_OP_DATA2,     0xC0011036U,
	    "IBS_MSR_OP_DATA2 = %#llx, expected %#llx",
	    (unsigned long long)(IBS_MSR_OP_DATA2),
	    (unsigned long long)(0xC0011036U));
	ATF_CHECK_EQ_MSG(IBS_MSR_OP_DATA3,     0xC0011037U,
	    "IBS_MSR_OP_DATA3 = %#llx, expected %#llx",
	    (unsigned long long)(IBS_MSR_OP_DATA3),
	    (unsigned long long)(0xC0011037U));
	ATF_CHECK_EQ_MSG(IBS_MSR_DC_LINADDR,   0xC0011038U,
	    "IBS_MSR_DC_LINADDR = %#llx, expected %#llx",
	    (unsigned long long)(IBS_MSR_DC_LINADDR),
	    (unsigned long long)(0xC0011038U));
	ATF_CHECK_EQ_MSG(IBS_MSR_DC_PHYSADDR,  0xC0011039U,
	    "IBS_MSR_DC_PHYSADDR = %#llx, expected %#llx",
	    (unsigned long long)(IBS_MSR_DC_PHYSADDR),
	    (unsigned long long)(0xC0011039U));
	ATF_CHECK_EQ_MSG(IBS_MSR_IBSCTL,       0xC001103AU,
	    "IBS_MSR_IBSCTL = %#llx, expected %#llx",
	    (unsigned long long)(IBS_MSR_IBSCTL),
	    (unsigned long long)(0xC001103AU));
	ATF_CHECK_EQ_MSG(IBS_MSR_IBSBRTARGET,  0xC001103BU,
	    "IBS_MSR_IBSBRTARGET = %#llx, expected %#llx",
	    (unsigned long long)(IBS_MSR_IBSBRTARGET),
	    (unsigned long long)(0xC001103BU));
	ATF_CHECK_EQ_MSG(IBS_MSR_ICIBSEXTDCTL, 0xC001103CU,
	    "IBS_MSR_ICIBSEXTDCTL = %#llx, expected %#llx",
	    (unsigned long long)(IBS_MSR_ICIBSEXTDCTL),
	    (unsigned long long)(0xC001103CU));
	ATF_CHECK_EQ_MSG(IBS_MSR_IBSOPDATA4,   0xC001103DU,
	    "IBS_MSR_IBSOPDATA4 = %#llx, expected %#llx",
	    (unsigned long long)(IBS_MSR_IBSOPDATA4),
	    (unsigned long long)(0xC001103DU));
}

/* -----------------------------------------------------------------------
 * TC-UNIT-MASK-07: CPUID constants
 * ----------------------------------------------------------------------- */
ATF_TC_WITHOUT_HEAD(ibs_unit_cpuid_constants);
ATF_TC_BODY(ibs_unit_cpuid_constants, tc)
{
	ATF_CHECK_EQ_MSG(IBS_CPUID_IBSID, 0x8000001BU,
	    "IBS_CPUID_IBSID = %#llx, expected %#llx",
	    (unsigned long long)(IBS_CPUID_IBSID),
	    (unsigned long long)(0x8000001BU));
	/* CPUID 0x8000001B EAX feature bits */
	ATF_CHECK_EQ_MSG(IBS_CPUID_FETCH_SAMPLING, (1U << 0),
	    "IBS_CPUID_FETCH_SAMPLING = %#llx, expected %#llx",
	    (unsigned long long)(IBS_CPUID_FETCH_SAMPLING),
	    (unsigned long long)((1U << 0)));
	ATF_CHECK_EQ_MSG(IBS_CPUID_OP_SAMPLING,    (1U << 1),
	    "IBS_CPUID_OP_SAMPLING = %#llx, expected %#llx",
	    (unsigned long long)(IBS_CPUID_OP_SAMPLING),
	    (unsigned long long)((1U << 1)));
	ATF_CHECK_EQ_MSG(IBS_CPUID_RDWROPCNT,      (1U << 2),
	    "IBS_CPUID_RDWROPCNT = %#llx, expected %#llx",
	    (unsigned long long)(IBS_CPUID_RDWROPCNT),
	    (unsigned long long)((1U << 2)));
	ATF_CHECK_EQ_MSG(IBS_CPUID_OPCNT,          (1U << 3),
	    "IBS_CPUID_OPCNT = %#llx, expected %#llx",
	    (unsigned long long)(IBS_CPUID_OPCNT),
	    (unsigned long long)((1U << 3)));
	ATF_CHECK_EQ_MSG(IBS_CPUID_BRANCH_TARGET_ADDR, (1U << 4),
	    "IBS_CPUID_BRANCH_TARGET_ADDR = %#llx, expected %#llx",
	    (unsigned long long)(IBS_CPUID_BRANCH_TARGET_ADDR),
	    (unsigned long long)((1U << 4)));
	ATF_CHECK_EQ_MSG(IBS_CPUID_OP_DATA_4,      (1U << 5),
	    "IBS_CPUID_OP_DATA_4 = %#llx, expected %#llx",
	    (unsigned long long)(IBS_CPUID_OP_DATA_4),
	    (unsigned long long)((1U << 5)));
	ATF_CHECK_EQ_MSG(IBS_CPUID_ZEN4_IBS,       (1U << 6),
	    "IBS_CPUID_ZEN4_IBS = %#llx, expected %#llx",
	    (unsigned long long)(IBS_CPUID_ZEN4_IBS),
	    (unsigned long long)((1U << 6)));
}

/* -----------------------------------------------------------------------
 * TC-UNIT-MASK-08: No overlap among IBS_FETCH_* masks in IBSFETCHCTL
 * ----------------------------------------------------------------------- */
ATF_TC_WITHOUT_HEAD(ibs_unit_no_mask_overlap_fetch);
ATF_TC_BODY(ibs_unit_no_mask_overlap_fetch, tc)
{
	const uint64_t masks[] = {
		IBS_FETCH_MAXCNT,
		IBS_FETCH_CNT,
		IBS_FETCH_LAT,
		IBS_FETCH_EN,
		IBS_FETCH_VAL,
		IBS_FETCH_COMP,
		IBS_IC_MISS,
		IBS_PHY_ADDR_VALID,
		IBS_L1TLB_PGSZ,
		IBS_L1TLB_MISS,
		IBS_L2TLB_MISS,
		IBS_RAND_EN,
		IBS_FETCH_L2_MISS,
		IBS_L3_MISS_ONLY,
	};
	const int n = (int)(sizeof(masks) / sizeof(masks[0]));

	for (int i = 0; i < n; i++) {
		for (int j = i + 1; j < n; j++) {
			ATF_CHECK_MSG((masks[i] & masks[j]) == 0ULL,
			    "IBS_FETCH masks[%d] and masks[%d] overlap "
			    "(0x%016llx & 0x%016llx = 0x%016llx)",
			    i, j,
			    (unsigned long long)masks[i],
			    (unsigned long long)masks[j],
			    (unsigned long long)(masks[i] & masks[j]));
		}
	}
}

/* -----------------------------------------------------------------------
 * TC-UNIT-MASK-09: No overlap among IBS_OP_* control masks in IBSOPCTL
 * ----------------------------------------------------------------------- */
ATF_TC_WITHOUT_HEAD(ibs_unit_no_mask_overlap_op);
ATF_TC_BODY(ibs_unit_no_mask_overlap_op, tc)
{
	const uint64_t masks[] = {
		IBS_OP_MAXCNT,
		IBS_OP_L3_MISS_ONLY,
		IBS_OP_EN,
		IBS_OP_VAL,
		IBS_CNT_CTL,
		IBS_OP_MAXCNT_EXT,
		IBS_OP_CURCNT,
		IBS_LDLAT_THRSH,
		IBS_LDLAT_EN,
	};
	const int n = (int)(sizeof(masks) / sizeof(masks[0]));

	for (int i = 0; i < n; i++) {
		for (int j = i + 1; j < n; j++) {
			ATF_CHECK_MSG((masks[i] & masks[j]) == 0ULL,
			    "IBS_OP masks[%d] and masks[%d] overlap "
			    "(0x%016llx & 0x%016llx = 0x%016llx)",
			    i, j,
			    (unsigned long long)masks[i],
			    (unsigned long long)masks[j],
			    (unsigned long long)(masks[i] & masks[j]));
		}
	}
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, ibs_unit_fetch_ctl_mask_values);
	ATF_TP_ADD_TC(tp, ibs_unit_op_ctl_mask_values);
	ATF_TP_ADD_TC(tp, ibs_unit_op_data1_mask_values);
	ATF_TP_ADD_TC(tp, ibs_unit_op_data2_mask_values);
	ATF_TP_ADD_TC(tp, ibs_unit_op_data3_mask_values);
	ATF_TP_ADD_TC(tp, ibs_unit_msr_address_map);
	ATF_TP_ADD_TC(tp, ibs_unit_cpuid_constants);
	ATF_TP_ADD_TC(tp, ibs_unit_no_mask_overlap_fetch);
	ATF_TP_ADD_TC(tp, ibs_unit_no_mask_overlap_op);
	return (atf_no_error());
}
