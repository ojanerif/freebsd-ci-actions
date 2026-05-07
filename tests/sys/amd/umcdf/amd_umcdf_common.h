/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * Author: Davi Chaves Azevedo
 *
 * Common AMD UMC/DF hwpmc test helpers.
 *
 * These tests intentionally detect the AMD family/model/stepping before any
 * PMU-event or hwpmc allocation work.  Family 1Ah is split between Zen 5 and
 * Zen 6, so model decoding is part of the test contract.
 */

#ifndef _AMD_UMCDF_COMMON_H_
#define	_AMD_UMCDF_COMMON_H_

#include <sys/param.h>
#include <sys/cpuctl.h>
#include <sys/ioctl.h>
#include <sys/pmc.h>

#include <machine/pmc_mdep.h>

#include <atf-c.h>

#include <errno.h>
#include <fcntl.h>
#include <pmc.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#define	AMD_UMCDF_CPUID_VENDOR		0x00000000U
#define	AMD_UMCDF_CPUID_FMS		0x00000001U
#define	AMD_UMCDF_CPUID_EXTMAX		0x80000000U
#define	AMD_UMCDF_CPUID_EXTFEATURES	0x80000001U
#define	AMD_UMCDF_CPUID_EXTPERFMON	0x80000022U

#define	AMD_UMCDF_VENDOR_EBX		0x68747541U /* Auth */
#define	AMD_UMCDF_VENDOR_EDX		0x69746e65U /* enti */
#define	AMD_UMCDF_VENDOR_ECX		0x444d4163U /* cAMD */

#define	AMD_UMCDF_ID2_PNXC		(1U << 24)

#define	AMD_UMCDF_EXTPERFMON_PRESENT	0x1U
#define	AMD_UMCDF_EXTPERFMON_CORE_PMCS(x)	((x) & 0x0fU)
#define	AMD_UMCDF_EXTPERFMON_LBR_V2_DEPTH(x)	(((x) >> 4) & 0x3fU)
#define	AMD_UMCDF_EXTPERFMON_DF_PMCS(x)		(((x) >> 10) & 0x3fU)

#define	AMD_UMCDF_DF1_TO_EVENTMASK(x)	(((x) & 0xffU) | \
	(((uint64_t)(x) & 0x0f00U) << 24) | \
	(((uint64_t)(x) & 0x3000U) << 47))
#define	AMD_UMCDF_DF1_TO_UNITMASK(x)	(((x) & 0xffU) << 8)

#define	AMD_UMCDF_DF2_TO_EVENTMASK(x)	(((x) & 0xffU) | \
	(((uint64_t)(x) & 0x7f00U) << 24))
#define	AMD_UMCDF_DF2_TO_UNITMASK(x)	((((x) & 0xffU) << 8) | \
	(((uint64_t)(x) & 0x0f00U) << 16))

#define	AMD_UMCDF_DF_DEFAULT_PMCS	4U
#define	AMD_UMCDF_MAX_DF_PMCS		64U
#define	AMD_UMCDF_CACHELINE		64U

enum amd_umcdf_zen_generation {
	AMD_UMCDF_ZEN_PRE_ZEN = 0,
	AMD_UMCDF_ZEN_1,
	AMD_UMCDF_ZEN_PLUS,
	AMD_UMCDF_ZEN_2,
	AMD_UMCDF_ZEN_3,
	AMD_UMCDF_ZEN_3_PLUS,
	AMD_UMCDF_ZEN_4,
	AMD_UMCDF_ZEN_5,
	AMD_UMCDF_ZEN_6,
	AMD_UMCDF_ZEN_FUTURE,
	AMD_UMCDF_ZEN_UNKNOWN
};

struct amd_umcdf_cpu {
	uint32_t	family;
	uint32_t	model;
	uint32_t	stepping;
	uint32_t	ext_high;
	uint32_t	amd_feature2_ecx;
	enum amd_umcdf_zen_generation zen;
	bool		is_amd;
};

struct amd_umcdf_perfmon_v2 {
	uint32_t	raw_eax;
	uint32_t	raw_ebx;
	uint32_t	raw_ecx;
	uint32_t	raw_edx;
	uint32_t	core_pmcs;
	uint32_t	df_pmcs;
	uint32_t	lbr_v2_depth;
	uint32_t	active_umc_mask;
	bool		leaf_available;
	bool		present;
};

struct amd_umcdf_event_candidate {
	const char	*name;
	const char	*reason;
	uint32_t	event_code;
	uint32_t	umask;
};

static volatile uint64_t amd_umcdf_sink;

static inline const char *
amd_umcdf_zen_name(enum amd_umcdf_zen_generation zen)
{
	switch (zen) {
	case AMD_UMCDF_ZEN_PRE_ZEN:
		return ("pre-Zen");
	case AMD_UMCDF_ZEN_1:
		return ("Zen 1");
	case AMD_UMCDF_ZEN_PLUS:
		return ("Zen+");
	case AMD_UMCDF_ZEN_2:
		return ("Zen 2");
	case AMD_UMCDF_ZEN_3:
		return ("Zen 3");
	case AMD_UMCDF_ZEN_3_PLUS:
		return ("Zen 3+");
	case AMD_UMCDF_ZEN_4:
		return ("Zen 4");
	case AMD_UMCDF_ZEN_5:
		return ("Zen 5");
	case AMD_UMCDF_ZEN_6:
		return ("Zen 6");
	case AMD_UMCDF_ZEN_FUTURE:
		return ("future AMD Zen-family generation");
	case AMD_UMCDF_ZEN_UNKNOWN:
	default:
		return ("unknown AMD generation");
	}
}

static inline int
amd_umcdf_do_cpuid(uint32_t level, uint32_t regs[4])
{
	cpuctl_cpuid_args_t args;
	int error, fd;

	fd = open("/dev/cpuctl0", O_RDONLY);
	if (fd < 0)
		return (errno);

	memset(&args, 0, sizeof(args));
	args.level = level;
	if (ioctl(fd, CPUCTL_CPUID, &args) < 0) {
		error = errno;
		(void)close(fd);
		return (error);
	}

	regs[0] = args.data[0];
	regs[1] = args.data[1];
	regs[2] = args.data[2];
	regs[3] = args.data[3];
	(void)close(fd);
	return (0);
}

static inline enum amd_umcdf_zen_generation
amd_umcdf_map_zen(uint32_t family, uint32_t model)
{
	if (family < 0x17)
		return (AMD_UMCDF_ZEN_PRE_ZEN);
	if (family > 0x1a)
		return (AMD_UMCDF_ZEN_FUTURE);

	switch (family) {
	case 0x17:
		if (model >= 0x08 && model <= 0x0f)
			return (AMD_UMCDF_ZEN_PLUS);
		if ((model >= 0x01 && model <= 0x07) ||
		    (model >= 0x11 && model <= 0x17))
			return (AMD_UMCDF_ZEN_1);
		if (model >= 0x18 && model <= 0x1f)
			return (AMD_UMCDF_ZEN_PLUS);
		if ((model >= 0x31 && model <= 0x3f) ||
		    (model >= 0x60 && model <= 0x6f) ||
		    (model >= 0x71 && model <= 0x7f) ||
		    (model >= 0x90 && model <= 0x9f) ||
		    (model >= 0xa0 && model <= 0xaf))
			return (AMD_UMCDF_ZEN_2);
		break;
	case 0x19:
		if ((model >= 0x10 && model <= 0x1f) ||
		    (model >= 0x60 && model <= 0x7f) ||
		    (model >= 0xa0 && model <= 0xaf))
			return (AMD_UMCDF_ZEN_4);
		if (model >= 0x40 && model <= 0x4f)
			return (AMD_UMCDF_ZEN_3_PLUS);
		if (model <= 0x0f || (model >= 0x20 && model <= 0x2f) ||
		    (model >= 0x50 && model <= 0x5f))
			return (AMD_UMCDF_ZEN_3);
		break;
	case 0x1a:
		if ((model >= 0x50 && model <= 0x5f) ||
		    (model >= 0x80 && model <= 0xaf) ||
		    (model >= 0xc0 && model <= 0xcf))
			return (AMD_UMCDF_ZEN_6);
		if (model <= 0x2f || (model >= 0x40 && model <= 0x4f) ||
		    (model >= 0x60 && model <= 0x7f))
			return (AMD_UMCDF_ZEN_5);
		break;
	}

	return (AMD_UMCDF_ZEN_UNKNOWN);
}

static inline int
amd_umcdf_read_cpu(struct amd_umcdf_cpu *cpu)
{
	uint32_t base_family, base_model, ext_family, ext_model;
	uint32_t regs[4];
	int error;

	memset(cpu, 0, sizeof(*cpu));
	cpu->zen = AMD_UMCDF_ZEN_UNKNOWN;

	error = amd_umcdf_do_cpuid(AMD_UMCDF_CPUID_VENDOR, regs);
	if (error != 0)
		return (error);

	cpu->is_amd = regs[1] == AMD_UMCDF_VENDOR_EBX &&
	    regs[3] == AMD_UMCDF_VENDOR_EDX && regs[2] == AMD_UMCDF_VENDOR_ECX;
	if (!cpu->is_amd)
		return (0);

	error = amd_umcdf_do_cpuid(AMD_UMCDF_CPUID_FMS, regs);
	if (error != 0)
		return (error);

	base_family = (regs[0] >> 8) & 0x0f;
	ext_family = (regs[0] >> 20) & 0xff;
	base_model = (regs[0] >> 4) & 0x0f;
	ext_model = (regs[0] >> 16) & 0x0f;
	cpu->stepping = regs[0] & 0x0f;
	cpu->family = (base_family == 0x0f) ? base_family + ext_family :
	    base_family;
	cpu->model = (base_family == 0x0f || base_family == 0x06) ?
	    (ext_model << 4) | base_model : base_model;
	cpu->zen = amd_umcdf_map_zen(cpu->family, cpu->model);

	error = amd_umcdf_do_cpuid(AMD_UMCDF_CPUID_EXTMAX, regs);
	if (error != 0)
		return (error);
	cpu->ext_high = regs[0];

	if (cpu->ext_high >= AMD_UMCDF_CPUID_EXTFEATURES) {
		error = amd_umcdf_do_cpuid(AMD_UMCDF_CPUID_EXTFEATURES, regs);
		if (error != 0)
			return (error);
		cpu->amd_feature2_ecx = regs[2];
	}

	return (0);
}

static inline void
amd_umcdf_skip_unless_amd(struct amd_umcdf_cpu *cpu)
{
	int error;

	error = amd_umcdf_read_cpu(cpu);
	if (error != 0)
		atf_tc_skip("Cannot query CPUID via /dev/cpuctl0: %s",
		    strerror(error));
	if (!cpu->is_amd)
		atf_tc_skip("CPU vendor is not AuthenticAMD");
}

static inline void
amd_umcdf_skip_unless_known_zen(struct amd_umcdf_cpu *cpu)
{
	amd_umcdf_skip_unless_amd(cpu);
	if (cpu->zen == AMD_UMCDF_ZEN_PRE_ZEN)
		atf_tc_skip("AMD Family %02xh is pre-Zen; UMC/DF test is not applicable",
		    cpu->family);
	if (cpu->zen == AMD_UMCDF_ZEN_FUTURE)
		atf_tc_skip("AMD Family %02xh is newer than the validated Zen map; "
		    "probe only after checking the PPR", cpu->family);
	if (cpu->zen == AMD_UMCDF_ZEN_UNKNOWN)
		atf_tc_skip("AMD Family %02xh Model %02xh is not in the validated "
		    "Zen map", cpu->family, cpu->model);
}

static inline void
amd_umcdf_skip_unless_hwpmc(void)
{
	if (pmc_init() < 0)
		atf_tc_skip("pmc_init() failed; is hwpmc loaded? %s",
		    strerror(errno));
}

static inline void
amd_umcdf_skip_unless_pmu_events(void)
{
	amd_umcdf_skip_unless_hwpmc();
	if (!pmc_pmu_enabled())
		atf_tc_skip("pmu-events support is not enabled in this hwpmc setup");
}

static inline bool
amd_umcdf_has_df_feature(const struct amd_umcdf_cpu *cpu)
{
	return ((cpu->amd_feature2_ecx & AMD_UMCDF_ID2_PNXC) != 0);
}

static inline bool
amd_umcdf_has_freebsd_umc_json(const struct amd_umcdf_cpu *cpu)
{
	return (cpu->zen == AMD_UMCDF_ZEN_4 ||
	    cpu->zen == AMD_UMCDF_ZEN_5 || cpu->zen == AMD_UMCDF_ZEN_6);
}

static inline int
amd_umcdf_read_perfmon_v2(const struct amd_umcdf_cpu *cpu,
    struct amd_umcdf_perfmon_v2 *pmv2)
{
	uint32_t regs[4];
	int error;

	memset(pmv2, 0, sizeof(*pmv2));
	if (cpu->ext_high < AMD_UMCDF_CPUID_EXTPERFMON)
		return (0);

	error = amd_umcdf_do_cpuid(AMD_UMCDF_CPUID_EXTPERFMON, regs);
	if (error != 0)
		return (error);

	pmv2->leaf_available = true;
	pmv2->raw_eax = regs[0];
	pmv2->raw_ebx = regs[1];
	pmv2->raw_ecx = regs[2];
	pmv2->raw_edx = regs[3];
	pmv2->present = (regs[0] & AMD_UMCDF_EXTPERFMON_PRESENT) != 0;
	pmv2->core_pmcs = AMD_UMCDF_EXTPERFMON_CORE_PMCS(regs[1]);
	pmv2->lbr_v2_depth = AMD_UMCDF_EXTPERFMON_LBR_V2_DEPTH(regs[1]);
	pmv2->df_pmcs = AMD_UMCDF_EXTPERFMON_DF_PMCS(regs[1]);
	pmv2->active_umc_mask = regs[2];
	return (0);
}

static inline bool
amd_umcdf_has_freebsd_df_json(const struct amd_umcdf_cpu *cpu)
{
	return (cpu->zen == AMD_UMCDF_ZEN_1 ||
	    cpu->zen == AMD_UMCDF_ZEN_PLUS || cpu->zen == AMD_UMCDF_ZEN_2 ||
	    cpu->zen == AMD_UMCDF_ZEN_3 || cpu->zen == AMD_UMCDF_ZEN_3_PLUS ||
	    cpu->zen == AMD_UMCDF_ZEN_4 || cpu->zen == AMD_UMCDF_ZEN_5);
}

static inline uint64_t
amd_umcdf_expected_df_config(const struct amd_umcdf_cpu *cpu,
    const struct amd_umcdf_event_candidate *event)
{
	if (cpu->zen >= AMD_UMCDF_ZEN_4)
		return (AMD_UMCDF_DF2_TO_EVENTMASK(event->event_code) |
		    AMD_UMCDF_DF2_TO_UNITMASK(event->umask));

	return (AMD_UMCDF_DF1_TO_EVENTMASK(event->event_code) |
	    AMD_UMCDF_DF1_TO_UNITMASK(event->umask));
}

static inline size_t
amd_umcdf_count_pmc_rows_with_prefix(int cpu, const char *prefix)
{
	struct pmc_pmcinfo *pmcinfo;
	size_t count, prefix_len;
	int i, npmc;

	ATF_REQUIRE_MSG(pmc_pmcinfo(cpu, &pmcinfo) == 0,
	    "pmc_pmcinfo(%d) failed: %s", cpu, strerror(errno));
	npmc = pmc_npmc(cpu);
	if (npmc < 0) {
		free(pmcinfo);
		atf_tc_fail("pmc_npmc(%d) failed: %s", cpu, strerror(errno));
	}

	count = 0;
	prefix_len = strlen(prefix);
	for (i = 0; i < npmc; i++) {
		if (strncmp(pmcinfo->pm_pmcs[i].pm_name, prefix,
		    prefix_len) == 0)
			count++;
	}

	free(pmcinfo);
	return (count);
}

static inline const struct amd_umcdf_event_candidate *
amd_umcdf_pick_pmu_event(const struct amd_umcdf_event_candidate *candidates,
    struct pmc_op_pmcallocate *cfg, int *last_error)
{
	const struct amd_umcdf_event_candidate *cand;
	int error;

	if (last_error != NULL)
		*last_error = ENOENT;
	for (cand = candidates; cand->name != NULL; cand++) {
		memset(cfg, 0, sizeof(*cfg));
		error = pmc_pmu_pmcallocate(cand->name, cfg);
		if (error == 0)
			return (cand);
		if (last_error != NULL)
			*last_error = error;
	}

	return (NULL);
}

static inline int
amd_umcdf_generate_memory_traffic(size_t bytes, unsigned int rounds)
{
	uint8_t *buf;
	size_t i;
	unsigned int r;

	if (bytes < AMD_UMCDF_CACHELINE)
		return (EINVAL);
	buf = malloc(bytes);
	if (buf == NULL)
		return (ENOMEM);

	memset(buf, 0xa5, bytes);
	for (r = 0; r < rounds; r++) {
		for (i = 0; i < bytes; i += AMD_UMCDF_CACHELINE) {
			buf[i] = (uint8_t)(buf[i] + r + (i >> 6));
			amd_umcdf_sink += buf[i];
		}
	}

	free(buf);
	return (0);
}

static inline void
amd_umcdf_release_pmc(pmc_id_t pmcid)
{
	if (pmcid != PMC_ID_INVALID)
		ATF_REQUIRE_MSG(pmc_release(pmcid) == 0,
		    "pmc_release(%u) failed: %s", pmcid, strerror(errno));
}

#endif /* _AMD_UMCDF_COMMON_H_ */
