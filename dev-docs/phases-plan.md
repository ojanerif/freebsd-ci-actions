# CI/CD Phases Plan
<!-- Last updated: 2026-05-11 | Author: ojanerif -->

---

## Phase 1 — Close Story Gaps (Due: Apr 30)
*All items required to meet Story 2–4 acceptance criteria.*

### P1-1: Add `--max-parallel-jobs 1` to runner registration (Story 1.4)

**File:** `freebsd-src/scripts/install-runner-freebsd.sh`

In the `config.sh` invocation (section 6), add the flag:
```sh
./config.sh \
    --url '$REPO_URL' \
    --token '$REG_TOKEN' \
    --labels '$RUNNER_LABELS' \
    --name '$RUNNER_NAME' \
    --max-parallel-jobs 1 \     # ← add
    --unattended
```
This prevents concurrent jobs from racing on MSRs and PMC state.

---

### P1-2: Add `workflow_dispatch` trigger to caller examples (Story 2.4)

**Files:** `freebsd-src/caller-examples/call-ibs-full.yml`, `call-smoke.yml`

```yaml
on:
  push:
    branches: [main, 'ibs-*']
    paths: ['sys/dev/hwpmc/**', 'sys/amd64/amd64/ibs*', 'sys/modules/hwpmc/**']
  pull_request:
    paths: ['sys/dev/hwpmc/**', 'sys/amd64/amd64/ibs*', 'sys/modules/hwpmc/**']
  workflow_dispatch:          # ← add
```

---

### P1-3: Capture dmesg + kernel config + CPUID artifacts (Story 2.3)

Add a new step in `freebsd-src/actions/setup-freebsd-build/action.yml` (runs after env validation):

```yaml
- name: Collect system info artifacts
  shell: sh
  run: |
    mkdir -p _sysinfo
    dmesg > _sysinfo/dmesg.txt
    cp /boot/kernel/kernel.conf _sysinfo/kernel.conf 2>/dev/null || true
    sysctl -a | grep -E 'hw\.(cpu|model|ncpu|physmem)' > _sysinfo/cpuinfo.txt
    # CPUID via cpuctl if available
    if [ -c /dev/cpuctl0 ]; then
      printf 'IBS CPUID leaf 0x8000001B:\n' >> _sysinfo/cpuinfo.txt
      sysctl -n hw.cpu_vendor >> _sysinfo/cpuinfo.txt 2>/dev/null || true
    fi
    uname -a >> _sysinfo/system.txt
    pkg info >> _sysinfo/packages.txt 2>/dev/null || true

- name: Upload system info
  uses: actions/upload-artifact@v4
  if: always()
  with:
    name: system-info
    path: _sysinfo/
    retention-days: 30
    if-no-files-found: warn
```

Also add `retention-days: 30` to the kernel config in `freebsd-src/configs/AMD_IBS_KERNCONF`:
- Capture the built kernel config: add `cp /boot/kernel.conf _sysinfo/` after `buildkernel` in `actions/build-kernel/scripts/build.sh`.

---

### P1-4: Set 30-day artifact retention (Story 3.4)

Add `retention-days: 30` to every `actions/upload-artifact@v4` call.

**Files to update:**

| File | Step name | Change |
|------|-----------|--------|
| `actions/run-atf-tests/action.yml:66` | Upload JUnit XML | + `retention-days: 30` |
| `actions/run-atf-tests/action.yml:73` | Upload HTML report | + `retention-days: 30` |
| `actions/report-results/action.yml:41` | Upload JUnit XML | + `retention-days: 30` |
| `actions/report-results/action.yml:49` | Upload HTML report | + `retention-days: 30` |

---

### P1-5: Add status badge to README (Story 3.3)

**File:** `freebsd-src/README.md` (and root `README.md`)

Add after the title line:

```markdown
[![Self-Test](https://github.com/ojanerif/freebsd-ci-actions/actions/workflows/self-test.yml/badge.svg)](https://github.com/ojanerif/freebsd-ci-actions/actions/workflows/self-test.yml)
```

This badge reflects the YAML/shell lint self-test that runs on GitHub-hosted Ubuntu runners — always green on a good commit, does not require the self-hosted FreeBSD runner.

---

### P1-6: Build timeout guard (Story 2.5)

**File:** `freebsd-src/workflows/ibs-full-test.yml`

Add job-level `timeout-minutes` to the build-and-test job:
```yaml
jobs:
  build-and-test:
    name: Build + IBS Test
    runs-on: [self-hosted, freebsd, amd-ibs]
    timeout-minutes: 30          # ← overall ceiling: 30 min
```

And in `actions/build-kernel/scripts/build.sh`, wrap the `make buildkernel` call:
```sh
timeout 600 make -j"${JOBS}" buildkernel KERNCONF="$KERNCONF" \
    || die "Kernel build timed out or failed"
```
`600` = 10 minutes. Test phase is already guarded by the `test_timeout` input.

---

## Phase 2 — Live Runner + E2E Validation (Due: May 9)
*Story 4 completion; requires bare-metal EPYC machine.*

### P2-1: Register the runner

```sh
# On the EPYC machine, as root:
sh scripts/install-runner-freebsd.sh \
    --repo https://github.com/ojanerif/freebsd-ci-actions \
    --token <REGISTRATION_TOKEN>
```

Verify in GitHub: **Settings → Actions → Runners** shows `freebsd-amd-<hostname>` as **Idle**.

### P2-2: E2E green-path validation (Story 4.1)

Trigger `call-ibs-full.yml` via `workflow_dispatch` on the main branch.

Expected result on EPYC Zen 4:
- Build: PASS
- IBS module load/unload: PASS
- ATF test results: ≥ 6 PASS, 1 SKIP (`ibs_ioctl_test:ibs_ioctl_not_implemented`)
- Artifacts: JUnit XML, HTML report, dmesg, kernel config, CPUID info all present
- Pipeline runtime: < 30 min

### P2-3: Negative-path validation (Story 4.2)

In a test branch, introduce an intentional failure:
```c
/* force: assert known-bad value */
ATF_REQUIRE_EQ(0xDEAD, ibs_get_maxcnt(0));
```
Trigger the pipeline. Verify:
- Job summary shows `FAILED: 1`
- `::error` annotation appears on the PR/commit
- JUnit XML correctly marks the test as `failure`
- Overall pipeline exits non-zero

Revert the change after validation.

### P2-4: Runbook write-up (Story 4.4)

Create `dev-docs/runbook.md` with:
- How to re-register the runner after a token expiry
- How to update the runner binary (re-run `install-runner-freebsd.sh`)
- How to debug a stuck/offline runner
- Log locations: `/var/log/gh-runner.log`
- How to trigger manual runs: `gh workflow run`

---

## Phase 3 — Test Coverage Expansion (May–June)
*Not on the Apr 30 critical path. Prioritized by test-plan coverage gaps.*

### P3-1: Activate NMI flood placeholders (TC-ROB-01, TC-ROB-02)

**Blocker:** Need a sysctl or kernel probe to confirm `dev.hwpmc.0.ibs_active`.  
**Action:** Add a `sysctl dev.hwpmc.0.ibs_active` check to `ibs_robustness_test.c`; remove placeholder skip when the sysctl is present.

### P3-2: Implement `ibs_ioctl_test` (TC-IBS-IOC-01)

Currently a graceful skip (`/dev/ibs0` not implemented in FreeBSD kernel).  
**Action:** When the kernel-side IBS ioctl API lands, implement the 5 test cases.

### P3-3: Cross-architecture CI job (Intel runner)

**Action:** Add a second runner with labels `self-hosted,freebsd,intel` and a new caller workflow that runs the full IBS suite, asserting all results are SKIP (not FAIL).

### P3-4: Uncore PMC tests (TC-UNC-*)

**Blocker:** FreeBSD hwpmc uncore support not yet upstream.  
**Action:** Stub out `tests/sys/amd/pmc/` with detection tests (TC-UNC-DET) once the kernel patch lands. L3, DF, UMC, C2C counters follow.

### P3-5: KASAN job

Add `ci/jobs/FreeBSD-main-amd64-KASAN_test_ibs/` mirroring the freebsd-ci pattern.  
Use `KERNCONF=GENERIC-KASAN`. Target the NMI handler and cpuctl MSR ioctl paths.

---

## Quick-win checklist for Apr 30

```
[x] P1-1: --max-parallel-jobs 1 in install-runner-freebsd.sh
[x] P1-2: workflow_dispatch in call-ibs-full.yml + call-smoke.yml
[x] P1-3: dmesg/kernel-config/CPUID artifact collection (setup-freebsd-build/action.yml)
[x] P1-4: retention-days: 30 on all 4 upload-artifact calls
[x] P1-5: status badge in freebsd-src/README.md
[x] P1-6: timeout-minutes: 30 on build-and-test job + 600s build guard in build.sh
[ ] Hardware: register EPYC runner (requires token from GitHub — STILL PENDING 2026-05-07)
[ ] Confluence: copy runner-setup.md + architecture ASCII to CI Design page
```

## Phase 2 Status — 2026-05-11

```
[ ] P2-1: Runner re-registration — token expired, need fresh token from GitHub Settings
[ ] P2-2: E2E green-path validation — blocked on P2-1
[ ] P2-3: Negative-path validation — blocked on P2-1
[ ] P2-4: Runbook (dev-docs/runbook.md) — not started
```

## Additional Items (added 2026-05-11)

### run.sh --auto mode
**Priority:** done
**Context:** run.sh can now orchestrate a full automated test cycle:
`--auto` builds the kernel, installs it, pre-compiles tests, writes a sentinel to
`/var/db/ibs-autotest-sentinel`, installs an rc.d service at
`/usr/local/etc/rc.d/ibs_autotest`, and reboots. After reboot the service runs the
suite, emails the report via dma→txsmtp.amd.com, then self-disables.
**Status:** implemented; dry-run verified

### run.sh suite and category selection
**Priority:** done
**Context:** `--suite IBS|UMCDF|PMC|ALL` and `--category TC-*` (repeatable) allow
partial runs without editing Kyuafiles manually. A filtered Kyuafile is generated
in `/tmp/ibs_kyuafile_<pid>.tmp` and cleaned up on exit.
**Status:** implemented; dry-run verified

### run.sh --help expansion
**Priority:** done
**Context:** --help now documents every command, suite, category, option, verdict
criteria, and file paths. Suitable as the primary reference instead of the wiki.
**Status:** done

### UMCDF suite integration into CI workflow
**Priority:** done
**Context:** `caller-examples/call-umcdf-full.yml` created; `ibs-full-test.yml` now
accepts a `tests_dir` input so UMCDF and future suites reuse the same reusable
workflow. UMCDF: 10 programs, 123 test cases.
**Status:** done (pending E2E run once runner is registered)

### Full IBS re-run at HEAD
**Priority:** high
**Context:** Last captured run (07e8153, 2026-05-06): 54 passed, 6 skipped, 1 failure
(ibs_hwpmc_getmsr_virtual_negative — likely fixed by DF2 encoding dispatch fix).
Need a full clean run at HEAD (current: 18798e0) to confirm all 34 tests pass.
**Status:** pending (runner registration required)

### --suite ALL implementation
**Priority:** medium
**Context:** `--suite ALL` parses correctly but `suite_install_dir("ALL")` falls
through to the IBS default. `run_all_tests()` does not iterate suites. A sequential
IBS → UMCDF → PMC run loop needs to be implemented.
**Status:** gap G8 — not yet implemented
