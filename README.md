# freebsd-ci-actions

FreeBSD CI integration and ATF test suite for AMD performance monitoring —
IBS (Instruction-Based Sampling), Uncore PMC (L3, DF, UMC, C2C), and
miscellaneous PMC / hwpmc API tests.

## Repository layout

```
freebsd-ci-actions/
├── run.sh                # IBS Test Suite Manager — clones source, builds and runs tests
├── ci/
│   └── tools/
│       ├── ci.conf       # VM image configuration for FreeBSD CI builds
│       └── freebsdci     # RC script: first-boot test orchestration
├── docs/
│   ├── ibs-tests.md      # IBS test reference: all test IDs, status, expected results
│   └── TODO.md           # Planned Uncore PMC and Misc PMC test cases
└── tests/
    └── sys/
        └── amd/
            └── ibs/      # ATF test suite (C + sh)
                ├── Makefile
                ├── ibs_utils.h              # Shared MSR definitions and helpers
                ├── ibs_msr_test.c           # [TC-IBS-MSR-01]  MSR smoke test
                ├── ibs_detect_test.c        # [TC-IBS-DET-01]  CPUID detection
                ├── ibs_api_test.c           # [TC-IBS-API-01]  MSR round-trip / reserved bits
                ├── ibs_cpu_test.c           # [TC-IBS-CPU-01]  CPU family/model
                ├── ibs_interrupt_test.c     # [TC-IBS-INT-01]  NMI / VALID bit
                ├── ibs_period_test.c        # [TC-IBS-CFG-01]  period encoding
                ├── ibs_routing_test.c       # [TC-IBS-RTG-01]  enable/disable / global CTL
                ├── ibs_data_accuracy_test.c # [TC-IBS-DAT-01]  DataSrc / Op Data fields
                ├── ibs_smp_test.c           # [TC-IBS-SMP-01]  per-CPU MSR isolation
                ├── ibs_stress_test.c        # [TC-IBS-STR-01]  1000× rapid enable/disable
                ├── ibs_l3miss_test.c        # [TC-IBS-FLT-01]  L3MissOnly filter (Zen 4+)
                ├── ibs_swfilt_test.sh       # [TC-IBS-SWF-01]  software filter bits
                ├── ibs_ioctl_test.c         # [TC-IBS-IOC-01]  ioctl API (planned)
                ├── ibs_unit_field_masks_test.c   # [TC-IBS-UNIT-01]
                ├── ibs_unit_helpers_test.c       # [TC-IBS-UNIT-02]
                ├── ibs_unit_datasrc_test.c       # [TC-IBS-UNIT-03]
                ├── ibs_unit_cpuid_parse_test.c   # [TC-IBS-UNIT-04]
                ├── ibs_unit_op_ext_maxcnt_test.c # [TC-IBS-UNIT-05]
                ├── ibs_unit_feature_flags_test.c # [TC-IBS-UNIT-06]
                ├── ibs_cpuctl_access_test.c      # [TC-IBS-DRV-01]
                ├── ibs_access_control_test.c     # [TC-IBS-SEC-01]
                ├── ibs_invalid_input_test.c      # [TC-IBS-INV-01]
                ├── ibs_robustness_test.c         # [TC-IBS-ROB-01]
                └── ibs_concurrency_test.c        # [TC-IBS-CON-01]
```

See [docs/ibs-tests.md](docs/ibs-tests.md) for the full test reference (IDs, categories, status, expected results).  
See [docs/TODO.md](docs/TODO.md) for planned Uncore PMC and Misc PMC test cases.

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
