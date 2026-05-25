---
module: tsc-tests
type: module
status: active
stack: C, ATF, kyua, FreeBSD make
last_modified: 2026-05-25
programs: 4
related: [[[ibs-test-suite]], [[pmc-tests]]]
tags: [freebsd-ci-actions, module, amd, tsc, atf]
---

# TSC Tests
> 4-program ATF test suite covering AMD TSC (Time Stamp Counter) detection,
> cross-CPU drift, invariant rate constancy, and stress monotonicity — all via
> CPUID probing and inline RDTSC (no MSR writes).

## Overview

Tests live in `tests/sys/amd/tsc/`. They exercise the TSC hardware via
CPUID ioctl queries (`/dev/cpuctl0`, `CPUCTL_CPUID`) and inline `rdtsc`.
No kernel modules required beyond cpuctl access (standard FreeBSD base).

All test cases require root (cpuctl ioctl). No hardware is written.

## Main Files

- `tests/sys/amd/tsc/tsc_utils.h` — CPUID helpers, TSC frequency computation,
  serialised `tsc_read()` (LFENCE + RDTSC), vendor/feature detection inlines.
  No ATF dependency. Pure C, portable to any amd64 context.
- `tests/sys/amd/tsc/tsc_detect_test.c` — 7 ATF cases (TC-TSC-DET-01..07)
- `tests/sys/amd/tsc/tsc_drift_test.c` — 5 ATF cases (TC-TSC-DRF-01..05); pthread pinning
- `tests/sys/amd/tsc/tsc_invariant_test.c` — 4 ATF cases (TC-TSC-INV-01..04); cpuset + sched_yield
- `tests/sys/amd/tsc/tsc_stress_test.c` — 3 ATF cases (TC-TSCSTR-01..03); pthread stressors
- `tests/sys/amd/tsc/Makefile` — out-of-tree ATF build; LIBADD+= pthread
- `tests/sys/amd/tsc/Kyuafile` — kyua test definitions (4 programs)

## Test Cases

### tsc_detect_test (TC-TSC-DET)

| ID | Name | Severity | Description |
|----|------|----------|-------------|
| 01 | `tsc_feature_present` | CRITICAL | CPUID.1 EDX[4] TSC bit present |
| 02 | `tsc_invariant_flag` | HIGH | CPUID.80000007 EDX[8] InvariantTSC |
| 03 | `tsc_rdtscp_present` | MEDIUM | CPUID.80000001 EDX[27] RDTSCP info |
| 04 | `tsc_art_ratio_leaf` | HIGH | CPUID.0x15 ART/TSC ratio coherence |
| 05 | `tsc_frequency_valid` | HIGH | Computed freq in [500 MHz, 10 GHz] |
| 06 | `tsc_read_monotonic` | CRITICAL | 1000 rdtsc pairs, no backwards jumps |
| 07 | `tsc_frequency_stable` | MEDIUM | CPUID vs wall-clock within 2% |

### tsc_drift_test (TC-TSC-DRF)

| ID | Name | Severity | Description |
|----|------|----------|-------------|
| 01 | `tsc_drift_same_cpu_zero` | CRITICAL | 1000 consecutive pairs on CPU0 < 10k cycles |
| 02 | `tsc_drift_two_cpu_monotonic` | HIGH | Simultaneous CPU0+CPUlast reads, delta < freq/1000 |
| 03 | `tsc_drift_bound_1000_iterations` | HIGH | 1000×oneshot threads far CPU, delta < freq/100 |
| 04 | `tsc_drift_across_sleep` | HIGH | 50 ms usleep, freq agreement < 5% |
| 05 | `tsc_drift_amd_invariant_guarantee` | MEDIUM | AMD InvariantTSC CPUID bit soft-CHECK |

### tsc_invariant_test (TC-TSC-INV)

| ID | Name | Severity | Description |
|----|------|----------|-------------|
| 01 | `tsc_invariant_cpuid_verified` | HIGH | Hard REQUIRE on InvariantTSC bit |
| 02 | `tsc_invariant_rate_across_10ms` | HIGH | 5×10 ms windows, variation < 1% |
| 03 | `tsc_invariant_monotonic_across_yield` | CRITICAL | 500 sched_yield, TSC advances every time |
| 04 | `tsc_invariant_frequency_vs_nominal` | MEDIUM | 200 ms busy loop, < 0.5% vs CPUID freq |

### tsc_stress_test (TC-TSCSTR)

| ID | Name | Severity | Description |
|----|------|----------|-------------|
| 01 | `tsc_stress_monotonic_under_load` | CRITICAL | 120 s poll 1 ms, strict monotone under CPU+mem load |
| 02 | `tsc_stress_frequency_stable_under_load` | HIGH | 12×10 s windows, variation < 2% under load |
| 03 | `tsc_stress_rapid_read_10m` | HIGH | 10M back-to-back rdtsc, all non-decreasing under load |

## tsc_utils.h API

```c
/* CPUID access */
int      tsc_cpuid(uint32_t leaf, uint32_t regs[4]);

/* Vendor / feature detection */
bool     tsc_cpu_is_amd(void);
uint32_t tsc_max_ext_leaf(void);
bool     tsc_feature_present(void);
bool     tsc_rdtscp_present(void);
bool     tsc_invariant_present(void);
bool     tsc_art_leaf_available(void);
bool     tsc_freq_leaf_available(void);

/* Frequency computation */
uint64_t tsc_compute_frequency(void);  /* returns Hz, 0 if unavailable */

/* TSC read */
uint64_t tsc_read(void);               /* LFENCE; RDTSC */

/* CPU identification */
uint32_t tsc_cpu_family(void);
uint32_t tsc_cpu_model(void);
uint32_t tsc_cpu_stepping(void);
```

## Dependencies

- devices: `/dev/cpuctl0` (cpuctl.ko or built-in; standard FreeBSD base)
- No `hwpmc.ko`, no libpmc
- kernel modules: none required

## Decisions

## [DECISION] [#6589] Standalone tsc/ module, not IBS sub-test
**Date:** 2026-05-25
**Status:** ACTIVE
**Author:** Osvaldo J. Filho
**Actor type:** ai-agent
**Source:** dashboard
**Session:** sess_2026-05-25_0000
**Requested by:** usr_osvaldo

**Context:** TSC detection was previously a single case (`ibs_cpu_tsc_frequency`)
inside `ibs_cpu_test.c`, tied to root+AMD requirements of the IBS suite.
SWLSVROS-6589 calls for a dedicated TSC module with its own utility header,
separate Makefile, and run.sh suite selector.

**Decision:** New `tests/sys/amd/tsc/` directory.  `tsc_utils.h` is a
self-contained header with no ATF dependency so future TSC drift and stress
tests (SWLSVROS-6556) can include it without pulling in IBS infrastructure.

**Rejected alternatives:** Adding TSC cases to ibs_cpu_test.c (wrong module
coupling; TSC is a general x86 feature, not IBS-specific).

**Consequences:** `run.sh` gains `TSC` suite selector; `suite_src_dir`,
`suite_install_dir`, `expand_suite_list`, and `get_test_meta` updated.
SWLSVROS-6556 depends on `tsc_utils.h` being here.

## [DECISION] [#6589] tsc_utils.h is read-only (no MSR writes)
**Date:** 2026-05-25
**Status:** ACTIVE
**Author:** Osvaldo J. Filho
**Actor type:** ai-agent
**Source:** dashboard
**Session:** sess_2026-05-25_0000
**Requested by:** usr_osvaldo

**Context:** IBS utils.h mixes CPUID reads with MSR read/write. TSC has no
writable MSRs relevant to detection; the TSC itself is read-only from
userspace (RDTSC instruction).

**Decision:** `tsc_utils.h` contains only CPUID ioctl queries and inline asm
`rdtsc`. No `write_msr` equivalent. This makes the header safe to include in
unit-tier tests.

**Consequences:** All 7 detect cases are safe to run in any environment with
`/dev/cpuctl0` access. No hardware state is modified.

## TODOs

## [DONE] [#6556] tsc_drift_test.c + tsc_invariant_test.c + tsc_stress_test.c
**Created:** 2026-05-25
**Completed:** 2026-05-25
**Status:** DONE
**Priority:** high
**Author:** Osvaldo J. Filho
**Actor type:** ai-agent
**Source:** dashboard
**Session:** sess_2026-05-25_0001
**Owner:** ojanerif

**Task:** Implement the three remaining TSC test files from SWLSVROS-6556,
all depending on `tsc_utils.h`. Drift test uses cpuset/pthread pinning and
atomic synchronisation. Stress test uses inline xorshift64 + 32 MiB stride
memory workers. All three compile clean (0 errors, 0 warnings).

**Acceptance criteria:** MET
- tsc_drift_same_cpu_zero, tsc_drift_two_cpu_monotonic, tsc_drift_bound: implemented
- tsc_stress_rapid_read_10m: 10M rdtsc calls, all non-decreasing (implemented)
- run.sh Phase 2 stress batch (TC-TSCSTR) includes tsc_stress_test via _srb_tsc_active
- Makefile LIBADD+= pthread; Kyuafile 4 entries; run.sh get_test_meta updated

**Files affected:**
- tests/sys/amd/tsc/tsc_drift_test.c (new, 5 ATF cases)
- tests/sys/amd/tsc/tsc_invariant_test.c (new, 4 ATF cases)
- tests/sys/amd/tsc/tsc_stress_test.c (new, 3 ATF cases)
- tests/sys/amd/tsc/Makefile (4 binaries, LIBADD+= pthread)
- tests/sys/amd/tsc/Kyuafile (4 atf_test_program entries)
- run.sh (get_test_meta 3 new entries; _srb_tsc_active; Batch 1 TSC block)

## Learning Log

2026-05-25 | Module created (SWLSVROS-6589). tsc_utils.h: 7 inlines, 2 CPUID leaves (0x15 ART ratio, 0x80000007 InvariantTSC), LFENCE+RDTSC wrapper. tsc_detect_test.c: 7 cases, all pass on AMD EPYC Zen 4 bare metal. Compiled clean with out-of-tree ATF. CPUID 0x15 on EPYC 9654: denom=1 num=160 crystal=25MHz → freq=4.0 GHz. | tsc-tests | agent_claude
2026-05-25 | SWLSVROS-6556 implemented. Added tsc_drift_test (5 cases: same-CPU zero delta, two-CPU atomic synchronised read, 1000-iter oneshot, 50ms sleep, AMD guarantee), tsc_invariant_test (4 cases: CPUID gate, 5×10ms windows <1%, 500 sched_yield strict monotone, 200ms busy loop <0.5% vs CPUID), tsc_stress_test (3 cases: 120s poll <1ms CRITICAL, 12×10s freq <2%, 10M rapid rdtsc). All compile clean. run.sh Batch 1 wired for TC-TSCSTR. Module promoted to 4 programs. | tsc-tests | agent_claude
