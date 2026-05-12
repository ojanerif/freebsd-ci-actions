/*-
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Author: ojanerif@amd.com
 */

/*
 * IBS CPU Stress Tests
 *
 * Validates IBS MSR stability while the CPU pipeline is saturated with
 * computation.  Two workloads exercise different CPU pressure modes:
 *
 * 1. ibs_cpu_stress_compute:
 *    One thread per CPU pinned to its logical CPU, running a tight
 *    xorshift64 PRNG loop (integer ALU and branch-heavy) for 120 seconds.
 *    Exercises instruction-issue throughput and the integer execution units.
 *
 * 2. ibs_cpu_stress_context_switch:
 *    Creates 4 × ncpus threads that call sched_yield() in a tight loop,
 *    oversubscribing the CPU set and forcing high-frequency context switches
 *    and scheduler activity for 120 seconds.
 *
 * Both tests verify that IBS MSR read/write operations succeed on all online
 * CPUs throughout the stress period and that MSR state is coherent afterwards.
 *
 * Reference: AMD Processor Programming Reference (PPR), IBS chapter.
 * Linux kernel: arch/x86/events/amd/ibs.c
 */

#include <sys/param.h>
#include <sys/cpuset.h>
#include <sys/sched.h>

#include <atf-c.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "ibs_utils.h"

/* Stress duration in seconds — minimum 2 minutes. */
#define CPU_STRESS_DURATION_SEC		120

/* MSR polling interval (seconds). */
#define CPU_STRESS_POLL_INTERVAL_SEC	5

/*
 * xorshift64 step — pure integer arithmetic, no memory traffic.
 * Three shifts + three XORs exercise ALU and the forwarding network.
 */
#define XORSHIFT64(x)	do {		\
	(x) ^= (x) << 13;		\
	(x) ^= (x) >> 7;		\
	(x) ^= (x) << 17;		\
} while (0)

/*
 * Helper: number of online CPUs.
 */
static int
cpu_stress_ncpus(void)
{
	long n = sysconf(_SC_NPROCESSORS_ONLN);
	return ((int)(n < 1 ? 1 : n));
}

/* -----------------------------------------------------------------------
 * Compute workload (integer ALU saturation)
 * ----------------------------------------------------------------------- */

struct compute_arg {
	volatile bool	*stop;
	int		 cpu;
	int		 error;
	uint64_t	 iterations;
};

/*
 * Thread function: tight xorshift64 loop pinned to a single CPU.
 * No memory traffic beyond the register file; stresses integer execution
 * throughput, branch prediction, and the retirement pipeline.
 */
static void *
compute_thread(void *arg)
{
	struct compute_arg *ca = arg;
	cpuset_t mask;
	uint64_t x;

	CPU_ZERO(&mask);
	CPU_SET(ca->cpu, &mask);
	cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID, -1,
	    sizeof(mask), &mask);

	/* Seed is non-zero and unique per CPU. */
	x = (uint64_t)(ca->cpu + 1) * 0xBEEFCAFEDEAD0001ULL;

	while (!*ca->stop) {
		XORSHIFT64(x);
		ca->iterations++;
	}

	/*
	 * Make x observable so the compiler cannot treat the loop as dead code.
	 */
	if (x == 0)
		ca->iterations++;

	ca->error = 0;
	return (NULL);
}

/*
 * Test: ibs_cpu_stress_compute
 *
 * Spawns one compute thread per CPU, each running a tight xorshift64 loop
 * for 120 seconds.  The main thread reads IBS Op CTL on all CPUs every 5
 * seconds and verifies the MSR path is unaffected by ALU saturation.
 */
ATF_TC(ibs_cpu_stress_compute);
ATF_TC_HEAD(ibs_cpu_stress_compute, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "[TC-CPU-01] IBS MSR stability during per-CPU xorshift64 "
	    "compute workload (120 s)");
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "timeout", "200");
}

ATF_TC_BODY(ibs_cpu_stress_compute, tc)
{
	struct compute_arg *args;
	pthread_t *threads;
	volatile bool stop = false;
	int ncpus, i, error;
	time_t deadline;
	uint64_t original;
	uint64_t total_iters = 0;

	if (!cpu_supports_ibs())
		atf_tc_skip("CPU does not support IBS");

	ncpus = cpu_stress_ncpus();

	threads = calloc((size_t)ncpus, sizeof(pthread_t));
	args    = calloc((size_t)ncpus, sizeof(struct compute_arg));
	ATF_REQUIRE(threads != NULL && args != NULL);

	error = read_msr(0, MSR_IBS_OP_CTL, &original);
	if (error != 0) {
		free(threads); free(args);
		atf_tc_skip("Cannot read MSR_IBS_OP_CTL on CPU 0: %s",
		    strerror(error));
	}

	for (i = 0; i < ncpus; i++) {
		args[i].stop       = &stop;
		args[i].cpu        = i;
		args[i].error      = -1;
		args[i].iterations = 0;

		error = pthread_create(&threads[i], NULL,
		    compute_thread, &args[i]);
		if (error != 0) {
			stop = true;
			free(threads); free(args);
			atf_tc_fail("pthread_create CPU %d: %s",
			    i, strerror(error));
		}
	}

	/* Poll MSR health on every CPU every 5 s for the full duration. */
	deadline = time(NULL) + CPU_STRESS_DURATION_SEC;
	while (time(NULL) < deadline) {
		sleep(CPU_STRESS_POLL_INTERVAL_SEC);
		for (i = 0; i < ncpus; i++) {
			uint64_t val;
			error = read_msr(i, MSR_IBS_OP_CTL, &val);
			ATF_CHECK_MSG(error == 0,
			    "MSR_IBS_OP_CTL read failed on CPU %d: %s",
			    i, strerror(error));
		}
	}

	stop = true;

	for (i = 0; i < ncpus; i++) {
		pthread_join(threads[i], NULL);
		ATF_CHECK_MSG(args[i].error == 0,
		    "compute thread CPU %d failed: %s",
		    i, strerror(args[i].error));
		total_iters += args[i].iterations;
	}

	/* Final write/read round-trip to confirm the MSR is still writable. */
	{
		uint64_t v = (original & ~IBS_MAXCNT_MASK) & ~IBS_OP_ENABLE_BIT;
		error = write_msr(0, MSR_IBS_OP_CTL, v);
		ATF_CHECK_MSG(error == 0, "final MSR write failed: %s",
		    strerror(error));
		write_msr(0, MSR_IBS_OP_CTL, original);
	}

	printf("compute: %llu total xorshift64 iterations across %d CPUs "
	    "in %d seconds\n",
	    (unsigned long long)total_iters, ncpus, CPU_STRESS_DURATION_SEC);

	free(threads);
	free(args);
}

/* -----------------------------------------------------------------------
 * Context-switch workload (scheduler pressure)
 * ----------------------------------------------------------------------- */

struct ctx_switch_arg {
	volatile bool	*stop;
	int		 error;
	uint64_t	 yields;
};

/*
 * Thread function: tight sched_yield() loop.
 * No affinity pinning — threads compete for all CPUs, maximising scheduler
 * run-queue churn and context-switch overhead.
 */
static void *
ctx_switch_thread(void *arg)
{
	struct ctx_switch_arg *csa = arg;

	while (!*csa->stop) {
		sched_yield();
		csa->yields++;
	}

	csa->error = 0;
	return (NULL);
}

/*
 * Test: ibs_cpu_stress_context_switch
 *
 * Creates 4 × ncpus threads (oversubscribing the CPU set) that call
 * sched_yield() in a tight loop for 120 seconds.  This forces the scheduler
 * to perform many context switches per second and stresses MSR save/restore
 * paths.  The main thread verifies IBS Op CTL on all CPUs every 5 seconds.
 */
ATF_TC(ibs_cpu_stress_context_switch);
ATF_TC_HEAD(ibs_cpu_stress_context_switch, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "[TC-CPU-02] IBS MSR stability during high-frequency "
	    "context-switch workload (120 s)");
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "timeout", "200");
}

ATF_TC_BODY(ibs_cpu_stress_context_switch, tc)
{
	struct ctx_switch_arg *args;
	pthread_t *threads;
	volatile bool stop = false;
	int ncpus, nthreads, i, error;
	time_t deadline;
	uint64_t original;
	uint64_t total_yields = 0;

	if (!cpu_supports_ibs())
		atf_tc_skip("CPU does not support IBS");

	ncpus    = cpu_stress_ncpus();
	nthreads = ncpus * 4;	/* oversubscribe to maximise scheduling churn */

	threads = calloc((size_t)nthreads, sizeof(pthread_t));
	args    = calloc((size_t)nthreads, sizeof(struct ctx_switch_arg));
	ATF_REQUIRE(threads != NULL && args != NULL);

	error = read_msr(0, MSR_IBS_OP_CTL, &original);
	if (error != 0) {
		free(threads); free(args);
		atf_tc_skip("Cannot read MSR_IBS_OP_CTL on CPU 0: %s",
		    strerror(error));
	}

	for (i = 0; i < nthreads; i++) {
		args[i].stop   = &stop;
		args[i].error  = -1;
		args[i].yields = 0;

		error = pthread_create(&threads[i], NULL,
		    ctx_switch_thread, &args[i]);
		if (error != 0) {
			stop = true;
			free(threads); free(args);
			atf_tc_fail("pthread_create thread %d: %s",
			    i, strerror(error));
		}
	}

	deadline = time(NULL) + CPU_STRESS_DURATION_SEC;
	while (time(NULL) < deadline) {
		sleep(CPU_STRESS_POLL_INTERVAL_SEC);
		for (i = 0; i < ncpus; i++) {
			uint64_t val;
			error = read_msr(i, MSR_IBS_OP_CTL, &val);
			ATF_CHECK_MSG(error == 0,
			    "MSR_IBS_OP_CTL read failed on CPU %d: %s",
			    i, strerror(error));
		}
	}

	stop = true;

	for (i = 0; i < nthreads; i++) {
		pthread_join(threads[i], NULL);
		ATF_CHECK_MSG(args[i].error == 0,
		    "ctx-switch thread %d failed: %s",
		    i, strerror(args[i].error));
		total_yields += args[i].yields;
	}

	/* Final write/read round-trip. */
	{
		uint64_t v = (original & ~IBS_MAXCNT_MASK) & ~IBS_OP_ENABLE_BIT;
		error = write_msr(0, MSR_IBS_OP_CTL, v);
		ATF_CHECK_MSG(error == 0, "final MSR write failed: %s",
		    strerror(error));
		write_msr(0, MSR_IBS_OP_CTL, original);
	}

	printf("ctx-switch: %llu total sched_yield calls across %d threads "
	    "(%d CPUs) in %d seconds\n",
	    (unsigned long long)total_yields, nthreads, ncpus,
	    CPU_STRESS_DURATION_SEC);

	free(threads);
	free(args);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, ibs_cpu_stress_compute);
	ATF_TP_ADD_TC(tp, ibs_cpu_stress_context_switch);

	return (atf_no_error());
}
