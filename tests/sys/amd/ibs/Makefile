#-
# Copyright (c) 2023, 2026 Advanced Micro Devices, Inc.
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Author: ojanerif@amd.com
#

TESTSDIR=	${TESTSBASE}/sys/amd/ibs

ATF_TESTS_C=	ibs_cpu_test \
		ibs_detect_test \
		ibs_msr_test \
		ibs_interrupt_test \
		ibs_data_accuracy_test \
		ibs_smp_test \
		ibs_api_test \
		ibs_stress_test \
		ibs_period_test \
		ibs_routing_test \
		ibs_l3miss_test \
		ibs_ioctl_test \
		ibs_unit_field_masks_test \
		ibs_unit_helpers_test \
		ibs_unit_datasrc_test \
		ibs_unit_cpuid_parse_test \
		ibs_unit_op_ext_maxcnt_test \
		ibs_unit_feature_flags_test \
		ibs_cpuctl_access_test \
		ibs_access_control_test \
		ibs_invalid_input_test \
		ibs_robustness_test \
		ibs_concurrency_test

ATF_TESTS_SH=	ibs_swfilt_test

LIBADD+=pthread

.include <bsd.test.mk>
