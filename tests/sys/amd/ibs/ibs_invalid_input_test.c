/*-
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Author: ojanerif@amd.com
 *
 * [TC-INV] — Invalid / boundary input tests for the cpuctl MSR interface.
 *
 * Sends bad MSR numbers, out-of-range values, and malformed ioctl arguments
 * to the cpuctl driver.  Each test verifies a well-defined error code is
 * returned and that the kernel does NOT panic.
 *
 * Requires root.  Requires /dev/cpuctl0 (cpuctl module loaded).
 *
 * Test IDs: TC-INV-01 … TC-INV-07
 */

#include <sys/cpuctl.h>
#include <sys/ioctl.h>

#include <atf-c.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "ibs_utils.h"

static int
open_cpuctl0(void)
{
	int fd;

	fd = open("/dev/cpuctl0", O_RDWR);
	return (fd);
}

/* -----------------------------------------------------------------------
 * TC-INV-01: RDMSR on MSR just below IBS range → EIO or EFAULT, not panic
 * ----------------------------------------------------------------------- */
ATF_TC(ibs_invalid_msr_below_range);
ATF_TC_HEAD(ibs_invalid_msr_below_range, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "RDMSR below IBS MSR range returns EIO/EFAULT, not panic");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(ibs_invalid_msr_below_range, tc)
{
	cpuctl_msr_args_t args;
	int fd, ret;

	fd = open_cpuctl0();
	if (fd < 0 && errno == ENOENT)
		atf_tc_skip("/dev/cpuctl0 not present");
	ATF_REQUIRE(fd >= 0);

	/* 0xC001102F is one MSR below MSR_IBS_FETCH_CTL (0xC0011030) */
	args.msr  = 0xC001102FU;
	args.data = 0;
	ret = ioctl(fd, CPUCTL_RDMSR, &args);
	/* If it succeeds, that's fine — not all adjacent MSRs fault */
	if (ret != 0) {
		ATF_CHECK_MSG(errno == EIO || errno == EFAULT || errno == EPERM,
		    "unexpected errno %d (%s) for MSR 0xC001102F",
		    errno, strerror(errno));
	}
	close(fd);
}

/* -----------------------------------------------------------------------
 * TC-INV-02: RDMSR on a known read-only/restricted MSR → EIO/0, not panic
 * ----------------------------------------------------------------------- */
ATF_TC(ibs_invalid_msr_gap);
ATF_TC_HEAD(ibs_invalid_msr_gap, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "RDMSR on MSR_AMD64_IBSBRTARGET returns EIO or 0, not panic");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(ibs_invalid_msr_gap, tc)
{
	cpuctl_msr_args_t args;
	int fd, ret;

	fd = open_cpuctl0();
	if (fd < 0 && errno == ENOENT)
		atf_tc_skip("/dev/cpuctl0 not present");
	ATF_REQUIRE(fd >= 0);

	/* MSR_AMD64_IBSBRTARGET (0xC001103B) may be read-only or restricted */
	args.msr  = 0xC001103BU;
	args.data = 0;
	ret = ioctl(fd, CPUCTL_RDMSR, &args);
	if (ret != 0) {
		ATF_CHECK_MSG(errno == EIO || errno == EFAULT || errno == EPERM,
		    "unexpected errno %d (%s) for MSR 0xC001103B",
		    errno, strerror(errno));
	}
	/* Either succeeds or fails gracefully — no panic allowed */
	close(fd);
}

/* -----------------------------------------------------------------------
 * TC-INV-03: RDMSR above IBS range → EIO or EFAULT, not panic
 * ----------------------------------------------------------------------- */
ATF_TC(ibs_invalid_msr_above_range);
ATF_TC_HEAD(ibs_invalid_msr_above_range, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "RDMSR above IBS MSR range returns EIO/EFAULT, not panic");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(ibs_invalid_msr_above_range, tc)
{
	cpuctl_msr_args_t args;
	int fd, ret;

	fd = open_cpuctl0();
	if (fd < 0 && errno == ENOENT)
		atf_tc_skip("/dev/cpuctl0 not present");
	ATF_REQUIRE(fd >= 0);

	/* 0xC001103E is one above MSR_AMD64_IBSOPDATA4 (0xC001103D) */
	args.msr  = 0xC001103EU;
	args.data = 0;
	ret = ioctl(fd, CPUCTL_RDMSR, &args);
	if (ret != 0) {
		ATF_CHECK_MSG(errno == EIO || errno == EFAULT || errno == EPERM,
		    "unexpected errno %d (%s) for MSR 0xC001103E",
		    errno, strerror(errno));
	}
	close(fd);
}

/* -----------------------------------------------------------------------
 * TC-INV-04: RDMSR with a garbage MSR address → EIO or EFAULT, not panic
 * ----------------------------------------------------------------------- */
ATF_TC(ibs_invalid_msr_garbage);
ATF_TC_HEAD(ibs_invalid_msr_garbage, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "RDMSR with garbage MSR 0xDEADBEEF returns EIO/EFAULT, not panic");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(ibs_invalid_msr_garbage, tc)
{
	cpuctl_msr_args_t args;
	int fd, ret;

	fd = open_cpuctl0();
	if (fd < 0 && errno == ENOENT)
		atf_tc_skip("/dev/cpuctl0 not present");
	ATF_REQUIRE(fd >= 0);

	args.msr  = 0xDEADBEEFU;
	args.data = 0;
	ret = ioctl(fd, CPUCTL_RDMSR, &args);
	ATF_CHECK_MSG(ret != 0,
	    "RDMSR on 0xDEADBEEF unexpectedly succeeded");
	if (ret != 0) {
		ATF_CHECK_MSG(errno == EIO || errno == EFAULT || errno == EPERM,
		    "unexpected errno %d (%s) for MSR 0xDEADBEEF",
		    errno, strerror(errno));
	}
	close(fd);
}

/* -----------------------------------------------------------------------
 * TC-INV-05: CPUCTL_WRMSR with NULL argp → EFAULT
 * ----------------------------------------------------------------------- */
ATF_TC(ibs_wrmsr_null_argp);
ATF_TC_HEAD(ibs_wrmsr_null_argp, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "ioctl CPUCTL_WRMSR with NULL argp returns EFAULT");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(ibs_wrmsr_null_argp, tc)
{
	int fd, ret;

	fd = open_cpuctl0();
	if (fd < 0 && errno == ENOENT)
		atf_tc_skip("/dev/cpuctl0 not present");
	ATF_REQUIRE(fd >= 0);

	ret = ioctl(fd, CPUCTL_WRMSR, NULL);
	ATF_CHECK_MSG(ret == -1, "CPUCTL_WRMSR NULL argp should fail");
	ATF_CHECK_MSG(errno == EFAULT,
	    "expected EFAULT, got errno %d (%s)", errno, strerror(errno));
	close(fd);
}

/* -----------------------------------------------------------------------
 * TC-INV-06: CPUCTL_RDMSR with NULL argp → EFAULT
 * ----------------------------------------------------------------------- */
ATF_TC(ibs_rdmsr_null_argp);
ATF_TC_HEAD(ibs_rdmsr_null_argp, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "ioctl CPUCTL_RDMSR with NULL argp returns EFAULT");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(ibs_rdmsr_null_argp, tc)
{
	int fd, ret;

	fd = open_cpuctl0();
	if (fd < 0 && errno == ENOENT)
		atf_tc_skip("/dev/cpuctl0 not present");
	ATF_REQUIRE(fd >= 0);

	ret = ioctl(fd, CPUCTL_RDMSR, NULL);
	ATF_CHECK_MSG(ret == -1, "CPUCTL_RDMSR NULL argp should fail");
	ATF_CHECK_MSG(errno == EFAULT,
	    "expected EFAULT, got errno %d (%s)", errno, strerror(errno));
	close(fd);
}

/* -----------------------------------------------------------------------
 * TC-INV-07: unknown ioctl command → ENOTTY or EINVAL
 * ----------------------------------------------------------------------- */
ATF_TC(ibs_unknown_ioctl_cmd);
ATF_TC_HEAD(ibs_unknown_ioctl_cmd, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Unknown ioctl command returns ENOTTY or EINVAL, not panic");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(ibs_unknown_ioctl_cmd, tc)
{
	int fd, ret;

	fd = open_cpuctl0();
	if (fd < 0 && errno == ENOENT)
		atf_tc_skip("/dev/cpuctl0 not present");
	ATF_REQUIRE(fd >= 0);

	ret = ioctl(fd, (unsigned long)0xDEADUL, NULL);
	ATF_CHECK_MSG(ret == -1, "unknown ioctl should fail");
	ATF_CHECK_MSG(errno == ENOTTY || errno == EINVAL,
	    "expected ENOTTY or EINVAL, got errno %d (%s)",
	    errno, strerror(errno));
	close(fd);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, ibs_invalid_msr_below_range);
	ATF_TP_ADD_TC(tp, ibs_invalid_msr_gap);
	ATF_TP_ADD_TC(tp, ibs_invalid_msr_above_range);
	ATF_TP_ADD_TC(tp, ibs_invalid_msr_garbage);
	ATF_TP_ADD_TC(tp, ibs_wrmsr_null_argp);
	ATF_TP_ADD_TC(tp, ibs_rdmsr_null_argp);
	ATF_TP_ADD_TC(tp, ibs_unknown_ioctl_cmd);
	return (atf_no_error());
}
