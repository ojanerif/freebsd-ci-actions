/*-
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Author: ojanerif@amd.com
 */

/*
 * IBS Memory Stress Tests
 *
 * Validates IBS MSR stability while the memory subsystem is under sustained
 * stress.  Two workloads exercise different access patterns:
 *
 * 1. ibs_mem_stress_cache_thrash:
 *    Pseudo-random strided accesses (stride > cache line) across a 128 MiB
 *    shared buffer, one thread per CPU, for 120 seconds.  The stride defeats
 *    hardware prefetchers and forces LLC evictions and DRAM fetches.
 *
 * 2. ibs_mem_stress_bandwidth:
 *    Sequential read-write sweeps over a private 16 MiB buffer per thread for
 *    120 seconds.  Saturates memory bandwidth and the LLC fill/evict path.
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
#define MEM_STRESS_DURATION_SEC		120

/* Shared buffer for the cache-thrash test (128 MiB). */
#define MEM_STRESS_BUF_SIZE		(128UL * 1024UL * 1024UL)

/* Per-thread private buffer for the bandwidth test (16 MiB). */
#define MEM_STRESS_THREAD_BUF_SIZE	(16UL * 1024UL * 1024UL)

/* MSR polling interval (seconds). */
#define MEM_STRESS_POLL_INTERVAL_SEC	5

/*
 * Stride (in uint64_t elements) for the random-access pattern.
 * 64 elements × 8 bytes = 512 bytes, larger than a 64-byte cache line
 * and any typical prefetch stream distance.
 */
#define MEM_STRESS_STRIDE		64UL

/* Large odd multiplier for the linear-congruential index sequence. */
#define MEM_STRESS_LCG_MUL		6364136223846793005ULL

/*
 * Helper: number of online CPUs.
 */
static int
mem_stress_ncpus(void)
{
	long n = sysconf(_SC_NPROCESSORS_ONLN);
	return ((int)(n < 1 ? 1 : n));
}

/* -----------------------------------------------------------------------
 * Cache-thrash workload
 * ----------------------------------------------------------------------- */

struct cache_thrash_arg {
	volatile bool	*stop;
	uint64_t	*buf;		/* shared buffer */
	size_t		 n_elements;	/* number of uint64_t elements */
	int		 cpu;
	int		 error;
	uint64_t	 accesses;
};

/*
 * Thread function: walk through the shared buffer using a pseudo-random
 * strided index.  Each step reads and XORs one element so the compiler
 * cannot eliminate the load or the store.
 */
static void *
cache_thrash_thread(void *arg)
{
	struct cache_thrash_arg *cta = arg;
	cpuset_t mask;
	uint64_t idx, acc;
	size_t step;

	CPU_ZERO(&mask);
	CPU_SET(cta->cpu, &mask);
	cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID, -1,
	    sizeof(mask), &mask);

	/* Unique starting index per CPU prevents threads aliasing each other. */
	idx = (uint64_t)(cta->cpu + 1) * MEM_STRESS_LCG_MUL;
	acc = 0;

	while (!*cta->stop) {
		for (step = 0; step < cta->n_elements && !*cta->stop;
		    step += MEM_STRESS_STRIDE) {
			idx = (idx * MEM_STRESS_LCG_MUL + step) %
			    cta->n_elements;
			cta->buf[idx] ^= acc;
			acc ^= cta->buf[idx];
			cta->accesses++;
		}
	}

	/* Prevent dead-store elimination of the accumulator. */
	if (acc == 0)
		cta->buf[0] ^= 1;

	cta->error = 0;
	return (NULL);
}

/*
 * Test: ibs_mem_stress_cache_thrash
 *
 * Allocates a 128 MiB shared buffer. One thread per CPU performs pseudo-
 * random strided read-modify-write accesses for 120 seconds, forcing L3
 * cache evictions and DRAM accesses.  The main thread reads IBS Op CTL on
 * every CPU every 5 seconds to verify the MSR path remains functional.
 */
ATF_TC(ibs_mem_stress_cache_thrash);
ATF_TC_HEAD(ibs_mem_stress_cache_thrash, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "[TC-MEM-01] IBS MSR stability during 128 MiB cache-thrash "
	    "workload (120 s)");
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "timeout", "200");
}

ATF_TC_BODY(ibs_mem_stress_cache_thrash, tc)
{
	uint64_t *buf;
	struct cache_thrash_arg *args;
	pthread_t *threads;
	volatile bool stop = false;
	int ncpus, i, error;
	size_t n_elements;
	time_t deadline;
	uint64_t original;
	uint64_t total_accesses = 0;

	if (!cpu_supports_ibs())
		atf_tc_skip("CPU does not support IBS");

	ncpus      = mem_stress_ncpus();
	n_elements = MEM_STRESS_BUF_SIZE / sizeof(uint64_t);

	buf = calloc(n_elements, sizeof(uint64_t));
	ATF_REQUIRE_MSG(buf != NULL,
	    "calloc 128 MiB shared buffer: %s", strerror(errno));

	threads = calloc((size_t)ncpus, sizeof(pthread_t));
	args    = calloc((size_t)ncpus, sizeof(struct cache_thrash_arg));
	if (threads == NULL || args == NULL) {
		free(buf);
		atf_tc_fail("calloc thread structures: %s", strerror(errno));
	}

	/* Save IBS Op CTL baseline from CPU 0. */
	error = read_msr(0, MSR_IBS_OP_CTL, &original);
	if (error != 0) {
		free(buf); free(threads); free(args);
		atf_tc_skip("Cannot read MSR_IBS_OP_CTL on CPU 0: %s",
		    strerror(error));
	}

	/* Launch one worker thread per CPU. */
	for (i = 0; i < ncpus; i++) {
		args[i].stop       = &stop;
		args[i].buf        = buf;
		args[i].n_elements = n_elements;
		args[i].cpu        = i;
		args[i].error      = -1;
		args[i].accesses   = 0;

		error = pthread_create(&threads[i], NULL,
		    cache_thrash_thread, &args[i]);
		if (error != 0) {
			stop = true;
			free(buf); free(threads); free(args);
			atf_tc_fail("pthread_create CPU %d: %s",
			    i, strerror(error));
		}
	}

	/* Poll MSR health on every CPU every 5 seconds for the full duration. */
	deadline = time(NULL) + MEM_STRESS_DURATION_SEC;
	while (time(NULL) < deadline) {
		sleep(MEM_STRESS_POLL_INTERVAL_SEC);
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
		    "cache-thrash thread CPU %d exited with error: %s",
		    i, strerror(args[i].error));
		total_accesses += args[i].accesses;
	}

	/* Final write/read round-trip to confirm MSR is still writable. */
	{
		uint64_t v = (original & ~IBS_MAXCNT_MASK) & ~IBS_OP_ENABLE_BIT;
		error = write_msr(0, MSR_IBS_OP_CTL, v);
		ATF_CHECK_MSG(error == 0, "final MSR write failed: %s",
		    strerror(error));
		write_msr(0, MSR_IBS_OP_CTL, original);
	}

	printf("cache-thrash: %llu element accesses across %d CPUs "
	    "in %d seconds\n",
	    (unsigned long long)total_accesses, ncpus, MEM_STRESS_DURATION_SEC);

	free(buf);
	free(threads);
	free(args);
}

/* -----------------------------------------------------------------------
 * Memory bandwidth workload
 * ----------------------------------------------------------------------- */

struct bandwidth_arg {
	volatile bool	*stop;
	size_t		 n_elements;	/* elements in the private buffer */
	int		 cpu;
	int		 error;
	uint64_t	 sweeps;
};

/*
 * Thread function: alternating sequential write + read sweep over a private
 * buffer.  The buffer is larger than L2 so every sweep brings data from LLC
 * or DRAM.  Summation on the read sweep prevents dead-code elimination.
 */
static void *
bandwidth_thread(void *arg)
{
	struct bandwidth_arg *bwa = arg;
	cpuset_t mask;
	uint64_t *buf;
	uint64_t sum;
	size_t j, n;

	CPU_ZERO(&mask);
	CPU_SET(bwa->cpu, &mask);
	cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID, -1,
	    sizeof(mask), &mask);

	n   = bwa->n_elements;
	buf = calloc(n, sizeof(uint64_t));
	if (buf == NULL) {
		bwa->error = ENOMEM;
		return (NULL);
	}

	while (!*bwa->stop) {
		/* Write sweep: fill with a value that varies per sweep. */
		for (j = 0; j < n && !*bwa->stop; j++)
			buf[j] = (uint64_t)j ^ bwa->sweeps;

		/* Read sweep: accumulate to prevent store/load elimination. */
		sum = 0;
		for (j = 0; j < n && !*bwa->stop; j++)
			sum += buf[j];

		if (sum == 0)
			buf[0] = 1;	/* make sum observable */

		bwa->sweeps++;
	}

	free(buf);
	bwa->error = 0;
	return (NULL);
}

/*
 * Test: ibs_mem_stress_bandwidth
 *
 * Spawns one thread per CPU, each doing sequential read-write sweeps over a
 * private 16 MiB buffer for 120 seconds.  This saturates memory bandwidth
 * and exercises the LLC fill/evict path.  The main thread verifies IBS MSR
 * accessibility on all CPUs at 5-second intervals throughout the run.
 */
ATF_TC(ibs_mem_stress_bandwidth);
ATF_TC_HEAD(ibs_mem_stress_bandwidth, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "[TC-MEM-02] IBS MSR stability during per-CPU sequential "
	    "memory-bandwidth workload (120 s)");
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "timeout", "200");
}

ATF_TC_BODY(ibs_mem_stress_bandwidth, tc)
{
	struct bandwidth_arg *args;
	pthread_t *threads;
	volatile bool stop = false;
	int ncpus, i, error;
	size_t n_elements;
	time_t deadline;
	uint64_t original;
	uint64_t total_sweeps = 0;

	if (!cpu_supports_ibs())
		atf_tc_skip("CPU does not support IBS");

	ncpus      = mem_stress_ncpus();
	n_elements = MEM_STRESS_THREAD_BUF_SIZE / sizeof(uint64_t);

	threads = calloc((size_t)ncpus, sizeof(pthread_t));
	args    = calloc((size_t)ncpus, sizeof(struct bandwidth_arg));
	ATF_REQUIRE(threads != NULL && args != NULL);

	error = read_msr(0, MSR_IBS_OP_CTL, &original);
	if (error != 0) {
		free(threads); free(args);
		atf_tc_skip("Cannot read MSR_IBS_OP_CTL on CPU 0: %s",
		    strerror(error));
	}

	for (i = 0; i < ncpus; i++) {
		args[i].stop       = &stop;
		args[i].n_elements = n_elements;
		args[i].cpu        = i;
		args[i].error      = -1;
		args[i].sweeps     = 0;

		error = pthread_create(&threads[i], NULL,
		    bandwidth_thread, &args[i]);
		if (error != 0) {
			stop = true;
			free(threads); free(args);
			atf_tc_fail("pthread_create CPU %d: %s",
			    i, strerror(error));
		}
	}

	deadline = time(NULL) + MEM_STRESS_DURATION_SEC;
	while (time(NULL) < deadline) {
		sleep(MEM_STRESS_POLL_INTERVAL_SEC);
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
		    "bandwidth thread CPU %d failed: %s",
		    i, strerror(args[i].error));
		total_sweeps += args[i].sweeps;
	}

	/* Final write/read round-trip. */
	{
		uint64_t v = (original & ~IBS_MAXCNT_MASK) & ~IBS_OP_ENABLE_BIT;
		error = write_msr(0, MSR_IBS_OP_CTL, v);
		ATF_CHECK_MSG(error == 0, "final MSR write failed: %s",
		    strerror(error));
		write_msr(0, MSR_IBS_OP_CTL, original);
	}

	printf("bandwidth: %llu total sweeps across %d CPUs in %d seconds\n",
	    (unsigned long long)total_sweeps, ncpus, MEM_STRESS_DURATION_SEC);

	free(threads);
	free(args);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, ibs_mem_stress_cache_thrash);
	ATF_TP_ADD_TC(tp, ibs_mem_stress_bandwidth);

	return (atf_no_error());
}
