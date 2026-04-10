# freebsd-ci-actions

FreeBSD CI integration and ATF test suite for AMD IBS (Instruction-Based Sampling).

## Repository layout

```
freebsd-ci-actions/
├── ci/
│   └── tools/
│       ├── ci.conf       # VM image configuration for FreeBSD CI builds
│       └── freebsdci     # RC script: first-boot test orchestration
└── tests/
    └── sys/
        └── amd/
            └── ibs/
                ├── Makefile
                ├── ibs_utils.h             # Shared MSR definitions and helpers
                ├── ibs_api_test.c          # MSR read/write round-trip
                ├── ibs_cpu_test.c          # CPU family/model detection
                ├── ibs_data_accuracy_test.c# Sample field extraction and validation
                ├── ibs_detect_test.c       # IBS feature detection via CPUID
                ├── ibs_interrupt_test.c    # NMI generation and handling
                ├── ibs_ioctl_test.c        # Kernel ioctl API (placeholder)
                ├── ibs_l3miss_test.c       # L3 miss filtering (Zen 4+)
                ├── ibs_msr_test.c          # Basic MSR smoke test
                ├── ibs_period_test.c       # Sampling period encoding
                ├── ibs_routing_test.c      # Enable/disable and routing config
                ├── ibs_smp_test.c          # Per-CPU isolation and concurrent access
                ├── ibs_stress_test.c       # Rapid cycling and long-running load
                └── ibs_swfilt_test.sh      # Software filter tests (ATF shell)
```

## Requirements

- FreeBSD on an AMD CPU that supports IBS (Family 10h / K10 or newer)
- Kernel modules: `cpuctl` (loaded by default on amd64)
- ATF (Automated Test Framework) — included in the FreeBSD base system
- `pthread` library — included in base

Zen 4+ tests (`ibs_l3miss_test`, extended data fields) require an AMD Family 19h
(Zen 4) or Family 1Ah (Zen 5) processor.

## Building

```sh
cd tests/sys/amd/ibs
make
```

This produces one binary per `ATF_TESTS_C` entry alongside the sources.

## Running

Run the full suite with kyua:

```sh
cd tests/sys/amd/ibs
kyua test
kyua report
```

Run a single test program directly:

```sh
./ibs_detect_test
atf-run ibs_detect_test | atf-report
```

Run a specific test case:

```sh
kyua test -k 'ibs_detect_test:ibs_detect'
```

## Test descriptions

For full per-test-case documentation including categories, severity, hardware
requirements, NMI race handling, and bug history, see [docs/ibs_tests.md](docs/ibs_tests.md).


| File | What it tests |
|---|---|
| `ibs_msr_test` | Smoke test: MSR read/write via cpuctl |
| `ibs_detect_test` | CPUID-based IBS feature detection (basic, extended, Zen 4) |
| `ibs_api_test` | MSR round-trip, reserved-bit handling |
| `ibs_cpu_test` | CPU family/model detection (10h–1Ah), TSC frequency |
| `ibs_interrupt_test` | NMI enable/disable, fetch/op VALID polling, AMD Errata #420 |
| `ibs_period_test` | Sampling period encoding and rollover (Fetch and Op) |
| `ibs_routing_test` | Enable/disable bits, count-control, global IBS CTL, random enable |
| `ibs_data_accuracy_test` | DataSrc encodings, Op Data fields, address registers |
| `ibs_smp_test` | Per-CPU MSR isolation, concurrent sampling, CPU migration |
| `ibs_stress_test` | Rapid enable/disable (1000×), period changes (500×), 60 s load |
| `ibs_l3miss_test` | L3MissOnly filtering in Fetch and Op (Zen 4+ only) |
| `ibs_ioctl_test` | Placeholder — kernel ioctl API not yet implemented |
| `ibs_swfilt_test` | Shell: software filter bits (exclude_user/kernel/hv) via rdmsr/wrmsr |

## CI integration

`ci/tools/ci.conf` is consumed by the FreeBSD release engineering VM build
scripts (`release/tools/`). It:

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
