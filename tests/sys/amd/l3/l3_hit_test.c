/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * Author: ojanerif@amd.com
 *
 * [TC-UNC-L3-02] AMD L3 cache hit counter tests.
 *
 * Primary event: l3_lookup_state.l3_hit  (EventSel=0x04, UMask=0xfe)
 * Fallback:      l3_lookup_state.all_coherent_accesses_to_l3 (UMask=0xff)
 *
 * Workload: 2 MB buffer accessed sequentially, repeated 128 times.  After
 * the warm-up pass the entire working set fits in L3 and subsequent
 * accesses are L3 hits.
 */

#include <sys/param.h>

#include <atf-c.h>

#include "amd_l3_common.h"

/*
 * Event candidate list for L3 hit counting.
 * UMask 0xfe counts all L3 lookups that hit; UMask 0xff is a superset
 * (hits + misses) and is accepted as a fallback for counter availability.
 */
static const struct amd_umcdf_event_candidate l3_hit_events[] = {
	{
		"l3_lookup_state.l3_hit",
		"L3 cache hits, Unit=L3PMC, EventSel=0x04 UMask=0xfe",
		0x04, 0xfe
	},
	{
		"l3_lookup_state.all_coherent_accesses_to_l3",
		"all coherent L3 accesses (hit superset), Unit=L3PMC, "
		"EventSel=0x04 UMask=0xff",
		0x04, 0xff
	},
	{ NULL, NULL, 0, 0 }
};

/* -------------------------------------------------------------------------
 * TC-UNC-L3-02a  L3 hit PMU metadata contract
 *
 * Same contract as the miss metadata test: verifies the L3 hit event name
 * appears in the FreeBSD PMU JSON (EOPNOTSUPP acceptable; ENOENT for all
 * candidates indicates missing JSON).
 * ---------------------------------------------------------------------- */
ATF_TC(l3_hit_pmu_metadata_contract);
ATF_TC_HEAD(l3_hit_pmu_metadata_contract, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Verify l3_lookup_state.l3_hit reaches libpmc and returns either "
	    "a valid allocation descriptor (runtime available) or EOPNOTSUPP "
	    "(metadata present, no backend).  ENOENT for all candidates "
	    "indicates missing l3-cache.json.");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(l3_hit_pmu_metadata_contract, tc)
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

	for (event = l3_hit_events; event->name != NULL; event++) {
		memset(&cfg, 0, sizeof(cfg));
		error = pmc_pmu_pmcallocate(event->name, &cfg);
		if (error == 0) {
			printf("L3 hit event '%s': allocation descriptor "
			    "available (class=%d)\n",
			    event->name, cfg.pm_class);
			return;
		}
		printf("L3 hit event '%s': pmc_pmu_pmcallocate returned "
		    "%s\n", event->name, strerror(error));
		if (error == EOPNOTSUPP)
			saw_eopnotsupp = true;
		else if (error == ENOENT)
			saw_enoent = true;
	}

	ATF_REQUIRE_MSG(!saw_enoent || saw_eopnotsupp,
	    "All L3 hit event candidates returned ENOENT: "
	    "l3-cache.json may be absent from the pmu-events table for %s",
	    amd_umcdf_zen_name(cpu.zen));

	if (saw_eopnotsupp)
		printf("L3 hit metadata present; runtime allocation not yet "
		    "available on this tree\n");
}

/* -------------------------------------------------------------------------
 * TC-UNC-L3-02b  L3 hit runtime smoke
 *
 * Allocates the first available hit-event counter, warms a 2 MB buffer
 * into L3, then drives 128 sequential traversals that stay in L3 cache.
 * Verifies the counter is monotonically non-decreasing.
 * ---------------------------------------------------------------------- */
ATF_TC(l3_hit_runtime_smoke);
ATF_TC_HEAD(l3_hit_runtime_smoke, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Smoke test: allocate l3_lookup_state.l3_hit, warm a 2 MB buffer "
	    "into L3, drive 128 sequential traversals that stay hot in cache, "
	    "and verify the counter is monotonically non-decreasing.");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(l3_hit_runtime_smoke, tc)
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

	event = amd_umcdf_pick_pmu_event(l3_hit_events, &cfg, &last_error);
	if (event == NULL)
		atf_tc_skip("No allocatable L3 hit event in this libpmc/hwpmc "
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

	error = amd_l3_generate_hit_traffic();
	if (error != 0) {
		(void)pmc_stop(pmcid);
		amd_umcdf_release_pmc(pmcid);
		atf_tc_fail("hit traffic generator failed: %s", strerror(error));
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

	printf("L3 hit counter '%s': before=%ju after=%ju delta=%ju\n",
	    event->name, (uintmax_t)before, (uintmax_t)after,
	    (uintmax_t)(after - before));

	ATF_CHECK_MSG(after >= before,
	    "L3 hit counter moved backwards: before=%ju after=%ju",
	    (uintmax_t)before, (uintmax_t)after);
	amd_umcdf_release_pmc(pmcid);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, l3_hit_pmu_metadata_contract);
	ATF_TP_ADD_TC(tp, l3_hit_runtime_smoke);
	return (atf_no_error());
}
