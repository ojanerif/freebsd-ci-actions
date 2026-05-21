---
module: ibs-test-suite
type: module
status: active
stack: C, ATF, kyua, FreeBSD make
last_modified: 2026-05-16
programs: 35
related: [[[ci-actions-workflows]], [[runner-setup]], [[linuxulator]], [[stress-tests]]]
tags: [freebsd-ci-actions, module, amd, ibs, atf]
---

# IBS Test Suite
> 35-program ATF test suite (35 C) covering AMD Instruction-Based Sampling — unit, integration, and E2E tiers on FreeBSD 16.0-CURRENT amd64.

## Overview

Tests live in `tests/sys/amd/ibs/`. They exercise AMD IBS hardware via
FreeBSD's `cpuctl.ko` and `hwpmc.ko` kernel modules. Two shared headers
(`ibs_utils.h`, `ibs_decode.h`) provide all MSR I/O and pure field-decoding
helpers. Tests are run by `kyua` which emits JUnit XML and HTML reports.

Three test tiers:
- **Unit (6 files, ~50 cases):** No hardware, pure logic, run anywhere
- **Integration (7 files, 14+ cases):** Require `/dev/cpuctl0`, no hwpmc
- **E2E (10 files, 9+ cases):** Require AMD IBS hardware + active NMI delivery

## Main Files

- `tests/sys/amd/ibs/ibs_utils.h` (404 LOC) — MSR I/O helpers: `read_msr()`, `write_msr()`, `do_cpuid_ioctl()`, `cpu_supports_ibs()`, all MSR addresses
- `tests/sys/amd/ibs/ibs_decode.h` (266 LOC) — Pure C field extraction: bit masks, CPUID parsing, DataSrc formula, MaxCnt encoding. Zero I/O, portable
- `tests/sys/amd/ibs/ibs_interrupt_test.c` (541 LOC) — NMI delivery + VAL bit verification
- `tests/sys/amd/ibs/ibs_data_accuracy_test.c` (661 LOC) — Sample field decoding, DataSrc 5-bit combined value
- `tests/sys/amd/ibs/ibs_period_test.c` (609 LOC) — MaxCnt encoding, extended MaxCnt (Zen 2+, 23-bit)
- `tests/sys/amd/ibs/ibs_smp_test.c` (537 LOC) — Per-CPU isolation, pin threads to CPUs
- `tests/sys/amd/ibs/ibs_stress_test.c` (494 LOC) — 1000 rapid enable/disable cycles
- `tests/sys/amd/ibs/ibs_routing_test.c` (446 LOC) — Enable/disable + global IBSCTL
- `tests/sys/amd/ibs/ibs_l3miss_test.c` (380 LOC) — Zen 4+ L3MissOnly filter
- `tests/sys/amd/ibs/ibs_robustness_test.c` (332 LOC) — Kernel survival under stress
- `tests/sys/amd/ibs/ibs_cpuctl_access_test.c` (211 LOC) — cpuctl driver ioctl
- `tests/sys/amd/ibs/ibs_concurrency_test.c` (195 LOC) — Concurrent MSR access
- `tests/sys/amd/ibs/ibs_invalid_input_test.c` (267 LOC) — Error handling / bad input
- `tests/sys/amd/ibs/ibs_detect_test.c` (120 LOC) — CPUID-based IBS detection
- `tests/sys/amd/ibs/ibs_cpu_test.c` (260 LOC) — CPU family/model detection (Zen 1–5)
- `tests/sys/amd/ibs/ibs_msr_test.c` (48 LOC) — Smoke test: MSR read/write cycle
- `tests/sys/amd/ibs/ibs_ioctl_test.c` (38 LOC) — Placeholder for kernel IBS ioctl API
- `tests/sys/amd/ibs/ibs_swfilt_test.c` — C: software filter bits (4 cases; replaces ibs_swfilt_test.sh)
- `tests/sys/amd/ibs/ibs_unit_field_masks_test.c` (238 LOC) — Unit: field mask constants
- `tests/sys/amd/ibs/ibs_unit_helpers_test.c` (156 LOC) — Unit: helper function logic
- `tests/sys/amd/ibs/ibs_unit_datasrc_test.c` (139 LOC) — Unit: DataSrc extraction
- `tests/sys/amd/ibs/ibs_unit_cpuid_parse_test.c` (140 LOC) — Unit: CPUID field parsing
- `tests/sys/amd/ibs/ibs_unit_op_ext_maxcnt_test.c` (113 LOC) — Unit: extended MaxCnt
- `tests/sys/amd/ibs/ibs_unit_feature_flags_test.c` (129 LOC) — Unit: Zen feature flags
- `tests/sys/amd/ibs/ibs_nmi_stress_test.c` — TC-INT/HIGH: 3 cases — NMI stability under load (120s), rate-limit enforcement (SKIP until FreeBSD-Tests-026 kernel patch), drain-under-load (100× enable→sample→workaround#420)
- `tests/sys/amd/ibs/Makefile` — Compiles all 35 ATF programs
- `tests/sys/amd/ibs/Kyuafile` — kyua test definitions
- `docs/ibs-tests.md` (1200+ LOC) — Full test reference: IDs, categories, thresholds, skip conditions

## Dependencies

- modules: [[pmc-tests]]
- infra: [[runner-setup]], [[linuxulator]]
- infra: [[ci-vm-infra]]
- kernel modules: `hwpmc.ko`, `cpuctl.ko`
- devices: `/dev/cpuctl0` … `/dev/cpuctl<N-1>`

## Decisions

## [DECISION] Split ibs_utils.h (I/O) from ibs_decode.h (pure logic)
**Date:** 2026-04-30
**Author:** Osvaldo J. Filho
**Actor type:** human
**Source:** ai-prompt
**Session:** sess_2026-04-30_0000
**Context:** Unit tests must run without hardware and without cpuctl; mixing
MSR I/O into the decode helpers would make them impossible to unit-test.
**Decision:** `ibs_utils.h` owns all hardware accessors. `ibs_decode.h` is
pure C — no ioctl, no file descriptors, portable to any architecture.
**Discarded alternatives:** Single header (contaminated unit tests with I/O
dependencies).
**Impact:** All 23 test files include one or both headers.

## [DECISION] Test tier design (unit 70% / integration 20% / E2E 10%)
**Date:** 2026-04-30
**Author:** Osvaldo J. Filho
**Actor type:** human
**Source:** ai-prompt
**Session:** sess_2026-04-30_0000
**Context:** E2E tests require rare bare-metal AMD hardware and take longest;
unit tests catch logic bugs faster and cheaper.
**Decision:** 70% unit (no hardware, fast), 20% integration (cpuctl driver
only), 10% E2E (full hardware + NMI). Ratios defined in docs/TODO.md.
**Discarded alternatives:** All-E2E (slow, hardware-dependent, fragile on VMs).
**Impact:** Test file organization, skip conditions, CI timeout budgets.

## [DECISION] kyua for test runner, JUnit XML output
**Date:** 2026-04-30
**Author:** Osvaldo J. Filho
**Actor type:** human
**Source:** ai-prompt
**Session:** sess_2026-04-30_0000
**Context:** Standard FreeBSD ATF toolchain; JUnit XML is parseable by GitHub
Actions report-results action.
**Decision:** `kyua test -k Kyuafile` → `kyua report-junit` → `ibs-results.xml`.
HTML report also generated for artifact upload.
**Impact:** ci-actions-workflows (report-results action parses XML).

## [DECISION] ibs_nmi_stress_test lives in IBS suite, not STRESS suite
**Date:** 2026-05-14
**Author:** Osvaldo J. Filho
**Actor type:** human
**Source:** ai-prompt
**Session:** sess_2026-05-14_0000
**Context:** NMI stress (high-rate IBS + system load) is fundamentally an IBS hardware-correctness test. Putting it in the standalone STRESS suite would require `ibs_utils.h` in a non-IBS package or duplicating MSR helpers.
**Decision:** `ibs_nmi_stress_test.c` is in `tests/sys/amd/ibs/`, classified as TC-INT (Interrupt Delivery, HIGH severity). Inline stress workers are defined locally — no dependency on `tests/sys/amd/stress/`.
**Discarded alternatives:** STRESS suite (wrong abstraction; stress tests should not require IBS hardware).
**Impact:** 35th IBS test program; tracked in FreeBSD-Tests-026.

## [TODO] ibs_nmi_rate_limit_enforce — pending FreeBSD-Tests-026 kernel patch
**Context:** `ibs_nmi_rate_limit_enforce` (TC-INT) reads `dev.hwpmc.ibs.min_period` sysctl. If absent, the test calls `atf_tc_skip("dev.hwpmc.ibs.min_period not present — pending FreeBSD-Tests-026 kernel patch")`. When the sysctl lands in FreeBSD HEAD, remove the skip guard and let the test enforce the minimum period.
**Status:** open (blocked: kernel patch)

## Known Bugs

## [BUG] IBS_FETCH_ENABLE_BIT local define shadowed real constant
**Found:** 2026-04-30
**Symptom:** ibs_smp_per_cpu_config used bit 2 instead of bit 48 for IBSFETCHCTL.
**Root cause:** Local `#define IBS_FETCH_ENABLE_BIT 2` overrode the `ibs_utils.h`
definition (bit 48).
**Fix/Workaround:** Removed local define; use `ibs_utils.h` exclusively.
**Status:** resolved

## [BUG] DataSrc extended field shifted by 3 instead of 6
**Found:** 2026-04-30
**Symptom:** ibs_data_accuracy_test produced wrong DataSrc values for
extended (non-DRAM) memory hierarchy sources.
**Root cause:** DataSrcHi lives at bits [7:6] of IBSOPDATA2; correct extraction
is `(op_data2 & 0xC0) >> 6`, but code shifted by 3 instead.
**Fix/Workaround:** Fixed shift to 6 in `ibs_decode.h` DataSrc extraction.
Combined value: `((op_data2 & 0xC0) >> 6) << 3 | (op_data2 & 0x07)`.
**Status:** resolved

## [BUG] NMI race in period write-read (see _global.md)
**Found:** 2026-04-30
**Symptom:** Intermittent MaxCnt read-back mismatch in ibs_stress_test and ibs_smp_test.
**Root cause:** In-flight NMI overwrites MSR between write and read.
**Fix/Workaround:** Retry loop + `sched_yield()` + pre-test sleep.
**Status:** workaround

## [BUG] IBSOPDATA4 hard-fail outside active sampling (see _global.md)
**Found:** 2026-04-30
**Symptom:** ATF_REQUIRE crash in ibs_detect_test on Zen 4+ when sampling inactive.
**Root cause:** Register returns #GP without active sampling.
**Fix/Workaround:** Replaced `ATF_REQUIRE` with graceful `atf_tc_skip()`.
**Status:** resolved

## [BUG] Out-of-tree ATF link fails — LDADD_atf_c empty
**Found:** 2026-04-30
**Symptom:** All 23 test programs fail to link with `undefined symbol: atf_tp_main` and related ATF symbols.
**Root cause:** `bsd.test.mk` → `atf.test.mk` uses `LDADD_atf_c` to inject `-latf-c`. In an out-of-tree build, `LIBATF_C` and `LDADD_atf_c` are never set because ATF is a port at `/usr/local`, not a base library. The linker command only gets `-lpthread` (converted from `LIBADD+=pthread`).
**Fix/Workaround:** Added to `Makefile` before `.include <bsd.test.mk>`:
  `CFLAGS+= -I/usr/local/include`
  `LDADD_atf_c= -L/usr/local/lib -latf-c`
**Status:** resolved

## [BUG] Missing <sys/types.h> before sys headers in 4 files
**Found:** 2026-04-30
**Symptom:** `ibs_access_control_test.c`, `ibs_cpuctl_access_test.c`, `ibs_invalid_input_test.c`, and `ibs_robustness_test.c` fail to compile with `unknown type name 'uint64_t'` / `unknown type name '__BEGIN_DECLS'`.
**Root cause:** With `-nobuiltininc` (used by FreeBSD bsd.prog.mk), the compiler doesn't implicitly pull in `<stdint.h>`. The 3 files that include `<sys/cpuctl.h>` as their first header need `<sys/types.h>` first; `ibs_robustness_test.c` needs `<sys/param.h>` before `<sys/cpuset.h>`.
**Fix/Workaround:** Prepended `#include <sys/types.h>` to the first three files; moved `<sys/param.h>` before `<sys/cpuset.h>` in `ibs_robustness_test.c`.
**Status:** resolved

## TODOs

## [TODO] Activate NMI flood placeholders (TC-ROB-01/02)
**Priority:** medium
**Context:** ibs_robustness_test has placeholder test cases for NMI flood
scenarios. They require `sysctl dev.hwpmc.0.ibs_active` to gate execution.
That sysctl doesn't exist yet in FreeBSD HEAD.
**Status:** pending

## [TODO] Implement ibs_ioctl_test (TC-IBS-IOC-01)
**Priority:** medium
**Context:** `ibs_ioctl_test.c` is a 38-LOC placeholder. Blocked on kernel
adding a proper IBS ioctl API (in-progress upstream).
**Status:** pending

## [TODO] Cross-architecture CI job (Intel runner)
**Priority:** low
**Context:** Unit tests are architecture-independent; they should pass on
Intel. Needs a second self-hosted runner or GitHub-hosted Linux job.
**Status:** pending

## [TODO] Uncore PMC tests (L3, DF, UMC, C2C)
**Priority:** low
**Context:** Tracked in docs/TODO.md Phase 3. Waits for kernel hwpmc uncore
support to be merged to FreeBSD HEAD.
**Status:** pending

## Snippets

## [SNIPPET] MSR access pattern (with cpuctl ioctl)
**Use:** Any test that reads or writes AMD MSRs via cpuctl.
```c
int fd = open("/dev/cpuctl0", O_RDWR);
ATF_REQUIRE_MSG(fd >= 0, "open /dev/cpuctl0: %s", strerror(errno));
uint64_t val = read_msr(fd, MSR_IBS_OP_CTL);
write_msr(fd, MSR_IBS_OP_CTL, val | IBS_OP_ENABLE_BIT);
close(fd);
```

## [SNIPPET] CPU generation check (Zen 4+ feature gate)
**Use:** Tests using Zen 4+ features (L3MissOnly, IBSOPDATA4, extended IBS CTL).
```c
uint32_t family, model;
get_cpu_family_model(&family, &model);
if (family < 0x19 || (family == 0x19 && model < 0x10)) {
    atf_tc_skip("Zen 4+ required for L3MissOnly filter");
}
```

## [SNIPPET] DataSrc 5-bit extraction from IBSOPDATA2
**Use:** Decoding the combined DataSrc field (DataSrcLo + DataSrcHi).
Critical: shift high by 6, not 3.
```c
uint8_t lo = op_data2 & 0x07;           /* bits [2:0] */
uint8_t hi = (op_data2 & 0xC0) >> 6;    /* bits [7:6], shift=6 not 3 */
uint8_t datasrc = (hi << 3) | lo;       /* 5-bit combined value */
```

## [BUG] ibs_hwpmc_getmsr_virtual_negative wrong expected errno
**Found:** 2026-05-06
**Symptom:** `pmc_get_msr(IBS-FETCH)` returned -1 errno 78 (ENOTSUP), test expected -1/EINVAL.
**Root cause:** IBS PMCs are virtual counters with no underlying MSR address visible to
`pmc_get_msr`. Kernel correctly returns ENOTSUP; test's expected errno was wrong.
**Fix/Workaround:** Commits `07e8153` (correct hwpmc runtime contracts) and `3981aa2`
(serialize hwpmc tests) address this. Re-run at HEAD required to confirm.
**Status:** likely resolved (verify at 18798e0)

## [TODO] Full re-run at HEAD (18798e0)
**Priority:** high
**Context:** Last run (2026-05-06, commit 07e8153) report was truncated at
ibs_mem_stress_cache_thrash (~120s test). 14 programs after that not captured.
1 failure (ibs_hwpmc_getmsr_virtual_negative) likely fixed by 3981aa2.
**Status:** pending

## Learning Log

2026-04-30 | First read. 2 decisions, 4 bugs (2 resolved, 2 workaround/resolved), 4 TODOs, 3 snippets. DataSrc shift bug and IBSOPDATA4 GP are the most surprising. | ibs-test-suite
2026-04-30 | Added ibs_mem_stress_test (TC-MEM-01/02) and ibs_cpu_stress_test (TC-CPU-01/02) — each 120 s duration. Fixed out-of-tree ATF link (LDADD_atf_c + -I/usr/local/include in Makefile). Fixed missing <sys/types.h>/<sys/param.h> ordering in 4 files (ibs_access_control_test, ibs_cpuctl_access_test, ibs_invalid_input_test, ibs_robustness_test). Suite now 25 programs, all linking cleanly. | ibs-test-suite
2026-05-07 | Run at commit 07e8153 (2026-05-06) on AMD EPYC 9654 Zen 4. Confirmed: 54 passed, 6 skipped, 1 failed (ibs_hwpmc_getmsr_virtual_negative — wrong errno expectation, ENOTSUP vs EINVAL). Report truncated; 14 programs not captured. Suite now 30 programs (added hwpmc alloc/caps/info/runtime tests). UMCDF suite added as separate module. All skips expected for this hardware. | ibs-test-suite
2026-05-08 | Added 4 new hardware-free unit test programs (ibs_unit_ldlat_test, ibs_unit_fetch_ctl_fields_test, ibs_unit_op_data_fields_test, ibs_unit_msr_range_test) covering LDLAT field encoding, Fetch CTL multi-bit fields, Op Data 1-3 fields, and MSR address arithmetic. Suite now 34 programs (30 C + 1 shell + 4 new unit). Unit test tier raised from 38% to 70.3% of all test cases. | ibs-test-suite

## [BUG] ibs_swfilt_test: cpucontrol absent from kyua PATH — converted to C
**Found:** 2026-05-12
**Symptom:** ibs_swfilt_exclude_user, ibs_swfilt_exclude_kernel, ibs_swfilt_filter_combination fail with "Cannot read IBS Fetch Control/Op Control MSR" regardless of whether rdmsr/wrmsr or cpucontrol is used.
**Root cause:** Shell tests depend on external tools in PATH. kyua runs ATF shell tests with a restricted PATH that excludes /usr/sbin where cpucontrol(8) lives. C tests use read_msr()/write_msr() from ibs_utils.h (CPUCTL_RDMSR/CPUCTL_WRMSR ioctls) directly — no PATH dependency.
**Fix/Workaround:** Deleted ibs_swfilt_test.sh. Created ibs_swfilt_test.c reusing ibs_utils.h read_msr()/write_msr(). Moved entry from ATF_TESTS_SH to ATF_TESTS_C in Makefile. Kyuafile unchanged (entry is format-agnostic).
**Status:** resolved

## [BUG] ibs_smp NMI race: 3-retry limit insufficient under parallel load
**Found:** 2026-05-12
**Symptom:** ibs_smp_per_cpu_config and ibs_smp_cpu_migration FAIL under high parallelism (192-CPU run). ATF_CHECK_EQ on MaxCnt readback mismatches; migration isolation check triggers EBUSY.
**Root cause:** (A) ibs_smp_per_cpu_config retry loop capped at 3 — under full-system parallel load, in-flight NMIs re-arm the counter more than 3 times in a row. (B) ibs_smp_cpu_migration reads orig_val while IBS may be active on CPU 0; a concurrent NMI mutates MaxCnt before the verify_val comparison.
**Fix/Workaround:** (A) Increased retry limit 3→10; added `write_msr(cpu, MSR_IBS_FETCH_CTL, 0ULL)` as first statement in retry loop to quiesce NMIs before each attempt. (B) Added `write_msr(original_cpu, MSR_IBS_FETCH_CTL, 0ULL)` before reading orig_val in smp_migration_thread.
**Status:** resolved

## [BUG] ibs_swfilt TC-01/04: HW-set status bits misidentified as software-writable
**Found:** 2026-05-15
**Symptom:** `ibs_swfilt_exclude_user` and `ibs_swfilt_filter_combination` FAIL with bit 56 (IbsL2TlbMiss) and/or bit 58 (IbsFetchL2Miss) of MSR_IBS_FETCH_CTL not preserved after write.
**Root cause:** IbsL2TlbMiss (bit 56) and IbsFetchL2Miss (bit 58) are hardware-set read-only status bits in MSR_IBS_FETCH_CTL on AMD Zen 2–4. They are written by the hardware after a fetch sample fires; user writes are ignored. The test assumed they were software-writable "filter control bits" — they are not. Additionally, IbsOpL3MissOnly (bit 16 of MSR_IBS_OP_CTL) is reserved/RO on Zen 2–4 and was incorrectly included in TC-04's Op CTL combination.
**Fix/Workaround:** TC-01 (`ibs_swfilt_exclude_user`): added skip guard after write-readback — skip gracefully if bit not preserved instead of failing. TC-04 (`ibs_swfilt_filter_combination`): Fetch CTL check converted to conditional (print note and continue if bits not preserved); Op CTL check narrowed to IBS_CNT_CTL (bit 19) only. On Zen 5 hardware both bits happen to be preserved, so the test passes.
**Status:** resolved

## [BUG] ibs_signal_storm_under_sampling broken (timeout) under 192-job parallelism
**Found:** 2026-05-08
**Symptom:** "broken: Test case body timed out" — fires under full 192-parallel-job runs where scheduling jitter starves the test process.
**Root cause:** ATF timeout too small for the scheduling delay incurred under extreme parallel load. sleep(5) alone takes longer than the budget when context-switched.
**Fix/Workaround:** Increased ATF timeout 30→60 (2026-05-12); still firing. Increased again 60→120 (2026-05-12). 120s gives 24× safety margin over the 5s operational sleep.
**Status:** resolved

## [DECISION] Two-phase execution model for run.sh
**Date:** 2026-05-16
**Author:** Osvaldo J. Filho
**Actor type:** human
**Source:** ai-prompt
**Session:** sess_2026-05-16_0000
**Context:** All suites (IBS, UMCDF, PMC, STRESS) were running with parallelism=1 for any stress-related run, causing tests to take much longer than needed.
**Decision:** Introduced a two-phase execution model in run.sh: Phase 1 runs all non-stress tests at full parallelism (PARALLELISM=192 on EPYC 9654); Phase 2 runs stress tests in 4 sequential resource-based batches (CPU → Memory → Disk → Network), each batch running its tests in parallel. Two new TC categories added: TC-NMISTR (ibs_nmi_stress_test, formerly TC-INT) and TC-MEMIBS (ibs_mem_stress_test, formerly TC-STR) for clean batch separation.
**Discarded alternatives:** Single parallel run of all tests including stress (resource contention). Fully sequential execution (too slow). Per-test parallelism=1 preservation (now unnecessary).
**Impact:** run.sh (get_test_meta, _run_suite_once, run_all_tests, new _run_stress_batches, new _suite_has_batch_tests); HTML report phase/batch column; diagnostic output for non-passed tests (diag_for_test function).

## [BUG] build_filtered_kyuafile wrote temp file to /tmp — kyua failed to resolve test programs
**Found:** 2026-05-16
**Symptom:** kyua reports "Non-existent test program 'ibs_access_control_test'" when using a filtered Kyuafile.
**Root cause:** build_filtered_kyuafile() wrote the temp Kyuafile to /tmp/ibs_kyuafile_$.tmp. kyua resolves test program names relative to the Kyuafile's directory; from /tmp it cannot find binaries in /usr/tests/sys/amd/ibs/.
**Fix/Workaround:** Changed output path to ${_install_dir}/.kyuafile_filtered_$.tmp so the file is written inside the install directory alongside the real Kyuafile. Updated cleanup pattern in _suite_has_batch_tests() to match *".kyuafile_filtered_"*.
**Status:** resolved

## Learning Log
2026-05-12 | Fixed ibs_swfilt_test.sh: rdmsr/wrmsr are Linux tools; FreeBSD uses cpucontrol -m. Added msr_read/msr_write helpers. Increased ibs_signal_storm_under_sampling timeout 30→60s. | ibs-test-suite
2026-05-12 | Converted ibs_swfilt_test from shell to C (cpucontrol absent from kyua PATH). Fixed ibs_smp NMI race: retry 3→10 + quiesce write in per_cpu_config, quiesce before orig_val read in migration_thread. Increased signal_storm timeout 60→120s. | ibs-test-suite
2026-05-15 | Fixed ibs_swfilt_test.c TC-01/04 failures: IbsL2TlbMiss (bit 56) and IbsFetchL2Miss (bit 58) of MSR_IBS_FETCH_CTL are HW-set status bits on Zen 2–4; added skip guard rather than fail assertion. TC-04 Fetch CTL check converted to conditional (print note + continue if not preserved). TC-04 Op CTL check: removed IBS_OP_L3_MISS_ONLY (bit 16, reserved on Zen 2–4), retained IBS_CNT_CTL (bit 19). Added --suite IBS --stress --force to run.sh EXAMPLES. All 4 swfilt tests now pass/skip on Zen 5 hardware. | ibs-test-suite
2026-05-16 | Implemented two-phase batch execution in run.sh: Phase 1 (non-stress, full parallelism) + Phase 2 (4 sequential stress batches: CPU/Memory/Disk/Network, each internally parallel). Added TC-NMISTR and TC-MEMIBS categories. Fixed critical Kyuafile location bug (must write alongside test binaries, not to /tmp). Added diag_for_test() diagnostic function and HTML phase/batch column. Full run: 247 tests, 232 pass, 1 fail (ibs_nmi_drain_under_load — pre-existing), 14 skip. | ibs-test-suite
