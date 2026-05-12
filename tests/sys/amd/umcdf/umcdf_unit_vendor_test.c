/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * Author: ojanerif@amd.com
 *
 * [TC-UNIT-UVEND] — AMD vendor identification constant unit tests.
 *
 * Tests the AMD vendor string constants and CPUID leaf addresses in
 * amd_umcdf_decode.h.
 *
 * CPUID leaf 0 returns the vendor string in EBX:EDX:ECX:
 *   "Auth" (EBX=0x68747541), "enti" (EDX=0x69746e65), "cAMD" (ECX=0x444d4163)
 *   → concatenated: "AuthenticAMD"
 *
 * No hardware access.  Runs on any architecture.
 *
 * Test IDs: TC-UNIT-UVEND-01 … TC-UNIT-UVEND-10
 */

#include <string.h>

#include <atf-c.h>

#include "amd_umcdf_decode.h"

/* TC-UNIT-UVEND-01: VENDOR_EBX spells "Auth" in little-endian */
ATF_TC_WITHOUT_HEAD(umcdf_unit_vendor_ebx_auth);
ATF_TC_BODY(umcdf_unit_vendor_ebx_auth, tc)
{
	ATF_CHECK_EQ(AMD_UMCDF_VENDOR_EBX, 0x68747541U);
}

/* TC-UNIT-UVEND-02: VENDOR_EDX spells "enti" in little-endian */
ATF_TC_WITHOUT_HEAD(umcdf_unit_vendor_edx_enti);
ATF_TC_BODY(umcdf_unit_vendor_edx_enti, tc)
{
	ATF_CHECK_EQ(AMD_UMCDF_VENDOR_EDX, 0x69746e65U);
}

/* TC-UNIT-UVEND-03: VENDOR_ECX spells "cAMD" in little-endian */
ATF_TC_WITHOUT_HEAD(umcdf_unit_vendor_ecx_camd);
ATF_TC_BODY(umcdf_unit_vendor_ecx_camd, tc)
{
	ATF_CHECK_EQ(AMD_UMCDF_VENDOR_ECX, 0x444d4163U);
}

/* TC-UNIT-UVEND-04: EBX+EDX+ECX reconstruct "AuthenticAMD" */
ATF_TC_WITHOUT_HEAD(umcdf_unit_vendor_string_reconstruct);
ATF_TC_BODY(umcdf_unit_vendor_string_reconstruct, tc)
{
	char buf[13];
	uint32_t ebx = AMD_UMCDF_VENDOR_EBX;
	uint32_t edx = AMD_UMCDF_VENDOR_EDX;
	uint32_t ecx = AMD_UMCDF_VENDOR_ECX;

	memcpy(buf + 0, &ebx, 4);
	memcpy(buf + 4, &edx, 4);
	memcpy(buf + 8, &ecx, 4);
	buf[12] = '\0';

	ATF_CHECK_MSG(strcmp(buf, "AuthenticAMD") == 0,
	    "reconstructed vendor string '%s' != 'AuthenticAMD'", buf);
}

/* TC-UNIT-UVEND-05: AMD_UMCDF_CPUID_VENDOR is 0 */
ATF_TC_WITHOUT_HEAD(umcdf_unit_vendor_cpuid_leaf0);
ATF_TC_BODY(umcdf_unit_vendor_cpuid_leaf0, tc)
{
	ATF_CHECK_EQ(AMD_UMCDF_CPUID_VENDOR, 0U);
}

/* TC-UNIT-UVEND-06: AMD_UMCDF_CPUID_FMS is 1 */
ATF_TC_WITHOUT_HEAD(umcdf_unit_vendor_cpuid_leaf1);
ATF_TC_BODY(umcdf_unit_vendor_cpuid_leaf1, tc)
{
	ATF_CHECK_EQ(AMD_UMCDF_CPUID_FMS, 1U);
}

/* TC-UNIT-UVEND-07: AMD_UMCDF_CPUID_EXTMAX is 0x80000000 */
ATF_TC_WITHOUT_HEAD(umcdf_unit_vendor_cpuid_extmax);
ATF_TC_BODY(umcdf_unit_vendor_cpuid_extmax, tc)
{
	ATF_CHECK_EQ(AMD_UMCDF_CPUID_EXTMAX, 0x80000000U);
}

/* TC-UNIT-UVEND-08: AMD_UMCDF_CPUID_EXTFEATURES is 0x80000001 */
ATF_TC_WITHOUT_HEAD(umcdf_unit_vendor_cpuid_extfeatures);
ATF_TC_BODY(umcdf_unit_vendor_cpuid_extfeatures, tc)
{
	ATF_CHECK_EQ(AMD_UMCDF_CPUID_EXTFEATURES, 0x80000001U);
}

/* TC-UNIT-UVEND-09: AMD_UMCDF_CPUID_EXTPERFMON is 0x80000022 */
ATF_TC_WITHOUT_HEAD(umcdf_unit_vendor_cpuid_extperfmon);
ATF_TC_BODY(umcdf_unit_vendor_cpuid_extperfmon, tc)
{
	ATF_CHECK_EQ(AMD_UMCDF_CPUID_EXTPERFMON, 0x80000022U);
}

/* TC-UNIT-UVEND-10: AMD_UMCDF_ID2_PNXC is the PerfNxtCore bit (bit 24 of ECX) */
ATF_TC_WITHOUT_HEAD(umcdf_unit_vendor_pnxc_position);
ATF_TC_BODY(umcdf_unit_vendor_pnxc_position, tc)
{
	ATF_CHECK_EQ(AMD_UMCDF_ID2_PNXC, (1U << 24));
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, umcdf_unit_vendor_ebx_auth);
	ATF_TP_ADD_TC(tp, umcdf_unit_vendor_edx_enti);
	ATF_TP_ADD_TC(tp, umcdf_unit_vendor_ecx_camd);
	ATF_TP_ADD_TC(tp, umcdf_unit_vendor_string_reconstruct);
	ATF_TP_ADD_TC(tp, umcdf_unit_vendor_cpuid_leaf0);
	ATF_TP_ADD_TC(tp, umcdf_unit_vendor_cpuid_leaf1);
	ATF_TP_ADD_TC(tp, umcdf_unit_vendor_cpuid_extmax);
	ATF_TP_ADD_TC(tp, umcdf_unit_vendor_cpuid_extfeatures);
	ATF_TP_ADD_TC(tp, umcdf_unit_vendor_cpuid_extperfmon);
	ATF_TP_ADD_TC(tp, umcdf_unit_vendor_pnxc_position);
	return (atf_no_error());
}
