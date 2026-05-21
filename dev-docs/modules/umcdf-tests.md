---
module: umcdf-tests
type: module
status: active
stack: C, ATF, kyua, FreeBSD make
last_modified: 2026-05-08
programs: 10
related: [[[pmc-tests]], [[ibs-test-suite]], [[ci-actions-workflows]]]
tags: [freebsd-ci-actions, module, amd, umc, df, pmc, atf]
---

# UMCDF Test Suite
> 3-program ATF test suite covering AMD UMC (Unified Memory Controller) and
> DF (Data Fabric) PMU validation via FreeBSD hwpmc on Zen 1–5 hardware.

## Overview

Tests live in `tests/sys/amd/umcdf/`. They validate AMD Data Fabric and UMC
PMU hardware via FreeBSD's `hwpmc.ko` kernel module and the `pmc_*` libpmc API.
A shared header `amd_umcdf_common.h` provides CPUID detection, Zen generation
classification, PMC lifecycle helpers, and event candidate structs.

Three programs, 8 test cases total:
- **umcdf_cpuid_test (2 cases):** CPUID detection and PerfMonV2 capability decoding
- **umcdf_df_test (3 cases):** Data Fabric PMU event enumeration, encoding, runtime smoke
- **umcdf_umc_test (3 cases):** UMC PMU capability probe, metadata contract, runtime smoke

All programs require root (`require.user = root`).
Runtime smoke tests gracefully skip when hwpmc UMC/DF support is absent.

## Main Files

- `tests/sys/amd/umcdf/amd_umcdf_common.h` — Shared header: `amd_umcdf_cpu` struct,
  Zen generation enum (`AMD_UMCDF_ZEN_1` … `AMD_UMCDF_ZEN_FUTURE`),
  `amd_umcdf_skip_unless_known_zen()`, `amd_umcdf_zen_name()`,
  `amd_umcdf_event_candidate` struct for PMC event validation
- `tests/sys/amd/umcdf/umcdf_cpuid_test.c` — CPUID detection tests
- `tests/sys/amd/umcdf/umcdf_df_test.c` — Data Fabric PMU tests; event tables for
  Zen 1/2/3 (`dram_channel_data_controller_0`, `remote_outbound_data_controller_0`)
  and Zen 4 (`local_processor_read_data_beats_cs0`) and Zen 5
  (`local_or_remote_socket_read_data_beats_dram_0`); also DF2 encoding variants
- `tests/sys/amd/umcdf/umcdf_umc_test.c` — UMC PMU tests; events: `umc_cas_cmd.rd`,
  `umc_cas_cmd.wr`, `umc_data_slot_clks.rd`, `umc_mem_clk`
- `tests/sys/amd/umcdf/Makefile` — Compiles 3 ATF programs
- `tests/sys/amd/umcdf/Kyuafile` — kyua test definitions (syntax 2)

## Test Cases

### umcdf_cpuid_test

| Case | Description |
|------|-------------|
| `umcdf_cpuid_generation_decode` | Decode AMD family/model/stepping and map to Zen generation before any UMC/DF PMC work; fails on non-Zen or FUTURE family |
| `umcdf_perfmonv2_capability_decode` | Decode CPUID Fn80000022 PerfMonV2 fields used by FreeBSD hwpmc for dynamic DF counter counts and active UMC mask |

### umcdf_df_test

| Case | Description |
|------|-------------|
| `umcdf_df_rows_match_cpuid` | DF PMU event table row count matches CPUID-reported counter count |
| `umcdf_df_pmu_maps_to_data_fabric` | `pmc_cpuinfo` reports expected DF PMU event names for this Zen generation |
| `umcdf_df_runtime_smoke` | Allocate → start → sample → stop → release DF PMC; full lifecycle without crash or error |

### umcdf_umc_test

| Case | Description |
|------|-------------|
| `umcdf_umc_capability_probe` | Probe Zen 4/5/6 UMC CPUID state without programming UMC MSRs |
| `umcdf_umc_pmu_metadata_contract` | `pmc_cpuinfo` exposes UMC PMU events (umc_cas_cmd.rd/wr, umc_data_slot_clks.rd, umc_mem_clk) |
| `umcdf_umc_runtime_smoke_if_supported` | Full UMC PMC lifecycle when UMC support present; skip if not (graceful) |

## Dependencies

- kernel modules: `hwpmc.ko`, `cpuctl.ko`
- libpmc API: `pmc_init()`, `pmc_allocate()`, `pmc_start()`, `pmc_read()`, `pmc_stop()`, `pmc_release()`
- CPUID: Fn80000022 (PerfMonV2), Fn8000001E (topology)
- infra: [[runner-setup]], [[ci-vm-infra]]
- modules: [[pmc-tests]], [[ibs-test-suite]]

## Decisions

## [DECISION] Separate umcdf/ from pmc/ directory
**Date:** 2026-05-07
**Author:** Osvaldo J. Filho
**Actor type:** human
**Source:** ai-prompt
**Session:** sess_2026-05-07_0000
**Context:** The original `tests/sys/amd/pmc/` is a placeholder waiting for
kernel hwpmc uncore support. UMCDF tests validate the hwpmc PMC API for UMC/DF
hardware — a different scope from pmcstat(8) and general libpmc API tests planned
for pmc/.
**Decision:** `tests/sys/amd/umcdf/` for UMC/DF PMC API validation;
`tests/sys/amd/pmc/` reserved for pmcstat(8)/libpmc general API tests.
**Discarded alternatives:** Merge into pmc/ (conflates distinct test scopes).
**Impact:** Kyuafile, CI workflow may need a separate job or extended run-atf-tests
invocation to cover both directories.

## [DECISION] Shared header amd_umcdf_common.h, not inline per-file
**Date:** 2026-05-07
**Author:** Osvaldo J. Filho
**Actor type:** human
**Source:** ai-prompt
**Session:** sess_2026-05-07_0000
**Context:** All three test programs need Zen generation classification and PMC
lifecycle helpers; duplicating them per-file invites divergence.
**Decision:** `amd_umcdf_common.h` provides all shared logic. ATF programs include
it directly (no separate .c compilation unit needed at this scale).
**Discarded alternatives:** Separate .c shared lib (overkill for 3 programs).
**Impact:** All three umcdf test programs.

## [DECISION] Graceful skip for UMC runtime smoke when unsupported
**Date:** 2026-05-07
**Author:** Osvaldo J. Filho
**Actor type:** human
**Source:** ai-prompt
**Session:** sess_2026-05-07_0000
**Context:** UMC PMC support in FreeBSD hwpmc is not yet complete for all Zen
generations. Tests must not fail on hardware where support is missing.
**Decision:** `umcdf_umc_runtime_smoke_if_supported` detects support at runtime
and calls `atf_tc_skip()` if absent. DF runtime smoke similarly hardened.
**Discarded alternatives:** Hard-require support (fails on older Zen or
pre-support kernels).
**Impact:** umcdf_umc_test.c, umcdf_df_test.c.

## Known Bugs

## [BUG] DF2 encoding dispatch incorrect before 4590da4
**Found:** 2026-05-07
**Symptom:** `umcdf_df_runtime_smoke` selected wrong event encoding for DF2 (second
Data Fabric instance); allocated wrong PMC event, causing unexpected error or wrong
sample count.
**Root cause:** DF2 encoding dispatch used base-event index instead of DF2-offset
event index.
**Fix/Workaround:** Fixed in commit `4590da4` (umcdf: fix DF2 encoding dispatch and
runtime-smoke skip). Status: resolved.
**Status:** resolved

## [BUG] UMC runtime smoke could hang under concurrent PMC access
**Found:** 2026-05-07
**Symptom:** `umcdf_umc_runtime_smoke_if_supported` occasionally blocked when
another test held a PMC resource, causing kyua to report a timeout.
**Root cause:** PMC allocation not serialized; concurrent test programs could
conflict on shared UMC PMC resources.
**Fix/Workaround:** Serialized by removing `is_exclusive` (kyua 0.13 compat) and
adding explicit serialization in `6d57d07` (umcdf: serialize UMC runtime smoke).
**Status:** resolved

## TODOs

## [TODO] Wire umcdf/ into CI workflow
**Priority:** medium
**Context:** Currently no CI composite action covers `tests/sys/amd/umcdf/`. The
`run-atf-tests` action targets `tests/sys/amd/ibs/` only. A second invocation or
an extended Kyuafile is needed.
**Status:** done
**Done:** 2026-05-21 — run.sh wires UMCDF as a named suite (line 251); DEFAULT expands to IBS+UMCDF+PMC. Verified by agent_claude sess_2026-05-21_1200.

## [TODO] Run umcdf suite for the first time and capture results
**Priority:** high
**Context:** Binaries compiled and fixed; no run results captured yet.
Target machine: ruby-9470host (AMD EPYC 9654, Zen 4).
**Status:** done
**Done:** 2026-05-21 — .full result files present for all 10 umcdf programs; suite runs via run.sh --suite UMCDF. Verified by agent_claude sess_2026-05-21_1200.

## [TODO] Add C2C (cache-to-cache) test cases
**Priority:** low
**Context:** C2C PMU events are planned (see phases-plan.md Phase 3) but require
specific inter-socket workload setup. Not included in current umcdf scope.
**Status:** pending

## Snippets

## [SNIPPET] Zen generation skip guard
**Use:** Any UMCDF test that requires a specific Zen generation or newer.
```c
struct amd_umcdf_cpu cpu;
amd_umcdf_skip_unless_known_zen(&cpu);
if (cpu.zen < AMD_UMCDF_ZEN_4) {
    atf_tc_skip("Zen 4+ required for UMC CPUID fields");
}
```

## [SNIPPET] PMC lifecycle smoke (DF or UMC)
**Use:** Any test that allocates, starts, reads, and releases a PMC counter.
```c
pmc_id_t pmcid;
if (pmc_allocate("event_name", PMC_MODE_SC, 0, cpu, &pmcid) < 0)
    atf_tc_skip("PMC event not supported on this CPU/kernel");
ATF_REQUIRE(pmc_start(pmcid) == 0);
/* workload */
pmc_value_t val;
ATF_REQUIRE(pmc_read(pmcid, &val) == 0);
ATF_REQUIRE(pmc_stop(pmcid) == 0);
ATF_REQUIRE(pmc_release(pmcid) == 0);
```

## Learning Log

2026-05-07 | New module. 3 programs, 8 test cases. Two bugs fixed before first run (DF2 encoding, UMC serialization). First run pending — target ruby-9470host. No run results available yet. | umcdf-tests
2026-05-08 | Introduced amd_umcdf_decode.h — pure decode header (no OS headers, no pmc). Refactored amd_umcdf_common.h to include decode.h. Added 7 unit test programs (umcdf_unit_zen_map_test, _df_encoding_test, _perfmonv2_test, _zen_name_test, _capabilities_test, _vendor_test, _df_config_dispatch_test) covering 115 new hardware-free test cases. Suite now 10 programs, 123 total test cases. Unit test share across both UMCDF+IBS suites is 70.3%. | umcdf-tests
