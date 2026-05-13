---
title: Jira Tickets — FreeBSD-Tests project
last_modified: 2026-05-13
registered_in_jira: "000–010, 042–043"
not_yet_registered: "011–025, 044–047"
---

# Jira Tickets — FreeBSD-Tests

Numbering convention:
- **Low numbers (000–…)** — planned stories and tasks, assigned sequentially.
- **High numbers (042+)** — unplanned bugs and hotfixes, appended as found.

---

## Registered in Jira (000–010, 042)

### FreeBSD-Tests-000
**Summary:** FreeBSD-Tests-000 - CI: initial freebsd-ci-actions framework, IBS test suite, and sos-git push infrastructure
**Start:** 2026-04-09
**End:** 2026-04-29
**Status:** Closed

---

### FreeBSD-Tests-001
**Summary:** FreeBSD-Tests-001 - IBS: expand ibs_msr_test.c — stub → full MSR matrix
**Start:** 2026-04-10
**End:** 2026-04-28
**Status:** Closed

---

### FreeBSD-Tests-002
**Summary:** FreeBSD-Tests-002 - IBS: create ibs_sysctl_test.c
**Start:** 2026-04-10
**End:** 2026-04-28
**Status:** Closed

---

### FreeBSD-Tests-042 *(unplanned)*
**Summary:** FreeBSD-Tests-042 - Bug: IBS: fix 2 test failures and add graceful skips for unimplemented kernel paths
**Start:** 2026-04-28
**End:** 2026-04-28
**Status:** Closed

---

### FreeBSD-Tests-003
**Summary:** FreeBSD-Tests-003 - PMC: add ATF tests for raw TSC output and EXTERROR negative paths (hwpmc_exterr_test)
**Description:**
Port and integrate `hwpmc_exterr_test` from freebsd/freebsd-src#2180 into `tests/sys/amd/pmc/`. The test validates EXTERROR diagnostic paths for generic hwpmc, AMD core PMCs, and IBS PMCs through 14 root-only ATF cases. Additionally adds `pmcstat_tsc_test.sh` as a shell smoke test for raw TSC-based PMC output (originated from SWLSVROS-6363).
Test programs require `hwpmc.ko`, run as root, and use ATF_TC_WITHOUT_HEAD — so `required_user=root` must be set via TEST_METADATA in the Makefile, matching the ibs/ pattern.
A `require_working_exterr()` guard was added to all 14 cases: it probes the kernel extended error population before each test body and gracefully skips if absent, keeping the suite green as SKIP rather than FAIL on kernels that regress this feature.
Files: `tests/sys/amd/pmc/hwpmc_exterr_test.c`, `tests/sys/amd/pmc/pmcstat_tsc_test.sh`, `tests/sys/amd/pmc/Makefile`, `tests/sys/amd/pmc/Kyuafile`.
**Start:** 2026-04-16
**End:** 2026-05-12
**Status:** Closed

---

### FreeBSD-Tests-004
**Summary:** FreeBSD-Tests-004 - CI: reusable GitHub Actions workflow, composite actions, Jira integration, and Linuxulator runner fixes
**Description:**
Design and implement the full GitHub Actions CI pipeline as a reusable workflow (`ibs-full-test.yml`) with 4 composite actions — `setup-freebsd-build`, `build-kernel`, `run-atf-tests`, `report-results` — callable by thin 10-line YAML callers in any repo.
Pipeline stages: (1) collect system info (dmesg, sysctl, kernel config artifact); (2) compile `hwpmc.ko` with a 600s timeout guard and 15-min job ceiling; (3) load module, run kyua, emit JUnit XML + HTML; (4) parse XML, post Markdown summary to `GITHUB_STEP_SUMMARY`, upload 6 artifact sets at 30-day retention.
Jira integration (login → comment on failure → transition on success) is secrets-gated via `if: ${{ secrets.JIRA_API_TOKEN != '' }}` so callers without Jira no-op silently.
Linuxulator fixes applied to the self-hosted runner: switch from `linux_base-c7` (GLIBCXX 3.4.19) to `linux_base-rl9` + `linux-rl9-icu`; add `nullfs` bind mount `/home/gh-runner` → `/usr/home/gh-runner` for .NET `realpath()` symlink issue; add `/bin/bash` symlink for runner shebang compatibility.
Caller examples: `caller-examples/call-ibs-full.yml`, `caller-examples/call-smoke.yml`.
**Start:** 2026-04-29
**End:** 2026-04-30
**Status:** Closed

---

### FreeBSD-Tests-005
**Summary:** FreeBSD-Tests-005 - UMCDF: create AMD UMC/DF/CPUID validation test suite — 10 programs, 123 test cases
**Description:**
Create `tests/sys/amd/umcdf/` — a new ATF test suite covering AMD Unified Memory Controller (UMC) and Data Fabric (DF) PMU validation via FreeBSD `hwpmc.ko`.
Suite structure: shared header `amd_umcdf_common.h` providing CPUID detection, Zen generation classification (Zen 1–6), PMC lifecycle helpers, and event candidate structs. Three integration programs (8 test cases): `umcdf_cpuid_test` (CPUID generation decode + PerfMonV2 capability), `umcdf_df_test` (DF event enumeration, encoding, runtime smoke for Zen 1–5), `umcdf_umc_test` (UMC capability probe, metadata contract, runtime smoke). Seven hardware-free unit programs (115 test cases) added subsequently: `umcdf_unit_zen_map_test`, `_df_encoding_test`, `_perfmonv2_test`, `_zen_name_test`, `_capabilities_test`, `_vendor_test`, `_df_config_dispatch_test`. Runtime smoke tests gracefully `atf_tc_skip()` when hwpmc UMC/DF support is absent. All programs require root.
Two bugs encountered and resolved before first run: DF2 encoding dispatch (FreeBSD-Tests-044) and UMC concurrent PMC block (FreeBSD-Tests-045).
**Start:** 2026-04-30
**End:** 2026-05-08
**Status:** Closed

---

### FreeBSD-Tests-006
**Summary:** FreeBSD-Tests-006 - IBS: add hwpmc runtime tests — capability, allocation, event discovery, runtime contracts
**Description:**
Expand the IBS ATF test suite from 25 to 30 programs by adding 5 hwpmc API integration test programs: `ibs_hwpmc_alloc_test` (PMC allocation lifecycle for IBS-Fetch and IBS-Op PMC classes), `ibs_hwpmc_caps_test` (capability flags reported by `pmc_cpuinfo` for IBS classes), `ibs_hwpmc_info_test` (hwpmc module info: class count, vendor, model), `ibs_hwpmc_event_discovery_test` (event enumeration — expected count and names per IBS class), `ibs_hwpmc_runtime_test` (full allocate → start → sample → stop → release contract for both Fetch and Op counters).
These tests exercise the `libpmc` API layer (`pmc_init`, `pmc_allocate`, `pmc_start`, `pmc_read`, `pmc_stop`, `pmc_release`) rather than direct MSR access, covering a different failure surface from the existing cpuctl-based tests. All require `hwpmc.ko` and root.
Validated on AMD EPYC 9654 (Zen 4): 54 passed, 6 skipped (expected on this hardware), 1 failure (`ibs_hwpmc_getmsr_virtual_negative` — wrong errno, tracked as FreeBSD-Tests-043).
**Start:** 2026-04-30
**End:** 2026-05-06
**Status:** Closed

---

### FreeBSD-Tests-007
**Summary:** FreeBSD-Tests-007 - run.sh: --auto mode — cron-safe kernel build, reboot, test, MIME email with already-tested guard
**Description:**
Implement `--auto` mode in `run.sh` to allow fully unattended test cycles triggered by cron or systemd. Full flow: (1) check if `$SRC_DIR` git HEAD equals `/var/db/ibs-autotest-last-commit` — if yes, send "already tested" email and exit cleanly; (2) `make buildkernel KERNCONF=…`; (3) `make installkernel` + `nextboot`; (4) pre-compile test suite; (5) write sentinel file `/var/db/ibs-autotest-sentinel` (captures `AUTOTEST_SRC_COMMIT`, recipient, suite, categories, kernconf); (6) install and enable `ibs_autotest` rc.d service; (7) reboot. After reboot the rc.d service sources the sentinel, runs the suite via `run.sh --run-all --force`, emails the verdict, writes tested commit hash to `/var/db/ibs-autotest-last-commit`, and self-disables.
`AUTO_MODE=1` flag makes `confirm_cmd()` return 0 immediately (no interactive prompt) and skips `check_boot_environment()`. `--suite`, `--category`, `--kernconf`, `--email` flags all respected in auto mode.
Email reports use MIME `multipart/mixed`: plain-text summary as body, `report.txt` and `report.xml` as attachments, sent from `freebsd-ci-actions@amd.com` via `atlsmtp10.amd.com:25` (STARTTLS) to `freebsd-test@mailman-svr.amd.com,ojanerif@amd.com` by default.
**Start:** 2026-05-11
**End:** 2026-05-13
**Status:** Closed

---

### FreeBSD-Tests-008
**Summary:** FreeBSD-Tests-008 - CI: artifact retention §8.1 tiered policy, secrets:inherit, flaky-test retry, UMCDF/L3 callers
**Description:**
Overhaul the GitHub Actions workflow infrastructure to align with CI/CD Design Proposal §8.1 and add robustness features.
Artifact retention revised from flat 30-day to tiered: JUnit XML + HTML + system-info → 90 days; kernel config + SHA → 180 days; PMC logs → 30 days. `ibs-full-test.yml` gains a `tests_dir` input so UMCDF, PMC, and future suites reuse the same workflow. `secrets: inherit` added to all caller files (`call-ibs-full.yml`, `call-smoke.yml`, `call-umcdf-full.yml`). `call-l3-full.yml` created as a disabled placeholder (`if: false`) for when L3 tests land. `build-kernel/scripts/build.sh` captures git SHA + kernel config into `_kernel-info/` artifact at 180-day retention. `run-tests.sh` fully rewritten: retries failing tests up to 3 attempts, distinguishes truly-failed from flaky, reports both categories separately. `NOTIFY_EMAIL` failure notification step added to `ibs-full-test.yml` (secrets-gated via `dawidd6/action-send-mail@v3`). `caller-examples/call-umcdf-full.yml` created covering the 10-program UMCDF suite.
**Start:** 2026-05-11
**End:** 2026-05-11
**Status:** Closed

---

### FreeBSD-Tests-009
**Summary:** FreeBSD-Tests-009 - L3: add L3 counter PMU test suite with Zen 1–6 JSON coverage
**Description:**
Create `tests/sys/amd/l3/` — ATF test suite for AMD L3 cache PMU counters via FreeBSD `hwpmc.ko`. Three programs sharing `amd_l3_common.h`:
`l3_detect_test` (TC-DET): 3 cases — capability probe (reads CPUID Fn80000022 PerfMonV2 without programming any MSR), L3 JSON coverage check (verifies FreeBSD ships L3 PMU JSON for detected Zen generation, Zen 1–6), hwpmc module sanity.
`l3_miss_test` (TC-UNC-L3-01): workload-driven L3 miss counter test — 64 MB buffer with 4 KB page-stride access to defeat the hardware stream prefetcher and produce capacity misses; primary event `l3_lookup_state.l3_miss` (EventSel=0x04, UMask=0x01), fallback to `l3_lookup_state.all_coherent_accesses_to_l3` (UMask=0xff).
`l3_hit_test` (TC-UNC-L3-02): workload-driven L3 hit counter test — 2 MB buffer with warm-up pass so subsequent accesses are L3 hits; primary event `l3_lookup_state.l3_hit` (UMask=0xfe), fallback to UMask=0xff.
All three programs skip gracefully on Zen generations or kernels lacking FreeBSD L3 PMU JSON support. All require root and `hwpmc.ko`.
**Start:** 2026-05-12
**End:** 2026-05-13
**Status:** Closed

---

### FreeBSD-Tests-010
**Summary:** FreeBSD-Tests-010 - CI: SMTP migration to atlsmtp10.amd.com, MIME multipart reports with attachments to freebsd-test mailing list
**Description:**
Migrate the system mail relay from `txsmtp.amd.com` to `atlsmtp10.amd.com:25` (STARTTLS, no auth) and update all email paths in `run.sh` to send structured MIME reports.
`/etc/dma/dma.conf` updated: `SMARTHOST atlsmtp10.amd.com`, `PORT 25`, `SECURETRANSFER`, `STARTTLS`, `MASQUERADE freebsd-ci-actions@amd.com`.
`run.sh` changes: `SENDER_EMAIL` variable added (default: `freebsd-ci-actions@amd.com`); `REPORT_EMAIL` default changed to `freebsd-test@mailman-svr.amd.com,ojanerif@amd.com`; new `send_mime_report` helper constructs RFC-compliant `multipart/mixed` messages (plain-text summary body + `report.txt` attachment + `report.xml` attachment) and pipes to `sendmail -f $SENDER_EMAIL`; `send_report_email` updated to call `send_mime_report` per recipient; rc.d service heredoc rewritten with equivalent inline MIME construction (hardcodes `freebsd-ci-actions@amd.com` since `SENDER_EMAIL` is not available in rc.d context); subject lines cleaned of non-ASCII em dashes (`—` → ` - `).
**Start:** 2026-05-13
**End:** 2026-05-13
**Status:** Closed

---

## Not yet registered in Jira (011–025)

### FreeBSD-Tests-011
**Summary:** FreeBSD-Tests-011 - Runner: register EPYC self-hosted runner with fresh GitHub token
**Description:**
The AMD EPYC bare-metal host (`ruby-9470host`) has `scripts/install-runner-freebsd.sh` applied and all Linuxulator fixes in place, but the runner is not registered because GitHub registration tokens expire after 60 minutes. A fresh token must be generated from GitHub Settings → Actions → Runners → New self-hosted runner, then `config.sh` re-run with labels `self-hosted,freebsd,amd-ibs` before the token expires. This is the blocker for FreeBSD-Tests-013 and FreeBSD-Tests-014.
**Start:** Open
**End:** —
**Status:** Open

---

### FreeBSD-Tests-012
**Summary:** FreeBSD-Tests-012 - CI: configure GitHub Actions email notifications to deliver to freebsd-test@mailman-svr.amd.com
**Description:**
The `NOTIFY_EMAIL` step in `ibs-full-test.yml` uses `dawidd6/action-send-mail@v3`, gated on `secrets.NOTIFY_EMAIL != ''`. Configure the workflow to deliver CI failure and success notifications to `freebsd-test@mailman-svr.amd.com`. Required GitHub repo secrets to add in `ojanerif/freebsd-src` settings: `NOTIFY_EMAIL=freebsd-test@mailman-svr.amd.com`, `SMTP_SERVER=atlsmtp10.amd.com`, `SMTP_PORT=25` (no auth required — AMD open relay, sender `freebsd-ci-actions@amd.com`).
The host-level `dma.conf` and `run.sh` SMTP path are already configured and verified (FreeBSD-Tests-010); this ticket covers only the GitHub Actions secrets and delivery verification to the list.
Acceptance criteria: a CI failure on the self-hosted runner produces an email to the freebsd-test list within 5 minutes of job completion, from `freebsd-ci-actions@amd.com`, with the workflow run URL and failure count in the body.
**Start:** Open
**End:** —
**Status:** Open

---

### FreeBSD-Tests-013
**Summary:** FreeBSD-Tests-013 - CI: E2E green-path validation — >=6 PASS / 1 SKIP on call-ibs-full.yml (Story 4.1)
**Description:**
With the runner registered (FreeBSD-Tests-011), trigger `call-ibs-full.yml` via `workflow_dispatch` on a known-good commit. Expected outcome: >=6 PASS, exactly 1 SKIP (IBSOPDATA4 outside active sampling), 0 FAIL. Verify that all 6 artifact sets are uploaded and accessible, runtime is under 30 minutes, Markdown summary appears in the workflow run, and JUnit XML is parseable by the report-results action.
**Start:** Open (blocked: FreeBSD-Tests-011)
**End:** —
**Status:** Open

---

### FreeBSD-Tests-014
**Summary:** FreeBSD-Tests-014 - CI: E2E negative-path validation — force failure, verify JUnit XML annotation (Story 4.2)
**Description:**
Introduce an intentional test failure (e.g. wrong expected value in one ATF assertion) and trigger `call-ibs-full.yml`. Verify that the workflow reports a non-zero exit, the JUnit XML contains a `<failure>` element, the GitHub Actions annotation marks the failing test, the Jira comment step fires (if secrets configured), and the `NOTIFY_EMAIL` step sends an alert to `freebsd-test@mailman-svr.amd.com`.
**Start:** Open (blocked: FreeBSD-Tests-011)
**End:** —
**Status:** Open

---

### FreeBSD-Tests-015
**Summary:** FreeBSD-Tests-015 - run.sh: implement --suite ALL sequential multi-suite run loop
**Description:**
`--suite ALL` is parsed correctly but `suite_install_dir("ALL")` falls through to the IBS default and `run_all_tests()` does not iterate over suites. Implement a sequential loop: IBS → UMCDF → PMC, each with its own Kyuafile and results directory. The loop should aggregate verdicts and produce a combined report before emailing.
**Start:** Open
**End:** —
**Status:** Open

---

### FreeBSD-Tests-016
**Summary:** FreeBSD-Tests-016 - CI: wire UMCDF test suite into CI workflow with composite action
**Description:**
`call-umcdf-full.yml` exists and passes `tests_dir: tests/sys/amd/umcdf` to the reusable workflow, but `run-atf-tests` has never been validated against the UMCDF directory. Confirm the composite action builds, runs kyua, and emits JUnit XML correctly for the 10-program UMCDF suite. Capture first CI run results.
**Start:** Open
**End:** —
**Status:** Open

---

### FreeBSD-Tests-017
**Summary:** FreeBSD-Tests-017 - CI: wire PMC test suite into CI workflow
**Description:**
No CI caller or composite action covers `tests/sys/amd/pmc/`. Create `caller-examples/call-pmc-full.yml` and validate that the reusable workflow (with `tests_dir: tests/sys/amd/pmc`) correctly builds `hwpmc_exterr_test` and `pmcstat_tsc_test.sh`, runs kyua as root, and uploads JUnit XML artifacts.
**Start:** Open
**End:** —
**Status:** Open

---

### FreeBSD-Tests-018
**Summary:** FreeBSD-Tests-018 - CI: configure Jira secrets in freebsd-src repo to activate Jira integration
**Description:**
Jira integration is fully implemented and secrets-gated. Three secrets must be added to the `ojanerif/freebsd-src` repository settings to activate it: `JIRA_BASE_URL` (`https://amd.atlassian.net`), `JIRA_USER_EMAIL` (AMD Atlassian email), `JIRA_API_TOKEN` (from `id.atlassian.com/manage-profile/security/api-tokens`). Once set, the workflow will login, comment the run URL + failure count on failure, and transition the Jira story to Done on success.
**Start:** Open
**End:** —
**Status:** Open

---

### FreeBSD-Tests-019
**Summary:** FreeBSD-Tests-019 - Docs: publish Confluence pages — runner guide, architecture diagram, runbook
**Description:**
All documentation content exists locally; it needs to be published to AMD Confluence. Three pages required: (1) `dev-docs/runner-and-cicd-guide.md` → runner setup + CI guide (Story 1.5); (2) architecture diagram + design overview from README (Story 4.4); (3) runbook: runner re-registration, binary update, debug troubleshooting, manual trigger instructions (Story 4.4). The Test Plan v1.4 also needs updating from Jenkins references to GitHub Actions (Story 4.3).
**Start:** Open
**End:** —
**Status:** Open

---

### FreeBSD-Tests-020
**Summary:** FreeBSD-Tests-020 - IBS: activate NMI flood placeholders TC-ROB-01/02 in ibs_robustness_test.c
**Description:**
`ibs_robustness_test.c` contains placeholder test cases for NMI flood scenarios (TC-ROB-01: flood under active Op sampling; TC-ROB-02: flood under active Fetch sampling) that are currently skipped unconditionally. Activation requires `sysctl dev.hwpmc.0.ibs_active` which does not yet exist in FreeBSD HEAD. When the kernel sysctl lands, remove the placeholder skip and implement the test body: enable IBS at high frequency, generate load, verify no kernel panic or hung counter.
**Start:** Open (blocked: kernel patch)
**End:** —
**Status:** Open

---

### FreeBSD-Tests-021
**Summary:** FreeBSD-Tests-021 - IBS: implement ibs_ioctl_test.c — 5 test cases for kernel IBS ioctl API (TC-IBS-IOC-01)
**Description:**
`ibs_ioctl_test.c` is currently a 38-LOC placeholder that skips all cases because `/dev/ibs0` does not exist. When the kernel-side IBS ioctl API lands in FreeBSD HEAD, implement 5 test cases: open/close device, get capability via ioctl, configure Fetch sampling via ioctl, configure Op sampling via ioctl, error handling for invalid ioctl arguments.
**Start:** Open (blocked: kernel IBS ioctl API)
**End:** —
**Status:** Open

---

### FreeBSD-Tests-022
**Summary:** FreeBSD-Tests-022 - Uncore PMC: implement L3/DF/UMC/C2C Phase 3 test cases (TC-UNC-*)
**Description:**
Phase 3 expansion of the PMC and UMCDF suites to cover all uncore counter types once kernel hwpmc uncore support lands in FreeBSD HEAD. Planned scope: 7 scenarios, 40+ test cases covering L3 cache counters (partially covered by FreeBSD-Tests-009), Data Fabric bandwidth and latency, UMC memory controller read/write bandwidth, and C2C (cache-to-cache) inter-socket traffic. C2C cases require specific multi-socket workload setup.
**Start:** Open (blocked: kernel hwpmc uncore support)
**End:** —
**Status:** Open

---

### FreeBSD-Tests-023
**Summary:** FreeBSD-Tests-023 - CI: add KASAN job — KERNCONF=GENERIC-KASAN
**Description:**
Add a CI job that builds and runs the IBS test suite against a KASAN kernel (`KERNCONF=GENERIC-KASAN`) to catch memory-safety issues in `hwpmc.ko` and the ATF test programs. Mirrors the FreeBSD RE CI pattern (`ci/jobs/FreeBSD-main-amd64-KASAN_test/`). Expected outcome: all tests PASS or SKIP with no new failures introduced by sanitizer instrumentation.
**Start:** Open
**End:** —
**Status:** Open

---

### FreeBSD-Tests-024
**Summary:** FreeBSD-Tests-024 - CI: wire L3 test suite into CI workflow
**Description:**
`call-l3-full.yml` exists in `caller-examples/` but is disabled (`if: false`) pending L3 test code. Now that `tests/sys/amd/l3/` is implemented (FreeBSD-Tests-009), enable the caller and validate that the reusable workflow correctly builds the three L3 programs (`l3_detect_test`, `l3_hit_test`, `l3_miss_test`), runs kyua as root, and uploads JUnit XML artifacts. Capture first CI run results on ruby-9470host (AMD EPYC 9654, Zen 4).
**Start:** Open
**End:** —
**Status:** Open

---

### FreeBSD-Tests-025
**Summary:** FreeBSD-Tests-025 - CI: cross-architecture Intel runner — assert all IBS tests SKIP, not FAIL
**Description:**
Add a second self-hosted runner with labels `self-hosted,freebsd,intel` and a caller workflow targeting it. The IBS unit tests are architecture-independent and should PASS on Intel; all hardware tests (cpuctl MSR access, hwpmc IBS allocation) should SKIP gracefully via the existing `amd_check_*` guards — not FAIL. This validates that skip paths are correct on non-AMD hardware and that no test hard-fails due to a missing AMD-only code path.
**Start:** Open (requires second runner)
**End:** —
**Status:** Open

---

## Unplanned bugs — registered in Jira (042–043)

> 042 and 043 were registered before 2026-05-13. 044–047 are not yet registered.


### FreeBSD-Tests-043 *(unplanned)*
**Summary:** FreeBSD-Tests-043 - Bug: IBS hwpmc getmsr errno mismatch — pmc_get_msr(IBS-FETCH) returns ENOTSUP, test expected EINVAL
**Description:**
`ibs_hwpmc_getmsr_virtual_negative` failed during the 2026-05-06 run on AMD EPYC 9654 Zen 4. The test called `pmc_get_msr()` on an IBS-FETCH virtual PMC and expected `errno == EINVAL`, but the kernel returned `errno 78 (ENOTSUP)`. Root cause: the kernel's `pmc_get_msr` path for IBS virtual PMCs returns ENOTSUP (operation not supported on this PMC class) rather than EINVAL (invalid argument). Fixed in commits `07e8153` (correct hwpmc runtime contracts) and `3981aa2` (serialize hwpmc tests).
**Found:** 2026-05-06
**Resolved:** 2026-05-06
**Status:** Closed

---

## Unplanned bugs — not yet registered in Jira (044–047)

### FreeBSD-Tests-044 *(unplanned)*
**Summary:** FreeBSD-Tests-044 - Bug: UMCDF DF2 encoding dispatch selects wrong event index for second Data Fabric instance
**Description:**
`umcdf_df_runtime_smoke` allocated the wrong PMC event for the DF2 instance (second Data Fabric). The encoding dispatch used the base-event index instead of the DF2-offset event index, causing an unexpected error or incorrect sample count. Fixed in commit `4590da4` (umcdf: fix DF2 encoding dispatch and runtime-smoke skip). Detected before the first full run.
**Found:** 2026-05-07
**Resolved:** 2026-05-07
**Status:** Closed

---

### FreeBSD-Tests-045 *(unplanned)*
**Summary:** FreeBSD-Tests-045 - Bug: UMCDF UMC runtime smoke blocks under concurrent PMC access — serialization missing
**Description:**
`umcdf_umc_runtime_smoke_if_supported` occasionally blocked when another test program held a shared UMC PMC resource, causing kyua to report a timeout. Root cause: PMC allocation was not serialized across test programs; concurrent programs competed on the same UMC PMC hardware resource. Fixed by removing `is_exclusive` metadata (kyua 0.13 compatibility) and adding explicit serialization in commit `6d57d07` (umcdf: serialize UMC runtime smoke). Detected before the first full run.
**Found:** 2026-05-06
**Resolved:** 2026-05-07
**Status:** Closed

---

### FreeBSD-Tests-046 *(unplanned)*
**Summary:** FreeBSD-Tests-046 - Bug: ibs_swfilt_test.sh fails — cpucontrol(8) absent from kyua restricted PATH, converted to C
**Description:**
`ibs_swfilt_exclude_user`, `ibs_swfilt_exclude_kernel`, and `ibs_swfilt_filter_combination` all failed with "Cannot read IBS Fetch/Op Control MSR". Root cause: kyua runs ATF shell tests with a restricted PATH that excludes `/usr/sbin` where `cpucontrol(8)` lives. Fix: deleted `ibs_swfilt_test.sh`, rewrote as `ibs_swfilt_test.c` using `read_msr()`/`write_msr()` from `ibs_utils.h` (CPUCTL_RDMSR/CPUCTL_WRMSR ioctls directly — no PATH dependency). Makefile entry moved from `ATF_TESTS_SH` to `ATF_TESTS_C`; Kyuafile unchanged.
**Found:** 2026-05-12
**Resolved:** 2026-05-12
**Status:** Closed

---

### FreeBSD-Tests-047 *(unplanned)*
**Summary:** FreeBSD-Tests-047 - Bug: ibs_smp NMI race under 192-CPU parallelism — retry limit 3 insufficient, quiesce write missing
**Description:**
`ibs_smp_per_cpu_config` and `ibs_smp_cpu_migration` failed intermittently under high parallelism (192-CPU run) with MaxCnt readback mismatches and `EBUSY` on migration isolation. Two root causes: (A) retry loop capped at 3 — under full-system parallel load, in-flight NMIs re-arm the counter more than 3 times consecutively; (B) `ibs_smp_cpu_migration` read `orig_val` while IBS was potentially active on CPU 0, so a concurrent NMI mutated MaxCnt before the `verify_val` comparison. Fix: (A) retry limit raised 3→10; `write_msr(cpu, MSR_IBS_FETCH_CTL, 0ULL)` added as first statement in each retry iteration to quiesce NMIs; (B) `write_msr(original_cpu, MSR_IBS_FETCH_CTL, 0ULL)` added before reading `orig_val` in `smp_migration_thread`.
**Found:** 2026-05-12
**Resolved:** 2026-05-12
**Status:** Closed
