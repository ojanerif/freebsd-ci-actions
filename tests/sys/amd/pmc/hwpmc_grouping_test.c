/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * Author: Davi Chaves Azevedo
 */

#include <sys/param.h>
#include <sys/pmc.h>

#include <atf-c.h>

#include <errno.h>
#include <pmc.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../amd_pmc_test_common.h"
#include "../umcdf/amd_umcdf_common.h"

static volatile uint64_t pmc_grouping_sink;

static void
generate_core_pmu_workload(void)
{
	uint64_t i, v;

	v = pmc_grouping_sink;
	for (i = 0; i < 1000000; i++)
		v += (i ^ (v << 1)) + 1;
	pmc_grouping_sink = v;
}

static void
require_pmu_core_event(const char *name, struct pmc_op_pmcallocate *cfg)
{
	int error;

	memset(cfg, 0, sizeof(*cfg));
	error = pmc_pmu_pmcallocate(name, cfg);
	if (error != 0)
		atf_tc_skip("PMU event %s is unavailable: %s", name,
		    strerror(error));
	ATF_REQUIRE_MSG((cfg->pm_flags & PMC_F_EV_PMU) != 0,
	    "PMU event %s did not set PMC_F_EV_PMU", name);
	ATF_REQUIRE_MSG(cfg->pm_class == PMC_CLASS_K8,
	    "PMU event %s mapped to class %d, expected PMC_CLASS_K8", name,
	    cfg->pm_class);
}

static void
require_row_disposition(pmc_id_t pmcid, enum pmc_disp expected,
    const char *context)
{
	struct pmc_pmcinfo *pmcinfo;
	unsigned int row;
	int npmc;

	row = PMC_ID_TO_ROWINDEX(pmcid);
	npmc = pmc_npmc(0);
	ATF_REQUIRE_MSG(npmc > 0, "pmc_npmc(0) failed: %s", strerror(errno));
	ATF_REQUIRE_MSG(row < (unsigned int)npmc,
	    "%s allocated invalid row %u, npmc=%d", context, row, npmc);

	ATF_REQUIRE_MSG(pmc_pmcinfo(0, &pmcinfo) == 0,
	    "pmc_pmcinfo(0) failed: %s", strerror(errno));
	ATF_CHECK_MSG(pmcinfo->pm_pmcs[row].pm_rowdisp == expected,
	    "%s row %u disposition is %s, expected %s", context, row,
	    pmc_name_of_disposition(pmcinfo->pm_pmcs[row].pm_rowdisp),
	    pmc_name_of_disposition(expected));
	free(pmcinfo);
}

ATF_TC(row_disposition_tracks_process_and_system_pmc);
ATF_TC_HEAD(row_disposition_tracks_process_and_system_pmc, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Verify hwpmc row disposition exposes FreeBSD's real PMC "
	    "resource grouping: THREAD for process PMCs and STANDALONE for "
	    "system PMCs.");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(row_disposition_tracks_process_and_system_pmc, tc)
{
	pmc_id_t pmcid;

	pmcid = PMC_ID_INVALID;
	amd_test_skip_unless_hwpmc();

	ATF_REQUIRE_MSG(pmc_allocate("SOFT-CLOCK.HARD", PMC_MODE_TC, 0,
	    PMC_CPU_ANY, &pmcid, 0) == 0,
	    "pmc_allocate(SOFT-CLOCK.HARD, TC) failed: %s", strerror(errno));
	require_row_disposition(pmcid, PMC_DISP_THREAD, "process-counting PMC");
	amd_test_release_pmc(pmcid);
	pmcid = PMC_ID_INVALID;

	ATF_REQUIRE_MSG(pmc_allocate("SOFT-CLOCK.HARD", PMC_MODE_SC, 0, 0,
	    &pmcid, 0) == 0,
	    "pmc_allocate(SOFT-CLOCK.HARD, SC) failed: %s", strerror(errno));
	require_row_disposition(pmcid, PMC_DISP_STANDALONE,
	    "system-counting PMC");
	amd_test_release_pmc(pmcid);
}

ATF_TC(amd_pmu_core_events_allocate_concurrently);
ATF_TC_HEAD(amd_pmu_core_events_allocate_concurrently, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Verify two portable AMD Zen core PMU events allocate together as "
	    "distinct hwpmc rows.");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(amd_pmu_core_events_allocate_concurrently, tc)
{
	struct pmc_op_pmcallocate cycles_cfg, instr_cfg;
	struct amd_umcdf_cpu cpu;
	pmc_value_t cycles_after, cycles_before, instr_after, instr_before;
	pmc_id_t cycles_pmc, instr_pmc;
	bool cycles_attached, cycles_started, instr_attached, instr_started;
	const char *failure;
	pid_t self;
	int error, failure_errno;

	cycles_pmc = PMC_ID_INVALID;
	instr_pmc = PMC_ID_INVALID;
	cycles_attached = cycles_started = false;
	instr_attached = instr_started = false;
	cycles_after = cycles_before = instr_after = instr_before = 0;
	failure = NULL;
	failure_errno = 0;
	self = getpid();

	amd_umcdf_skip_unless_known_zen(&cpu);
	amd_umcdf_skip_unless_pmu_events();

	require_pmu_core_event("instructions", &instr_cfg);
	require_pmu_core_event("unhalted-cycles", &cycles_cfg);

	error = pmc_allocate("instructions", PMC_MODE_TC, 0, PMC_CPU_ANY,
	    &instr_pmc, 0);
	if (error != 0 && (errno == EBUSY || errno == ENXIO || errno == ENOENT ||
	    errno == EOPNOTSUPP))
		atf_tc_skip("pmc_allocate(instructions) failed: %s",
		    strerror(errno));
	ATF_REQUIRE_MSG(error == 0, "pmc_allocate(instructions) failed: %s",
	    strerror(errno));

	error = pmc_allocate("unhalted-cycles", PMC_MODE_TC, 0, PMC_CPU_ANY,
	    &cycles_pmc, 0);
	if (error != 0 && (errno == EBUSY || errno == ENXIO || errno == ENOENT ||
	    errno == EOPNOTSUPP)) {
		amd_test_release_pmc(instr_pmc);
		atf_tc_skip("pmc_allocate(unhalted-cycles) failed: %s",
		    strerror(errno));
	}
	if (error != 0) {
		failure = "pmc_allocate(unhalted-cycles) failed";
		failure_errno = errno;
		goto out;
	}

	if (PMC_ID_TO_ROWINDEX(instr_pmc) == PMC_ID_TO_ROWINDEX(cycles_pmc)) {
		failure = "instructions and unhalted-cycles used the same row";
		goto out;
	}
	if (pmc_attach(instr_pmc, self) != 0) {
		failure = "pmc_attach(instructions) failed";
		failure_errno = errno;
		goto out;
	}
	instr_attached = true;
	if (pmc_attach(cycles_pmc, self) != 0) {
		failure = "pmc_attach(unhalted-cycles) failed";
		failure_errno = errno;
		goto out;
	}
	cycles_attached = true;

	if (pmc_read(instr_pmc, &instr_before) != 0) {
		failure = "pmc_read(instructions before) failed";
		failure_errno = errno;
		goto out;
	}
	if (pmc_read(cycles_pmc, &cycles_before) != 0) {
		failure = "pmc_read(unhalted-cycles before) failed";
		failure_errno = errno;
		goto out;
	}
	if (pmc_start(instr_pmc) != 0) {
		failure = "pmc_start(instructions) failed";
		failure_errno = errno;
		goto out;
	}
	instr_started = true;
	if (pmc_start(cycles_pmc) != 0) {
		failure = "pmc_start(unhalted-cycles) failed";
		failure_errno = errno;
		goto out;
	}
	cycles_started = true;

	generate_core_pmu_workload();

	if (pmc_stop(cycles_pmc) != 0) {
		failure = "pmc_stop(unhalted-cycles) failed";
		failure_errno = errno;
		cycles_started = false;
		goto out;
	}
	cycles_started = false;
	if (pmc_stop(instr_pmc) != 0) {
		failure = "pmc_stop(instructions) failed";
		failure_errno = errno;
		instr_started = false;
		goto out;
	}
	instr_started = false;
	if (pmc_read(instr_pmc, &instr_after) != 0) {
		failure = "pmc_read(instructions after) failed";
		failure_errno = errno;
		goto out;
	}
	if (pmc_read(cycles_pmc, &cycles_after) != 0) {
		failure = "pmc_read(unhalted-cycles after) failed";
		failure_errno = errno;
		goto out;
	}
	if (instr_after <= instr_before)
		failure = "instructions did not increase during workload";
	else if (cycles_after <= cycles_before)
		failure = "unhalted-cycles did not increase during workload";

out:
	if (cycles_started)
		(void)pmc_stop(cycles_pmc);
	if (instr_started)
		(void)pmc_stop(instr_pmc);
	if (cycles_attached)
		(void)pmc_detach(cycles_pmc, self);
	if (instr_attached)
		(void)pmc_detach(instr_pmc, self);
	amd_test_release_pmc(cycles_pmc);
	amd_test_release_pmc(instr_pmc);
	if (failure != NULL && failure_errno != 0)
		atf_tc_fail("%s: %s", failure, strerror(failure_errno));
	else if (failure != NULL)
		atf_tc_fail("%s", failure);
}

ATF_TC(system_sampling_requires_logfile_before_start);
ATF_TC_HEAD(system_sampling_requires_logfile_before_start, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Verify system sampling PMCs fail cleanly at pmc_start() when no "
	    "hwpmc log file is configured.");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(system_sampling_requires_logfile_before_start, tc)
{
	pmc_id_t pmcid;
	int error, start_errno;

	pmcid = PMC_ID_INVALID;
	amd_test_skip_unless_hwpmc();

	ATF_REQUIRE_MSG(pmc_allocate("SOFT-CLOCK.HARD", PMC_MODE_SS, 0, 0,
	    &pmcid, 10000) == 0,
	    "pmc_allocate(SOFT-CLOCK.HARD, SS) failed: %s", strerror(errno));
	errno = 0;
	error = pmc_start(pmcid);
	start_errno = errno;
	if (error == 0) {
		(void)pmc_stop(pmcid);
		amd_test_release_pmc(pmcid);
		atf_tc_fail("pmc_start() succeeded without a configured log file");
	}
	if (start_errno != EDOOFUS) {
		amd_test_release_pmc(pmcid);
		atf_tc_fail("pmc_start() failed with %s, expected EDOOFUS",
		    strerror(start_errno));
	}
	amd_test_release_pmc(pmcid);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, row_disposition_tracks_process_and_system_pmc);
	ATF_TP_ADD_TC(tp, amd_pmu_core_events_allocate_concurrently);
	ATF_TP_ADD_TC(tp, system_sampling_requires_logfile_before_start);
	return (atf_no_error());
}
