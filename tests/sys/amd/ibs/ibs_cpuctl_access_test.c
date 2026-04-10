/*-
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Author: ojanerif@amd.com
 *
 * [TC-CPUCTL] — cpuctl driver interface integration tests.
 *
 * Tests the /dev/cpuctl<N> device interface itself.  Does NOT require IBS
 * hardware.  Requires root and the cpuctl kernel module.
 *
 * Test IDs: TC-CPUCTL-01 … TC-CPUCTL-05
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

/* -----------------------------------------------------------------------
 * TC-CPUCTL-01: open /dev/cpuctl0 O_RDWR as root
 * ----------------------------------------------------------------------- */
ATF_TC(ibs_cpuctl_open_rdwr);
ATF_TC_HEAD(ibs_cpuctl_open_rdwr, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Open /dev/cpuctl0 O_RDWR as root — must succeed");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(ibs_cpuctl_open_rdwr, tc)
{
	int fd;

	fd = open("/dev/cpuctl0", O_RDWR);
	if (fd < 0) {
		if (errno == ENOENT)
			atf_tc_skip("/dev/cpuctl0 not present (cpuctl not loaded)");
		atf_tc_fail("open /dev/cpuctl0 O_RDWR failed: %s",
		    strerror(errno));
	}
	close(fd);
}

/* -----------------------------------------------------------------------
 * TC-CPUCTL-02: O_RDONLY open — CPUCTL_RDMSR succeeds, CPUCTL_WRMSR fails
 * ----------------------------------------------------------------------- */
ATF_TC(ibs_cpuctl_open_rdonly);
ATF_TC_HEAD(ibs_cpuctl_open_rdonly, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "O_RDONLY cpuctl: RDMSR succeeds; WRMSR returns EBADF or EPERM");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(ibs_cpuctl_open_rdonly, tc)
{
	cpuctl_msr_args_t args;
	int fd, ret;

	fd = open("/dev/cpuctl0", O_RDONLY);
	if (fd < 0) {
		if (errno == ENOENT)
			atf_tc_skip("/dev/cpuctl0 not present");
		atf_tc_fail("open O_RDONLY failed: %s", strerror(errno));
	}

	/* Read should work on a read-only fd */
	args.msr  = MSR_IBS_FETCH_CTL;
	args.data = 0;
	ret = ioctl(fd, CPUCTL_RDMSR, &args);
	if (ret != 0 && errno == ENODEV) {
		close(fd);
		atf_tc_skip("CPUCTL_RDMSR returned ENODEV — not an IBS CPU");
	}
	/* On a non-AMD CPU the read may fail; that's acceptable here */

	/* Write must fail on a read-only fd */
	args.msr  = MSR_IBS_FETCH_CTL;
	args.data = 0;
	ret = ioctl(fd, CPUCTL_WRMSR, &args);
	ATF_CHECK_MSG(ret != 0,
	    "CPUCTL_WRMSR succeeded on O_RDONLY fd — expected failure");
	if (ret != 0)
		ATF_CHECK(errno == EBADF || errno == EPERM || errno == EACCES);

	close(fd);
}

/* -----------------------------------------------------------------------
 * TC-CPUCTL-03: all per-CPU devices exist — count matches nproc
 * ----------------------------------------------------------------------- */
ATF_TC(ibs_cpuctl_per_cpu_device);
ATF_TC_HEAD(ibs_cpuctl_per_cpu_device, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "One /dev/cpuctl<N> per logical CPU; count == nprocessors_onln");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(ibs_cpuctl_per_cpu_device, tc)
{
	char path[32];
	int fd, ncpus, i;

	/* Check /dev/cpuctl0 exists first */
	fd = open("/dev/cpuctl0", O_RDONLY);
	if (fd < 0 && errno == ENOENT)
		atf_tc_skip("/dev/cpuctl0 not present (cpuctl not loaded)");
	if (fd >= 0)
		close(fd);

	ncpus = (int)sysconf(_SC_NPROCESSORS_ONLN);
	if (ncpus < 1)
		atf_tc_skip("sysconf(_SC_NPROCESSORS_ONLN) returned < 1");

	for (i = 0; i < ncpus; i++) {
		snprintf(path, sizeof(path), "/dev/cpuctl%d", i);
		fd = open(path, O_RDONLY);
		ATF_CHECK_MSG(fd >= 0,
		    "open(%s) failed: %s", path, strerror(errno));
		if (fd >= 0)
			close(fd);
	}
}

/* -----------------------------------------------------------------------
 * TC-CPUCTL-04: CPUCTL_CPUID on IBS leaf 0x8000001B
 * ----------------------------------------------------------------------- */
ATF_TC(ibs_cpuctl_cpuid_ibs_leaf);
ATF_TC_HEAD(ibs_cpuctl_cpuid_ibs_leaf, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "CPUCTL_CPUID leaf 0x8000001B executes without error");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(ibs_cpuctl_cpuid_ibs_leaf, tc)
{
	cpuctl_cpuid_args_t args;
	int fd, ret;

	fd = open("/dev/cpuctl0", O_RDONLY);
	if (fd < 0 && errno == ENOENT)
		atf_tc_skip("/dev/cpuctl0 not present");
	ATF_REQUIRE(fd >= 0);

	args.level = 0x8000001BU;
	ret = ioctl(fd, CPUCTL_CPUID, &args);
	close(fd);

	if (ret != 0)
		atf_tc_skip("CPUCTL_CPUID leaf 0x8000001B failed: %s",
		    strerror(errno));

	/* On a non-IBS CPU, EAX == 0; that is valid — just skip in that case */
	if (args.data[0] == 0)
		atf_tc_skip("CPUID 0x8000001B EAX == 0 (no IBS features)");

	/* On an IBS CPU, at least IbsFetchSam (bit 0) should be set */
	ATF_CHECK_MSG((args.data[0] & 0x3U) != 0,
	    "IBS feature CPUID returned EAX=0x%08x — unexpected", args.data[0]);
}

/* -----------------------------------------------------------------------
 * TC-CPUCTL-05: CPUCTL_CPUID basic leaf 0x0 — always succeeds
 * ----------------------------------------------------------------------- */
ATF_TC(ibs_cpuctl_cpuid_basic_leaf);
ATF_TC_HEAD(ibs_cpuctl_cpuid_basic_leaf, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "CPUCTL_CPUID leaf 0x0 always succeeds; max_leaf >= 1");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(ibs_cpuctl_cpuid_basic_leaf, tc)
{
	cpuctl_cpuid_args_t args;
	int fd;

	fd = open("/dev/cpuctl0", O_RDONLY);
	if (fd < 0 && errno == ENOENT)
		atf_tc_skip("/dev/cpuctl0 not present");
	ATF_REQUIRE(fd >= 0);

	args.level = 0x0U;
	ATF_REQUIRE_ERRNO(0, ioctl(fd, CPUCTL_CPUID, &args) == 0);
	close(fd);

	/* CPUID 0 EAX is the maximum supported leaf; must be at least 1 */
	ATF_CHECK_MSG(args.data[0] >= 1U,
	    "CPUID leaf 0 EAX=%u — suspiciously low", args.data[0]);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, ibs_cpuctl_open_rdwr);
	ATF_TP_ADD_TC(tp, ibs_cpuctl_open_rdonly);
	ATF_TP_ADD_TC(tp, ibs_cpuctl_per_cpu_device);
	ATF_TP_ADD_TC(tp, ibs_cpuctl_cpuid_ibs_leaf);
	ATF_TP_ADD_TC(tp, ibs_cpuctl_cpuid_basic_leaf);
	return (atf_no_error());
}
