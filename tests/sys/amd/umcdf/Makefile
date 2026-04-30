#-
# Copyright (c) 2026 Advanced Micro Devices, Inc.
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Author: Davi Chaves Azevedo
#

PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/sys/amd/umcdf
KYUAFILE=	yes

ATF_TESTS_C=	umcdf_cpuid_test \
		umcdf_df_test \
		umcdf_umc_test

LIBADD+=	pmc

.include <bsd.test.mk>
