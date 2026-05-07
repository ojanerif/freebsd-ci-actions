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

# ATF is installed as a port at /usr/local (not a base library on this host).
# Set LDADD_atf_c so atf.test.mk's per-test LDADD expansion finds the library,
# and extend CFLAGS so new source files locate <atf-c.h>.
CFLAGS+=	-I/usr/local/include
LDADD_atf_c=	-L/usr/local/lib -latf-c

.include <bsd.test.mk>
