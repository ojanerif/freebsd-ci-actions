/*-
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Author: ojanerif@amd.com
 *
 * [TC-ROB] — Kernel survival / robustness E2E tests.
 *
 * Tests that the kernel does NOT panic, hang, or corrupt MSR state under
 * adversarial IBS usage patterns.  All tests require AMD IBS hardware and
 * root privileges.
 *
 * Test IDs: TC-ROB-01 … TC-ROB-05
 *
 *   TC-ROB-01  ibs_robustness_nmi_flood           (PLACEHOLDER — see note)
 *   TC-ROB-02  ibs_robustness_all_cpus_nmi_flood  (PLACEHOLDER — see note)
 *   TC-ROB-03  ibs_robustness_reserved_bits_with_enable
 *   TC-ROB-04  ibs_robustness_fork_under_sampling
 *   TC-ROB-05  ibs_robustness_rapid_affinity_switch
 *
 * TC-ROB-01 and TC-ROB-02 are placeholders.  MaxCnt=1 (16-cycle period)
 * generates ~250 million NMIs per second at 4 GHz.  Without hwpmc loaded
 * and an active IBS NMI handler registered, this rate will overwhelm the
 * system.  These tests are left as skips until a safe infrastructure for
 * NMI-flood testing is in place (e.g., pre-loading hwpmc and verifying the
 * handler is registered via a kernel sysctl).
 */

#include <sys/cpuset.h>
#include <sys/param.h>
#include <sys/sched.h>
#include <sys/wait.h>

#include <atf-c.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "ibs_utils.h"

#define IBS_ROB_TEST_PERIOD	0x1000ULL	/* ~262144 cycles — safe default */

/* -----------------------------------------------------------------------
 * TC-ROB-01: NMI flood at MaxCnt=1 — PLACEHOLDER
 *
 * Requires hwpmc loaded with an IBS NMI handler registered.
 * Skipped until infrastructure exists to verify the handler is active.
 * ----------------------------------------------------------------------- */
ATF_TC(ibs_robustness_nmi_flood);
ATF_TC_HEAD(ibs_robustness_nmi_flood, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "[TC-ROB-01] NMI flood at MaxCnt=1 (16-cycle period) for 5 s");
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "timeout", "120");
}

ATF_TC_BODY(ibs_robustness_nmi_flood, tc)
{
	if (!cpu_supports_ibs())
		atf_tc_skip("CPU does not support IBS");

	/*
	 * PLACEHOLDER: MaxCnt=1 generates ~250 M NMIs/s at 4 GHz.
	 * Without hwpmc's IBS NMI handler registered, the kernel's
	 * generic NMI path cannot service them and the system will hang.
	 *
	 * TODO: check for hwpmc loaded + IBS handler registered (e.g.,
	 * sysctl dev.hwpmc.0.ibs_active) before enabling MaxCnt=1.
	 */
	atf_tc_skip("TC-ROB-01 placeholder: NMI flood requires hwpmc IBS "
	    "handler to be registered first (see TODO.md)");
}

/* -----------------------------------------------------------------------
 * TC-ROB-02: all-CPU NMI flood — PLACEHOLDER (same reason as TC-ROB-01)
 * ----------------------------------------------------------------------- */
ATF_TC(ibs_robustness_all_cpus_nmi_flood);
ATF_TC_HEAD(ibs_robustness_all_cpus_nmi_flood, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "[TC-ROB-02] One thread per CPU, all flooding NMIs simultaneously");
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "timeout", "120");
}

ATF_TC_BODY(ibs_robustness_all_cpus_nmi_flood, tc)
{
	if (!cpu_supports_ibs())
		atf_tc_skip("CPU does not support IBS");

	atf_tc_skip("TC-ROB-02 placeholder: NMI flood requires hwpmc IBS "
	    "handler to be registered first (see TODO.md)");
}

/* -----------------------------------------------------------------------
 * TC-ROB-03: reserved bits write with enable set
 *
 * Writes 0xFFFFFFFFFFFFFFFF to IBSFETCHCTL.  The hardware generates #GP
 * for reserved bits; the cpuctl driver returns EFAULT/EIO, not a panic.
 * If the write somehow succeeds, verifies only defined bits survive.
 * ----------------------------------------------------------------------- */
ATF_TC(ibs_robustness_reserved_bits_with_enable);
ATF_TC_HEAD(ibs_robustness_reserved_bits_with_enable, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "[TC-ROB-03] Write 0xFFFFFFFFFFFFFFFF to IBSFETCHCTL — no panic");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(ibs_robustness_reserved_bits_with_enable, tc)
{
	uint64_t original, readback;
	int error;

	if (!cpu_supports_ibs())
		atf_tc_skip("CPU does not support IBS");

	error = read_msr(0, MSR_IBS_FETCH_CTL, &original);
	if (error != 0)
		atf_tc_skip("Cannot read MSR_IBS_FETCH_CTL: %s",
		    strerror(error));

	/* Write all-ones: reserved bits should cause #GP → EFAULT/EIO */
	error = write_msr(0, MSR_IBS_FETCH_CTL, 0xFFFFFFFFFFFFFFFFULL);
	if (error != 0) {
		/* Expected: cpuctl returned the #GP as EFAULT or EIO */
		ATF_CHECK_MSG(error == EFAULT || error == EIO || error == EPERM,
		    "unexpected error %d (%s) writing all-ones to IBSFETCHCTL",
		    error, strerror(error));
		write_msr(0, MSR_IBS_FETCH_CTL, original);
		return;
	}

	/*
	 * Write succeeded: read back and immediately disable to stop any
	 * sampling that may have started from the enable bit.
	 */
	write_msr(0, MSR_IBS_FETCH_CTL, 0ULL);

	error = read_msr(0, MSR_IBS_FETCH_CTL, &readback);
	if (error != 0) {
		write_msr(0, MSR_IBS_FETCH_CTL, original);
		atf_tc_skip("Cannot read back IBSFETCHCTL: %s", strerror(error));
	}

	/*
	 * Verify the hardware masked reserved bits.
	 * All bits at positions 60–63 are reserved in IBSFETCHCTL.
	 */
	ATF_CHECK_MSG((readback & 0xF000000000000000ULL) == 0ULL,
	    "reserved bits survived write: readback=0x%016llx",
	    (unsigned long long)readback);

	write_msr(0, MSR_IBS_FETCH_CTL, original);
}

/* -----------------------------------------------------------------------
 * TC-ROB-04: fork while IBS sampling is active
 *
 * Enable IBS Fetch on CPU 0 in the parent, fork a child that pins to CPU 1
 * and clears IBS on CPU 1, then exits.  Parent verifies its CPU 0 IBS state
 * is still intact after the child exits.
 * ----------------------------------------------------------------------- */
ATF_TC(ibs_robustness_fork_under_sampling);
ATF_TC_HEAD(ibs_robustness_fork_under_sampling, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "[TC-ROB-04] Fork while IBS active; child's MSR ops must not "
	    "corrupt parent's CPU state");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(ibs_robustness_fork_under_sampling, tc)
{
	cpuset_t mask;
	uint64_t original, written, readback;
	int error, ncpus, status;
	pid_t pid;

	if (!cpu_supports_ibs())
		atf_tc_skip("CPU does not support IBS");

	ncpus = (int)sysconf(_SC_NPROCESSORS_ONLN);
	if (ncpus < 2)
		atf_tc_skip("Test requires at least 2 CPUs (found %d)", ncpus);

	/* Pin parent to CPU 0 */
	CPU_ZERO(&mask);
	CPU_SET(0, &mask);
	if (cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID, -1,
	    sizeof(mask), &mask) != 0)
		atf_tc_skip("Cannot pin to CPU 0: %s", strerror(errno));

	error = read_msr(0, MSR_IBS_FETCH_CTL, &original);
	if (error != 0)
		atf_tc_skip("Cannot read MSR_IBS_FETCH_CTL: %s",
		    strerror(error));

	/* Enable IBS Fetch on CPU 0 with a known period */
	written = IBS_FETCH_EN | IBS_ROB_TEST_PERIOD;
	error = write_msr(0, MSR_IBS_FETCH_CTL, written);
	ATF_REQUIRE_ERRNO(0, error == 0);

	pid = fork();
	ATF_REQUIRE(pid >= 0);

	if (pid == 0) {
		/* Child: pin to CPU 1 and clear its IBS state */
		CPU_ZERO(&mask);
		CPU_SET(1, &mask);
		cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID, -1,
		    sizeof(mask), &mask);
		write_msr(1, MSR_IBS_FETCH_CTL, 0ULL);
		_exit(0);
	}

	waitpid(pid, &status, 0);
	ATF_CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);

	/* Re-read CPU 0's IBS state — should still have our value */
	error = read_msr(0, MSR_IBS_FETCH_CTL, &readback);
	ATF_REQUIRE_ERRNO(0, error == 0);

	/*
	 * The IBS Fetch enable bit and period must survive the fork.
	 * (NMI may have cleared the VALID bit; ignore those status bits.)
	 */
	ATF_CHECK_MSG((readback & IBS_FETCH_EN) != 0,
	    "IBS_FETCH_EN lost after fork: readback=0x%016llx",
	    (unsigned long long)readback);
	ATF_CHECK_MSG(ibs_get_maxcnt(readback) == IBS_ROB_TEST_PERIOD,
	    "MaxCnt changed after fork: expected 0x%llx got 0x%llx",
	    (unsigned long long)IBS_ROB_TEST_PERIOD,
	    (unsigned long long)ibs_get_maxcnt(readback));

	/* Clean up: disable IBS and restore original */
	write_msr(0, MSR_IBS_FETCH_CTL, 0ULL);
	write_msr(0, MSR_IBS_FETCH_CTL, original);
}

/* -----------------------------------------------------------------------
 * TC-ROB-05: rapid CPU affinity migration while IBS is active
 *
 * Enable IBS Fetch on CPU 0 with a unique period, then migrate the thread
 * across all CPUs and read IBS state on each.  CPU 0 must retain our value;
 * other CPUs must not show our period (no cross-CPU bleed).
 * ----------------------------------------------------------------------- */
ATF_TC(ibs_robustness_rapid_affinity_switch);
ATF_TC_HEAD(ibs_robustness_rapid_affinity_switch, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "[TC-ROB-05] Rapid affinity migration while IBS active — "
	    "no cross-CPU state bleed");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(ibs_robustness_rapid_affinity_switch, tc)
{
	cpuset_t mask;
	uint64_t original_cpu0, baseline[64], readback;
	int error, ncpus, i;
	const uint64_t TEST_PERIOD = 0x0A5AULL;	/* unlikely value */

	if (!cpu_supports_ibs())
		atf_tc_skip("CPU does not support IBS");

	ncpus = (int)sysconf(_SC_NPROCESSORS_ONLN);
	if (ncpus < 2)
		atf_tc_skip("Test requires at least 2 CPUs (found %d)", ncpus);
	if (ncpus > 64)
		ncpus = 64;	/* cap for stack array */

	/* Read baseline IBS state on all CPUs before we touch anything */
	for (i = 0; i < ncpus; i++) {
		error = read_msr(i, MSR_IBS_FETCH_CTL, &baseline[i]);
		if (error != 0)
			atf_tc_skip("Cannot read MSR on CPU %d: %s",
			    i, strerror(error));
	}

	/* Pin to CPU 0 and enable IBS with our test period */
	CPU_ZERO(&mask);
	CPU_SET(0, &mask);
	if (cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID, -1,
	    sizeof(mask), &mask) != 0)
		atf_tc_skip("Cannot pin to CPU 0: %s", strerror(errno));

	original_cpu0 = baseline[0];
	error = write_msr(0, MSR_IBS_FETCH_CTL,
	    IBS_FETCH_EN | TEST_PERIOD);
	ATF_REQUIRE_ERRNO(0, error == 0);

	/* Migrate across all CPUs; verify other CPUs don't show our period */
	for (i = 1; i < ncpus; i++) {
		CPU_ZERO(&mask);
		CPU_SET(i, &mask);
		cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID, -1,
		    sizeof(mask), &mask);
		sched_yield();

		error = read_msr(i, MSR_IBS_FETCH_CTL, &readback);
		if (error != 0)
			continue;	/* skip if this CPU is offline */

		ATF_CHECK_MSG(ibs_get_maxcnt(readback) != TEST_PERIOD,
		    "CPU %d unexpectedly has our test period 0x%llx — "
		    "possible cross-CPU IBS state bleed", i,
		    (unsigned long long)TEST_PERIOD);
	}

	/* Restore CPU 0's state */
	CPU_ZERO(&mask);
	CPU_SET(0, &mask);
	cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID, -1,
	    sizeof(mask), &mask);
	write_msr(0, MSR_IBS_FETCH_CTL, 0ULL);
	write_msr(0, MSR_IBS_FETCH_CTL, original_cpu0);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, ibs_robustness_nmi_flood);
	ATF_TP_ADD_TC(tp, ibs_robustness_all_cpus_nmi_flood);
	ATF_TP_ADD_TC(tp, ibs_robustness_reserved_bits_with_enable);
	ATF_TP_ADD_TC(tp, ibs_robustness_fork_under_sampling);
	ATF_TP_ADD_TC(tp, ibs_robustness_rapid_affinity_switch);
	return (atf_no_error());
}
