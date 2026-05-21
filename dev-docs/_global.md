---
scope: global
last_modified: 2026-05-11
tags: [global, architecture, patterns]
---

# Global Patterns & Decisions

Patterns appearing in 3 or more modules belong here.
Module-specific decisions stay in their own file.

## Architecture Overview

freebsd-ci-actions is a GitHub Actions CI system for AMD hardware performance
monitoring tests on FreeBSD 16.0-CURRENT. It consists of:
- **IBS test suite:** 34-program ATF suite in C covering AMD Instruction-Based Sampling
- **UMCDF test suite:** 10-program ATF suite (3 integration + 7 unit) covering AMD UMC and Data Fabric PMU
- **PMC test suite:** placeholder (1 shell test); planned expansion pending kernel support
- **CI actions:** four composite GitHub Actions + one reusable workflow
- **Self-hosted runner:** bare-metal AMD EPYC via FreeBSD's Linuxulator
- **run.sh:** local lifecycle manager — compile, run (with suite/category selection),
  report, commit to sos-git, and `--auto` (cron-safe kernel build + reboot +
  post-reboot test + email; skips unchanged source via `/var/db/ibs-autotest-last-commit`)

The project is sponsored by AMD and tracked in Jira story SWLSVROS-6363.

## Cross-cutting Decisions

## [DECISION] Self-hosted runner on bare metal, not GitHub-hosted
**Date:** 2026-04-30
**Context:** IBS requires MSR write access (/dev/cpuctl) and kernel module
loading (kldload hwpmc.ko) — capabilities unavailable on GitHub-hosted Linux VMs.
**Decision:** Bare-metal AMD EPYC host with FreeBSD + Linuxulator to run the
GitHub Actions runner binary. Runner labels: `self-hosted`, `freebsd`, `amd-ibs`.
**Discarded alternatives:** FreeBSD jails (share host kernel but still need
delegated kldload), GitHub-hosted (no FreeBSD/hardware access).
**Impact:** runner-setup, linuxulator, ci-actions-workflows

## [DECISION] Reusable workflow pattern over monolithic job
**Date:** 2026-04-30
**Context:** Multiple caller repos need to trigger the same build/test/report
pipeline; Jira integration should be optional.
**Decision:** Single reusable workflow (`ibs-full-test.yml`) called by thin
caller files. Jira steps are secrets-gated (no-op if unconfigured).
**Discarded alternatives:** Copy workflow into each caller repo (maintenance
nightmare), always-on Jira (blocks repos without Jira secrets).
**Impact:** ci-actions-workflows, jira

## [DECISION] Tiered artifact retention aligned to CI/CD Design Proposal §8.1
**Date:** 2026-05-11 (revised from 2026-04-30)
**Context:** Design proposal §8.1 specifies per-artifact retention tiers.
Flat 30-day policy was incorrect.
**Decision:** JUnit XML → 90 days; HTML reports → 90 days; system-info → 90 days;
kernel config+SHA → 180 days; PMC logs → 30 days.
**Applied in:** report-results/action.yml, run-atf-tests/action.yml,
setup-freebsd-build/action.yml, build-kernel/action.yml
**Impact:** ci-actions-workflows, github-actions-api

## [DECISION] Rocky Linux 9 (rl9) packages for Linuxulator
**Date:** 2026-04-30
**Context:** GitHub Actions runner (.NET runtime) needs GLIBCXX 3.4.20+.
CentOS 7 (default linux_base-c7) ships only 3.4.19.
**Decision:** Use `linux_base-rl9` + `linux-rl9-icu` (Rocky Linux 9 base).
**Discarded alternatives:** CentOS 7 (GLIBCXX too old), building from source
(maintenance burden).
**Impact:** linuxulator, runner-setup

## Shared Utilities & Patterns

## [SNIPPET] Graceful skip on missing hardware/privilege
**Use:** Any test that requires IBS hardware, root, SMP, or kernel module access.
Apply at the top of each test case before any MSR or ioctl calls.
```c
if (!cpu_supports_ibs()) {
    atf_tc_skip("IBS not supported on this CPU");
}
if (geteuid() != 0) {
    atf_tc_skip("Test requires root privileges");
}
```

## [SNIPPET] Secrets-gated CI step
**Use:** Any optional integration (Jira, Slack, etc.) in GitHub Actions YAML.
The step runs only when the secret is set; graceful no-op otherwise.
```yaml
- name: Jira login
  if: ${{ secrets.JIRA_API_TOKEN != '' }}
  uses: atlassian/gajira-login@v3
  env:
    JIRA_BASE_URL: ${{ secrets.JIRA_BASE_URL }}
    JIRA_USER_EMAIL: ${{ secrets.JIRA_USER_EMAIL }}
    JIRA_API_TOKEN: ${{ secrets.JIRA_API_TOKEN }}
```

## [SNIPPET] Timeout guard for kernel build
**Use:** Any CI step running a long make/build command where a hung build
would block the runner indefinitely.
```bash
timeout 600 make -j$(sysctl -n hw.ncpu) KMODNAME || {
  echo "::error::Build timed out or failed"
  exit 1
}
```

## Known Systemic Issues

## [BUG] NMI race between MSR write and read in sampling tests
**Found:** 2026-04-30
**Symptom:** Test reads back a different MaxCnt value than it wrote; intermittent
failure on multi-CPU systems under load.
**Root cause:** In-flight NMI fires between the write and read, hwpmc NMI handler
overwrites the MSR with the next sample value before the test can read it.
**Fix/Workaround:** Retry loop + `sched_yield()` + pre-test sleep in ibs_stress_test
and ibs_smp_test.
**Status:** workaround

## [BUG] IBSOPDATA4 hard-fail outside active sampling
**Found:** 2026-04-30
**Symptom:** `ATF_REQUIRE(read_msr(...))` crashes test on Zen 4+ when IBS
sampling is not active; register returns #GP.
**Root cause:** IBSOPDATA4 is a read-only status register only valid during
active sampling.
**Fix/Workaround:** Changed from `ATF_REQUIRE` to graceful `atf_tc_skip()` in
ibs_detect_test.
**Status:** resolved

## New Module Template

When creating a new module file, copy this template exactly:

```markdown
---
module: <id>
type: module | api | infra
status: active
stack: <stack>
last_modified: <today>
related: []
tags: [freebsd-ci-actions, <type>]
---

# <Module Name>
> One sentence purpose.

## Overview

## Main Files
- `path/file` — responsibility

## Dependencies
- modules: [[name]]
- apis: [[name]]
- infra: [[name]]

## Decisions

## Known Bugs

## TODOs

## Snippets

## Learning Log
```

## Learning Log

2026-04-30 | Bootstrap complete. 6 modules/infra/api entries in index. 3 global patterns, 2 systemic bugs documented. | all
2026-05-07 | IBS suite: 30 programs confirmed at HEAD. First full run (2026-05-06): 54 passed, 6 expected-skips, 1 errno mismatch failure (likely fixed). UMCDF suite added (3 programs, 8 cases, binaries compiled). All Phase 1 CI infra items complete. Phase 2 blocked on runner token. | all
2026-05-11 | run.sh: --auto mode (kernel build→reboot→test→email via rc.d), --suite/--category selection, expanded --help. CI workflow fixes: artifact retention now matches §8.1, secrets:inherit added to all callers, flaky-test retry in run-tests.sh, kernel-config artifact added at 180 days. Bug fixes: run.sh git push refspec, missing trap, dry-run guards for sentinel/rcd/kernel-build. UMCDF: 10 programs, 123 test cases. IBS: 34 programs total. | all
