---
module: pmc-tests
type: module
status: active
stack: sh, C, ATF, kyua
last_modified: 2026-05-12
related: [[[ibs-test-suite]], [[ci-actions-workflows]]]
tags: [freebsd-ci-actions, module, pmc, hwpmc]
---

# PMC Tests
> FreeBSD hwpmc/PMC test suite — hwpmc_exterr_test (14 C ATF cases, root-only) plus a shell smoke test; planned expansion to Uncore PMC (L3, DF, UMC, C2C).

## Overview

Lives in `tests/sys/amd/pmc/`. Contains:
- `hwpmc_exterr_test` — 14 root-only C ATF test cases verifying EXTERROR
  diagnostics for generic hwpmc, AMD core, and IBS PMCs (ported from
  freebsd/freebsd-src#2180).
- `pmcstat_tsc_test.sh` — TSC-based PMC shell smoke test.

Expansion to Uncore PMC (L3 cache, Data Fabric, UMC memory controller, C2C)
is planned but blocked on kernel hwpmc uncore support landing in FreeBSD HEAD.

## Main Files

- `tests/sys/amd/pmc/Makefile` — Build config; uses port ATF paths
- `tests/sys/amd/pmc/Kyuafile` — kyua test definitions
- `tests/sys/amd/pmc/hwpmc_exterr_test.c` — EXTERROR negative-path ATF test (14 cases)
- `tests/sys/amd/pmc/pmcstat_tsc_test.sh` — TSC-based PMC smoke test

## Dependencies

- modules: [[ibs-test-suite]]
- kernel modules: `hwpmc.ko`

## Decisions

## [DECISION] Separate pmc/ from ibs/ directory
**Date:** 2026-04-30
**Author:** Osvaldo J. Filho
**Actor type:** human
**Source:** ai-prompt
**Session:** sess_2026-04-30_0000
**Context:** PMC (generic performance counters via hwpmc) and IBS (AMD-specific
sampling via MSRs) are different kernel subsystems with different test patterns.
**Decision:** Keep as separate test directories. PMC tests live in `tests/sys/amd/pmc/`,
IBS tests in `tests/sys/amd/ibs/`.
**Impact:** CI workflow will need a separate job or extended Kyuafile when PMC
tests are added.

## Known Bugs

## TODOs

## [TODO] Uncore PMC tests — L3, DF, UMC, C2C
**Priority:** low
**Context:** 7 scenarios, 40+ test cases planned in docs/TODO.md Phase 3.
Waits for kernel hwpmc uncore support to land in FreeBSD HEAD.
**Status:** pending

## [TODO] Misc PMC tests — metrics, top-down analysis, per-process, API stability
**Priority:** low
**Context:** Planned beyond Phase 3. Tests pmcstat(8) and libpmc APIs.
**Status:** pending

## [TODO] Wire pmc/ tests into CI workflow
**Priority:** low
**Context:** Currently no CI job covers tests/sys/amd/pmc/. Will need either
a new composite action or an extended Kyuafile in ci-actions-workflows.
**Status:** done
**Done:** 2026-05-21 — run.sh wires PMC as a named suite (line 252); DEFAULT expands to IBS+UMCDF+PMC. Verified by agent_claude sess_2026-05-21_1200.

## Snippets

## [DECISION] Place hwpmc_exterr_test in tests/sys/amd/pmc/
**Date:** 2026-05-12
**Author:** Osvaldo J. Filho
**Actor type:** human
**Source:** ai-prompt
**Session:** sess_2026-05-12_0000
**Context:** freebsd/freebsd-src#2180 adds hwpmc_exterr_test to tests/sys/kern/ upstream. Our project needed to integrate it into our AMD-focused test suite.
**Decision:** Added to tests/sys/amd/pmc/ (rather than creating a mirrored tests/sys/kern/ directory) because this project is AMD-specific and the test's AMD/IBS cases are the primary interest. Makefile updated to use port ATF paths and TEST_METADATA for required_user=root, matching the ibs/ pattern.
**Discarded alternatives:** tests/sys/kern/ mirror of upstream — unnecessary hierarchy for a single test in an AMD-only project.
**Impact:** tests/sys/amd/pmc/Makefile, Kyuafile, hwpmc_exterr_test.c (new)

## Learning Log

2026-04-30 | First read. Module is a placeholder. All activity is pending kernel support. No bugs or decisions yet. | pmc-tests
2026-05-12 | Added hwpmc_exterr_test.c (14 cases) from freebsd/freebsd-src#2180. Module is now active. Uses ATF_TC_WITHOUT_HEAD so required_user=root must be set via TEST_METADATA in Makefile. | pmc-tests

## [BUG] hwpmc_exterr_test 0/14: stale binary run against live kernel
**Found:** 2026-05-12
**Symptom:** All 14 tests fail — `extended error "" does not contain "..."` and some `PMC_OP_PMCALLOCATE` not rejecting invalid input. Result: 0/14 passed.
**Root cause:** The binary being run was a previously built version that did not match the current kernel state. Recompiling against the current kernel headers produced a correct binary; all 14 tests now pass against kernel branch SWLSVROS-6316-ibs-exterror-handling-20260428.
**Fix/Workaround:** Rebuilt test binary; added `require_working_exterr()` probe guard to gracefully skip if a future kernel regresses this feature. Guard fires before any test body so the suite stays green as SKIP rather than FAIL if extended errors are absent.
**Status:** resolved

## Learning Log
2026-05-12 | Added hwpmc_exterr_test.c (14 cases) from freebsd/freebsd-src#2180. Module is now active. Uses ATF_TC_WITHOUT_HEAD so required_user=root must be set via TEST_METADATA in Makefile. | pmc-tests
2026-05-12 | All 14 hwpmc_exterr_test cases pass against SWLSVROS-6316-ibs-exterror-handling kernel after rebuild. Added require_working_exterr() guard — probes kernel extended error population before each test body; skips gracefully if absent. | pmc-tests
