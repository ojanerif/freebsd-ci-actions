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

### File: `ibs_unit_field_masks_test.c`   [TC-UNIT]

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

### File: `ibs_unit_helpers_test.c`   [TC-UNIT]

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

### File: `ibs_unit_datasrc_test.c`   [TC-UNIT]

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

### File: `ibs_unit_cpuid_parse_test.c`   [TC-UNIT]

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

### File: `ibs_unit_op_ext_maxcnt_test.c`   [TC-UNIT]

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

### File: `ibs_unit_feature_flags_test.c`   [TC-UNIT]

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

#### File: `ibs_cpuctl_access_test.c`   [TC-INT]

Tests the cpuctl driver interface itself, not IBS hardware.

| Test case | What is checked |
|---|---|
| `ibs_cpuctl_open_rdwr` | `/dev/cpuctl0` opens O_RDWR as root; returns valid fd |
| `ibs_cpuctl_open_rdonly` | Open O_RDONLY; CPUCTL_RDMSR succeeds, CPUCTL_WRMSR returns EBADF/EPERM |
| `ibs_cpuctl_per_cpu_device` | `/dev/cpuctl0` through `/dev/cpuctl<ncpus-1>` all open; verify N equals sysconf(_SC_NPROCESSORS_ONLN) |
| `ibs_cpuctl_cpuid_ibs_leaf` | CPUCTL_CPUID with leaf 0x8000001B returns; result may be zero on non-IBS CPU (skip), non-zero on IBS CPU |
| `ibs_cpuctl_cpuid_basic_leaf` | CPUCTL_CPUID leaf 0x0 always succeeds; verify max_leaf >= 1 |

#### File: `ibs_access_control_test.c`   [TC-INT]

| Test case | What is checked |
|---|---|
| `ibs_nonroot_msr_read` | As unprivileged user: CPUCTL_RDMSR on MSR_IBS_FETCH_CTL returns EPERM or EACCES |
| `ibs_nonroot_msr_write` | As unprivileged user: CPUCTL_WRMSR on MSR_IBS_FETCH_CTL returns EPERM or EACCES |
| `ibs_nonroot_cpuctl_open` | As unprivileged user: open("/dev/cpuctl0", O_RDWR) returns EACCES |
| `ibs_root_msr_accessible` | As root: read MSR_IBS_FETCH_CTL returns 0 (or ENODEV on non-IBS CPU — skip) |

#### File: `ibs_invalid_input_test.c`   [TC-INT]

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
| `ibs_robustness_nmi_flood` | `ibs_robustness_test.c` | MaxCnt=1 (16-cycle period), Fetch+Op both enabled 5 s; NMI storm must not trigger watchdog or panic |
| `ibs_robustness_all_cpus_nmi_flood` | same | One thread per CPU all flooding simultaneously; system must remain responsive |
| `ibs_robustness_reserved_bits_with_enable` | same | Write 0xFFFFFFFFFFFFFFFF to IBSFETCHCTL and IBSOPCTL (enable bits set); kernel must not forward #GP as panic; verify EFAULT/EIO or readback shows only defined bits |
| `ibs_robustness_fork_under_sampling` | same | Enable IBS Op in parent, fork(), child disables IBS and exits; parent's MSR state must be unchanged |
| `ibs_robustness_rapid_affinity_switch` | same | Enable IBS Fetch on CPU 0, migrate thread rapidly across all CPUs; verify no cross-CPU state bleed |

#### Wrong architecture / non-AMD CI job

| Task | Description |
|---|---|
| `ibs_cross_arch_all_skip` (CI job) | Run full kyua IBS suite on a non-AMD runner (Intel); assert every result is SKIP, none is FAIL.  Add as a separate CI matrix job in `ci/jobs/FreeBSD-main-intel-test_ibs/`. |
| `ibs_cross_arch_cpuid_mismatch` | On the AMD machine, run a test that clears AMDID2_IBS from a captured CPUID value and passes it to the refactored pure helpers; verify all helpers return false/skip — catches tests that bypass the CPUID gate. |

#### Concurrency

| Test case | File | What is tested |
|---|---|---|
| `ibs_concurrent_multiprocess` | `ibs_concurrency_test.c` | fork() N children (one per CPU), each opens `/dev/cpuctl<N>` and hammers its MSRs; all must exit 0 — no cross-process fd corruption |
| `ibs_signal_storm_under_sampling` | same | Enable IBS Op (short period) + send SIGALRM at 1 kHz from second thread; neither signal handler nor NMI path should SIGSEGV |

---

## Test count targets

| Tier | Target tests | Status |
|---|---|---|
| Unit (70%) | ~65 | 0 written — all TODO |
| Integration (20%) | ~18 | ~14 existing + 4 new TODO |
| E2E (10%) | ~9 | ~7 existing + 2–3 new TODO |
| **Total** | **~92** | **~21 existing** |

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
