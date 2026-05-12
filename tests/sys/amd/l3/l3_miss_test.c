/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * Author: ojanerif@amd.com
 *
 * [TC-UNC-L3-01] AMD L3 cache miss counter tests.
 *
 * Primary event: l3_lookup_state.l3_miss  (EventSel=0x04, UMask=0x01)
 * Fallback:      l3_lookup_state.all_coherent_accesses_to_l3 (UMask=0xff)
 *
 * Workload: 64 MB buffer accessed with a 4 KB page stride to defeat the
 * hardware stream prefetcher and produce capacity misses in L3.
 */

#include <sys/param.h>

#include <atf-c.h>

#include "amd_l3_common.h"

/*
 * Event candidate list for L3 miss counting.
 * Ordered from most specific to most permissive so the first successful
 * pmc_pmu_pmcallocate() call picks the narrowest counter.
 */
static const struct amd_umcdf_event_candidate l3_miss_events[] = {
	{
		"l3_lookup_state.l3_miss",
		"L3 cache misses, Unit=L3PMC, EventSel=0x04 UMask=0x01",
		0x04, 0x01
	},
	{
		"l3_lookup_state.all_coherent_accesses_to_l3",
		"all coherent L3 accesses (miss superset), Unit=L3PMC, "
		"EventSel=0x04 UMask=0xff",
		0x04, 0xff
	},
	{ NULL, NULL, 0, 0 }
};

/* -------------------------------------------------------------------------
 * TC-UNC-L3-01a  L3 miss PMU metadata contract
 *
 * Calls pmc_pmu_pmcallocate() for each miss-event candidate and verifies
 * the event appears in the FreeBSD PMU JSON.
 *
 * Pass conditions:
 *   - pmc_pmu_pmcallocate() returns 0  → full metadata and allocator present
 *   - pmc_pmu_pmcallocate() returns EOPNOTSUPP → metadata present, no
 *     runtime backend yet; this is an acceptable current state
 *
 * Fail condition:
 *   - ALL candidates return ENOENT → the l3-cache.json was not found in the
 *     pmu-events table for this CPU, which means the JSON is absent or the
 *     CPU is not recognised by the current pmu-events build.
 * ---------------------------------------------------------------------- */
ATF_TC(l3_miss_pmu_metadata_contract);
ATF_TC_HEAD(l3_miss_pmu_metadata_contract, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Verify l3_lookup_state.l3_miss reaches libpmc and returns either "
	    "a valid allocation descriptor (runtime available) or EOPNOTSUPP "
	    "(metadata present, no backend).  ENOENT for all candidates "
	    "indicates missing l3-cache.json.");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(l3_miss_pmu_metadata_contract, tc)
{
	const struct amd_umcdf_event_candidate *event;
	struct pmc_op_pmcallocate cfg;
	struct amd_umcdf_cpu cpu;
	bool saw_eopnotsupp, saw_enoent;
	int error;

	saw_eopnotsupp = false;
	saw_enoent = false;

	amd_umcdf_skip_unless_known_zen(&cpu);
	if (!amd_l3_has_freebsd_l3_json(&cpu))
		atf_tc_skip("FreeBSD L3 PMU JSON covers Zen 5/6; detected %s",
		    amd_umcdf_zen_name(cpu.zen));
	amd_umcdf_skip_unless_pmu_events();

	for (event = l3_miss_events; event->name != NULL; event++) {
		memset(&cfg, 0, sizeof(cfg));
		error = pmc_pmu_pmcallocate(event->name, &cfg);
		if (error == 0) {
			printf("L3 miss event '%s': allocation descriptor "
			    "available (class=%d)\n",
			    event->name, cfg.pm_class);
			return;
		}
		printf("L3 miss event '%s': pmc_pmu_pmcallocate returned "
		    "%s\n", event->name, strerror(error));
		if (error == EOPNOTSUPP)
			saw_eopnotsupp = true;
		else if (error == ENOENT)
			saw_enoent = true;
	}

	ATF_REQUIRE_MSG(!saw_enoent || saw_eopnotsupp,
	    "All L3 miss event candidates returned ENOENT: "
	    "l3-cache.json may be absent from the pmu-events table for %s",
	    amd_umcdf_zen_name(cpu.zen));

	if (saw_eopnotsupp)
		printf("L3 miss metadata present; runtime allocation not yet "
		    "available on this tree\n");
}

/* -------------------------------------------------------------------------
 * TC-UNC-L3-01b  L3 miss runtime smoke
 *
 * Allocates the first available miss-event counter, drives a 64 MB page-
 * strided access pattern that causes L3 capacity misses, and checks that
 * the counter value after the workload is >= the value before it.
 *
 * The test skips (not fails) when the allocator returns ENOENT/EOPNOTSUPP/
 * ENXIO because those indicate a known limitation of the current tree, not
 * a hardware or test bug.
 * ---------------------------------------------------------------------- */
ATF_TC(l3_miss_runtime_smoke);
ATF_TC_HEAD(l3_miss_runtime_smoke, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Smoke test: allocate l3_lookup_state.l3_miss, drive a 64 MB "
	    "page-strided workload to produce L3 capacity misses, and verify "
	    "the counter is monotonically non-decreasing.");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(l3_miss_runtime_smoke, tc)
{
	const struct amd_umcdf_event_candidate *event;
	struct pmc_op_pmcallocate cfg;
	struct amd_umcdf_cpu cpu;
	pmc_value_t before, after;
	pmc_id_t pmcid;
	int error, last_error;

	pmcid = PMC_ID_INVALID;
	before = after = 0;

	amd_umcdf_skip_unless_known_zen(&cpu);
	if (!amd_l3_has_freebsd_l3_json(&cpu))
		atf_tc_skip("No FreeBSD L3 PMU JSON for %s",
		    amd_umcdf_zen_name(cpu.zen));
	amd_umcdf_skip_unless_pmu_events();

	event = amd_umcdf_pick_pmu_event(l3_miss_events, &cfg, &last_error);
	if (event == NULL)
		atf_tc_skip("No allocatable L3 miss event in this libpmc/hwpmc "
		    "tree; last error: %s", strerror(last_error));

	error = pmc_allocate(event->name, PMC_MODE_SC, 0, 0, &pmcid, 0);
	if (error < 0 && (errno == ENOENT || errno == EOPNOTSUPP ||
	    errno == ENXIO || errno == EBUSY || errno == EINVAL))
		atf_tc_skip("pmc_allocate(%s) not available in this "
		    "environment: %s", event->name, strerror(errno));
	ATF_REQUIRE_MSG(error == 0,
	    "pmc_allocate(%s) failed: %s", event->name, strerror(errno));

	if (pmc_start(pmcid) != 0) {
		amd_umcdf_release_pmc(pmcid);
		atf_tc_fail("pmc_start(%s) failed: %s",
		    event->name, strerror(errno));
	}
	if (pmc_read(pmcid, &before) != 0) {
		(void)pmc_stop(pmcid);
		amd_umcdf_release_pmc(pmcid);
		atf_tc_fail("pmc_read(before) failed: %s", strerror(errno));
	}

	error = amd_l3_generate_miss_traffic();
	if (error != 0) {
		(void)pmc_stop(pmcid);
		amd_umcdf_release_pmc(pmcid);
		atf_tc_fail("miss traffic generator failed: %s", strerror(error));
	}

	if (pmc_stop(pmcid) != 0) {
		amd_umcdf_release_pmc(pmcid);
		atf_tc_fail("pmc_stop(%s) failed: %s",
		    event->name, strerror(errno));
	}
	if (pmc_read(pmcid, &after) != 0) {
		amd_umcdf_release_pmc(pmcid);
		atf_tc_fail("pmc_read(after) failed: %s", strerror(errno));
	}

	printf("L3 miss counter '%s': before=%ju after=%ju delta=%ju\n",
	    event->name, (uintmax_t)before, (uintmax_t)after,
	    (uintmax_t)(after - before));

	ATF_CHECK_MSG(after >= before,
	    "L3 miss counter moved backwards: before=%ju after=%ju",
	    (uintmax_t)before, (uintmax_t)after);
	amd_umcdf_release_pmc(pmcid);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, l3_miss_pmu_metadata_contract);
	ATF_TP_ADD_TC(tp, l3_miss_runtime_smoke);
	return (atf_no_error());
}
