/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * Author: ojanerif@amd.com
 *
 * Pure AMD UMC/DF decode helpers — no hardware I/O, no OS-specific headers.
 * Include only <stdbool.h> and <stdint.h>.  Safe to use on any platform.
 *
 * These helpers are the prerequisite for the [TC-UNIT-*] unit test tier.
 * Hardware accessors (amd_umcdf_do_cpuid, amd_umcdf_read_cpu, pmc_*) remain
 * in amd_umcdf_common.h and must NOT be included here.
 */

#ifndef _AMD_UMCDF_DECODE_H_
#define	_AMD_UMCDF_DECODE_H_

#include <stdbool.h>
#include <stdint.h>

/* -----------------------------------------------------------------------
 * CPUID leaf constants
 * ----------------------------------------------------------------------- */
#define	AMD_UMCDF_CPUID_VENDOR		0x00000000U
#define	AMD_UMCDF_CPUID_FMS		0x00000001U
#define	AMD_UMCDF_CPUID_EXTMAX		0x80000000U
#define	AMD_UMCDF_CPUID_EXTFEATURES	0x80000001U
#define	AMD_UMCDF_CPUID_EXTPERFMON	0x80000022U

/* -----------------------------------------------------------------------
 * AMD vendor string constants (returned by CPUID leaf 0)
 * EBX="Auth", EDX="enti", ECX="cAMD" → "AuthenticAMD"
 * ----------------------------------------------------------------------- */
#define	AMD_UMCDF_VENDOR_EBX		0x68747541U /* "Auth" */
#define	AMD_UMCDF_VENDOR_EDX		0x69746e65U /* "enti" */
#define	AMD_UMCDF_VENDOR_ECX		0x444d4163U /* "cAMD" */

/* AMD feature2 ECX (CPUID 0x80000001) PerfNxtCore bit */
#define	AMD_UMCDF_ID2_PNXC		(1U << 24)

/* -----------------------------------------------------------------------
 * PerfMonV2 (CPUID Fn80000022) parsing macros
 * The EBX register of this leaf contains per-class PMC counts.
 * ----------------------------------------------------------------------- */
#define	AMD_UMCDF_EXTPERFMON_PRESENT		0x1U
#define	AMD_UMCDF_EXTPERFMON_CORE_PMCS(x)	((x) & 0x0fU)
#define	AMD_UMCDF_EXTPERFMON_LBR_V2_DEPTH(x)	(((x) >> 4) & 0x3fU)
#define	AMD_UMCDF_EXTPERFMON_DF_PMCS(x)		(((x) >> 10) & 0x3fU)

/* -----------------------------------------------------------------------
 * DF PMC count defaults
 * ----------------------------------------------------------------------- */
#define	AMD_UMCDF_DF_DEFAULT_PMCS	4U
#define	AMD_UMCDF_MAX_DF_PMCS		64U
#define	AMD_UMCDF_CACHELINE		64U

/* -----------------------------------------------------------------------
 * DF1 event/unit mask encoding (Zen 1 – Zen 3+)
 *
 * AMD PPR DF1 PerfEventSel format:
 *   bits  7:0  EventSelect[7:0]   → PERFEVTSEL bits  7:0
 *   bits 11:8  EventSelect[11:8]  → PERFEVTSEL bits 35:32
 *   bits 13:12 EventSelect[13:12] → PERFEVTSEL bits 60:59
 *   bits  7:0  UnitMask[7:0]      → PERFEVTSEL bits 15:8
 * ----------------------------------------------------------------------- */
#define	AMD_UMCDF_DF1_TO_EVENTMASK(x)	(((x) & 0xffU) | \
	(((uint64_t)(x) & 0x0f00U) << 24) | \
	(((uint64_t)(x) & 0x3000U) << 47))
#define	AMD_UMCDF_DF1_TO_UNITMASK(x)	(((x) & 0xffU) << 8)

/* -----------------------------------------------------------------------
 * DF2 event/unit mask encoding (Zen 4+)
 *
 * AMD PPR DF2 PerfEventSel format:
 *   bits  7:0  EventSelect[7:0]   → PERFEVTSEL bits  7:0
 *   bits 14:8  EventSelect[14:8]  → PERFEVTSEL bits 38:32
 *   bits  7:0  UnitMask[7:0]      → PERFEVTSEL bits 15:8
 *   bits 11:8  UnitMask[11:8]     → PERFEVTSEL bits 27:24
 * ----------------------------------------------------------------------- */
#define	AMD_UMCDF_DF2_TO_EVENTMASK(x)	(((x) & 0xffU) | \
	(((uint64_t)(x) & 0x7f00U) << 24))
#define	AMD_UMCDF_DF2_TO_UNITMASK(x)	((((x) & 0xffU) << 8) | \
	(((uint64_t)(x) & 0x0f00U) << 16))

/* -----------------------------------------------------------------------
 * Zen generation enumeration
 * Values are ordered oldest-to-newest so comparisons like zen >= ZEN_4 work.
 * ----------------------------------------------------------------------- */
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

/* -----------------------------------------------------------------------
 * CPU descriptor (populated by amd_umcdf_read_cpu() in amd_umcdf_common.h)
 * ----------------------------------------------------------------------- */
struct amd_umcdf_cpu {
	uint32_t	family;
	uint32_t	model;
	uint32_t	stepping;
	uint32_t	ext_high;
	uint32_t	amd_feature2_ecx;
	enum amd_umcdf_zen_generation zen;
	bool		is_amd;
};

/* -----------------------------------------------------------------------
 * PerfMonV2 descriptor (populated by amd_umcdf_read_perfmon_v2())
 * ----------------------------------------------------------------------- */
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

/* -----------------------------------------------------------------------
 * PMU event candidate (used by amd_umcdf_pick_pmu_event())
 * ----------------------------------------------------------------------- */
struct amd_umcdf_event_candidate {
	const char	*name;
	const char	*reason;
	uint32_t	event_code;
	uint32_t	umask;
};

/* -----------------------------------------------------------------------
 * Pure helper functions — no I/O, no system calls
 * ----------------------------------------------------------------------- */

/*
 * Map a human-readable name to an enum amd_umcdf_zen_generation value.
 */
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

/*
 * Map an AMD CPU family+model pair to a Zen generation.
 * Pure function — no hardware access required.
 */
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

/* Returns true if the CPU has AMD PerfNxtCore (DF PMC) support. */
static inline bool
amd_umcdf_has_df_feature(const struct amd_umcdf_cpu *cpu)
{
	return ((cpu->amd_feature2_ecx & AMD_UMCDF_ID2_PNXC) != 0);
}

/* Returns true if FreeBSD ships UMC PMU event JSON for this generation. */
static inline bool
amd_umcdf_has_freebsd_umc_json(const struct amd_umcdf_cpu *cpu)
{
	return (cpu->zen == AMD_UMCDF_ZEN_4 ||
	    cpu->zen == AMD_UMCDF_ZEN_5 || cpu->zen == AMD_UMCDF_ZEN_6);
}

/* Returns true if FreeBSD ships DF PMU event JSON for this generation. */
static inline bool
amd_umcdf_has_freebsd_df_json(const struct amd_umcdf_cpu *cpu)
{
	return (cpu->zen == AMD_UMCDF_ZEN_1 ||
	    cpu->zen == AMD_UMCDF_ZEN_PLUS || cpu->zen == AMD_UMCDF_ZEN_2 ||
	    cpu->zen == AMD_UMCDF_ZEN_3 || cpu->zen == AMD_UMCDF_ZEN_3_PLUS ||
	    cpu->zen == AMD_UMCDF_ZEN_4 || cpu->zen == AMD_UMCDF_ZEN_5);
}

/*
 * Return the expected 64-bit hwpmc config value for a given DF event on this
 * CPU.  Uses DF2 encoding for Zen 4 and later; DF1 for all earlier Zen.
 */
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

#endif /* _AMD_UMCDF_DECODE_H_ */
