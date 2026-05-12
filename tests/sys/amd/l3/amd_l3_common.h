/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * Author: ojanerif@amd.com
 *
 * Common AMD L3 cache PMU test helpers.
 *
 * Reuses the UMCDF infrastructure (Zen-generation detection, CPUID access,
 * PMC lifecycle helpers) via an -I flag pointing at tests/sys/amd/umcdf/.
 * L3-specific workload generators are defined here.
 *
 * Pure decode helpers (no I/O) remain in amd_umcdf_decode.h and are pulled
 * in transitively through amd_umcdf_common.h.
 */

#ifndef _AMD_L3_COMMON_H_
#define	_AMD_L3_COMMON_H_

#include "amd_umcdf_common.h"

/*
 * Workload sizing.
 *
 * MISS_SIZE: 64 MB with a page stride defeats the hardware prefetcher and
 * overflows the L3 of most desktop/laptop AMD CPUs (typically 32–96 MB).
 * Server parts with very large L3 (e.g. Genoa, 3D V-Cache) will see fewer
 * misses; the counter monotonicity check remains valid regardless.
 *
 * HIT_SIZE: 2 MB fits comfortably in L3 on any Zen CPU.  Repeated sequential
 * traversal keeps the working set hot.
 */
#define	AMD_L3_MISS_BUFFER_SIZE		(64 * 1024 * 1024UL)
#define	AMD_L3_MISS_PAGE_STRIDE		4096U	/* defeats HW prefetcher */
#define	AMD_L3_MISS_ROUNDS		4U

#define	AMD_L3_HIT_BUFFER_SIZE		(2 * 1024 * 1024UL)
#define	AMD_L3_HIT_ROUNDS		128U

/*
 * Returns true if FreeBSD ships L3 PMU event JSON for this Zen generation.
 * Based on the pmu-events JSON files present in the tree (amdzen5/l3-cache.json
 * and amdzen6/l3-cache.json).  Extend when earlier generations are added.
 */
static inline bool
amd_l3_has_freebsd_l3_json(const struct amd_umcdf_cpu *cpu)
{
	return (cpu->zen == AMD_UMCDF_ZEN_5 || cpu->zen == AMD_UMCDF_ZEN_6);
}

/*
 * Generate a cache-unfriendly workload to drive L3 misses.
 *
 * Allocates AMD_L3_MISS_BUFFER_SIZE bytes and accesses it with
 * AMD_L3_MISS_PAGE_STRIDE stride across AMD_L3_MISS_ROUNDS passes.
 * Page-stride access defeats the hardware stream prefetcher and causes
 * capacity misses once the working set exceeds L3.
 */
static inline int
amd_l3_generate_miss_traffic(void)
{
	uint8_t *buf;
	size_t i;
	unsigned int r;

	buf = (uint8_t *)malloc(AMD_L3_MISS_BUFFER_SIZE);
	if (buf == NULL)
		return (ENOMEM);

	/* Populate: brings the entire buffer into memory. */
	memset(buf, 0x5a, AMD_L3_MISS_BUFFER_SIZE);

	/*
	 * Page-stride passes: accesses are spaced by 4 KB so adjacent
	 * accesses target different cache sets, preventing line reuse.
	 * The sink accumulation prevents the compiler from eliding reads.
	 */
	for (r = 0; r < AMD_L3_MISS_ROUNDS; r++) {
		for (i = 0; i < AMD_L3_MISS_BUFFER_SIZE;
		    i += AMD_L3_MISS_PAGE_STRIDE) {
			buf[i] = (uint8_t)(buf[i] ^ (uint8_t)(r + (i >> 12)));
			amd_umcdf_sink += buf[i];
		}
	}

	free(buf);
	return (0);
}

/*
 * Generate a cache-friendly workload to drive L3 hits.
 *
 * Allocates AMD_L3_HIT_BUFFER_SIZE bytes (2 MB), warms it into L3 with
 * a sequential pass, then repeatedly traverses it at cacheline stride.
 * After the warm-up pass the entire working set fits in L3 and each
 * subsequent access is an L3 hit (assuming no cache pressure from other
 * CPU activities).
 */
static inline int
amd_l3_generate_hit_traffic(void)
{
	uint8_t *buf;
	size_t i;
	unsigned int r;

	buf = (uint8_t *)malloc(AMD_L3_HIT_BUFFER_SIZE);
	if (buf == NULL)
		return (ENOMEM);

	/* Warm-up: load the working set into L3. */
	memset(buf, 0xa5, AMD_L3_HIT_BUFFER_SIZE);

	/* Repeated sequential traversal at cacheline granularity. */
	for (r = 0; r < AMD_L3_HIT_ROUNDS; r++) {
		for (i = 0; i < AMD_L3_HIT_BUFFER_SIZE;
		    i += AMD_UMCDF_CACHELINE) {
			buf[i] = (uint8_t)(buf[i] + (uint8_t)(r + i));
			amd_umcdf_sink += buf[i];
		}
	}

	free(buf);
	return (0);
}

#endif /* _AMD_L3_COMMON_H_ */
