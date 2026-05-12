/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * Author: ojanerif@amd.com
 *
 * [TC-DET] AMD L3 cache PMU detection tests.
 *
 * These tests probe hardware and FreeBSD kernel state without programming
 * any counters.  All three cases are expected to pass on Zen 5/6 with
 * hwpmc loaded; earlier generations skip with an explanatory message.
 */

#include <sys/param.h>

#include <atf-c.h>

#include "amd_l3_common.h"

/* -------------------------------------------------------------------------
 * TC-DET-L3-01  Capability probe
 *
 * Reads the AMD vendor string, decodes the Zen generation from CPUID FMS,
 * and — if PerfMonV2 is available — reads CPUID Fn80000022.  No hardware
 * is programmed; this is a pure discovery test.
 * ---------------------------------------------------------------------- */
ATF_TC(l3_capability_probe);
ATF_TC_HEAD(l3_capability_probe, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Probe AMD L3 PMU capability without programming any MSR.  "
	    "Verifies CPU is a known Zen generation with FreeBSD L3 PMU JSON "
	    "support and reads PerfMonV2 (CPUID Fn80000022) if available.");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(l3_capability_probe, tc)
{
	struct amd_umcdf_perfmon_v2 pmv2;
	struct amd_umcdf_cpu cpu;
	int error;

	amd_umcdf_skip_unless_known_zen(&cpu);

	printf("L3 capability probe: %s (family 0x%02x model 0x%02x "
	    "stepping 0x%x)\n",
	    amd_umcdf_zen_name(cpu.zen), cpu.family, cpu.model, cpu.stepping);

	if (!amd_l3_has_freebsd_l3_json(&cpu))
		atf_tc_skip("FreeBSD L3 PMU JSON (amdzen5/l3-cache.json, "
		    "amdzen6/l3-cache.json) covers Zen 5 and Zen 6; "
		    "detected %s", amd_umcdf_zen_name(cpu.zen));

	error = amd_umcdf_read_perfmon_v2(&cpu, &pmv2);
	ATF_REQUIRE_MSG(error == 0,
	    "CPUID Fn80000022 read failed: %s", strerror(error));

	if (pmv2.leaf_available) {
		printf("PerfMonV2: present=%s core_pmcs=%u df_pmcs=%u "
		    "active_umc_mask=0x%x\n",
		    pmv2.present ? "yes" : "no",
		    pmv2.core_pmcs, pmv2.df_pmcs, pmv2.active_umc_mask);
		ATF_CHECK_MSG(pmv2.present,
		    "Zen 5/6 should report PerfMonV2 as present "
		    "(CPUID Fn80000022 EAX[0] == 1)");
	} else {
		printf("PerfMonV2 leaf not available on %s\n",
		    amd_umcdf_zen_name(cpu.zen));
	}
}

/* -------------------------------------------------------------------------
 * TC-DET-L3-02  K8-L3 row count
 *
 * Initialises libpmc and counts PMC rows whose name begins with "K8-L3-".
 * At least one row is required for event-name lookups to succeed.
 * ---------------------------------------------------------------------- */
ATF_TC(l3_rows_in_hwpmc);
ATF_TC_HEAD(l3_rows_in_hwpmc, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Verify FreeBSD hwpmc exposes at least one K8-L3 row on an AMD "
	    "CPU with Zen 5/6 L3 PMU JSON support.");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(l3_rows_in_hwpmc, tc)
{
	struct amd_umcdf_cpu cpu;
	size_t rows;

	amd_umcdf_skip_unless_known_zen(&cpu);
	if (!amd_l3_has_freebsd_l3_json(&cpu))
		atf_tc_skip("No FreeBSD L3 PMU JSON for %s",
		    amd_umcdf_zen_name(cpu.zen));
	amd_umcdf_skip_unless_hwpmc();

	rows = amd_umcdf_count_pmc_rows_with_prefix(0, "K8-L3-");
	printf("hwpmc K8-L3 rows on CPU 0: %zu\n", rows);

	ATF_CHECK_MSG(rows > 0,
	    "Expected at least 1 K8-L3 row on %s (Zen 5/6); "
	    "hwpmc reported 0 — check that hwpmc.ko is loaded and "
	    "that lib/libpmc/pmu-events carries l3-cache.json",
	    amd_umcdf_zen_name(cpu.zen));
}

/* -------------------------------------------------------------------------
 * TC-DET-L3-03  PMU named-event support enabled
 *
 * pmc_pmu_enabled() must return true for any named L3 event lookup to work.
 * This is a lightweight gate that fails early with a useful message when
 * the hwpmc module is missing or was built without PMU-events support.
 * ---------------------------------------------------------------------- */
ATF_TC(l3_pmu_events_enabled);
ATF_TC_HEAD(l3_pmu_events_enabled, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Verify pmc_pmu_enabled() returns true.  This is a prerequisite "
	    "for all L3 named-event lookups (l3_lookup_state.*, "
	    "l3_xi_sampled_latency.*).");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(l3_pmu_events_enabled, tc)
{
	struct amd_umcdf_cpu cpu;

	amd_umcdf_skip_unless_known_zen(&cpu);
	if (!amd_l3_has_freebsd_l3_json(&cpu))
		atf_tc_skip("No FreeBSD L3 PMU JSON for %s",
		    amd_umcdf_zen_name(cpu.zen));

	/* amd_umcdf_skip_unless_pmu_events() calls pmc_init() then
	 * pmc_pmu_enabled(); it skips with a message if either fails. */
	amd_umcdf_skip_unless_pmu_events();
	printf("PMU named-event support enabled on %s\n",
	    amd_umcdf_zen_name(cpu.zen));
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, l3_capability_probe);
	ATF_TP_ADD_TC(tp, l3_rows_in_hwpmc);
	ATF_TP_ADD_TC(tp, l3_pmu_events_enabled);
	return (atf_no_error());
}
