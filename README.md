# freebsd-ci-actions

FreeBSD CI integration and ATF test suite for AMD performance monitoring — covering
IBS (Instruction-Based Sampling), Uncore PMC (L3, DF, UMC, C2C), and miscellaneous
PMC / hwpmc API tests.

## Repository layout

```
freebsd-ci-actions/
├── run.sh                # IBS Test Suite Manager — clones source, builds and runs tests
├── ci/
│   └── tools/
│       ├── ci.conf       # VM image configuration for FreeBSD CI builds
│       └── freebsdci     # RC script: first-boot test orchestration
└── tests/
    └── sys/
        └── amd/
            └── ibs/
                ├── Makefile
                ├── ibs_utils.h              # Shared MSR definitions and helpers
                ├── ibs_api_test.c
                ├── ibs_cpu_test.c
                ├── ibs_data_accuracy_test.c
                ├── ibs_detect_test.c
                ├── ibs_interrupt_test.c
                ├── ibs_ioctl_test.c         # placeholder
                ├── ibs_l3miss_test.c
                ├── ibs_msr_test.c
                ├── ibs_period_test.c
                ├── ibs_routing_test.c
                ├── ibs_smp_test.c
                ├── ibs_stress_test.c
                └── ibs_swfilt_test.sh
```

## Requirements

- FreeBSD on an AMD CPU that supports IBS (Family 10h / K10 or newer)
- Kernel modules: `cpuctl` (loaded by default on amd64), `hwpmc` for PMC tests
- ATF (Automated Test Framework) — included in the FreeBSD base system
- `pthread` library — included in base
- Root privileges (MSR access, PMC driver)

Zen 4+ tests require AMD Family 19h (Zen 4) or Family 1Ah (Zen 5).

## Building

```sh
cd tests/sys/amd/ibs
make
```

## Running

```sh
# Full test manager (clones source, builds, installs, runs)
./run.sh

# Run with kyua directly after building
cd tests/sys/amd/ibs
kyua test
kyua report

# Single test case
kyua test -k 'ibs_detect_test:ibs_detect'
```

---

## Test categories

### IBS — Instruction-Based Sampling

Validates AMD IBS hardware sampling via direct MSR access on FreeBSD.
Framework: ATF + Kyua. Privileges: root (cpuctl MSR access).

| File | Category | What it tests | Status |
|---|---|---|---|
| `ibs_msr_test` | Smoke | MSR read/write via cpuctl | Implemented |
| `ibs_detect_test` | Detection | CPUID-based IBS feature detection (basic, extended, Zen 4) | Implemented |
| `ibs_api_test` | API | MSR round-trip, reserved-bit handling | Implemented |
| `ibs_cpu_test` | Detection | CPU family/model (10h–1Ah), TSC frequency | Implemented |
| `ibs_interrupt_test` | Interrupt | NMI enable/disable, VALID polling, AMD Errata #420 | Implemented |
| `ibs_period_test` | Config | Sampling period encoding and rollover (Fetch and Op) | Implemented |
| `ibs_routing_test` | Config | Enable/disable bits, count-control, global IBS CTL | Implemented |
| `ibs_data_accuracy_test` | Data | DataSrc encodings, Op Data fields, address registers | Implemented |
| `ibs_smp_test` | SMP | Per-CPU MSR isolation, concurrent sampling, migration | Implemented |
| `ibs_stress_test` | Stress | Rapid enable/disable (1000×), period changes, 60 s load | Implemented |
| `ibs_l3miss_test` | Filter | L3MissOnly filtering in Fetch and Op (Zen 4+ only) | Implemented |
| `ibs_swfilt_test` | Filter | Software filter bits (exclude_user/kernel/hv) via rdmsr/wrmsr | Implemented |
| `ibs_ioctl_test` | API | Kernel ioctl API | **Placeholder** — kernel-side not yet implemented |

---

### Uncore PMC — Off-Core Performance Monitoring

> **Status: Planned — not yet implemented.**
> These test cases are pending kernel-side hwpmc uncore support.

Validates AMD uncore PMU units (L3 cache, Data Fabric, UMC, C2C) through the FreeBSD
hwpmc subsystem. Unlike per-core PMU events, uncore counters monitor shared hardware
resources across multiple cores and NUMA nodes.

Framework: ATF + Kyua. Target: FreeBSD 14.x / CURRENT with hwpmc compiled in.

#### TC-UNC-DET — Uncore PMU Detection

Verify the kernel correctly identifies available uncore PMU units via CPUID and exposes
them through hwpmc.

| Test ID | Description | Expected Result |
|---|---|---|
| TC-UNC-DET-01 | Check dmesg for L3, DF, UMC, C2C detection on supported AMD CPU | All supported units detected |
| TC-UNC-DET-02 | Check sysctl for uncore PMU capabilities (`hw.pmc.amd_l3_*`, `amd_df_*`, `amd_umc_*`) | Correct capability flags per unit |
| TC-UNC-DET-03 | Run on unsupported/pre-Zen CPU; verify graceful fallback | No panic; units disabled cleanly |
| TC-UNC-DET-04 | Verify reported unit count matches hardware topology for L3, DF, UMC | Count matches physical topology |

#### TC-UNC-L3 — L3 Cache PMU

Confirm L3 cache counters correctly track hit, miss, and eviction events across slices.

| Test ID | Description | Expected Result |
|---|---|---|
| TC-UNC-L3-01 | L3 miss counter; cache-unfriendly workload | Counter increments proportionally |
| TC-UNC-L3-02 | L3 hit counter; cache-friendly workload | High hit rate reflected |
| TC-UNC-L3-03 | L3 eviction counter; fill cache beyond capacity | Eviction count increases |
| TC-UNC-L3-04 | Verify counters scoped per cache slice; pin to one CCX | Activity isolated to CCX |
| TC-UNC-L3-05 | Disable / re-enable; verify no stale values | Counter resets cleanly |
| TC-UNC-L3-06 | Simultaneous collection on all L3 slices | Independent counts; no corruption |

#### TC-UNC-DF — Data Fabric PMU

Verify DF PMU counters measure inter-die traffic, memory controller requests, and bandwidth.

| Test ID | Description | Expected Result |
|---|---|---|
| TC-UNC-DF-01 | DF memory read counter; memory-intensive workload | Counter increments proportionally |
| TC-UNC-DF-02 | DF memory write counter; write-intensive workload | Counter increments proportionally |
| TC-UNC-DF-03 | DF remote NUMA counter; cross-NUMA workload on multi-socket | Reflects cross-socket traffic |
| TC-UNC-DF-04 | DF counters during idle system | No spurious increments |
| TC-UNC-DF-05 | Rapid enable/disable cycle (100×) | No panic, lockup, MSR corruption |
| TC-UNC-DF-06 | Invalid event selector | Error returned; no counter enabled |
| TC-UNC-DF-07 | DF bandwidth vs known memory benchmark | Within 10% of benchmark result |

#### TC-UNC-UMC — Unified Memory Controller PMU

Confirm UMC counters accurately reflect DRAM bandwidth and access patterns per channel.

| Test ID | Description | Expected Result |
|---|---|---|
| TC-UNC-UMC-01 | UMC read bandwidth; streaming memory read workload | Consistent with expected footprint |
| TC-UNC-UMC-02 | UMC write bandwidth; streaming write workload | Reflects write traffic |
| TC-UNC-UMC-03 | DRAM access per channel; verify per-channel distribution | Matches firmware interleaving policy |
| TC-UNC-UMC-04 | Workload on NUMA node 0; verify node 1 UMC near zero | Node 1 shows no significant activity |
| TC-UNC-UMC-05 | UMC counter during idle | Stable at zero |
| TC-UNC-UMC-06 | Simultaneous collection on all channels under stress | Independent counts; no corruption |
| TC-UNC-UMC-07 | Disable / re-enable; verify clean reset | Counter resets correctly |

#### TC-UNC-MEM — Memory Subsystem (cross-validation)

Cross-validate DF and UMC readings against known workload behavior and OS statistics.

| Test ID | Description | Expected Result |
|---|---|---|
| TC-UNC-MEM-01 | Cross-validate DF memory requests vs UMC access counters | Consistent within expected margin |
| TC-UNC-MEM-02 | Memory counters vs numastat-reported NUMA traffic | Consistent with OS-level stats |
| TC-UNC-MEM-03 | NUMA-local workload; remote counters near zero | Local/remote split matches binding |
| TC-UNC-MEM-04 | Force NUMA-remote access; verify remote counters increase | Reflects cross-node volume |
| TC-UNC-MEM-05 | Simultaneous collection on all NUMA nodes for 60 s | Stable; no kernel warnings |

#### TC-UNC-C2C — Cache-to-Cache Interconnect PMU

Verify C2C PMU counters capture direct cache-line transfers including cross-CCX and cross-die.

| Test ID | Description | Expected Result |
|---|---|---|
| TC-UNC-C2C-01 | C2C counter; producer-consumer on same CCX | Intra-CCX transfers reflected |
| TC-UNC-C2C-02 | C2C counter; producer-consumer on different CCX dies | Cross-CCX transfer volume |
| TC-UNC-C2C-03 | C2C workload between sockets on multi-socket system | Cross-socket events captured |
| TC-UNC-C2C-04 | No sharing between cores; C2C counter stays zero | No C2C events on independent workload |
| TC-UNC-C2C-05 | Disable / re-enable mid-workload | Clean resume from zero; no stale state |
| TC-UNC-C2C-06 | False-sharing workload; verify invalidation traffic captured | Invalidation counter increases |

---

### Misc PMC — Metrics, Top-Down Analysis, Per-Process, API

> **Status: Planned — not yet implemented.**

Cross-cutting PMC validation: derived metrics (IPC, miss rate, bandwidth), top-down
microarchitecture analysis, per-process attach/detach, per-thread counting, stack
walking, ELF symbol lookup, and hwpmc/libpmc API stability.

Framework: ATF + Kyua. Target: FreeBSD 14.x / CURRENT with hwpmc compiled in.

#### TC-MISC-METRICS — Performance Metrics

Derived metrics from raw counter pairs must be accurate and stable.

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

#### TC-MISC-TOPDOWN — Top-Down Microarchitecture Analysis

Top-down Level 1 categories (Retiring, Bad Speculation, Frontend Bound, Backend Bound)
must sum to 100% of pipeline slots and reflect the expected workload profile.

| Test ID | Description | Expected Result |
|---|---|---|
| TC-MISC-TOPDOWN-01 | All four top-down categories collected; verify they sum to 100% | Sum within 1% margin |
| TC-MISC-TOPDOWN-02 | Compute-bound workload; Retiring dominates | Retiring fraction significantly highest |
| TC-MISC-TOPDOWN-03 | High branch misprediction workload; Bad Speculation dominates | Reflects misprediction-heavy pattern |
| TC-MISC-TOPDOWN-04 | Large code footprint; Frontend Bound dominates | Reflects fetch/decode bottleneck |
| TC-MISC-TOPDOWN-05 | Pointer-chasing workload; Backend Bound dominates | Reflects memory latency stall |
| TC-MISC-TOPDOWN-06 | Per-core top-down in SMP; no cross-core interference | Each core reports independently |
| TC-MISC-TOPDOWN-07 | Repeated runs of same workload; verify result stability | Reproducible within 5% across runs |

#### TC-MISC-PROC — Per-Process PMC

Runtime attach/detach, inter-process isolation, per-thread counting, stack walking,
ELF symbol lookup.

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

#### TC-MISC-API — API Stability (hwpmc / libpmc)

Verify hwpmc kernel interface and libpmc userspace library are stable and correct
across representative usage patterns.

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

---

## CI integration

`ci/tools/ci.conf` is consumed by FreeBSD release engineering VM build scripts
(`release/tools/`). It:

- Enables the `freebsdci` RC service on first boot
- Installs optional packages required by the broader FreeBSD test suite
- Configures `loader.conf`, `kyua.conf`, `rc.conf`, and `sysctl.conf` inside the
  VM image for unattended test runs

`ci/tools/freebsdci` is the RC script copied into the VM. On first boot it:

1. Extracts CI metadata from the tar device (`/dev/vtbd1`)
2. Runs either a smoke pass or the full `kyua test` suite
3. Saves results as JUnit XML and plain text
4. Shuts down the VM

## License

BSD 2-Clause — see individual file headers.
