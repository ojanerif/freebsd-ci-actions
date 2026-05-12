/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * Author: ojanerif@amd.com
 *
 * [TC-UNIT-ZNAME] — amd_umcdf_zen_name() unit tests.
 *
 * Tests the pure amd_umcdf_zen_name() function from amd_umcdf_decode.h.
 * Verifies that each enum value returns a non-NULL, non-empty string,
 * and that well-known generation names contain their expected version number.
 *
 * No hardware access.  Runs on any architecture.
 *
 * Test IDs: TC-UNIT-ZNAME-01 … TC-UNIT-ZNAME-11
 */

#include <string.h>

#include <atf-c.h>

#include "amd_umcdf_decode.h"

/* TC-UNIT-ZNAME-01: PRE_ZEN returns non-NULL, non-empty string */
ATF_TC_WITHOUT_HEAD(umcdf_unit_zname_pre_zen);
ATF_TC_BODY(umcdf_unit_zname_pre_zen, tc)
{
	const char *s = amd_umcdf_zen_name(AMD_UMCDF_ZEN_PRE_ZEN);

	ATF_REQUIRE_MSG(s != NULL, "zen_name(PRE_ZEN) returned NULL");
	ATF_CHECK_MSG(strlen(s) > 0, "zen_name(PRE_ZEN) returned empty string");
}

/* TC-UNIT-ZNAME-02: ZEN_1 name contains "1" */
ATF_TC_WITHOUT_HEAD(umcdf_unit_zname_zen1);
ATF_TC_BODY(umcdf_unit_zname_zen1, tc)
{
	const char *s = amd_umcdf_zen_name(AMD_UMCDF_ZEN_1);

	ATF_REQUIRE_MSG(s != NULL, "zen_name(ZEN_1) returned NULL");
	ATF_CHECK_MSG(strchr(s, '1') != NULL,
	    "zen_name(ZEN_1) = '%s' does not contain '1'", s);
}

/* TC-UNIT-ZNAME-03: ZEN_PLUS returns non-NULL, non-empty string */
ATF_TC_WITHOUT_HEAD(umcdf_unit_zname_zen_plus);
ATF_TC_BODY(umcdf_unit_zname_zen_plus, tc)
{
	const char *s = amd_umcdf_zen_name(AMD_UMCDF_ZEN_PLUS);

	ATF_REQUIRE_MSG(s != NULL, "zen_name(ZEN_PLUS) returned NULL");
	ATF_CHECK_MSG(strlen(s) > 0, "zen_name(ZEN_PLUS) returned empty string");
}

/* TC-UNIT-ZNAME-04: ZEN_2 name contains "2" */
ATF_TC_WITHOUT_HEAD(umcdf_unit_zname_zen2);
ATF_TC_BODY(umcdf_unit_zname_zen2, tc)
{
	const char *s = amd_umcdf_zen_name(AMD_UMCDF_ZEN_2);

	ATF_REQUIRE_MSG(s != NULL, "zen_name(ZEN_2) returned NULL");
	ATF_CHECK_MSG(strchr(s, '2') != NULL,
	    "zen_name(ZEN_2) = '%s' does not contain '2'", s);
}

/* TC-UNIT-ZNAME-05: ZEN_3 name contains "3" */
ATF_TC_WITHOUT_HEAD(umcdf_unit_zname_zen3);
ATF_TC_BODY(umcdf_unit_zname_zen3, tc)
{
	const char *s = amd_umcdf_zen_name(AMD_UMCDF_ZEN_3);

	ATF_REQUIRE_MSG(s != NULL, "zen_name(ZEN_3) returned NULL");
	ATF_CHECK_MSG(strchr(s, '3') != NULL,
	    "zen_name(ZEN_3) = '%s' does not contain '3'", s);
}

/* TC-UNIT-ZNAME-06: ZEN_3_PLUS returns non-NULL, non-empty string */
ATF_TC_WITHOUT_HEAD(umcdf_unit_zname_zen3plus);
ATF_TC_BODY(umcdf_unit_zname_zen3plus, tc)
{
	const char *s = amd_umcdf_zen_name(AMD_UMCDF_ZEN_3_PLUS);

	ATF_REQUIRE_MSG(s != NULL, "zen_name(ZEN_3_PLUS) returned NULL");
	ATF_CHECK_MSG(strlen(s) > 0, "zen_name(ZEN_3_PLUS) returned empty string");
}

/* TC-UNIT-ZNAME-07: ZEN_4 name contains "4" */
ATF_TC_WITHOUT_HEAD(umcdf_unit_zname_zen4);
ATF_TC_BODY(umcdf_unit_zname_zen4, tc)
{
	const char *s = amd_umcdf_zen_name(AMD_UMCDF_ZEN_4);

	ATF_REQUIRE_MSG(s != NULL, "zen_name(ZEN_4) returned NULL");
	ATF_CHECK_MSG(strchr(s, '4') != NULL,
	    "zen_name(ZEN_4) = '%s' does not contain '4'", s);
}

/* TC-UNIT-ZNAME-08: ZEN_5 name contains "5" */
ATF_TC_WITHOUT_HEAD(umcdf_unit_zname_zen5);
ATF_TC_BODY(umcdf_unit_zname_zen5, tc)
{
	const char *s = amd_umcdf_zen_name(AMD_UMCDF_ZEN_5);

	ATF_REQUIRE_MSG(s != NULL, "zen_name(ZEN_5) returned NULL");
	ATF_CHECK_MSG(strchr(s, '5') != NULL,
	    "zen_name(ZEN_5) = '%s' does not contain '5'", s);
}

/* TC-UNIT-ZNAME-09: ZEN_6 name contains "6" */
ATF_TC_WITHOUT_HEAD(umcdf_unit_zname_zen6);
ATF_TC_BODY(umcdf_unit_zname_zen6, tc)
{
	const char *s = amd_umcdf_zen_name(AMD_UMCDF_ZEN_6);

	ATF_REQUIRE_MSG(s != NULL, "zen_name(ZEN_6) returned NULL");
	ATF_CHECK_MSG(strchr(s, '6') != NULL,
	    "zen_name(ZEN_6) = '%s' does not contain '6'", s);
}

/* TC-UNIT-ZNAME-10: ZEN_FUTURE returns non-NULL, non-empty string */
ATF_TC_WITHOUT_HEAD(umcdf_unit_zname_future);
ATF_TC_BODY(umcdf_unit_zname_future, tc)
{
	const char *s = amd_umcdf_zen_name(AMD_UMCDF_ZEN_FUTURE);

	ATF_REQUIRE_MSG(s != NULL, "zen_name(ZEN_FUTURE) returned NULL");
	ATF_CHECK_MSG(strlen(s) > 0, "zen_name(ZEN_FUTURE) returned empty string");
}

/* TC-UNIT-ZNAME-11: ZEN_UNKNOWN returns non-NULL, non-empty string */
ATF_TC_WITHOUT_HEAD(umcdf_unit_zname_unknown);
ATF_TC_BODY(umcdf_unit_zname_unknown, tc)
{
	const char *s = amd_umcdf_zen_name(AMD_UMCDF_ZEN_UNKNOWN);

	ATF_REQUIRE_MSG(s != NULL, "zen_name(ZEN_UNKNOWN) returned NULL");
	ATF_CHECK_MSG(strlen(s) > 0, "zen_name(ZEN_UNKNOWN) returned empty string");
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, umcdf_unit_zname_pre_zen);
	ATF_TP_ADD_TC(tp, umcdf_unit_zname_zen1);
	ATF_TP_ADD_TC(tp, umcdf_unit_zname_zen_plus);
	ATF_TP_ADD_TC(tp, umcdf_unit_zname_zen2);
	ATF_TP_ADD_TC(tp, umcdf_unit_zname_zen3);
	ATF_TP_ADD_TC(tp, umcdf_unit_zname_zen3plus);
	ATF_TP_ADD_TC(tp, umcdf_unit_zname_zen4);
	ATF_TP_ADD_TC(tp, umcdf_unit_zname_zen5);
	ATF_TP_ADD_TC(tp, umcdf_unit_zname_zen6);
	ATF_TP_ADD_TC(tp, umcdf_unit_zname_future);
	ATF_TP_ADD_TC(tp, umcdf_unit_zname_unknown);
	return (atf_no_error());
}
