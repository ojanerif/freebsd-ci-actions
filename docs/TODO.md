# IBS Test Suite — TODO

Target coverage mix: **70% Unit · 20% Integration · 10% E2E**

All new test files live under `tests/sys/amd/ibs/` and use the ATF-C
framework (`atf-c.h`) unless noted otherwise.

---

## Reuse from freebsd/freebsd-ci

`freebsd-freebsdi-ci/` is a local clone of https://github.com/freebsd/freebsd-ci
kept for reference (see `.gitignore`).  The following patterns can be lifted
directly.

### CI job skeleton

Create `ci/jobs/FreeBSD-main-amd64-test_ibs/` mirroring existing jobs:

| File | Source to copy/adapt | Notes |
|---|---|---|
| `build.sh` | `jobs/FreeBSD-main-amd64-test/build.sh` | Set `KERNCONF=GENERIC`, point kyua at `tests/sys/amd/ibs` |
| `meta/run.sh` | `jobs/FreeBSD-main-amd64-test/meta/run.sh` | Call `run-kyua.sh`; add IBS sysctl / module load steps |
| `xfail-list` | `jobs/FreeBSD-main-aarch64-KASAN_test/xfail-list` | Seed with all tests that skip on non-AMD or non-root; processed by `xmlstarlet` |

### KASAN job

Add `ci/jobs/FreeBSD-main-amd64-KASAN_test_ibs/` using `KERNCONF=GENERIC-KASAN`.
Run the full IBS suite under the Address Sanitizer kernel — the NMI handler
path and cpuctl MSR ioctl are good KASAN targets (heap use-after-free,
out-of-bounds in sample buffers).

### Test runner scripts to reuse as-is

| Script | Path in freebsd-ci | What it does |
|---|---|---|
| `run-kyua.sh` | `scripts/test/subr/run-kyua.sh` | cd /usr/tests; kyua test; kyua report-junit |
| `run-tests.sh` | `scripts/test/run-tests.sh` | bhyve/QEMU VM orchestration, timeout, meta disk, xfail patching |
| `create-meta.sh` / `extract-meta.sh` | `scripts/test/` | Passes in-VM scripts via tar; retrieves results |

### New subr scripts to write (modelled on disable-*.sh)

- `disable-non-ibs-tests.sh` — comment out non-IBS Kyuafile entries when
  running the IBS-only job, so unrelated failures don't pollute the report
- `setup-ibs-env.sh` — load `cpuctl.ko` if not built-in, set
  `hw.pmc.ibs_enable=1` sysctl, verify `/dev/cpuctl0` exists

### xfail-list format (xmlstarlet-based, from run-tests.sh lines 112–125)

```
# format: classname:testname  (one per line)
sys.amd.ibs.ibs_l3miss_test:ibs_l3miss_detect_zen4
sys.amd.ibs.ibs_cpu_test:ibs_cpu_zen5_detection
sys.amd.ibs.ibs_ioctl_test:ibs_ioctl_not_implemented
```

---

## 70% — Unit Tests

Unit tests require **no hardware, no kernel, no MSR access**.  They test
pure C logic that currently lives inline in `ibs_utils.h`.

### Prerequisite: split ibs_utils.h

Refactor `ibs_utils.h` to separate two concerns:

1. **Pure helpers** — bit manipulation, field extraction, constant
   verification.  These take `uint64_t` values directly and have no I/O.
   Move to a new `ibs_decode.h` (or a `static inline`-only section clearly
   marked "no I/O").

2. **Hardware accessors** — `read_msr`, `write_msr`, `do_cpuid_ioctl`,
   `cpu_supports_ibs`, `cpu_is_zen4`, etc.  Keep in `ibs_utils.h`.

This split is the prerequisite for all unit tests below.

---

### File: `ibs_unit_field_masks_test.c`   [TC-UNIT-MASK] ✅ Implemented

Verify every constant in `ibs_utils.h` / `ibs_decode.h` matches the AMD PPR.
No hardware.  All cases run on any architecture.

| Test case | What is checked |
|---|---|
| `ibs_unit_fetch_ctl_mask_values` | IBS_FETCH_MAXCNT=0xFFFF, IBS_FETCH_EN=bit48, IBS_FETCH_VAL=bit49, IBS_FETCH_COMP=bit50, IBS_L3_MISS_ONLY=bit59, IBS_RAND_EN=bit57 |
| `ibs_unit_op_ctl_mask_values` | IBS_OP_MAXCNT=0xFFFF, IBS_OP_EN=bit17, IBS_OP_VAL=bit18, IBS_CNT_CTL=bit19, IBS_OP_MAXCNT_EXT=bits20-26, IBS_OP_L3_MISS_ONLY=bit16 |
| `ibs_unit_op_data1_mask_values` | IBS_COMP_TO_RET_CTR=bits0-15, IBS_TAG_TO_RET_CTR=bits16-31, IBS_OP_RETURN=bit34, IBS_OP_BRN_TAKEN=bit35, IBS_OP_BRN_MISP=bit36, IBS_OP_BRN_RET=bit37 |
| `ibs_unit_op_data2_mask_values` | IBS_DATA_SRC_LO=bits0-2, IBS_RMT_NODE=bit4, IBS_CACHE_HIT_ST=bit5, IBS_DATA_SRC_HI=bits6-7 |
| `ibs_unit_op_data3_mask_values` | IBS_LD_OP=bit0, IBS_ST_OP=bit1, IBS_DC_MISS=bit7, IBS_DC_LIN_ADDR_VALID=bit17, IBS_DC_PHY_ADDR_VALID=bit18, IBS_DC_MISS_LAT=bits32-47 |
| `ibs_unit_msr_address_map` | All MSR address constants match AMD PPR table (0xC0011030..0xC001103D) |
| `ibs_unit_cpuid_constants` | CPUID_IBSID=0x8000001B, feature bit positions 0-6 correct |
| `ibs_unit_no_mask_overlap_fetch` | All IBS_FETCH_* masks are mutually non-overlapping (bitwise AND of any two = 0) |
| `ibs_unit_no_mask_overlap_op` | All IBS_OP_* masks in IBSOPCTL are mutually non-overlapping |

---

### File: `ibs_unit_helpers_test.c`   [TC-UNIT-HELP] ✅ Implemented

Test `ibs_get_maxcnt`, `ibs_set_maxcnt`, and `ibs_maxcnt_to_period` (the
last function needs to be added to `ibs_decode.h`).

| Test case | What is checked |
|---|---|
| `ibs_unit_get_maxcnt_zero` | ibs_get_maxcnt(0) == 0 |
| `ibs_unit_get_maxcnt_max` | ibs_get_maxcnt(0xFFFF) == 0xFFFF |
| `ibs_unit_get_maxcnt_no_bleed` | ibs_get_maxcnt(0xFFFFFFFFFFFF0000ULL) == 0 (upper bits don't bleed) |
| `ibs_unit_get_maxcnt_mid` | ibs_get_maxcnt(0x00DEADBEEF001234ULL) == 0x1234 |
| `ibs_unit_set_maxcnt_basic` | ibs_set_maxcnt(0, 0x1000) == 0x1000 |
| `ibs_unit_set_maxcnt_preserves_upper` | ibs_set_maxcnt(0xFFFFFFFFFFFF0000ULL, 0x1234) == 0xFFFFFFFFFFFF1234ULL |
| `ibs_unit_set_maxcnt_clears` | ibs_set_maxcnt(0xFFFFULL, 0) == 0 |
| `ibs_unit_set_maxcnt_clamps` | ibs_set_maxcnt(0, 0x1FFFF) == 0xFFFF (extra bits masked off) |
| `ibs_unit_roundtrip_all` | for n in 0..0xFFFF: ibs_get_maxcnt(ibs_set_maxcnt(0, n)) == n |
| `ibs_unit_period_min` | ibs_maxcnt_to_period(1) == 16 (0x10) |
| `ibs_unit_period_max` | ibs_maxcnt_to_period(0xFFFF) == 0xFFFF0 |
| `ibs_unit_period_zero` | ibs_maxcnt_to_period(0) == 0 |
| `ibs_unit_period_mid` | ibs_maxcnt_to_period(0x100) == 0x1000 |
| `ibs_unit_period_shift` | ibs_maxcnt_to_period(n) == n << 4 for representative n values |

---

### File: `ibs_unit_datasrc_test.c`   [TC-UNIT-DSRC] ✅ Implemented

Test DataSrc field extraction from `MSR_IBS_OP_DATA2`.  These are pure
bit-field operations; no hardware needed.

Add helper to `ibs_decode.h`:
```c
static inline uint32_t ibs_get_data_src(uint64_t op_data2);
```
where result = `((op_data2 & IBS_DATA_SRC_HI) >> 6) << 3 | (op_data2 & IBS_DATA_SRC_LO)`.

| Test case | Input op_data2 | Expected result |
|---|---|---|
| `ibs_unit_datasrc_l1` | 0x00 | 0 (local L1) |
| `ibs_unit_datasrc_l2` | 0x01 | 1 (local L2) |
| `ibs_unit_datasrc_dram` | 0x03 | 3 (local DRAM) |
| `ibs_unit_datasrc_remote_dram` | 0x04 | 4 (remote DRAM) |
| `ibs_unit_datasrc_hi_shift` | bits[7:6]=0x1, bits[2:0]=0x0 → value 0x40 | 0x08 (hi<<3) |
| `ibs_unit_datasrc_ext_8` | bits[7:6]=0x1, bits[2:0]=0x0 → 0x40 | 0x08 |
| `ibs_unit_datasrc_ext_c` | bits[7:6]=0x1, bits[2:0]=0x4 → 0x44 | 0x0C |
| `ibs_unit_datasrc_ext_d` | bits[7:6]=0x1, bits[2:0]=0x5 → 0x45 | 0x0D |
| `ibs_unit_datasrc_no_bleed` | 0xFFFFFFFFFFFFFF18ULL (bits outside [7:6] and [2:0] set) | same as 0x18 |
| `ibs_unit_datasrc_regression_shift6` | Regression: old code shifted hi by 3; verify shift is 6 |

---

### File: `ibs_unit_cpuid_parse_test.c`   [TC-UNIT-CPUID] ✅ Implemented

Extract CPU family/model/stepping parsing logic from `cpu_is_zen4`,
`cpu_is_zen5`, `cpu_get_family` into a pure helper:
```c
static inline uint32_t ibs_cpuid_family(uint32_t eax);
static inline uint32_t ibs_cpuid_model(uint32_t eax);
```
Unit test against known EAX values from AMD PPR.

| Test case | Input EAX | Expected family |
|---|---|---|
| `ibs_unit_family_zen1` | 0x00800F11 | 0x17 (Zen 1) |
| `ibs_unit_family_zen2` | 0x00870F10 | 0x17 (Zen 2 — same family) |
| `ibs_unit_family_zen3` | 0x00A00F10 | 0x19 (Zen 3) |
| `ibs_unit_family_zen4` | 0x00A20F10 | 0x19 (Zen 4) |
| `ibs_unit_family_zen5` | 0x00B10F00 | 0x1A (Zen 5) |
| `ibs_unit_family_intel` | 0x000306C3 | 0x06 (not AMD IBS range) |
| `ibs_unit_model_extraction` | 0x00A20F10 | model = 0x21 (extended model correct) |
| `ibs_unit_stepping_extraction` | 0x00A20F12 | stepping = 2 |
| `ibs_unit_is_zen4_true` | EAX 0x00A20F10 | cpu_is_zen4_from_eax() == true |
| `ibs_unit_is_zen4_false_zen3` | EAX 0x00A00F10 | cpu_is_zen4_from_eax() == false |
| `ibs_unit_is_zen5_true` | EAX 0x00B10F00 | cpu_is_zen5_from_eax() == true |

---

### File: `ibs_unit_op_ext_maxcnt_test.c`   [TC-UNIT-EXT] ✅ Implemented

Test the Zen 2+ extended MaxCnt field in IBSOPCTL (bits 26:20).

Add helper to `ibs_decode.h`:
```c
static inline uint64_t ibs_op_get_full_maxcnt(uint64_t opctl);
static inline uint64_t ibs_op_set_full_maxcnt(uint64_t opctl, uint64_t maxcnt);
```
where full_maxcnt = base[15:0] | (ext[26:20] << 16) — a 23-bit value.

| Test case | What is checked |
|---|---|
| `ibs_unit_ext_maxcnt_base_only` | full_maxcnt(0x1234) == 0x1234 (no ext bits) |
| `ibs_unit_ext_maxcnt_ext_only` | full_maxcnt(0x0100000) == 0x10000 (ext=1, base=0) |
| `ibs_unit_ext_maxcnt_combined` | full_maxcnt(0x0101234) == 0x11234 |
| `ibs_unit_ext_maxcnt_max` | full_maxcnt with ext=0x7F, base=0xFFFF == 0x7FFFFF |
| `ibs_unit_ext_set_roundtrip` | get_full(set_full(0, n)) == n for n in {0, 1, 0xFFFF, 0x10000, 0x7FFFFF} |
| `ibs_unit_ext_shift_is_20` | IBS_OP_MAXCNT_EXT_SHIFT == 20 |
| `ibs_unit_ext_mask_7bits` | IBS_OP_MAXCNT_EXT covers exactly 7 bits (bits 20-26) |

---

### File: `ibs_unit_feature_flags_test.c`   [TC-UNIT-FEAT] ✅ Implemented

Verify IBS CPUID feature flag bit positions and combinations.

Add pure helper to `ibs_decode.h`:
```c
static inline bool ibs_feat_fetch_sampling(uint32_t cpuid_ibsid_eax);
static inline bool ibs_feat_op_sampling(uint32_t cpuid_ibsid_eax);
static inline bool ibs_feat_zen4(uint32_t cpuid_ibsid_eax);
```

| Test case | What is checked |
|---|---|
| `ibs_unit_feat_fetch_sam_bit` | IBS_CPUID_FETCH_SAMPLING == (1 << 0) |
| `ibs_unit_feat_op_sam_bit` | IBS_CPUID_OP_SAMPLING == (1 << 1) |
| `ibs_unit_feat_rdwropcnt_bit` | IBS_CPUID_RDWROPCNT == (1 << 2) |
| `ibs_unit_feat_opcnt_bit` | IBS_CPUID_OPCNT == (1 << 3) |
| `ibs_unit_feat_brntarget_bit` | IBS_CPUID_BRANCH_TARGET_ADDR == (1 << 4) |
| `ibs_unit_feat_opdata4_bit` | IBS_CPUID_OP_DATA_4 == (1 << 5) |
| `ibs_unit_feat_zen4_bit` | IBS_CPUID_ZEN4_IBS == (1 << 6) |
| `ibs_unit_feat_parse_zen4` | ibs_feat_zen4(0x7F) == true, ibs_feat_zen4(0x3F) == false |
| `ibs_unit_feat_parse_none` | ibs_feat_fetch_sampling(0) == false |
| `ibs_unit_feat_independent_bits` | All six bits are independent (no aliases) |

---

## 20% — Integration Tests

Integration tests require the **cpuctl kernel driver** (`/dev/cpuctl0`) but
do NOT require actual IBS sampling hardware or active NMI delivery.  They
test the MSR read/write path through the kernel.

### Existing tests that already cover this tier

These count toward the 20% as-is (no changes needed):
- `ibs_msr_test:ibs_msr_read_write` — MSR read/write round-trip via cpuctl
- `ibs_api_test:ibs_fetch_ctl_roundtrip` — MaxCnt round-trip (Fetch)
- `ibs_api_test:ibs_op_ctl_roundtrip` — MaxCnt round-trip (Op)
- `ibs_api_test:ibs_ctl_reserved_bits` — reserved bit isolation via hardware
- `ibs_detect_test:ibs_detect` — CPUID leaf via cpuctl ioctl
- `ibs_detect_test:ibs_detect_extended` — extended CPUID leaf
- `ibs_detect_test:ibs_detect_msr_access` — MSR accessibility check
- `ibs_period_test:ibs_fetch_period_*` — period field encoding via hardware
- `ibs_period_test:ibs_op_period_*` — period field encoding via hardware
- `ibs_routing_test:ibs_fetch_enable_disable` — enable bit round-trip
- `ibs_routing_test:ibs_op_enable_disable` — enable bit round-trip

### New integration tests to add

#### File: `ibs_cpuctl_access_test.c`   [TC-CPUCTL] ✅ Implemented

Tests the cpuctl driver interface itself, not IBS hardware.

| Test case | What is checked |
|---|---|
| `ibs_cpuctl_open_rdwr` | `/dev/cpuctl0` opens O_RDWR as root; returns valid fd |
| `ibs_cpuctl_open_rdonly` | Open O_RDONLY; CPUCTL_RDMSR succeeds, CPUCTL_WRMSR returns EBADF/EPERM |
| `ibs_cpuctl_per_cpu_device` | `/dev/cpuctl0` through `/dev/cpuctl<ncpus-1>` all open; verify N equals sysconf(_SC_NPROCESSORS_ONLN) |
| `ibs_cpuctl_cpuid_ibs_leaf` | CPUCTL_CPUID with leaf 0x8000001B returns; result may be zero on non-IBS CPU (skip), non-zero on IBS CPU |
| `ibs_cpuctl_cpuid_basic_leaf` | CPUCTL_CPUID leaf 0x0 always succeeds; verify max_leaf >= 1 |

#### File: `ibs_access_control_test.c`   [TC-ACCTL] ✅ Implemented

| Test case | What is checked |
|---|---|
| `ibs_nonroot_msr_read` | As unprivileged user: CPUCTL_RDMSR on MSR_IBS_FETCH_CTL returns EPERM or EACCES |
| `ibs_nonroot_msr_write` | As unprivileged user: CPUCTL_WRMSR on MSR_IBS_FETCH_CTL returns EPERM or EACCES |
| `ibs_nonroot_cpuctl_open` | As unprivileged user: open("/dev/cpuctl0", O_RDWR) returns EACCES |
| `ibs_root_msr_accessible` | As root: read MSR_IBS_FETCH_CTL returns 0 (or ENODEV on non-IBS CPU — skip) |

#### File: `ibs_invalid_input_test.c`   [TC-INV] ✅ Implemented

| Test case | What is checked |
|---|---|
| `ibs_invalid_msr_below_range` | CPUCTL_RDMSR on 0xC001102F (one below IBS range) returns EFAULT or EIO, not panic |
| `ibs_invalid_msr_gap` | CPUCTL_RDMSR on 0xC001103B (MSR_AMD64_IBSBRTARGET — may be read-only or restricted) returns EIO/0 — not panic |
| `ibs_invalid_msr_above_range` | CPUCTL_RDMSR on 0xC001103E (one above IBSOPDATA4) returns EFAULT or EIO |
| `ibs_invalid_msr_garbage` | CPUCTL_RDMSR on 0xDEADBEEF returns EFAULT or EIO |
| `ibs_wrmsr_null_argp` | ioctl(fd, CPUCTL_WRMSR, NULL) returns EFAULT |
| `ibs_rdmsr_null_argp` | ioctl(fd, CPUCTL_RDMSR, NULL) returns EFAULT |
| `ibs_unknown_ioctl_cmd` | ioctl(fd, 0xDEAD, NULL) returns ENOTTY or EINVAL |

---

## 10% — End-to-End (E2E) Tests

E2E tests require actual **AMD IBS hardware** and active NMI delivery.  All
must call `atf_tc_skip("CPU does not support IBS")` when not on AMD+IBS.

### Existing tests that already cover this tier

These count toward the 10% as-is:
- `ibs_interrupt_test` (all cases) — NMI delivery and VAL bit observation
- `ibs_data_accuracy_test` (all cases) — real sample field decoding
- `ibs_smp_test` (all cases) — multi-CPU sampling
- `ibs_stress_test` (all cases) — rapid MSR cycling, long-running, concurrent
- `ibs_l3miss_test` (all cases) — L3MissOnly filter

### New E2E tests to add

#### Kernel survival / robustness

These confirm the **kernel does not panic or hang** under adversarial input.
A SKIP is acceptable; a crash or watchdog trip is a hard failure.

| Test case | File | What is tested |
|---|---|---|
| `ibs_robustness_nmi_flood` [TC-ROB-01] | `ibs_robustness_test.c` | MaxCnt=1 (16-cycle period), Fetch+Op both enabled 5 s; NMI storm must not trigger watchdog or panic — **PLACEHOLDER: skips until hwpmc IBS NMI handler verified active** |
| `ibs_robustness_all_cpus_nmi_flood` [TC-ROB-02] | same | One thread per CPU all flooding simultaneously — **PLACEHOLDER: same prerequisite as TC-ROB-01** |
| `ibs_robustness_reserved_bits_with_enable` [TC-ROB-03] | same | Write 0xFFFFFFFFFFFFFFFF to IBSFETCHCTL (enable bits set); kernel must not forward #GP as panic; verify EFAULT/EIO or readback shows only defined bits — ✅ Implemented |
| `ibs_robustness_fork_under_sampling` [TC-ROB-04] | same | Enable IBS Fetch on CPU 0, fork(), child pins CPU 1 and clears that CPU's IBS, exits; parent CPU 0 MSR state must be unchanged — ✅ Implemented |
| `ibs_robustness_rapid_affinity_switch` [TC-ROB-05] | same | Enable IBS Fetch on CPU 0, migrate thread across all CPUs; verify no cross-CPU state bleed — ✅ Implemented |

#### Wrong architecture / non-AMD CI job

| Task | Description |
|---|---|
| `ibs_cross_arch_all_skip` (CI job) | Run full kyua IBS suite on a non-AMD runner (Intel); assert every result is SKIP, none is FAIL.  Add as a separate CI matrix job in `ci/jobs/FreeBSD-main-intel-test_ibs/`. |
| `ibs_cross_arch_cpuid_mismatch` | On the AMD machine, run a test that clears AMDID2_IBS from a captured CPUID value and passes it to the refactored pure helpers; verify all helpers return false/skip — catches tests that bypass the CPUID gate. |

#### Concurrency

| Test case | File | What is tested |
|---|---|---|
| `ibs_concurrent_multiprocess` [TC-CON-01] | `ibs_concurrency_test.c` | fork() N children (one per CPU), each opens `/dev/cpuctl<N>` and hammers its MSRs; all must exit 0 — ✅ Implemented |
| `ibs_signal_storm_under_sampling` [TC-CON-02] | same | Enable IBS Op + SIGALRM at 1 kHz from second thread; no SIGSEGV or crash — ✅ Implemented |

---

## Test count targets

| Tier | Target tests | Status |
|---|---|---|
| Unit (70%) | ~65 | 50 written — `ibs_unit_field_masks_test` (9), `ibs_unit_helpers_test` (14), `ibs_unit_datasrc_test` (10), `ibs_unit_cpuid_parse_test` (11), `ibs_unit_op_ext_maxcnt_test` (7), `ibs_unit_feature_flags_test` (10); `ibs_decode.h` prerequisite split done |
| Integration (20%) | ~18 | ~14 existing + 16 new — `ibs_cpuctl_access_test` (5 / TC-CPUCTL), `ibs_access_control_test` (4 / TC-ACCTL), `ibs_invalid_input_test` (7 / TC-INV) |
| E2E (10%) | ~9 | ~7 existing + 7 new — `ibs_robustness_test` (3 impl + 2 placeholder / TC-ROB), `ibs_concurrency_test` (2 / TC-CON) |
| **Total** | **~92** | **~73 written** (~21 pre-existing + 50 unit + 16 integration + 7 E2E new) |

---

## Notes

- Unit tests have no `require.user=root` and no `require.machine=amd64` — they
  must build and pass on any platform.
- Integration and E2E tests must set `atf_tc_set_md_var(tc, "require.user", "root")`.
- E2E tests targeting IBS hardware must call `cpu_supports_ibs()` at the top
  of the body and `atf_tc_skip()` if false.
- NMI flood tests need `atf_tc_set_md_var(tc, "timeout", "120")`.
- The cross-arch CI job belongs in a separate `jobs/FreeBSD-main-intel-test_ibs`
  directory; it cannot share a runner with the AMD IBS machines.

---

## Uncore PMC — Off-Core Performance Monitoring

> **Status: Planned — pending kernel-side hwpmc uncore support.**

Validates AMD uncore PMU units (L3 cache, Data Fabric, UMC, C2C) through the
FreeBSD hwpmc subsystem.

Framework: ATF + Kyua.  Target: FreeBSD 14.x / CURRENT with hwpmc compiled in.

### TC-UNC-DET — Uncore PMU Detection

| Test ID | Description | Expected Result |
|---|---|---|
| TC-UNC-DET-01 | Check dmesg for L3, DF, UMC, C2C detection on supported AMD CPU | All supported units detected |
| TC-UNC-DET-02 | Check sysctl for uncore PMU capabilities (`hw.pmc.amd_l3_*`, `amd_df_*`, `amd_umc_*`) | Correct capability flags per unit |
| TC-UNC-DET-03 | Run on unsupported/pre-Zen CPU; verify graceful fallback | No panic; units disabled cleanly |
| TC-UNC-DET-04 | Verify reported unit count matches hardware topology for L3, DF, UMC | Count matches physical topology |

### TC-UNC-L3 — L3 Cache PMU

| Test ID | Description | Expected Result |
|---|---|---|
| TC-UNC-L3-01 | L3 miss counter; cache-unfriendly workload | Counter increments proportionally |
| TC-UNC-L3-02 | L3 hit counter; cache-friendly workload | High hit rate reflected |
| TC-UNC-L3-03 | L3 eviction counter; fill cache beyond capacity | Eviction count increases |
| TC-UNC-L3-04 | Verify counters scoped per cache slice; pin to one CCX | Activity isolated to CCX |
| TC-UNC-L3-05 | Disable / re-enable; verify no stale values | Counter resets cleanly |
| TC-UNC-L3-06 | Simultaneous collection on all L3 slices | Independent counts; no corruption |

### TC-UNC-DF — Data Fabric PMU

| Test ID | Description | Expected Result |
|---|---|---|
| TC-UNC-DF-01 | DF memory read counter; memory-intensive workload | Counter increments proportionally |
| TC-UNC-DF-02 | DF memory write counter; write-intensive workload | Counter increments proportionally |
| TC-UNC-DF-03 | DF remote NUMA counter; cross-NUMA workload on multi-socket | Reflects cross-socket traffic |
| TC-UNC-DF-04 | DF counters during idle system | No spurious increments |
| TC-UNC-DF-05 | Rapid enable/disable cycle (100×) | No panic, lockup, MSR corruption |
| TC-UNC-DF-06 | Invalid event selector | Error returned; no counter enabled |
| TC-UNC-DF-07 | DF bandwidth vs known memory benchmark | Within 10% of benchmark result |

### TC-UNC-UMC — Unified Memory Controller PMU

| Test ID | Description | Expected Result |
|---|---|---|
| TC-UNC-UMC-01 | UMC read bandwidth; streaming memory read workload | Consistent with expected footprint |
| TC-UNC-UMC-02 | UMC write bandwidth; streaming write workload | Reflects write traffic |
| TC-UNC-UMC-03 | DRAM access per channel; verify per-channel distribution | Matches firmware interleaving policy |
| TC-UNC-UMC-04 | Workload on NUMA node 0; verify node 1 UMC near zero | Node 1 shows no significant activity |
| TC-UNC-UMC-05 | UMC counter during idle | Stable at zero |
| TC-UNC-UMC-06 | Simultaneous collection on all channels under stress | Independent counts; no corruption |
| TC-UNC-UMC-07 | Disable / re-enable; verify clean reset | Counter resets correctly |

### TC-UNC-MEM — Memory Subsystem (cross-validation)

| Test ID | Description | Expected Result |
|---|---|---|
| TC-UNC-MEM-01 | Cross-validate DF memory requests vs UMC access counters | Consistent within expected margin |
| TC-UNC-MEM-02 | Memory counters vs numastat-reported NUMA traffic | Consistent with OS-level stats |
| TC-UNC-MEM-03 | NUMA-local workload; remote counters near zero | Local/remote split matches binding |
| TC-UNC-MEM-04 | Force NUMA-remote access; verify remote counters increase | Reflects cross-node volume |
| TC-UNC-MEM-05 | Simultaneous collection on all NUMA nodes for 60 s | Stable; no kernel warnings |

### TC-UNC-C2C — Cache-to-Cache Interconnect PMU

| Test ID | Description | Expected Result |
|---|---|---|
| TC-UNC-C2C-01 | C2C counter; producer-consumer on same CCX | Intra-CCX transfers reflected |
| TC-UNC-C2C-02 | C2C counter; producer-consumer on different CCX dies | Cross-CCX transfer volume |
| TC-UNC-C2C-03 | C2C workload between sockets on multi-socket system | Cross-socket events captured |
| TC-UNC-C2C-04 | No sharing between cores; C2C counter stays zero | No C2C events on independent workload |
| TC-UNC-C2C-05 | Disable / re-enable mid-workload | Clean resume from zero; no stale state |
| TC-UNC-C2C-06 | False-sharing workload; verify invalidation traffic captured | Invalidation counter increases |

---

## Misc PMC — Metrics, Top-Down Analysis, Per-Process, API

> **Status: Planned — not yet implemented.**

Framework: ATF + Kyua.  Target: FreeBSD 14.x / CURRENT with hwpmc compiled in.

### TC-MISC-METRICS — Performance Metrics

| Test ID | Description | Expected Result |
|---|---|---|
| TC-MISC-METRICS-01 | IPC for compute-bound workload (retired instructions + cycles) | Consistent with theoretical throughput |
| TC-MISC-METRICS-02 | IPC for memory-bound workload (streaming) | Notably lower than compute-bound |
| TC-MISC-METRICS-03 | L1/L2/L3 miss rates on cache-thrashing workload | Miss rates increase per cache tier |
| TC-MISC-METRICS-04 | Cache miss rate for cache-friendly workload | Near-zero L2/L3 miss rate |
| TC-MISC-METRICS-05 | Branch misprediction rate; unpredictable branch pattern | Reflects unpredictability |
| TC-MISC-METRICS-06 | Branch misprediction rate; fully predictable branches | Near zero |
| TC-MISC-METRICS-07 | UMC bandwidth at known transfer rate | Within 10% of theoretical throughput |
| TC-MISC-METRICS-08 | All metric counter pairs collected concurrently | No multiplexing conflicts |

### TC-MISC-TOPDOWN — Top-Down Microarchitecture Analysis

| Test ID | Description | Expected Result |
|---|---|---|
| TC-MISC-TOPDOWN-01 | All four top-down categories collected; verify they sum to 100% | Sum within 1% margin |
| TC-MISC-TOPDOWN-02 | Compute-bound workload; Retiring dominates | Retiring fraction significantly highest |
| TC-MISC-TOPDOWN-03 | High branch misprediction workload; Bad Speculation dominates | Reflects misprediction-heavy pattern |
| TC-MISC-TOPDOWN-04 | Large code footprint; Frontend Bound dominates | Reflects fetch/decode bottleneck |
| TC-MISC-TOPDOWN-05 | Pointer-chasing workload; Backend Bound dominates | Reflects memory latency stall |
| TC-MISC-TOPDOWN-06 | Per-core top-down in SMP; no cross-core interference | Each core reports independently |
| TC-MISC-TOPDOWN-07 | Repeated runs of same workload; verify result stability | Reproducible within 5% across runs |

### TC-MISC-PROC — Per-Process PMC

| Test ID | Description | Expected Result |
|---|---|---|
| TC-MISC-PROC-01 | Attach counter to running process mid-execution | Counting starts from attach point |
| TC-MISC-PROC-02 | Detach counter; verify counting stops; value preserved | Accumulated value retained |
| TC-MISC-PROC-03 | Attach/detach rapid cycle (100×) | No panic, leak, or corruption |
| TC-MISC-PROC-04 | Attach to process A and B simultaneously; independent workloads | No cross-process leakage |
| TC-MISC-PROC-05 | Two processes with identical workloads | Equivalent counter values |
| TC-MISC-PROC-06 | Per-thread counter on multi-threaded process | Values proportional to thread workload |
| TC-MISC-PROC-07 | One active thread, one idle; verify asymmetry | Active thread counter significantly higher |
| TC-MISC-PROC-08 | Monitored process exits unexpectedly | No resource leak; session cleaned up |
| TC-MISC-PROC-09 | Attach to process owned by different user without root | EPERM returned |
| TC-MISC-PROC-10 | Stack unwinding on binary with debug symbols | Full call chain with correct symbols |
| TC-MISC-PROC-11 | Stack unwinding on stripped binary | Graceful fallback; no crash |
| TC-MISC-PROC-12 | Merged kernel + userspace stacks | Both frames in correct order |

### TC-MISC-API — API Stability (hwpmc / libpmc)

| Test ID | Description | Expected Result |
|---|---|---|
| TC-MISC-API-01 | Open/close hwpmc session in tight loop (1000×) | No fd/memory leak or kernel warning |
| TC-MISC-API-02 | All major libpmc entry points in correct sequence | All return success |
| TC-MISC-API-03 | libpmc entry points in incorrect order | Correct POSIX error codes; no crash |
| TC-MISC-API-04 | `pmc_read()` on idle counter repeated | Stable or monotonically increasing |
| TC-MISC-API-05 | Unknown ioctl command to hwpmc | EINVAL or ENOTTY; no kernel panic |
| TC-MISC-API-06 | Allocate maximum supported PMC counters | All up to limit; ENOMEM beyond |
| TC-MISC-API-07 | `pmc_name_of_event()` / `pmc_event_names_of_class()` repeated | Consistent results; no memory corruption |
| TC-MISC-API-08 | Concurrent libpmc calls from two threads on same session | No race, crash, or data corruption |
