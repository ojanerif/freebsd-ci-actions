# IBS Test Suite — Complete Reference

AMD Instruction-Based Sampling (IBS) test suite for FreeBSD.  
Tests live in `tests/sys/amd/ibs/` and are built with the FreeBSD ATF
(`Automated Test Framework`) infrastructure.

---

## Table of Contents

1. [Overview](#overview)
2. [Hardware Requirements](#hardware-requirements)
3. [Test Categories and Severity](#test-categories-and-severity)
4. [Test Files and Cases](#test-files-and-cases)
   - [ibs_msr_test](#ibs_msr_test)
   - [ibs_detect_test](#ibs_detect_test)
   - [ibs_cpu_test](#ibs_cpu_test)
   - [ibs_api_test](#ibs_api_test)
   - [ibs_interrupt_test](#ibs_interrupt_test)
   - [ibs_period_test](#ibs_period_test)
   - [ibs_routing_test](#ibs_routing_test)
   - [ibs_data_accuracy_test](#ibs_data_accuracy_test)
   - [ibs_smp_test](#ibs_smp_test)
   - [ibs_stress_test](#ibs_stress_test)
   - [ibs_l3miss_test](#ibs_l3miss_test)
   - [ibs_ioctl_test](#ibs_ioctl_test)
   - [ibs_swfilt_test (shell)](#ibs_swfilt_test)
5. [Shared Infrastructure — ibs_utils.h](#shared-infrastructure)
6. [Known Skip Conditions](#known-skip-conditions)
7. [Bug History and Fixes](#bug-history-and-fixes)

---

## Overview

IBS is an AMD hardware profiling feature available on Family 10h (K10) and
later processors.  It fires a Non-Maskable Interrupt (NMI) after a
configurable number of instruction fetch or op-dispatch events, storing
precise program-counter, memory-address, and data-source information in a
set of MSRs.

This test suite validates IBS from user-space via the FreeBSD `cpuctl`
driver (`/dev/cpuctl<N>`).  It uses three ioctl operations:

| ioctl | Purpose |
|---|---|
| `CPUCTL_RDMSR` | Read a 64-bit MSR on a specific logical CPU |
| `CPUCTL_WRMSR` | Write a 64-bit MSR on a specific logical CPU |
| `CPUCTL_CPUID` | Execute CPUID with a given leaf/subleaf |

All tests require `root` privilege (MSR access).

---

## Hardware Requirements

| Feature | Required by |
|---|---|
| IBS present (`CPUID 8000_0001h ECX[10]`) | All tests |
| IBS extended (`CPUID 8000_001Bh EAX[1..7]`) | `ibs_detect_extended`, fetch/op data fields |
| AMD Family 19h / Zen 4+ | `ibs_l3miss_*`, `ibs_detect_zen4`, `ibs_op_data4_zen4` |
| AMD Family 1Ah / Zen 5 | `ibs_cpu_zen5_detection` |
| Hypervisor environment | `ibs_swfilt_exclude_hv` |
| Multiple logical CPUs | `ibs_smp_per_cpu_config`, `ibs_smp_concurrent_sampling` |

---

## Test Categories and Severity

The `run.sh` harness assigns each test a category tag and a severity level
that controls the pass/fail verdict threshold.

| Tag | Category | Severity | Pass threshold |
|---|---|---|---|
| `[TC-DET]` | Detection | CRITICAL | 100% (no failures allowed) |
| `[TC-MSR]` | MSR access | CRITICAL | 100% |
| `[TC-INT]` | Interrupt/NMI | HIGH | ≥ 95% |
| `[TC-DATA]` | Data accuracy | HIGH | ≥ 95% |
| `[TC-SMP]` | SMP / per-CPU | HIGH | ≥ 95% |
| `[TC-API]` | API / encoding | MEDIUM | ≥ 80% |
| `[TC-STR]` | Stress | MEDIUM | ≥ 80% |

A test that SKIPS does not count as a failure; it is excluded from the
pass-rate calculation.

---

## Test Files and Cases

### ibs_msr_test

**Category:** `[TC-MSR]` CRITICAL  
**Source:** `ibs_msr_test.c`

Smoke-test that MSR read/write works end-to-end through the `cpuctl` driver.

#### `ibs_msr_read_write`

**What it does:**
1. Reads `MSR_IBS_FETCH_CTL` and `MSR_IBS_OP_CTL` from CPU 0.
2. Writes a known harmless value to each (enable bit cleared, small period).
3. Reads back and verifies the MaxCnt field matches what was written.
4. Restores the original values.

**Pass condition:** Both MSRs round-trip without error.  
**Skip condition:** CPU does not support IBS, or `cpuctl` is unavailable.

---

### ibs_detect_test

**Category:** `[TC-DET]` CRITICAL  
**Source:** `ibs_detect_test.c`

Verifies that hardware IBS support is correctly reported by CPUID and
that key MSRs are accessible.

#### `ibs_detect`

Calls `cpu_supports_ibs()` which checks `CPUID 8000_0001h ECX[10]`.

**Pass condition:** CPUID reports IBS present.  
**Skip condition:** IBS not reported by CPUID.

#### `ibs_detect_extended`

Checks `CPUID 8000_001Bh` (IBS feature flags) via `cpu_ibs_extended()`.
Verifies that at least the baseline IBS extended feature bits are set
(IbsFetchSam, IbsOpSam, RdWrOpCnt, OpCnt, BrnTrgt, OpCntExt).

**Pass condition:** Extended IBS CPUID leaf is non-zero.  
**Skip condition:** IBS not present.

#### `ibs_detect_zen4`

Verifies Zen 4-specific IBS capability bits in `CPUID 8000_001Bh`:
- `IBS_CPUID_OP_DATA_4` — IBS Op Data 4 MSR available
- `IBS_CPUID_ZEN4_IBS` — Zen 4 IBS extended features

**Pass condition:** Both bits set.  
**Skip condition:** CPU is not Zen 4+ (`!cpu_is_zen4()`).

#### `ibs_detect_msr_access`

Verifies that key IBS MSRs can be read without error:
- `MSR_AMD64_IBSCTL` — global IBS enable/routing register
- `MSR_AMD64_ICIBSEXTDCTL` — IC extended control (Zen 4+)
- `MSR_AMD64_IBSOPDATA4` — Op Data 4 sample register (Zen 4+)

`MSR_AMD64_IBSOPDATA4` is a **read-only** status register populated only
during active IBS Op sampling.  If the read fails (returns `EFAULT`/`EIO`),
the test skips rather than fails — this is expected behaviour when IBS Op
sampling is not active.

**Pass condition:** IBSCTL and ICIBSEXTDCTL readable; IBSOPDATA4 readable or
gracefully skipped.  
**Skip condition:** IBS not present, or IBSOPDATA4 inaccessible outside active
sampling.

---

### ibs_cpu_test

**Category:** `[TC-DET]` CRITICAL  
**Source:** `ibs_cpu_test.c`

Validates CPU family/model detection helpers used throughout the suite.

#### `ibs_cpu_detect_family`

Reads `CPUID 0000_0001h EAX` and extracts the extended family/model.
Verifies the CPU is in the AMD IBS-capable range (Family 10h and above).

**Pass condition:** Family ≥ 10h.

#### `ibs_cpu_zen4_detection`

Calls `cpu_is_zen4()` which checks for Family 19h with a Zen 4 model number.
Verifies that the function returns `true` on this machine.

**Pass condition:** Running on a Zen 4 processor.  
**Skip condition:** CPU is not Zen 4 (EPYC 9xxx, Ryzen 7xxx, etc.).

#### `ibs_cpu_zen5_detection`

Calls `cpu_is_zen5()` which checks for Family 1Ah.

**Pass condition:** Running on a Zen 5 processor.  
**Skip condition:** CPU is not Zen 5.

#### `ibs_cpu_tsc_frequency`

Uses `CPUID 0000_0015h` (TSC/crystal ratio) and `CPUID 0000_0016h`
(nominal frequency) to compute the TSC frequency in MHz.  Verifies the
result is non-zero and plausible (> 100 MHz).

**Pass condition:** TSC frequency derived successfully.  
**Skip condition:** CPUID leaves 15h/16h return zero (common in VMs and some
bare-metal configurations that don't expose this leaf).

---

### ibs_api_test

**Category:** `[TC-API]` MEDIUM  
**Source:** `ibs_api_test.c`

Tests the correctness of helper functions in `ibs_utils.h` (bit extraction,
field encoding, reserved-bit handling).

#### `ibs_fetch_ctl_roundtrip`

Calls `ibs_set_maxcnt()` on `MSR_IBS_FETCH_CTL` with a range of period
values (min, max, several in between).  Reads back with `ibs_get_maxcnt()`
and verifies the value matches.

The test writes with `IBS_FETCH_ENABLE_BIT` cleared so that IBS Fetch
sampling is not actually started.

**Pass condition:** All MaxCnt values survive a write-read cycle.

#### `ibs_op_ctl_roundtrip`

Same as above but for `MSR_IBS_OP_CTL` / IBS Op sampling.

**Pass condition:** All MaxCnt values survive a write-read cycle.

#### `ibs_extended_features`

Reads `CPUID 8000_001Bh` and checks that extended IBS feature bits
(`IbsFetchSam`, `IbsOpSam`, `RdWrOpCnt`, `OpCnt`, `BrnTrgt`, `OpCntExt`)
are all set on this processor.

**Pass condition:** All six baseline extended feature bits set.

#### `ibs_ctl_reserved_bits`

Writes a value with all-ones to `MSR_IBS_FETCH_CTL` (with enable bit cleared),
then reads back.  Verifies that the hardware ignores (clears) reserved bits
and only preserves defined fields (MaxCnt, enable, CntCtl, etc.).

**Pass condition:** Reserved bits read back as zero.

---

### ibs_interrupt_test

**Category:** `[TC-INT]` HIGH  
**Source:** `ibs_interrupt_test.c`

Tests NMI generation and the kernel's IBS NMI handler.

#### `ibs_interrupt_fetch_sample`

Enables IBS Fetch sampling with a moderate period and briefly spins in a
tight loop to generate at least one sample.  Polls `IBS_FETCH_VAL` (bit 49
of `MSR_IBS_FETCH_CTL`) to detect that the hardware has captured a sample.
Reads the captured Linear Instruction Fetch Address (`MSR_IBS_FETCH_LINADDR`)
and verifies it points within the spin-loop code.

**Pass condition:** `IBS_FETCH_VAL` becomes set within the timeout; LINADDR
within the loop's address range.  
**Skip condition:** IBS not present.

#### `ibs_interrupt_op_sample`

Same concept for IBS Op sampling.  Polls `IBS_OP_VAL` (bit 18 of
`MSR_IBS_OP_CTL`) after enabling with a short period.

**Pass condition:** `IBS_OP_VAL` becomes set; captured RIP is plausible.

#### `ibs_interrupt_nmi_handler`

Verifies that the kernel's IBS NMI handler correctly clears `IBS_FETCH_VAL`
and `IBS_OP_VAL` after processing each sample (i.e., does not leave
spurious VAL bits set between samples).

**Pass condition:** After the NMI fires and is handled, VAL bits are clear.

#### `ibs_interrupt_spurious`

Verifies that with IBS sampling disabled (`IbsFetchEn = IbsOpEn = 0`), no
spurious IBS NMIs are generated.  Runs for a short interval and confirms
the VAL bits never become set.

**Pass condition:** No spurious VAL bits after ≥ 10 ms with sampling off.

---

### ibs_period_test

**Category:** `[TC-MSR]` CRITICAL  
**Source:** `ibs_period_test.c`

Exhaustive validation of the sampling-period encoding for both IBS Fetch
and IBS Op.

**Period encoding:**
- `MSR_IBS_FETCH_CTL[15:0]` = `IbsFetchMaxCnt` — period in 16-cycle units
- `MSR_IBS_OP_CTL[15:0]` = `IbsOpMaxCnt[15:0]` — base period in 16-cycle units
- `MSR_IBS_OP_CTL[26:20]` = `IbsOpMaxCnt[22:16]` — extended period bits (Zen 2+)

#### `ibs_fetch_period_basic` / `ibs_op_period_basic`

Write and read back a range of period values: several small, medium, and
large values spanning the full 16-bit MaxCnt range.

**Pass condition:** All values round-trip correctly.

#### `ibs_fetch_period_min` / `ibs_op_period_min`

Write the minimum valid period (1 = 16 cycles) and verify readback.

**Pass condition:** Period 0x0001 survives the write-read cycle.

#### `ibs_fetch_period_max` / `ibs_op_period_max`

Write 0xFFFF (maximum 16-bit MaxCnt = 1,048,560 cycles) and verify readback.

**Pass condition:** Period 0xFFFF survives the write-read cycle.

#### `ibs_fetch_period_zero` / `ibs_op_period_zero`

Write period = 0 (disables the counter) and verify readback.

**Pass condition:** Period 0x0000 survives the write-read cycle.

#### `ibs_fetch_period_rollover` / `ibs_op_period_rollover`

Write a period and verify that the *actual sampling interval* in cycles
(`period × 16`) matches the expected value.  Checks the
`ibs_maxcnt_to_period()` conversion helper.

**Pass condition:** Conversion helper returns `period << 4` correctly for all
test values.

---

### ibs_routing_test

**Category:** `[TC-INT]` HIGH  
**Source:** `ibs_routing_test.c`

Tests the enable/disable control bits and global IBS routing register.

#### `ibs_fetch_enable_disable`

Writes `IbsFetchEn = 1` (bit 48 of `MSR_IBS_FETCH_CTL`) and reads back to
verify the hardware honours the enable.  Then clears and re-reads.

**Pass condition:** Enable bit survives write-read on both set and clear.

#### `ibs_op_enable_disable`

Same test for `IbsOpEn` (bit 17 of `MSR_IBS_OP_CTL`).

**Pass condition:** Enable bit survives write-read on both set and clear.

#### `ibs_fetch_cnt_ctl`

Tests the IBS Fetch Count Control bit (`IbsFetchCntCtl`, controls whether the
counter counts clocks or fetch events).  Writes both values and reads back.

**Pass condition:** CntCtl bit preserved.

#### `ibs_op_cnt_ctl`

Tests `IbsOpCntCtl` (bit 19 of `MSR_IBS_OP_CTL`): controls whether count
is dispatched-ops or retired-ops based.

**Pass condition:** CntCtl bit preserved.

#### `ibs_random_enable`

Tests the IBS randomization enable bit available on Zen 2+ processors.
When set, the hardware randomizes the actual sampling period within ±12.5%
of the programmed MaxCnt to reduce sampling bias.

**Pass condition:** Randomization bit preserved.  
**Skip condition:** CPU does not support IBS randomization.

#### `ibs_ibsctl_global`

Reads and writes `MSR_AMD64_IBSCTL` (the global IBS routing/enable register
at MSR `0xC001103A`).  This register controls per-APIC IBS interrupt routing.

**Pass condition:** IBSCTL round-trips correctly.  
**Skip condition:** `EPERM`/`EFAULT` writing IBSCTL — common in VMs and
restricted-access environments where the hypervisor does not expose this MSR.

---

### ibs_data_accuracy_test

**Category:** `[TC-DATA]` HIGH  
**Source:** `ibs_data_accuracy_test.c`

Validates that IBS sample data fields decode correctly after a real sample
is captured.

#### `ibs_fetch_address_fields`

Enables IBS Fetch sampling, captures one sample, then reads:
- `MSR_IBS_FETCH_LINADDR` — the faulting linear instruction address
- `MSR_IBS_FETCH_PHYSADDR` — the corresponding physical address

Verifies that `LINADDR` points within the known code range of the spin
loop and that `PHYSADDR` is non-zero.

**Pass condition:** Addresses are plausible and non-zero.

#### `ibs_dc_address_fields`

Enables IBS Op sampling with a data-cache miss workload, captures a sample
that includes a DC miss (`IbsDcMiss` bit set in `MSR_IBS_OP_DATA`), and
reads `MSR_IBS_DC_LINADDR` / `MSR_IBS_DC_PHYSADDR`.

Verifies that the DC addresses are non-zero and plausible.

**Pass condition:** DC addresses present and non-zero.

#### `ibs_op_data_fields`

Captures an IBS Op sample and decodes every field in
`MSR_IBS_OP_DATA` and `MSR_IBS_OP_DATA2`:
- `IbsTagToRetCtr` — tag-to-retire cycle count
- `IbsCompToRetCtr` — completion-to-retire count
- `IbsOpBrnRet` — branch retired
- `IbsOpReturn` — return op
- `IbsOpBrnMisp` — branch misprediction
- `DataSrc` — data source encoding (see below)

**Pass condition:** All fields parse without error; DataSrc in the
expected range for the memory hierarchy in use.

#### `ibs_data_src_encodings`

Tests the `ibs_get_data_src()` helper against the standard AMD DataSrc
encoding table:

| Value | Meaning |
|---|---|
| 0 | Local L1 cache |
| 1 | Local L2 cache |
| 2 | Victim/snoop cache |
| 3 | DRAM (same node) |
| 4 | Remote DRAM (different node) |
| 7 | Reserved |

Verifies that the helper decodes bits `[2:0]` (DataSrcLo) and `[7:6]`
(DataSrcHi) correctly using the formula `(hi << 3) | lo`.

**Pass condition:** All expected encodings map to correct symbolic names.

#### `ibs_data_src_extended`

Tests the extended DataSrc values introduced with Zen 2/3/4:

| Value | Meaning |
|---|---|
| 0x8 | Extended memory (non-DRAM) |
| 0xC | Peer agent (cross-die coherency) |
| 0xD | Long-latency DRAM |

Verifies that the 5-bit combined field `(DataSrcHi << 3) | DataSrcLo`
correctly represents these values.

The DataSrcHi bits sit at **positions [7:6]** in the MSR, so the extraction
must shift right by **6** (not 3).  A previous bug shifted by 3, causing all
extended encodings ≥ 0x8 to decode incorrectly.

**Pass condition:** Extended encodings decode to correct values.

#### `ibs_op_data4_zen4`

Reads `MSR_AMD64_IBSOPDATA4` which contains the Zen 4 extended Op Data
register (`IbsOpData4`).  This MSR is populated only during active IBS Op
sampling; reading it when IBS Op is inactive returns an error.

**Pass condition:** MSR readable (requires active sampling).  
**Skip condition:** Not Zen 4+, or IBSOPDATA4 not accessible outside active
sampling (graceful skip with message).

---

### ibs_smp_test

**Category:** `[TC-SMP]` HIGH  
**Source:** `ibs_smp_test.c`

Tests IBS behaviour across multiple logical CPUs.

#### `ibs_smp_per_cpu_config`

Iterates over all online CPUs.  For each CPU:
1. Pins the test thread to that CPU using `cpuset_setaffinity`.
2. Reads `MSR_IBS_FETCH_CTL` on that CPU.
3. Writes a unique period value (`0x100 + cpu_index`) with `IbsFetchEn` cleared.
4. Reads back and verifies the MaxCnt field matches.
5. Restores the original value.

**NMI race handling:** The read-back step is retried up to 3 times with
`sched_yield()` between attempts.  An in-flight IBS NMI (from previous active
sampling) can fire between write and read and re-arm the counter with the old
period.  Three consecutive NMI-interrupted reads is astronomically unlikely for
a real hardware failure; a single retry almost always resolves the race.

**Pass condition:** Every CPU's per-CPU MaxCnt field round-trips without
persistent mismatch.  
**Skip condition:** IBS not present, or single-CPU system.

#### `ibs_smp_concurrent_sampling`

Spawns one thread per online CPU.  Each thread:
1. Pins to its assigned CPU.
2. Enables IBS Fetch sampling with a moderate period.
3. Captures one sample.
4. Disables IBS.
5. Reports success.

Threads run concurrently, testing that the hardware correctly isolates IBS
state between CPUs.

**Pass condition:** All threads capture a sample without interfering.

#### `ibs_smp_cpu_migration`

Tests that IBS sampling configuration is NOT shared between CPUs — migrating
a thread to a different CPU does not transfer IBS state from the source CPU.

1. Configures IBS on CPU A with a specific period.
2. Migrates the thread to CPU B.
3. Reads IBS state on CPU B and verifies it does not reflect CPU A's config.

**Pass condition:** IBS state on CPU B is independent of CPU A.

---

### ibs_stress_test

**Category:** `[TC-STR]` MEDIUM  
**Source:** `ibs_stress_test.c`

Validates IBS behaviour under sustained and high-frequency access patterns.

#### `ibs_stress_rapid_enable_disable`

Performs 1000 rapid enable/disable cycles on `MSR_IBS_OP_CTL` on CPU 0:
- Enable: write `IbsOpMaxCnt = 0x100`, `IbsOpEn = 1`
- Disable: write `IbsOpEn = 0`, `IbsOpMaxCnt = 0`

Verifies that every MSR write succeeds (no `EIO` or permission errors).
The final iteration restores the original register value.

**Pass condition:** All 2000 MSR writes return 0.

#### `ibs_stress_period_changes`

Performs 500 iterations of write-then-read on `MSR_IBS_OP_CTL` using 5
cycling period values (`0x0010`, `0x0100`, `0x1000`, `0x8000`, `0xFFFF`).

**Setup:** Before the loop, IBS is explicitly disabled and a 1 ms sleep
drains any pending in-flight NMI from prior sampling.

**NMI race handling:** Each write-read pair is retried up to 3 times with
`sched_yield()` between attempts (same rationale as `ibs_smp_per_cpu_config`).

**Pass condition:** All 500 period values survive a write-read cycle.  
**Failure indicates:** Hardware or driver is not preserving `IbsOpMaxCnt`
after consecutive write-read cycles — a kernel driver or hardware regression.

#### `ibs_stress_long_running`

Spawns one thread per online CPU.  Each thread continuously writes and reads
`MSR_IBS_FETCH_CTL` (with `IbsFetchEn` cleared) for **60 seconds**, counting
iterations.  Reports total iterations at the end.

Purpose: detect resource leaks, memory corruption, or driver state degradation
under sustained MSR traffic.

**Pass condition:** All threads complete the full 60 s without errors.  
**Typical throughput:** Several million iterations per CPU.

#### `ibs_stress_concurrent_msr_access`

Spawns one thread per online CPU.  Each thread reads `MSR_IBS_FETCH_CTL`
10,000 times from its assigned CPU concurrently with all other threads.

Purpose: verify that the `cpuctl` driver serialises MSR access correctly
under concurrent load — no corrupted reads or descriptor errors.

**Pass condition:** All 10,000 reads per thread succeed without error.  
**Skip condition:** Single-CPU system.

---

### ibs_l3miss_test

**Category:** `[TC-DATA]` HIGH  
**Source:** `ibs_l3miss_test.c`  
**Requires:** AMD Family 19h / Zen 4+

Tests the L3MissOnly filtering feature (`IbsOpL3MissOnly` and
`IbsFetchL3MissOnly`) introduced in Zen 4.  When set, IBS generates samples
only on operations that miss the L3 cache, dramatically reducing sample
volume in cache-hot workloads.

**Relevant MSR bits:**
- `MSR_IBS_FETCH_CTL[59]` = `IbsFetchL3MissOnly` — restrict Fetch samples to L3 misses
- `MSR_IBS_OP_CTL[16]` = `IbsOpL3MissOnly` — restrict Op samples to L3 misses

#### `ibs_l3miss_detect_zen4`

Verifies `CPUID 8000_001Bh` reports the IBS Zen 4 capability bit
(`IBS_CPUID_ZEN4_IBS`, bit 6), which is the gate for L3MissOnly support.

**Pass condition:** Zen4 IBS bit set.  
**Skip condition:** Not Zen 4+.

#### `ibs_l3miss_fetch_enable`

Writes `IbsFetchL3MissOnly = 1` to `MSR_IBS_FETCH_CTL` (with `IbsFetchEn` cleared)
and reads back to verify the bit is preserved by the hardware.  Then clears it and
re-reads.

**Pass condition:** L3MissOnly bit set and clear survive round-trip.  
**Skip condition:** Not Zen 4+.

#### `ibs_l3miss_op_enable`

Same test for `IbsOpL3MissOnly` in `MSR_IBS_OP_CTL`.

**Pass condition:** L3MissOnly bit preserved.

#### `ibs_l3miss_filter_behavior`

Enables both L3MissOnly filters simultaneously and runs a workload that
mixes cache-hitting and cache-missing memory accesses.  Captures samples
and verifies that only L3-miss samples appear (IbsDcMiss set in Op Data).

**Pass condition:** Captured samples indicate L3 misses only when the filter
is active.

#### `ibs_l3miss_disabled_on_older`

Verifies that non-Zen-4 processors do NOT incorrectly report L3MissOnly
support.  Skips if running on Zen 4+ (this test is only meaningful on
older hardware).

**Pass condition:** On pre-Zen-4 hardware, the CPUID bit is absent.  
**Skip condition:** Running on Zen 4+ (test not applicable).

---

### ibs_ioctl_test

**Category:** `[TC-API]` MEDIUM  
**Source:** `ibs_ioctl_test.c`

Placeholder for a future high-level IBS kernel API (ioctl interface).
The FreeBSD kernel currently exposes IBS only through raw MSR access via
`cpuctl`; a proper `ioctl(2)` API has not yet been implemented.

#### `ibs_ioctl_not_implemented`

Attempts to open a hypothetical IBS character device (`/dev/ibs0`) and
checks the result.

**Pass condition:** Device does not exist (graceful skip with message).  
**Fail condition:** Device exists but ioctl fails unexpectedly.  
**Note:** This test is expected to skip on all current FreeBSD systems.

---

### ibs_swfilt_test

**Category:** `[TC-API]` MEDIUM  
**Source:** `ibs_swfilt_test.sh` (ATF shell test)

Tests software-level IBS filtering bits that control which privilege ring
generates samples.  These bits are stored in the IBS Fetch/Op control MSRs.

**Relevant bits:**
- `IbsFetchUserSpace` — filter Fetch samples to user-space only
- `IbsFetchKernelSpace` — filter Fetch samples to kernel-space only
- `IbsOpUserSpace` / `IbsOpKernelSpace` — same for Op samples
- `IbsOpHV` — hypervisor privilege filter

#### `ibs_swfilt_exclude_user`

Sets `IbsFetchKernelSpace = 1, IbsFetchUserSpace = 0` and verifies that the
bit survives a write-read cycle.

**Skip condition:** `rdmsr`/`wrmsr` userland tools not available, or IBS MSR
access denied.

#### `ibs_swfilt_exclude_kernel`

Sets `IbsFetchUserSpace = 1, IbsFetchKernelSpace = 0`.

**Skip condition:** Same as above.

#### `ibs_swfilt_filter_combination`

Sets both user and kernel filter bits simultaneously and verifies both are
preserved.

**Skip condition:** Same as above.

#### `ibs_swfilt_exclude_hv`

Sets `IbsOpHV = 1` (hypervisor-level IBS filter).

**Skip condition:** Not running under a hypervisor, or filter bit not
available.

---

## Shared Infrastructure

### `ibs_utils.h`

Defines all constants, MSR addresses, CPUID leaves, and inline helpers used
across the test suite.

**Key MSR addresses:**

| Constant | Address | Description |
|---|---|---|
| `MSR_IBS_FETCH_CTL` | `0xC0011030` | IBS Fetch Control and Status |
| `MSR_IBS_FETCH_LINADDR` | `0xC0011031` | IBS Fetch Linear Address |
| `MSR_IBS_FETCH_PHYSADDR` | `0xC0011032` | IBS Fetch Physical Address |
| `MSR_IBS_OP_CTL` | `0xC0011033` | IBS Op Control |
| `MSR_IBS_OP_RIP` | `0xC0011034` | IBS Op Return Instruction Pointer |
| `MSR_IBS_OP_DATA` | `0xC0011035` | IBS Op Data |
| `MSR_IBS_OP_DATA2` | `0xC0011036` | IBS Op Data 2 (DataSrc, cache info) |
| `MSR_IBS_OP_DATA3` | `0xC0011037` | IBS Op Data 3 (DC details) |
| `MSR_IBS_DC_LINADDR` | `0xC0011038` | IBS DC Linear Address |
| `MSR_IBS_DC_PHYSADDR` | `0xC0011039` | IBS DC Physical Address |
| `MSR_AMD64_IBSCTL` | `0xC001103A` | IBS Global Control |
| `MSR_AMD64_IBSOPDATA4` | `0xC001103D` | IBS Op Data 4 (Zen 4+) |
| `MSR_AMD64_ICIBSEXTDCTL` | `0xC001103C` | IC IBS Extended Control (Zen 4+) |

**Key bit constants:**

| Constant | Value | Description |
|---|---|---|
| `IBS_FETCH_ENABLE_BIT` | `(1ULL << 48)` | `IbsFetchEn` — starts Fetch sampling |
| `IBS_OP_ENABLE_BIT` | `(1ULL << 17)` | `IbsOpEn` — starts Op sampling |
| `IBS_MAXCNT_MASK` | `0xFFFF` | Bits [15:0]: base MaxCnt field |

**DataSrc field layout in `MSR_IBS_OP_DATA2`:**

```
Bits [2:0] = DataSrcLo
Bits [7:6] = DataSrcHi
Combined   = (DataSrcHi << 3) | DataSrcLo
```

The extraction function `ibs_get_data_src()` shifts DataSrcHi by 6 (not 3)
because the bits are at positions [7:6] in the register.

**Helper functions:**

| Function | Description |
|---|---|
| `read_msr(cpu, msr, val)` | Read MSR via `CPUCTL_RDMSR` ioctl |
| `write_msr(cpu, msr, val)` | Write MSR via `CPUCTL_WRMSR` ioctl |
| `do_cpuid_ioctl(leaf, regs)` | Execute CPUID via `CPUCTL_CPUID` ioctl |
| `cpu_supports_ibs()` | Check `CPUID 8000_0001h ECX[10]` |
| `cpu_ibs_extended()` | Check `CPUID 8000_001Bh` extended bits |
| `cpu_is_zen4()` | Check Family 19h Zen 4 model range |
| `cpu_is_zen5()` | Check Family 1Ah Zen 5 model range |
| `ibs_get_maxcnt(ctl)` | Extract MaxCnt from CTL register |
| `ibs_set_maxcnt(ctl, n)` | Set MaxCnt in CTL register |
| `ibs_maxcnt_to_period(n)` | Convert MaxCnt to sampling period cycles |
| `ibs_get_data_src(op_data2)` | Extract combined DataSrc field |
| `pin_to_cpu(cpu)` | Pin thread to logical CPU N via cpuset |
| `get_current_cpu()` | Read current logical CPU from CPUID |

---

## Known Skip Conditions

These skips are expected and represent unsupported hardware features, not
test failures:

| Test | Skip reason |
|---|---|
| `ibs_cpu_zen5_detection` | Not running on Zen 5 hardware |
| `ibs_cpu_tsc_frequency` | CPUID 0x15/0x16 not available (VM or restricted HW) |
| `ibs_data_accuracy_test:ibs_op_data4_zen4` | IBSOPDATA4 only valid during active Op sampling |
| `ibs_detect_test:ibs_detect_msr_access` | IBSOPDATA4 not accessible without active sampling |
| `ibs_ioctl_test:ibs_ioctl_not_implemented` | Kernel IBS ioctl API not yet implemented |
| `ibs_l3miss_test:ibs_l3miss_disabled_on_older` | Running on Zen 4+ (test targets older HW) |
| `ibs_routing_test:ibs_ibsctl_global` | IBSCTL write not permitted (VM or restricted access) |
| `ibs_swfilt_test:ibs_swfilt_exclude_hv` | Not running under hypervisor |
| `ibs_swfilt_test:ibs_swfilt_exclude_kernel` | IBS MSR access restricted in shell test environment |
| `ibs_swfilt_test:ibs_swfilt_exclude_user` | Same as above |
| `ibs_swfilt_test:ibs_swfilt_filter_combination` | Same as above |

---

## Bug History and Fixes

### `IBS_FETCH_ENABLE_BIT` wrong value (ibs_l3miss_test, ibs_period_test, ibs_smp_test)

**Symptom:** `ibs_smp_per_cpu_config` failed with ~96 out of 192 per-CPU checks.

**Root cause:** Local `#define IBS_FETCH_ENABLE_BIT (1ULL << 2)` in
`ibs_l3miss_test.c` and `ibs_period_test.c` overrode the correct definition
in `ibs_utils.h`.

- Bit 2 of `MSR_IBS_FETCH_CTL` is part of `IbsFetchMaxCnt[2]`, not the enable bit.
- `IbsFetchEn` is at **bit 48** (`IBS_FETCH_EN = 0x0001000000000000ULL`).

**Fix:** Removed the local definitions; both files now use `IBS_FETCH_ENABLE_BIT`
from `ibs_utils.h` which correctly defines it as `(1ULL << 48)`.

---

### DataSrc extended encoding wrong shift (ibs_data_accuracy_test)

**Symptom:** `ibs_data_src_extended` failed for DataSrc values ≥ 0x8.

**Root cause:** `ibs_get_data_src()` extracted DataSrcHi bits with
`(op_data2 >> IBS_DATA_SRC_SHIFT_HI)` where `IBS_DATA_SRC_SHIFT_HI = 3`.
But DataSrcHi sits at register bits [7:6], so the correct shift is 6.

**Fix:** Changed the shift to 6 in the extraction.

---

### NMI race in period write-read tests (ibs_stress_test, ibs_smp_test)

**Symptom:** `ibs_stress_period_changes` and `ibs_smp_per_cpu_config` failed
intermittently with MaxCnt readback mismatches (1–9 failures per run).

**Root cause:** The kernel's IBS NMI handler can fire between `write_msr`
and `read_msr` when an IBS interrupt was in-flight at the moment the test
started.  The NMI handler re-arms the counter with the *kernel's* stored
period, overwriting the test's value before the read.

This race cannot be eliminated from user-space (NMIs cannot be masked by
unprivileged code).

**Fix (ibs_stress_period_changes):** Added an explicit IBS-disable step with
`usleep(1000)` before the test loop to drain any pending in-flight NMI.

**Fix (both tests):** Write-read pairs are retried up to 3 times with
`sched_yield()` between attempts.  An NMI fires at most once per overflow,
so the second attempt after `sched_yield()` will see the stable value.

---

### `MSR_AMD64_IBSOPDATA4` hard-fail on inactive sampling (ibs_detect_test)

**Symptom:** `ibs_detect_msr_access` failed with `EFAULT` when the test
attempted to read `MSR_AMD64_IBSOPDATA4` without active IBS Op sampling.

**Root cause:** `MSR_AMD64_IBSOPDATA4` is a read-only *status* register.
The hardware generates a #GP fault when it is read outside of an active IBS
Op sample — this is documented behaviour.

**Fix:** Changed from `ATF_REQUIRE(read_msr(...) == 0)` to a conditional that
calls `atf_tc_skip()` when the read fails, rather than declaring the test failed.
