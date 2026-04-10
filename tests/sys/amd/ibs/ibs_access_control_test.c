/*-
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Author: ojanerif@amd.com
 *
 * [TC-ACCTL] — IBS MSR access control integration tests.
 *
 * Verifies that unprivileged users are denied access to /dev/cpuctl and
 * IBS MSRs, and that root is permitted.  Uses fork()+setuid() to execute
 * the operations as an unprivileged user inside the same test run.
 *
 * Requires root (so the child can call setuid to drop privileges).
 *
 * Test IDs: TC-ACCTL-01 … TC-ACCTL-04
 */

#include <sys/cpuctl.h>
#include <sys/ioctl.h>
#include <sys/wait.h>

#include <atf-c.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "ibs_utils.h"

/* Exit codes used by the forked child */
#define CHILD_PASS	0	/* operation was blocked as expected */
#define CHILD_SKIP	77	/* could not drop privileges; skip */
#define CHILD_FAIL_OPEN	1	/* open unexpectedly succeeded */
#define CHILD_FAIL_IOCTL 2	/* ioctl unexpectedly succeeded */
#define CHILD_FAIL_OTHER 3	/* unexpected errno */

static uid_t
get_unprivileged_uid(void)
{
	struct passwd *pw;

	pw = getpwnam("nobody");
	if (pw != NULL)
		return (pw->pw_uid);
	return ((uid_t)65534);
}

/* -----------------------------------------------------------------------
 * TC-ACCTL-01: unprivileged user — open /dev/cpuctl0 O_RDWR returns EPERM/EACCES
 * ----------------------------------------------------------------------- */
ATF_TC(ibs_nonroot_cpuctl_open);
ATF_TC_HEAD(ibs_nonroot_cpuctl_open, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Unprivileged open of /dev/cpuctl0 O_RDWR must fail with EPERM/EACCES");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(ibs_nonroot_cpuctl_open, tc)
{
	uid_t unpriv_uid = get_unprivileged_uid();
	pid_t pid;
	int status, fd_check;

	/* Verify the device exists before forking */
	fd_check = open("/dev/cpuctl0", O_RDWR);
	if (fd_check < 0 && errno == ENOENT)
		atf_tc_skip("/dev/cpuctl0 not present (cpuctl not loaded)");
	if (fd_check >= 0)
		close(fd_check);

	pid = fork();
	ATF_REQUIRE(pid >= 0);

	if (pid == 0) {
		/* Child: drop to unprivileged user */
		if (setuid(unpriv_uid) != 0)
			_exit(CHILD_SKIP);

		int fd = open("/dev/cpuctl0", O_RDWR);
		if (fd >= 0) {
			close(fd);
			_exit(CHILD_FAIL_OPEN);
		}
		if (errno == EACCES || errno == EPERM)
			_exit(CHILD_PASS);
		_exit(CHILD_FAIL_OTHER);
	}

	waitpid(pid, &status, 0);
	ATF_REQUIRE(WIFEXITED(status));

	switch (WEXITSTATUS(status)) {
	case CHILD_PASS:
		break;
	case CHILD_SKIP:
		atf_tc_skip("Cannot drop to unprivileged UID %u", unpriv_uid);
	case CHILD_FAIL_OPEN:
		atf_tc_fail("open /dev/cpuctl0 O_RDWR succeeded as UID %u",
		    unpriv_uid);
	default:
		atf_tc_fail("unexpected errno in child (exit %d)",
		    WEXITSTATUS(status));
	}
}

/* -----------------------------------------------------------------------
 * TC-ACCTL-02: unprivileged user — CPUCTL_RDMSR returns EPERM/EACCES
 * ----------------------------------------------------------------------- */
ATF_TC(ibs_nonroot_msr_read);
ATF_TC_HEAD(ibs_nonroot_msr_read, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Unprivileged CPUCTL_RDMSR must fail with EPERM/EACCES");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(ibs_nonroot_msr_read, tc)
{
	uid_t unpriv_uid = get_unprivileged_uid();
	pid_t pid;
	int status, fd_check;

	fd_check = open("/dev/cpuctl0", O_RDWR);
	if (fd_check < 0 && errno == ENOENT)
		atf_tc_skip("/dev/cpuctl0 not present");
	if (fd_check >= 0)
		close(fd_check);

	pid = fork();
	ATF_REQUIRE(pid >= 0);

	if (pid == 0) {
		if (setuid(unpriv_uid) != 0)
			_exit(CHILD_SKIP);

		/* read_msr opens the device internally */
		uint64_t val;
		int err = read_msr(0, MSR_IBS_FETCH_CTL, &val);
		/* Expect EACCES or EPERM from the open() inside read_msr */
		if (err == EACCES || err == EPERM)
			_exit(CHILD_PASS);
		/* Possibly ENOENT if no /dev/cpuctl0 — acceptable */
		if (err == ENOENT)
			_exit(CHILD_SKIP);
		/* Any success is a failure */
		if (err == 0)
			_exit(CHILD_FAIL_IOCTL);
		_exit(CHILD_FAIL_OTHER);
	}

	waitpid(pid, &status, 0);
	ATF_REQUIRE(WIFEXITED(status));

	switch (WEXITSTATUS(status)) {
	case CHILD_PASS:
		break;
	case CHILD_SKIP:
		atf_tc_skip("Cannot drop to unprivileged UID %u or device missing",
		    unpriv_uid);
	case CHILD_FAIL_IOCTL:
		atf_tc_fail("read_msr succeeded as UID %u — expected EPERM/EACCES",
		    unpriv_uid);
	default:
		atf_tc_fail("unexpected result in child (exit %d)",
		    WEXITSTATUS(status));
	}
}

/* -----------------------------------------------------------------------
 * TC-ACCTL-03: unprivileged user — CPUCTL_WRMSR returns EPERM/EACCES
 * ----------------------------------------------------------------------- */
ATF_TC(ibs_nonroot_msr_write);
ATF_TC_HEAD(ibs_nonroot_msr_write, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Unprivileged CPUCTL_WRMSR must fail with EPERM/EACCES");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(ibs_nonroot_msr_write, tc)
{
	uid_t unpriv_uid = get_unprivileged_uid();
	pid_t pid;
	int status, fd_check;

	fd_check = open("/dev/cpuctl0", O_RDWR);
	if (fd_check < 0 && errno == ENOENT)
		atf_tc_skip("/dev/cpuctl0 not present");
	if (fd_check >= 0)
		close(fd_check);

	pid = fork();
	ATF_REQUIRE(pid >= 0);

	if (pid == 0) {
		if (setuid(unpriv_uid) != 0)
			_exit(CHILD_SKIP);

		int err = write_msr(0, MSR_IBS_FETCH_CTL, 0ULL);
		if (err == EACCES || err == EPERM)
			_exit(CHILD_PASS);
		if (err == ENOENT)
			_exit(CHILD_SKIP);
		if (err == 0)
			_exit(CHILD_FAIL_IOCTL);
		_exit(CHILD_FAIL_OTHER);
	}

	waitpid(pid, &status, 0);
	ATF_REQUIRE(WIFEXITED(status));

	switch (WEXITSTATUS(status)) {
	case CHILD_PASS:
		break;
	case CHILD_SKIP:
		atf_tc_skip("Cannot drop to unprivileged UID %u or device missing",
		    unpriv_uid);
	case CHILD_FAIL_IOCTL:
		atf_tc_fail("write_msr succeeded as UID %u — expected EPERM/EACCES",
		    unpriv_uid);
	default:
		atf_tc_fail("unexpected result in child (exit %d)",
		    WEXITSTATUS(status));
	}
}

/* -----------------------------------------------------------------------
 * TC-ACCTL-04: root — read_msr on MSR_IBS_FETCH_CTL succeeds
 * ----------------------------------------------------------------------- */
ATF_TC(ibs_root_msr_accessible);
ATF_TC_HEAD(ibs_root_msr_accessible, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Root can read MSR_IBS_FETCH_CTL via cpuctl");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(ibs_root_msr_accessible, tc)
{
	uint64_t val;
	int err;

	err = read_msr(0, MSR_IBS_FETCH_CTL, &val);
	if (err == ENOENT)
		atf_tc_skip("/dev/cpuctl0 not present (cpuctl not loaded)");
	if (err == ENODEV || err == EIO)
		atf_tc_skip("MSR_IBS_FETCH_CTL inaccessible — not an IBS CPU");
	ATF_REQUIRE_MSG(err == 0,
	    "read_msr as root returned %d (%s)", err, strerror(err));
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, ibs_nonroot_cpuctl_open);
	ATF_TP_ADD_TC(tp, ibs_nonroot_msr_read);
	ATF_TP_ADD_TC(tp, ibs_nonroot_msr_write);
	ATF_TP_ADD_TC(tp, ibs_root_msr_accessible);
	return (atf_no_error());
}
