/*-
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Author: ojanerif@amd.com
 *
 * [TC-UNIT-MSRRANGE] — IBS MSR address range unit tests.
 *
 * Verifies the layout of the IBS MSR address table from ibs_decode.h:
 *
 *   IBS_MSR_FETCH_CTL    0xC0011030   Fetch Control
 *   IBS_MSR_FETCH_LINADDR 0xC0011031  Fetch Linear Address
 *   IBS_MSR_FETCH_PHYSADDR 0xC0011032 Fetch Physical Address
 *   IBS_MSR_OP_CTL       0xC0011033  Op Control
 *   IBS_MSR_OP_RIP       0xC0011034  Op Instruction Pointer
 *   IBS_MSR_OP_DATA1     0xC0011035  Op Data 1
 *   IBS_MSR_OP_DATA2     0xC0011036  Op Data 2
 *   IBS_MSR_OP_DATA3     0xC0011037  Op Data 3
 *   IBS_MSR_DC_LINADDR   0xC0011038  DC Linear Address
 *   IBS_MSR_DC_PHYSADDR  0xC0011039  DC Physical Address
 *   IBS_MSR_IBSCTL       0xC001103A  IBS Control (global)
 *   IBS_MSR_IBSBRTARGET  0xC001103B  Branch Target Address
 *   IBS_MSR_ICIBSEXTDCTL 0xC001103C  IC IBS Extended Control
 *   IBS_MSR_IBSOPDATA4   0xC001103D  IBS Op Data 4 (Zen 4+)
 *
 * No hardware access.  Runs on any architecture.
 *
 * Test IDs: TC-UNIT-MSRRANGE-01 … TC-UNIT-MSRRANGE-14
 */

#include <atf-c.h>

#include "ibs_decode.h"

/* TC-UNIT-MSRRANGE-01: fetch range starts at 0xC0011030 */
ATF_TC_WITHOUT_HEAD(ibs_unit_msr_fetch_ctl_base);
ATF_TC_BODY(ibs_unit_msr_fetch_ctl_base, tc)
{
	ATF_CHECK_EQ(IBS_MSR_FETCH_CTL, 0xC0011030U);
}

/* TC-UNIT-MSRRANGE-02: Op CTL is 3 addresses after Fetch CTL */
ATF_TC_WITHOUT_HEAD(ibs_unit_msr_op_ctl_offset);
ATF_TC_BODY(ibs_unit_msr_op_ctl_offset, tc)
{
	ATF_CHECK_EQ(IBS_MSR_OP_CTL, IBS_MSR_FETCH_CTL + 3U);
}

/* TC-UNIT-MSRRANGE-03: Op Data 1 is 5 addresses after Fetch CTL */
ATF_TC_WITHOUT_HEAD(ibs_unit_msr_op_data1_offset);
ATF_TC_BODY(ibs_unit_msr_op_data1_offset, tc)
{
	ATF_CHECK_EQ(IBS_MSR_OP_DATA1, IBS_MSR_FETCH_CTL + 5U);
}

/* TC-UNIT-MSRRANGE-04: DC Physical Address is 9 addresses after Fetch CTL */
ATF_TC_WITHOUT_HEAD(ibs_unit_msr_dc_physaddr_offset);
ATF_TC_BODY(ibs_unit_msr_dc_physaddr_offset, tc)
{
	ATF_CHECK_EQ(IBS_MSR_DC_PHYSADDR, IBS_MSR_FETCH_CTL + 9U);
}

/* TC-UNIT-MSRRANGE-05: IBSCTL is 10 addresses after Fetch CTL */
ATF_TC_WITHOUT_HEAD(ibs_unit_msr_ibsctl_offset);
ATF_TC_BODY(ibs_unit_msr_ibsctl_offset, tc)
{
	ATF_CHECK_EQ(IBS_MSR_IBSCTL, IBS_MSR_FETCH_CTL + 10U);
}

/* TC-UNIT-MSRRANGE-06: IBSBRTARGET is 11 addresses after Fetch CTL */
ATF_TC_WITHOUT_HEAD(ibs_unit_msr_ibsbrtarget_offset);
ATF_TC_BODY(ibs_unit_msr_ibsbrtarget_offset, tc)
{
	ATF_CHECK_EQ(IBS_MSR_IBSBRTARGET, IBS_MSR_FETCH_CTL + 11U);
}

/* TC-UNIT-MSRRANGE-07: ICIBSEXTDCTL is 12 addresses after Fetch CTL */
ATF_TC_WITHOUT_HEAD(ibs_unit_msr_icibsextdctl_offset);
ATF_TC_BODY(ibs_unit_msr_icibsextdctl_offset, tc)
{
	ATF_CHECK_EQ(IBS_MSR_ICIBSEXTDCTL, IBS_MSR_FETCH_CTL + 12U);
}

/* TC-UNIT-MSRRANGE-08: IBSOPDATA4 is 13 addresses after Fetch CTL */
ATF_TC_WITHOUT_HEAD(ibs_unit_msr_ibsopdata4_offset);
ATF_TC_BODY(ibs_unit_msr_ibsopdata4_offset, tc)
{
	ATF_CHECK_EQ(IBS_MSR_IBSOPDATA4, IBS_MSR_FETCH_CTL + 13U);
}

/* TC-UNIT-MSRRANGE-09: total IBS MSR span is 14 registers (inclusive) */
ATF_TC_WITHOUT_HEAD(ibs_unit_msr_total_span);
ATF_TC_BODY(ibs_unit_msr_total_span, tc)
{
	ATF_CHECK_EQ(IBS_MSR_IBSOPDATA4 - IBS_MSR_FETCH_CTL + 1U, 14U);
}

/* TC-UNIT-MSRRANGE-10: Fetch group has exactly 3 MSRs (CTL, LINADDR, PHYSADDR) */
ATF_TC_WITHOUT_HEAD(ibs_unit_msr_fetch_group_size);
ATF_TC_BODY(ibs_unit_msr_fetch_group_size, tc)
{
	ATF_CHECK_EQ(IBS_MSR_FETCH_PHYSADDR - IBS_MSR_FETCH_CTL + 1U, 3U);
}

/* TC-UNIT-MSRRANGE-11: Op Data MSRs (DATA1–DATA3) are 3 consecutive addresses */
ATF_TC_WITHOUT_HEAD(ibs_unit_msr_op_data_consecutive);
ATF_TC_BODY(ibs_unit_msr_op_data_consecutive, tc)
{
	ATF_CHECK_EQ(IBS_MSR_OP_DATA2, IBS_MSR_OP_DATA1 + 1U);
	ATF_CHECK_EQ(IBS_MSR_OP_DATA3, IBS_MSR_OP_DATA1 + 2U);
}

/* TC-UNIT-MSRRANGE-12: DC address pair is consecutive */
ATF_TC_WITHOUT_HEAD(ibs_unit_msr_dc_addr_consecutive);
ATF_TC_BODY(ibs_unit_msr_dc_addr_consecutive, tc)
{
	ATF_CHECK_EQ(IBS_MSR_DC_PHYSADDR, IBS_MSR_DC_LINADDR + 1U);
}

/* TC-UNIT-MSRRANGE-13: all IBS MSR addresses are in the 0xC0011030-0x3D range */
ATF_TC_WITHOUT_HEAD(ibs_unit_msr_all_in_range);
ATF_TC_BODY(ibs_unit_msr_all_in_range, tc)
{
	const uint32_t addrs[] = {
		IBS_MSR_FETCH_CTL, IBS_MSR_FETCH_LINADDR, IBS_MSR_FETCH_PHYSADDR,
		IBS_MSR_OP_CTL, IBS_MSR_OP_RIP,
		IBS_MSR_OP_DATA1, IBS_MSR_OP_DATA2, IBS_MSR_OP_DATA3,
		IBS_MSR_DC_LINADDR, IBS_MSR_DC_PHYSADDR,
		IBS_MSR_IBSCTL, IBS_MSR_IBSBRTARGET,
		IBS_MSR_ICIBSEXTDCTL, IBS_MSR_IBSOPDATA4,
	};
	const int n = (int)(sizeof(addrs) / sizeof(addrs[0]));
	int i;

	for (i = 0; i < n; i++) {
		ATF_CHECK_MSG(addrs[i] >= IBS_MSR_FETCH_CTL &&
		    addrs[i] <= IBS_MSR_IBSOPDATA4,
		    "MSR address 0x%08x is outside the expected IBS range",
		    addrs[i]);
	}
}

/* TC-UNIT-MSRRANGE-14: all IBS MSR addresses are unique */
ATF_TC_WITHOUT_HEAD(ibs_unit_msr_all_unique);
ATF_TC_BODY(ibs_unit_msr_all_unique, tc)
{
	const uint32_t addrs[] = {
		IBS_MSR_FETCH_CTL, IBS_MSR_FETCH_LINADDR, IBS_MSR_FETCH_PHYSADDR,
		IBS_MSR_OP_CTL, IBS_MSR_OP_RIP,
		IBS_MSR_OP_DATA1, IBS_MSR_OP_DATA2, IBS_MSR_OP_DATA3,
		IBS_MSR_DC_LINADDR, IBS_MSR_DC_PHYSADDR,
		IBS_MSR_IBSCTL, IBS_MSR_IBSBRTARGET,
		IBS_MSR_ICIBSEXTDCTL, IBS_MSR_IBSOPDATA4,
	};
	const int n = (int)(sizeof(addrs) / sizeof(addrs[0]));
	int i, j;

	for (i = 0; i < n; i++) {
		for (j = i + 1; j < n; j++) {
			ATF_CHECK_MSG(addrs[i] != addrs[j],
			    "duplicate MSR address 0x%08x at positions %d and %d",
			    addrs[i], i, j);
		}
	}
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, ibs_unit_msr_fetch_ctl_base);
	ATF_TP_ADD_TC(tp, ibs_unit_msr_op_ctl_offset);
	ATF_TP_ADD_TC(tp, ibs_unit_msr_op_data1_offset);
	ATF_TP_ADD_TC(tp, ibs_unit_msr_dc_physaddr_offset);
	ATF_TP_ADD_TC(tp, ibs_unit_msr_ibsctl_offset);
	ATF_TP_ADD_TC(tp, ibs_unit_msr_ibsbrtarget_offset);
	ATF_TP_ADD_TC(tp, ibs_unit_msr_icibsextdctl_offset);
	ATF_TP_ADD_TC(tp, ibs_unit_msr_ibsopdata4_offset);
	ATF_TP_ADD_TC(tp, ibs_unit_msr_total_span);
	ATF_TP_ADD_TC(tp, ibs_unit_msr_fetch_group_size);
	ATF_TP_ADD_TC(tp, ibs_unit_msr_op_data_consecutive);
	ATF_TP_ADD_TC(tp, ibs_unit_msr_dc_addr_consecutive);
	ATF_TP_ADD_TC(tp, ibs_unit_msr_all_in_range);
	ATF_TP_ADD_TC(tp, ibs_unit_msr_all_unique);
	return (atf_no_error());
}
