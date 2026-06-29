/*-
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Author: ojanerif@amd.com
 *
 * [TC-UNIT-OPDATA] — IBS Op Data 1–3 field unit tests.
 *
 * Tests the counter and status fields in the IBS Op Data MSRs:
 *
 *   Op Data 1 (0xC0011035):
 *     IBS_COMP_TO_RET_CTR  bits 15: 0
 *     IBS_TAG_TO_RET_CTR   bits 31:16
 *     IBS_OP_RETURN        bit  34
 *     IBS_OP_BRN_TAKEN     bit  35
 *     IBS_OP_BRN_MISP      bit  36
 *     IBS_OP_BRN_RET       bit  37
 *
 *   Op Data 2 (0xC0011036):
 *     IBS_DATA_SRC_LO      bits  2:0
 *     IBS_RMT_NODE         bit   4
 *     IBS_CACHE_HIT_ST     bit   5
 *     IBS_DATA_SRC_HI      bits  7:6
 *
 *   Op Data 3 (0xC0011037):
 *     IBS_LD_OP            bit   0
 *     IBS_ST_OP            bit   1
 *     IBS_DC_MISS          bit   7
 *     IBS_DC_LIN_ADDR_VALID bit  17
 *     IBS_DC_PHY_ADDR_VALID bit  18
 *     IBS_DC_MISS_LAT      bits 47:32
 *
 * No hardware access.  Runs on any architecture.
 *
 * Test IDs: TC-UNIT-OPDATA-01 … TC-UNIT-OPDATA-15
 */

#include <atf-c.h>

#include "ibs_decode.h"

/* TC-UNIT-OPDATA-01: IBS_COMP_TO_RET_CTR covers bits 15:0 */
ATF_TC_WITHOUT_HEAD(ibs_unit_comp_to_ret_mask);
ATF_TC_BODY(ibs_unit_comp_to_ret_mask, tc)
{
	ATF_CHECK_EQ_MSG(IBS_COMP_TO_RET_CTR, 0x000000000000ffffULL,
	    "IBS_COMP_TO_RET_CTR mask: got %#llx, expected %#llx",
	    (unsigned long long)(IBS_COMP_TO_RET_CTR),
	    (unsigned long long)(0x000000000000ffffULL));
}

/* TC-UNIT-OPDATA-02: IBS_TAG_TO_RET_CTR covers bits 31:16 */
ATF_TC_WITHOUT_HEAD(ibs_unit_tag_to_ret_mask);
ATF_TC_BODY(ibs_unit_tag_to_ret_mask, tc)
{
	ATF_CHECK_EQ_MSG(IBS_TAG_TO_RET_CTR, 0x00000000ffff0000ULL,
	    "IBS_TAG_TO_RET_CTR mask: got %#llx, expected %#llx",
	    (unsigned long long)(IBS_TAG_TO_RET_CTR),
	    (unsigned long long)(0x00000000ffff0000ULL));
}

/* TC-UNIT-OPDATA-03: COMP and TAG counter fields do not overlap */
ATF_TC_WITHOUT_HEAD(ibs_unit_comp_tag_no_overlap);
ATF_TC_BODY(ibs_unit_comp_tag_no_overlap, tc)
{
	ATF_CHECK_EQ_MSG((IBS_COMP_TO_RET_CTR & IBS_TAG_TO_RET_CTR), 0ULL,
	    "COMP/TAG counter overlap: got %#llx, expected %#llx",
	    (unsigned long long)((IBS_COMP_TO_RET_CTR & IBS_TAG_TO_RET_CTR)),
	    (unsigned long long)(0ULL));
}

/* TC-UNIT-OPDATA-04: extract COMP counter from a composite Op Data 1 value */
ATF_TC_WITHOUT_HEAD(ibs_unit_comp_extraction);
ATF_TC_BODY(ibs_unit_comp_extraction, tc)
{
	/* COMP=0xBEEF, TAG=0xDEAD packed into bits 31:0 */
	uint64_t opdata1 = (0xDEADULL << 16) | 0xBEEFULL;
	uint64_t comp = opdata1 & IBS_COMP_TO_RET_CTR;
	uint64_t tag  = (opdata1 & IBS_TAG_TO_RET_CTR) >> 16;

	ATF_CHECK_EQ_MSG(comp, 0xBEEFULL,
	    "COMP counter extraction: got %#llx, expected %#llx",
	    (unsigned long long)(comp), (unsigned long long)(0xBEEFULL));
	ATF_CHECK_EQ_MSG(tag, 0xDEADULL,
	    "TAG counter extraction: got %#llx, expected %#llx",
	    (unsigned long long)(tag), (unsigned long long)(0xDEADULL));
}

/* TC-UNIT-OPDATA-05: IBS_OP_RETURN is bit 34 */
ATF_TC_WITHOUT_HEAD(ibs_unit_op_return_bit34);
ATF_TC_BODY(ibs_unit_op_return_bit34, tc)
{
	ATF_CHECK_EQ_MSG(IBS_OP_RETURN, (1ULL << 34),
	    "IBS_OP_RETURN bit34: got %#llx, expected %#llx",
	    (unsigned long long)(IBS_OP_RETURN),
	    (unsigned long long)((1ULL << 34)));
}

/* TC-UNIT-OPDATA-06: IBS_OP_BRN_TAKEN is bit 35 */
ATF_TC_WITHOUT_HEAD(ibs_unit_op_brn_taken_bit35);
ATF_TC_BODY(ibs_unit_op_brn_taken_bit35, tc)
{
	ATF_CHECK_EQ_MSG(IBS_OP_BRN_TAKEN, (1ULL << 35),
	    "IBS_OP_BRN_TAKEN bit35: got %#llx, expected %#llx",
	    (unsigned long long)(IBS_OP_BRN_TAKEN),
	    (unsigned long long)((1ULL << 35)));
}

/* TC-UNIT-OPDATA-07: IBS_OP_BRN_MISP is bit 36 */
ATF_TC_WITHOUT_HEAD(ibs_unit_op_brn_misp_bit36);
ATF_TC_BODY(ibs_unit_op_brn_misp_bit36, tc)
{
	ATF_CHECK_EQ_MSG(IBS_OP_BRN_MISP, (1ULL << 36),
	    "IBS_OP_BRN_MISP bit36: got %#llx, expected %#llx",
	    (unsigned long long)(IBS_OP_BRN_MISP),
	    (unsigned long long)((1ULL << 36)));
}

/* TC-UNIT-OPDATA-08: IBS_OP_BRN_RET is bit 37 */
ATF_TC_WITHOUT_HEAD(ibs_unit_op_brn_ret_bit37);
ATF_TC_BODY(ibs_unit_op_brn_ret_bit37, tc)
{
	ATF_CHECK_EQ_MSG(IBS_OP_BRN_RET, (1ULL << 37),
	    "IBS_OP_BRN_RET bit37: got %#llx, expected %#llx",
	    (unsigned long long)(IBS_OP_BRN_RET),
	    (unsigned long long)((1ULL << 37)));
}

/* TC-UNIT-OPDATA-09: branch status bits 34-37 are consecutive and independent */
ATF_TC_WITHOUT_HEAD(ibs_unit_brn_bits_consecutive);
ATF_TC_BODY(ibs_unit_brn_bits_consecutive, tc)
{
	/* Each bit must be a distinct power of two */
	ATF_CHECK_EQ_MSG(IBS_OP_BRN_TAKEN, IBS_OP_RETURN << 1,
	    "IBS_OP_BRN_TAKEN consecutive: got %#llx, expected %#llx",
	    (unsigned long long)(IBS_OP_BRN_TAKEN),
	    (unsigned long long)(IBS_OP_RETURN << 1));
	ATF_CHECK_EQ_MSG(IBS_OP_BRN_MISP,  IBS_OP_RETURN << 2,
	    "IBS_OP_BRN_MISP consecutive: got %#llx, expected %#llx",
	    (unsigned long long)(IBS_OP_BRN_MISP),
	    (unsigned long long)(IBS_OP_RETURN << 2));
	ATF_CHECK_EQ_MSG(IBS_OP_BRN_RET,   IBS_OP_RETURN << 3,
	    "IBS_OP_BRN_RET consecutive: got %#llx, expected %#llx",
	    (unsigned long long)(IBS_OP_BRN_RET),
	    (unsigned long long)(IBS_OP_RETURN << 3));
}

/* TC-UNIT-OPDATA-10: IBS_LD_OP is bit 0, IBS_ST_OP is bit 1 */
ATF_TC_WITHOUT_HEAD(ibs_unit_ld_st_op_bits);
ATF_TC_BODY(ibs_unit_ld_st_op_bits, tc)
{
	ATF_CHECK_EQ_MSG(IBS_LD_OP, (1ULL << 0),
	    "IBS_LD_OP bit0: got %#llx, expected %#llx",
	    (unsigned long long)(IBS_LD_OP),
	    (unsigned long long)((1ULL << 0)));
	ATF_CHECK_EQ_MSG(IBS_ST_OP, (1ULL << 1),
	    "IBS_ST_OP bit1: got %#llx, expected %#llx",
	    (unsigned long long)(IBS_ST_OP),
	    (unsigned long long)((1ULL << 1)));
}

/* TC-UNIT-OPDATA-11: IBS_DC_MISS is bit 7 */
ATF_TC_WITHOUT_HEAD(ibs_unit_dc_miss_bit7);
ATF_TC_BODY(ibs_unit_dc_miss_bit7, tc)
{
	ATF_CHECK_EQ_MSG(IBS_DC_MISS, (1ULL << 7),
	    "IBS_DC_MISS bit7: got %#llx, expected %#llx",
	    (unsigned long long)(IBS_DC_MISS),
	    (unsigned long long)((1ULL << 7)));
}

/* TC-UNIT-OPDATA-12: IBS_DC_LIN_ADDR_VALID is bit 17, PHY is bit 18 */
ATF_TC_WITHOUT_HEAD(ibs_unit_dc_addr_valid_bits);
ATF_TC_BODY(ibs_unit_dc_addr_valid_bits, tc)
{
	ATF_CHECK_EQ_MSG(IBS_DC_LIN_ADDR_VALID, (1ULL << 17),
	    "IBS_DC_LIN_ADDR_VALID bit17: got %#llx, expected %#llx",
	    (unsigned long long)(IBS_DC_LIN_ADDR_VALID),
	    (unsigned long long)((1ULL << 17)));
	ATF_CHECK_EQ_MSG(IBS_DC_PHY_ADDR_VALID, (1ULL << 18),
	    "IBS_DC_PHY_ADDR_VALID bit18: got %#llx, expected %#llx",
	    (unsigned long long)(IBS_DC_PHY_ADDR_VALID),
	    (unsigned long long)((1ULL << 18)));
}

/* TC-UNIT-OPDATA-13: IBS_DC_MISS_LAT covers bits 47:32 of Op Data 3 */
ATF_TC_WITHOUT_HEAD(ibs_unit_dc_miss_lat_mask);
ATF_TC_BODY(ibs_unit_dc_miss_lat_mask, tc)
{
	ATF_CHECK_EQ_MSG(IBS_DC_MISS_LAT, 0x0000ffff00000000ULL,
	    "IBS_DC_MISS_LAT mask: got %#llx, expected %#llx",
	    (unsigned long long)(IBS_DC_MISS_LAT),
	    (unsigned long long)(0x0000ffff00000000ULL));
}

/* TC-UNIT-OPDATA-14: extract DC miss latency from a packed Op Data 3 value */
ATF_TC_WITHOUT_HEAD(ibs_unit_dc_miss_lat_extraction);
ATF_TC_BODY(ibs_unit_dc_miss_lat_extraction, tc)
{
	/* Pack latency=0x1234 into bits 47:32 alongside LD_OP and DC_MISS */
	uint64_t opdata3 = (0x1234ULL << 32) | IBS_LD_OP | IBS_DC_MISS;
	uint64_t lat = (opdata3 & IBS_DC_MISS_LAT) >> 32;

	ATF_CHECK_EQ_MSG(lat, 0x1234ULL,
	    "DC miss latency extraction: got %#llx, expected %#llx",
	    (unsigned long long)(lat), (unsigned long long)(0x1234ULL));
	/* Verify LD_OP and DC_MISS are still readable */
	ATF_CHECK_MSG((opdata3 & IBS_LD_OP) != 0ULL, "LD_OP lost");
	ATF_CHECK_MSG((opdata3 & IBS_DC_MISS) != 0ULL, "DC_MISS lost");
}

/* TC-UNIT-OPDATA-15: RMT_NODE is bit 4, CACHE_HIT_ST is bit 5 in Op Data 2 */
ATF_TC_WITHOUT_HEAD(ibs_unit_op_data2_bits);
ATF_TC_BODY(ibs_unit_op_data2_bits, tc)
{
	ATF_CHECK_EQ_MSG(IBS_RMT_NODE,     (1ULL << 4),
	    "IBS_RMT_NODE bit4: got %#llx, expected %#llx",
	    (unsigned long long)(IBS_RMT_NODE),
	    (unsigned long long)((1ULL << 4)));
	ATF_CHECK_EQ_MSG(IBS_CACHE_HIT_ST, (1ULL << 5),
	    "IBS_CACHE_HIT_ST bit5: got %#llx, expected %#llx",
	    (unsigned long long)(IBS_CACHE_HIT_ST),
	    (unsigned long long)((1ULL << 5)));
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, ibs_unit_comp_to_ret_mask);
	ATF_TP_ADD_TC(tp, ibs_unit_tag_to_ret_mask);
	ATF_TP_ADD_TC(tp, ibs_unit_comp_tag_no_overlap);
	ATF_TP_ADD_TC(tp, ibs_unit_comp_extraction);
	ATF_TP_ADD_TC(tp, ibs_unit_op_return_bit34);
	ATF_TP_ADD_TC(tp, ibs_unit_op_brn_taken_bit35);
	ATF_TP_ADD_TC(tp, ibs_unit_op_brn_misp_bit36);
	ATF_TP_ADD_TC(tp, ibs_unit_op_brn_ret_bit37);
	ATF_TP_ADD_TC(tp, ibs_unit_brn_bits_consecutive);
	ATF_TP_ADD_TC(tp, ibs_unit_ld_st_op_bits);
	ATF_TP_ADD_TC(tp, ibs_unit_dc_miss_bit7);
	ATF_TP_ADD_TC(tp, ibs_unit_dc_addr_valid_bits);
	ATF_TP_ADD_TC(tp, ibs_unit_dc_miss_lat_mask);
	ATF_TP_ADD_TC(tp, ibs_unit_dc_miss_lat_extraction);
	ATF_TP_ADD_TC(tp, ibs_unit_op_data2_bits);
	return (atf_no_error());
}
