---
module: l3-tests
type: module
status: active
stack: C, ATF, kyua, libpmc, FreeBSD make
last_modified: 2026-05-25
programs: 3
related: [[[umcdf-tests]], [[pmc-tests]], [[ibs-test-suite]]]
tags: [freebsd-ci-actions, module, amd, l3, atf, pmc]
---

# L3 Tests
> 3-program ATF test suite covering AMD L3 cache PMU detection, miss-counter
> smoke test, and hit-counter smoke test.  All programs reuse the UMCDF CPU
> detection and PMC lifecycle infrastructure (`amd_umcdf_common.h`) via an
> `-I../umcdf` compiler flag.

## Overview

Tests live in `tests/sys/amd/l3/`.  They exercise the AMD L3 PMU via
`libpmc` (`pmc_pmu_pmcallocate`, `pmc_allocate`, `pmc_start/stop/read`)
using named events from the FreeBSD pmu-events JSON tables
(`l3_lookup_state.*`).

All cases require root.  The detect and metadata cases skip cleanly when
the CPU is not a known Zen generation or when the pmu-events JSON is
absent.  Runtime smoke cases additionally skip when `pmc_allocate` returns
`ENOENT`/`EOPNOTSUPP`/`ENXIO` — indicating a known limitation of the
current tree rather than a hardware or test bug.

## Main Files

- `tests/sys/amd/l3/amd_l3_common.h` — L3-specific workload generators
  (`amd_l3_generate_miss_traffic`, `amd_l3_generate_hit_traffic`) and
  `amd_l3_has_freebsd_l3_json()` Zen-generation gate.  Includes
  `amd_umcdf_common.h` transitively; no ATF dependency.
- `tests/sys/amd/l3/l3_detect_test.c` — 3 ATF cases (TC-DET-L3-01..03)
- `tests/sys/amd/l3/l3_miss_test.c` — 2 ATF cases (TC-UNC-L3-01a/b)
- `tests/sys/amd/l3/l3_hit_test.c` — 2 ATF cases (TC-UNC-L3-02a/b)
- `tests/sys/amd/l3/Makefile` — out-of-tree ATF build; `LIBADD+= pmc`;
  `-I../umcdf` for shared headers
- `tests/sys/amd/l3/Kyuafile` — 3 `atf_test_program{}` entries

## Test Cases

### l3_detect_test (TC-DET-L3)

| ID | Name | Severity | Description |
|----|------|----------|-------------|
| 01 | `l3_capability_probe` | HIGH | Probes AMD vendor, Zen generation, PerfMonV2 (CPUID Fn80000022); no MSR writes |
| 02 | `l3_rows_in_hwpmc` | HIGH | Counts `K8-L3-*` rows via `amd_umcdf_count_pmc_rows_with_prefix`; ≥1 required |
| 03 | `l3_pmu_events_enabled` | HIGH | `pmc_pmu_enabled()` must return true; gate for all named-event lookups |

### l3_miss_test (TC-UNC-L3-01)

Primary event: `l3_lookup_state.l3_miss` (EventSel=0x04 UMask=0x01)
Fallback: `l3_lookup_state.all_coherent_accesses_to_l3` (UMask=0xff)
Workload: 64 MB buffer, 4 KB page-stride, 4 rounds — defeats HW prefetcher.

| ID | Name | Severity | Description |
|----|------|----------|-------------|
| 01a | `l3_miss_pmu_metadata_contract` | HIGH | `pmc_pmu_pmcallocate` returns 0 or EOPNOTSUPP; ENOENT for all candidates = fail |
| 01b | `l3_miss_runtime_smoke` | HIGH | Allocate, start, drive 64 MB miss workload, read; counter must be non-decreasing |

### l3_hit_test (TC-UNC-L3-02)

Primary event: `l3_lookup_state.l3_hit` (EventSel=0x04 UMask=0xfe)
Fallback: `l3_lookup_state.all_coherent_accesses_to_l3` (UMask=0xff)
Workload: 2 MB buffer warmed into L3, then 128 sequential traversals at
cacheline stride — keeps working set hot.

| ID | Name | Severity | Description |
|----|------|----------|-------------|
| 02a | `l3_hit_pmu_metadata_contract` | HIGH | Same metadata contract as miss; ENOENT for all candidates = fail |
| 02b | `l3_hit_runtime_smoke` | HIGH | Allocate, start, drive 2 MB hit workload (128 rounds), read; counter must be non-decreasing |

## Key Constants (amd_l3_common.h)

```c
AMD_L3_MISS_BUFFER_SIZE   64 MiB   /* capacity miss workload — overflows most Zen L3 */
AMD_L3_MISS_PAGE_STRIDE   4096     /* 4 KB stride defeats HW stream prefetcher */
AMD_L3_MISS_ROUNDS        4        /* 4 passes */

AMD_L3_HIT_BUFFER_SIZE    2 MiB    /* fits in L3 on all Zen CPUs */
AMD_L3_HIT_ROUNDS         128      /* repeated traversals keep set hot */
```

## Dependencies

- `LIBADD+= pmc` — links against `/usr/lib/libpmc.a`
- `-I../umcdf` — pulls in `amd_umcdf_common.h`, `amd_umcdf_decode.h`,
  and related UMCDF CPU-detection and PMC lifecycle helpers
- `hwpmc.ko` must be loaded (or built into the kernel)
- Named PMU events require FreeBSD pmu-events JSON for the target CPU
  (`l3_lookup_state.*` with `"Unit": "L3PMC"`)
- Zen generation: Zen 1 through Zen 6 are gated by `amd_l3_has_freebsd_l3_json()`

## Decisions

## [DECISION] [#6551] L3 suite reuses UMCDF CPU-detection infrastructure
**Date:** 2026-05-25
**Status:** ACTIVE
**Author:** Osvaldo J. Filho
**Actor type:** ai-agent
**Source:** dashboard
**Session:** sess_2026-05-25_0001
**Requested by:** usr_osvaldo

**Context:** The L3 PMU shares the same Zen-generation detection path,
CPUID ioctl pattern, and PMC lifecycle helpers as the UMCDF suite.
Duplicating those into a separate `l3_utils.h` would create maintenance
burden and drift.

**Decision:** `amd_l3_common.h` includes `amd_umcdf_common.h` directly
via `-I../umcdf` in the Makefile. Only L3-specific constants
(`AMD_L3_MISS_BUFFER_SIZE`, etc.) and workload generators live in the L3
header. All CPU detection, PMC lifecycle, and event-candidate helpers are
inherited from UMCDF.

**Consequences:** `tests/sys/amd/l3/` has a build-time dependency on
`tests/sys/amd/umcdf/` headers. Both directories must be present to build.
UMCDF header changes may affect L3 compilation.

## TODOs

## [TODO] [#6591] l3_utils.h + l3_config_test.c + l3_msr_test.c
**Created:** 2026-05-25
**Status:** pending
**Priority:** high
**Author:** Osvaldo J. Filho
**Actor type:** human
**Source:** dashboard
**Session:** sess_2026-05-25_0001
**Owner:** ojanerif
**Due:** 2026-05-29

**Task:** Expand the L3 suite per SWLSVROS-6591.  Planned additions:
- `l3_msr_test.c` — direct MSR read/write tests for L3 PMU control registers
- `l3_config_test.c` — L3 configuration and slice mapping tests

**Files affected:**
- tests/sys/amd/l3/l3_msr_test.c (new)
- tests/sys/amd/l3/l3_config_test.c (new)
- tests/sys/amd/l3/Makefile (expand ATF_TESTS_C)
- tests/sys/amd/l3/Kyuafile (add entries)
- run.sh get_test_meta (add new binary names)

## Learning Log

2026-05-25 | Module doc created retroactively (gap discovered during Jira report review — SWLSVROS-6551 delivered l3_detect_test, l3_hit_test, l3_miss_test + amd_l3_common.h but no dev-docs entry existed). 3 programs, 7 ATF cases. Reuses UMCDF infrastructure via -I../umcdf. Miss workload: 64 MiB 4KB-stride (defeats HW prefetcher). Hit workload: 2 MiB 128-round warm+traverse. Not yet wired into run.sh suite selector. | l3-tests | agent_claude
