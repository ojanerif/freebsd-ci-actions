---
module: pmc-grouping-validation
type: module
status: active
stack: C, sh, DTrace, ATF, kyua, libpmc, FreeBSD make
last_modified: 2026-05-28
programs: 3
related: [[[pmc-tests]], [[umcdf-tests]], [[l3-tests]]]
tags: [freebsd-ci-actions, module, amd, pmc, hwpmc, grouping]
---

# PMC Grouping Validation

This module documents the C/sh-first validation added for FreeBSD `hwpmc(4)`
AMD core PMC grouping characterization.  The tests live under
`tests/sys/amd/pmc/` and intentionally avoid new Python dependencies so they are
usable in base-system CI images.

## Target and generation gate

Every new AMD core-PMC runtime case first gates on:

- `hwpmc(4)` loaded and `pmc_init()` working.
- `kern.hwpmc.cpuid` / CPUID reporting `AuthenticAMD`.
- the repository Zen generation map in `tests/sys/amd/umcdf/amd_umcdf_common.h`.
- PMU-events support through `pmc_pmu_enabled()`.
- Kyua config `amd.pmc.grouping.runtime=true`.

This keeps the AMD PMC rule explicit: no event semantics are assumed until the
target family/model/stepping has been decoded to a known Zen generation.  The
portable event used by the C tests is the FreeBSD PMU name `unhalted-cycles`,
which maps to AMD core PMCx076, Cycles Not in Halt.  The shell `pmcstat(8)` race
test uses the pmcstat-visible name `ls_not_halted_cyc` for the same silicon
event.

## Two transactions under test

`hwpmc(4)` exposes two separate transactions today:

1. **Allocation**: `PMC_OP_PMCALLOCATE` serializes under `pmc_sx`, validates a
   single request, picks one row, calls the AMD backend allocation hook, then
   binds the software descriptor to a hardware row.
2. **Start**: `PMC_OP_PMCSTART` serializes separately.  For AMD system-mode
   PMCs, `amd_start_pmc()` writes `PerfEvtSelN | AMD_PMC_ENABLE` immediately.
   For process-mode PMCs, `pmc_start()` marks software state RUNNING and the AMD
   `wrmsr` happens later from `pmc_process_csw_in()` when the attached process
   runs.

There is no in-tree `PMCGROUPALLOCATE` or `PMCGROUPSTART` ABI in the checked
FreeBSD source trees.  The tests assert the current one-row-at-a-time contract
and leave one expected-fail bridge for future group-start semantics.

## Structural observability

### `pmcinfo_snapshot.c` / `.h`

`tests/sys/amd/pmc/pmcinfo_snapshot.c` is a pure libpmc helper.  It walks every
online CPU with `pmc_pmcinfo(cpu, &info)` and snapshots:

- CPU and row index.
- row name and class.
- enabled state.
- row disposition: `PMC_DISP_FREE`, `PMC_DISP_THREAD`, or
  `PMC_DISP_STANDALONE`.
- owner PID, mode, event, flags, reload count.

The helper is the structural oracle for rollback checks.  It observes the
FreeBSD reporting surface; it does not read MSRs and does not infer hidden
kernel pointers such as `phw_pmc` directly.

### `msr_snapshot.c` / `.h`

`tests/sys/amd/pmc/msr_snapshot.c` reads AMD core PMC MSRs through `cpuctl(4)`:

- `PerfEvtSel[0..5]`: `MSRC001_0200`, `0202`, ..., `020A`.
- `PerfCtr[0..5]`: `MSRC001_0201`, `0203`, ..., `020B`.
- `PerfCntrGlobalCtl`: `MSRC000_0301`, only if CPUID
  `Fn80000022_EAX[0]` reports PerfMonV2.

The MSR helper distinguishes silicon state from the libpmc/hwpmc snapshot: the
allocation tests use `pmcinfo`; the start tests inspect the AMD `PerfEvtSel.En`
bit directly.

## New ATF coverage

The existing `hwpmc_grouping_test` keeps its previous row-disposition and
concurrent-allocation smoke cases, then adds:

- `alloc_reserves_distinct_rows`: two AMD core `pmc_allocate()` calls return
  distinct row indexes and visible `PMC_DISP_THREAD` ownership.
- `alloc_rollback_on_oversubscribe`: the snapshot before a failed
  oversubscription attempt equals the snapshot after the failure.
- `alloc_no_partial_pmcid_visibility`: a pthread-barrier race returns either a
  visible `pmcid` or an errno with `PMC_ID_INVALID`; no half-pmcid state is
  reported.
- `alloc_mode_class_atomicity`: THREAD-saturated AMD core rows exclude a
  STANDALONE/system allocation without changing the row snapshot.
- `start_sys_mode_writes_msr_immediately`: `PMC_MODE_SC` sets AMD
  `PerfEvtSel.En` immediately after `pmc_start()`.
- `start_proc_mode_defers_to_csw`: process mode leaves `PerfEvtSel.En` clear
  after `pmc_start()` while the attached child is blocked, then accumulates
  counts only after that child runs.  The separate DTrace case observes the
  later `amd_start_pmc` call because external `cpuctl(4)` reads would evict the
  target thread and stop a process-mode row.
- `start_skew_is_sequential_per_row`: DTrace FBT on `amd_start_pmc` records
  adjacent row-start timestamps from sequential AMD backend starts.
- `group_start_atomicity_EXPECTED_FAIL`: expected-fail bridge documenting the
  missing `PMCGROUPSTART`/global-arm transaction until a measured future-ABI
  assertion exists.

The new shell ATF program `pmcstat_group_alloc_atomicity_test.sh` races two
full-width `pmcstat -p ls_not_halted_cyc ...` process-counting invocations for a
configurable number of iterations.  It checks that at least one racing run
succeeds, successful runs produce non-empty output, failed runs produce
diagnostics, and the libpmc-backed `pmcinfo_thread_count` helper reports that
AMD `PMC_CLASS_K8` THREAD row count returns to the pre-race baseline after each
iteration.  This uses the same `pmc_pmcinfo()` reporting surface as the C
snapshot tests instead of `pmccontrol -s`, which only prints per-CPU PMC state
and does not expose per-PID ownership.

Runtime precondition: do not run unrelated `pmcstat(8)`/`hwpmc(4)` consumers on
the machine while this shell race is executing.  Kyua `parallelism=1` prevents
in-suite PMC contamination, but an external process holding AMD core THREAD rows
can legitimately move the baseline and make the residue assertion fail.  Do not
use ATF `is_exclusive` metadata here; the Kyua/ATF version on supported FreeBSD
test hosts rejects that property during `__test_cases_list__` enumeration.

## DTrace tooling

`tools/pmu_trace/trace_alloc_start.d` emits CSV:

```text
kind,cpu,ri,pid,ns
alloc,<cpu>,-,<pid>,<duration-ns>
md_alloc,<cpu>,<ri>,<pid>,<timestamp-ns>
start,<cpu>,<ri>,<pid>,<timestamp-ns>
release,<cpu>,-,<pid>,<timestamp-ns>
```

Run it with `-c` or `-p` so `$target` is defined; the probes are target-filtered
to avoid contamination from unrelated PMC users.  Use `-Z` so optional FBT probes
do not hard-fail when a kernel lacks a matching symbol:

```sh
dtrace -Z -q -s tools/pmu_trace/trace_alloc_start.d -o trace.csv \
    -c '/usr/sbin/pmcstat -C -q -p ls_not_halted_cyc -p ls_not_halted_cyc -- sleep 1'
tools/pmu_trace/summarize_trace.sh trace.csv
```

`summarize_trace.sh` is sh/awk-only and prints start-pair count plus min,
median, mean, and max adjacent `amd_start_pmc` timestamp deltas.  For an even
number of adjacent row-start pairs, the median is the average of the two middle
sorted deltas.

## Verification commands

On a FreeBSD AMD Zen machine:

```sh
kldload hwpmc || true
kldload cpuctl || true
make -C tests/sys/amd/pmc
kyua -v test_suites.FreeBSD.amd.pmc.grouping.runtime=true \
    -v parallelism=1 \
    test --kyuafile tests/sys/amd/pmc/Kyuafile
```

For the shell race iteration count:

```sh
kyua -v test_suites.FreeBSD.amd.pmc.grouping.runtime=true \
    -v test_suites.FreeBSD.amd.pmc.grouping.atomicity.iterations=50 \
    -v parallelism=1 \
    test --kyuafile tests/sys/amd/pmc/Kyuafile \
    pmcstat_group_alloc_atomicity_test
```

Expected result today: all new runtime cases either PASS/SKIP based on hardware
and DTrace availability, with exactly one expected failure from
`group_start_atomicity_EXPECTED_FAIL` when that case is executed.
Hardware-sensitive PMC runtime jobs must remain serialized; the reusable
`run-atf-tests` action accepts `kyua_args` for passing the same `-v` settings in
CI.

## Existing Python collector

`py-scripts/pmc_grouping_skew_collect.py` remains the richer offline/statistical
collector.  The CI-facing additions use `tools/pmu_trace/summarize_trace.sh` for
DTrace summaries instead of adding another Python join layer.
