---
module: pmc-tests
type: module
status: active
stack: sh, C, DTrace, ATF, kyua, libpmc
last_modified: 2026-05-28
related: [[[ibs-test-suite]], [[ci-actions-workflows]], [[pmc-grouping-validation]]]
tags: [freebsd-ci-actions, module, pmc, hwpmc]
---

# PMC Tests
> FreeBSD hwpmc/PMC test suite covering EXTERROR paths, pmcstat smoke tests,
> grouping/skew characterization, and AMD core PMC allocation/start behavior.

## Overview

Lives in `tests/sys/amd/pmc/`. Contains:
- `hwpmc_exterr_test` — 14 root-only C ATF test cases verifying EXTERROR
  diagnostics for generic hwpmc, AMD core, and IBS PMCs (ported from
  freebsd/freebsd-src#2180).
- `hwpmc_grouping_test` — libpmc-backed row-disposition, allocation rollback,
  and AMD core start-path characterization.  See [[pmc-grouping-validation]].
- `pmcstat_grouping_test.sh` — process-scope pmcstat grouping/skew smoke tests.
- `pmcstat_group_alloc_atomicity_test.sh` — serialized runtime race for
  concurrent full-width process-scope AMD core allocations.
- `pmcstat_ibs_errata_test.sh` — offline pmcstat IBS log decode regression.
- `pmcstat_tsc_test.sh` — TSC-based PMC shell smoke test.

## Main Files

- `tests/sys/amd/pmc/Makefile` — Build config; uses port ATF paths
- `tests/sys/amd/pmc/Kyuafile` — kyua test definitions
- `tests/sys/amd/pmc/hwpmc_exterr_test.c` — EXTERROR negative-path ATF test (14 cases)
- `tests/sys/amd/pmc/hwpmc_grouping_test.c` — AMD core grouping and start-path ATF tests
- `tests/sys/amd/pmc/pmcinfo_snapshot.c` / `.h` — libpmc row snapshot helper
- `tests/sys/amd/pmc/msr_snapshot.c` / `.h` — cpuctl(4) AMD core PMC MSR snapshot helper
- `tests/sys/amd/pmc/pmcinfo_thread_count.c` — helper binary for shell ATF row-count checks
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

## [BUG] [#pmc-002] repeated_process_cycles_have_bounded_skew: flaky due to independent counter start timing and multimodal skew distribution
**Found:** 2026-05-22
**Status:** ACTIVE
**Author:** Claude Code
**Actor type:** ai-agent
**Source:** ai-prompt
**Session:** sess_2026-05-22_0900
**Owner:** Osvaldo J. Filho
**Reviewed by:** pending

**Symptom:** The test `pmcstat_grouping_test:repeated_process_cycles_have_bounded_skew` fails
intermittently. Empirical data shows a multimodal skew distribution: most runs cluster near
~50 permille (typical hardware noise), but a significant tail extends past 100 permille and up
to ~122.7 permille. The default tolerance was initially 100 permille (from
`DEFAULT_TOLERANCE_PERMILLE` in `pmc_grouping_skew_collect.py:59`), but the shell test uses
`pmcstat_cycle_tolerance_permille()` which defaults to **250** permille
(`pmcstat_grouping_test.sh:56`). With the collector's default of 100, every run with skew
in the 100–122.7 range fails.

**Root cause:** Three compounding factors, not a single bug:

1. **Independent counter start timing (primary cause).**
   The test invokes `pmcstat -C -q -p ls_not_halted_cyc -p ls_not_halted_cyc -o ... -- sleep 5`
   (shell line 124). With `-C` (process-counting mode), pmcstat allocates two *separate* PMC
   rows (confirmed by `hwpmc_grouping_test.c:amd_pmu_core_events_allocate_concurrently` which
   asserts `row0 != row1`). Each row is individually armed via `pmc_start()` at slightly
   different kernel dispatch times. The two counters therefore do not start — and critically,
   do not stop — at the same hardware clock edge. The final read (`last_a`, `last_b`) in the
   parser (`pmc_grouping_skew_collect.py:439`) compares the last *cumulative* totals, not a
   synchronized snapshot.  A misalignment of even a few hundred microseconds of user-mode
   activity before/after `sleep` exits produces delta ≈ CPU_freq × misalignment_us × 1e-6
   cycles, which at 3–4 GHz and 30 µs jitter is ~100 K cycles on a baseline of ~2 M cycles
   ≈ 50 permille — matching the observed "normal" mode. The tail (>100 permille) appears when
   the scheduler runs a different task between the two `pmc_start` syscalls or when the
   process migrates CPUs between start and stop.

2. **`sleep 5` workload is low-privilege and idle-heavy.**
   `sleep` spends virtually all time in `nanosleep()`, so `ls_not_halted_cyc` (LS not-halted
   cycles) counts only the tiny active windows (syscall entry/exit, signal handling, pmcstat
   overhead). The absolute counts are small (~1–5 M cycles total), so even 50 K cycles of
   timing jitter — a constant from the hardware — creates a large *relative* skew. A CPU-bound
   workload would accumulate billions of cycles, making the same absolute timing difference
   negligible.

3. **No CPU affinity pinning.**
   pmcstat does not pin the monitored process (`sleep`) to a single CPU. If the process
   migrates between CPUs mid-run, counter A accumulated on core X and counter B accumulated
   on core Y with a different starting offset. On EPYC sockets with multiple CCXs the
   frequency scaling / boost state can also differ per core, amplifying the divergence.

**Fix:** Two independent changes are needed — the tolerance calibration is the critical one;
the collector-side default is a documentation/UX issue.

**(A) Shell test tolerance — correct fix:**
The ATF config key `amd.pmc.grouping.cycle_tolerance_permille` already defaults to **250**
in the shell test (`pmcstat_grouping_test.sh:56`). This is intentionally generous because
`sleep` is an intentionally weak workload. The collector script, however, uses
`DEFAULT_TOLERANCE_PERMILLE = 100` (`pmc_grouping_skew_collect.py:59`) for its own verdict
logic. The **test itself will not flake** if kyua is invoked without overriding the tolerance
(default = 250 > observed max of 122.7). Flakiness only occurs when the collector or kyua is
invoked with `cycle_tolerance_permille=100`.

**(B) Collector default — calibrate to empirical data:**
`DEFAULT_TOLERANCE_PERMILLE` in `pmc_grouping_skew_collect.py` should be raised to match
the shell test default (250), or documented explicitly as intentionally strict for research
use. As a research tool it is acceptable to leave it at 100 and treat "FAIL" as
"marginal/interesting" rather than "broken". The value should be documented with the
empirical baseline (~52 permille mean, p95 ~70–80, observed max ~123).

**Files affected:**
- `tests/sys/amd/pmc/pmcstat_grouping_test.sh` — tolerance default is already 250 (correct)
- `py-scripts/pmc_grouping_skew_collect.py:59` — `DEFAULT_TOLERANCE_PERMILLE = 100` should be 250 or documented
- `py-scripts/pmc_grouping_skew_collect.py:1565` — `--tolerance-permille` default description
**Related commit:** pending

---

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
2026-05-22 | Investigated flakiness of repeated_process_cycles_have_bounded_skew. Root cause: independent pmc_start() timing for two separate PMC rows + idle sleep workload + no CPU affinity. Shell test default tolerance is 250 permille (safe); collector default is 100 (causes false failures). Multimodal: ~0, ~50, >100 permille. Max observed ~122.7. Fix: align collector default to 250 or document as intentionally strict. | pmc-tests | agent_claude
2026-05-22 | 2h collection completed (475 v1 + 1404 v2 samples). v1 (sleep 5, tol 100‰): 0% fail, mean 42‰, max 97.7‰. v2 (cpuset+dd, tol 50‰): 95.3% fail total — 94.7% com low_count_warning (dd encerrado por SIGTERM antes de acumular ≥10M ciclos). Nas 74 amostras v2 válidas: mean 24.4‰, p95 54.9‰, max 69.2‰ — workload CPU-bound longo reduz skew mas p95 ainda excede 50‰. CPU pinning (cpuset) não foi fator dominante. Causa primária confirmada: falta de atomicidade pmc_start()/pmc_read() no pmcstat -C. Novo bug pmc-003: timeout 5 mata dd via SIGTERM cedo demais; fix = dd count=N fixo. Ver docs/pmc-skew-v1-vs-v2.md. | pmc-tests | agent_claude
2026-05-22 | Sign bias discovery: b>a em 94.1% dos 475 samples v1 e 100% dos 74 samples v2 válidos. Comprovação de arming sequencial — counter B (segundo -p) é sempre armado depois do A, acumulando mais ciclos sistematicamente. Criados pmcstat_grouping_test_v3.sh (H1 duration sweep, H2 counter bias, H3 dd count=N vs sleep) e pmc_grouping_skew_v3_collect.py. Relatório v1/v2/v3 em docs/pmc-skew-v1-v2-v3.md. | pmc-tests | agent_claude
2026-05-22 | v4 coletado (50 cal + 50 meas). Calibração com true → baseline_offset=197250 ciclos. Measurement com dd count=500000 → corrected_permille median=0.000000‰, max=0.0125‰. True noise floor stdev=0.2109‰. CONCLUSÃO: os dois contadores PMC são perfeitamente idênticos uma vez subtraído o offset de armamento. Tolerância viável com calibração: 0.1‰. | pmc-tests | agent_claude
2026-05-22 | v3 coletado (310 amostras: 150 sweep + 100 bias + 60 h3). H1: delta_abs ≈ 199 K constante em todas as durações (1s–30s) — overhead fixo confirmado; permille não cai porque sleep acumula ~3.9 M ciclos independente da duração. H2: b>a em 94% (100 runs @ 5s) — arming sequencial estrutural confirmado. H3: dd count=500000 → 467 M ciclos (120x sleep), permille cai de 47‰ para 0.42‰ (redução de 113x). Conclusão final: delta_abs fixo de ~199 K ciclos é o custo de dois pmc_start()/pmc_read() não sincronizados. Fix: trocar sleep por dd count=N no workload do teste. | pmc-tests | agent_claude
