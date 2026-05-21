# CI/CD Implementation Status
<!-- Last updated: 2026-05-11 | Author: ojanerif -->

Maps each Jira story to its actual implementation state in this repo.
Reference: https://amd.atlassian.net/wiki/spaces/ALK/pages/1551491076/CI+design

---

## Story 0 — Confluence Documentation
**Priority:** High | **Due:** Apr 11 | **Status:** External (not tracked in git)

Content for Confluence exists in the repo:
- Architecture diagram in `freebsd-src/README.md`
- Runner topology in `freebsd-src/configs/runner-setup.md`
- Artifact policy implicit in `actions/report-results/action.yml`

| Task | Status | Notes |
|------|--------|-------|
| 0.1 Create CI Design parent page | ⬜ External | All content ready; page must be published at the Confluence URL |

---

## Story 1 — Self-Hosted Runner Setup
**Priority:** Critical | **Due:** Apr 16 | **Status:** Runner installed; registration needs fresh token

| Task | Status | Location | Gap |
|------|--------|----------|-----|
| 1.1 Install runner on EPYC bare-metal | ✅ Script + manual fixes applied | `scripts/install-runner-freebsd.sh` | Registration blocked by expired token — needs fresh token from GitHub |
| 1.2 Register runner with label | ✅ Labels confirmed | Labels: `self-hosted,freebsd,amd-ibs` (story requirement met) | Need fresh token to complete |
| 1.3 Configure auto-start via rc.d | ✅ Implemented | `install-runner-freebsd.sh` §9 | — |
| 1.4 Disable concurrent jobs | ✅ N/A | Self-hosted runners run 1 job at a time by default; `--max-parallel-jobs` is not a real config.sh option (removed from script) | — |
| 1.5 Document setup in Confluence | ⬜ External | `dev-docs/runner-and-cicd-guide.md` is the complete guide | Copy/paste to Confluence |

**Linuxulator fixes applied (2026-04-29):**
- Switched `linux_base-c7` → `linux_base-rl9` (Rocky Linux 9): GLIBCXX 3.4.29 required
- Installed `linux-rl9-icu`: .NET ICU dependency (exit 134 without it)
- Nullfs bind `/home/gh-runner → /usr/home/gh-runner`: .NET realpath() fix (exit 133 without it)
- Persisted nullfs in `/etc/fstab`
- Script now uses `/usr/local/bin/bash` for config.sh (FreeBSD sh cannot run runner scripts)
- Removed invalid `--max-parallel-jobs` flag from config.sh call

**Next action:** Generate fresh token at `https://github.com/ojanerif/freebsd-src/settings/actions/runners/new` and run:
```sh
sudo -u gh-runner /usr/local/bin/bash /home/gh-runner/actions-runner/config.sh \
    --url https://github.com/ojanerif/freebsd-src \
    --token <NEW_TOKEN> \
    --labels self-hosted,freebsd,amd-ibs \
    --name freebsd-amd-$(hostname) \
    --replace --unattended
```

---

## Story 2 — Base Workflow: Build & Test Pipeline
**Priority:** Critical | **Due:** Apr 21 | **Status:** Mostly done, 3 gaps

| Task | Status | Location | Gap |
|------|--------|----------|-----|
| 2.1 Create ibs-tests.yml (Checkout→Build→Install→Run) | ✅ Done | `freebsd-src/workflows/ibs-full-test.yml` + `caller-examples/call-ibs-full.yml` | — |
| 2.2 JUnit XML via `kyua report-junit` | ✅ Done | `actions/run-atf-tests/scripts/run-tests.sh:67` | — |
| 2.3 Capture dmesg, kernel config, CPUID info | ✅ Fixed | `actions/setup-freebsd-build/action.yml` — `system-info` artifact added | — |
| 2.4 Triggers: push + manual dispatch | ✅ Fixed | `caller-examples/call-ibs-full.yml` + `call-smoke.yml` — `workflow_dispatch:` added | — |
| 2.5 Timeout guards (10 min build / 15 min test) | ✅ Fixed | `workflows/ibs-full-test.yml` 30-min job ceiling; `build.sh` 600s `timeout` guard | — |

**Acceptance criteria gaps:**
- Push trigger works but manual dispatch unavailable.
- No dmesg / kernel config / CPUID artifacts downloadable.
- No explicit build timeout; a stuck make could block the runner indefinitely.

---

## Story 3 — Test Result Reporting & Artifacts
**Priority:** High | **Due:** Apr 25 | **Status:** 2 gaps

| Task | Status | Location | Gap |
|------|--------|----------|-----|
| 3.1 Parse JUnit XML for inline workflow summary | ✅ Done | `actions/report-results/scripts/generate-report.sh` | — |
| 3.2 Archive Kyua HTML + dmesg | ⚠️ Partial | HTML ✅; dmesg ❌ not captured yet (Story 2.3 gap) | Depends on 2.3 fix |
| 3.3 Add status badge to README | ✅ Fixed | `freebsd-src/README.md` — badge linking to `self-test.yml` added | — |
| 3.4 Set 30-day artifact retention | ✅ Fixed | All 5 `upload-artifact@v4` calls now include `retention-days: 30` | — |

**Acceptance criteria gaps:**
- Status badge missing — cannot render on README.
- Artifacts retained 90 days instead of 30.

---

## Story 4 — Pipeline Validation & Documentation
**Priority:** High | **Due:** Apr 30 | **Status:** Blocked on runner + code gaps

| Task | Status | Blocker |
|------|--------|---------|
| 4.1 E2E: verify 6 PASS / 1 SKIP on good commit | ⬜ Not started | Runner not registered; Stories 2–3 gaps open |
| 4.2 Negative test: force failure to verify reporting | ⬜ Not started | Same as above |
| 4.3 Update Test Plan v1.4 (Jenkins → GitHub Actions) | ⬜ External | Confluence edit |
| 4.4 Final Confluence: architecture diagram + runbook | ⬜ External | ASCII arch exists in README; runbook needs write-up |

---

## Summary Matrix

| Story | Tasks | Done | Partial | Missing |
|-------|-------|------|---------|---------|
| 0 — Docs | 1 | 0 | 0 | 1 (external) |
| 1 — Runner | 5 | 4 | 0 | 1 (token) |
| 2 — Workflow | 5 | 5 | 0 | 0 |
| 3 — Reporting | 4 | 4 | 0 | 0 |
| 4 — Validation | 4 | 0 | 0 | 4 |

**Status as of 2026-05-07:**
1. ~~`--max-parallel-jobs 1` missing (Story 1.4)~~ — applied in ci-infra-fixes.patch
2. ~~`workflow_dispatch` missing (Story 2.4)~~ — fixed
3. ~~dmesg / kernel config / CPUID artifacts missing (Story 2.3)~~ — fixed
4. ~~`retention-days: 30` missing (Story 3.4)~~ — fixed
5. ~~Status badge missing (Story 3.3)~~ — fixed
6. ~~Linuxulator runner crashes (exit 133, exit 134)~~ — fixed (rl9 base, ICU, nullfs bind)
7. **Runner re-registration pending** — token expired; generate new at GitHub Settings
8. **E2E validation** (Story 4.1/4.2 — requires runner registered + pipeline trigger)
9. **Confluence** (Stories 0.1, 4.3, 4.4 — external edits required)

## Additional Work Since Apr 30

- IBS suite expanded: 25 → 30 programs (hwpmc alloc/caps/info/runtime tests added)
- UMCDF suite created: `tests/sys/amd/umcdf/` — 3 programs, 8 cases (UMC + DF PMU validation)
- IBS run on 2026-05-06: 54 passed, 6 skipped, 1 failed (hwpmc_getmsr errno mismatch — likely fixed)
- UMCDF: compiled and fixed; first run pending
- `run.sh` extended with `--fetch`/`--push` flags

## Fixes Applied 2026-05-11

- **BUG run.sh:518** — `git push origin "$SOS_BRANCH"` → `git push origin "HEAD:$SOS_BRANCH"` (wrong refspec when local branch ≠ SOS_BRANCH)
- **caller-examples/call-ibs-full.yml** — Added `pull_request` + nightly/weekly `schedule` triggers (§4.3)
- **caller-examples/call-smoke.yml** — Added `pull_request` trigger (§4.3)
- **actions/report-results/action.yml** — JUnit XML + HTML retention: 30 → 90 days (§8.1)
- **actions/setup-freebsd-build/action.yml** — system-info retention: 30 → 90 days (§8.1)
- **ibs-full-test.yml** — Added `tests_dir` input so callers can point to any test suite
- **caller-examples/call-umcdf-full.yml** — Created; covers §4.1 umc-tests.yml + df-tests.yml

## Fixes Applied 2026-05-11 (round 2)

- **run-atf-tests/action.yml** — kyua-junit-xml + kyua-html-report: 30 → 90 days; added PMC log conditional upload (30 days)
- **run-atf-tests/scripts/run-tests.sh** — Full rewrite: retry failing tests up to 3 total attempts; flaky detection; truly-failed vs flaky reporting
- **build-kernel/scripts/build.sh** — Captures git SHA + kernel config into `_kernel-info/` on success
- **build-kernel/action.yml** — Adds `kernel-config-and-sha` artifact with `retention-days: 180`
- **ibs-full-test.yml** — Added `NOTIFY_EMAIL` failure notification step (secrets-gated via `dawidd6/action-send-mail@v3`)
- **caller-examples/call-ibs-full.yml** — Added `secrets: inherit`
- **caller-examples/call-smoke.yml** — Added `secrets: inherit`
- **caller-examples/call-umcdf-full.yml** — Added `secrets: inherit`
- **caller-examples/call-l3-full.yml** — Created placeholder (disabled with `if: false` until L3 test suite exists)

## Fixes Applied 2026-05-11 (round 3) — run.sh

- **run.sh: `--auto` mode** — New command; orchestrates full automated test cycle across a reboot:
  build kernel (`make buildkernel KERNCONF=…`) → install kernel → pre-compile tests →
  write sentinel (`/var/db/ibs-autotest-sentinel`) → install rc.d service
  (`/usr/local/etc/rc.d/ibs_autotest`) → reboot. After reboot the service runs the
  test suite, emails the report to a configurable address via dma/txsmtp.amd.com, and
  self-disables via `sysrc -x ibs_autotest_enable`.
- **run.sh: `--suite IBS|UMCDF|PMC|ALL`** — Suite selection; drives `suite_src_dir()` /
  `suite_install_dir()` helpers so all commands operate on the right source and install
  paths without hardcoded `ibs/` strings.
- **run.sh: `--category TC-*`** — Per-category test selection (repeatable flag). Calls
  `build_filtered_kyuafile()` to generate a temporary Kyuafile containing only the
  `atf_test_program` entries whose category code matches. kyua receives this filtered
  file via `--kyuafile`.
- **run.sh: `--kernconf`** — Kernel config for `--auto` build (default: GENERIC).
- **run.sh: `--email`** — Override the report recipient for `--auto` and the rc.d service.
- **run.sh: `--help` expanded** — Full per-command documentation: description, what files
  are read/written, what is required, side effects. Includes VERDICT CRITERIA and FILES
  sections.
- **run.sh: interactive menu** — Option `3) Run by category` now calls
  `build_filtered_kyuafile()` before `run_all_tests()` (was missing; filter had no effect).
  Menu shows current suite and includes option `a) AUTO`.
- **run.sh BUG: missing `trap`** — `_ibs_cleanup` was defined but `trap _ibs_cleanup EXIT INT TERM`
  was never installed. Temp files would leak on signal. Fixed.
- **run.sh BUG: `build_kernel_from_src` source-dir check before dry-run guard** — `exit 1`
  fired before the `DRY_RUN` early-return when `$SRC_DIR` was absent. Swapped order so
  `--dry-run` always works regardless of whether the source tree exists.
- **run.sh BUG: `write_autotest_sentinel` and `install_rcd_service` missing dry-run guards** —
  Both functions performed real writes in `--dry-run` mode, leaving a sentinel file and
  an rc.d service on disk. Added `DRY_RUN` checks to both; cleaned up the artefacts left
  by the first dry-run before the fix.

## Fixes Applied 2026-05-11 (round 4) — run.sh --auto cron-safe

- **run.sh: `--auto` no-prompt mode** — `confirm_cmd()` returns 0 immediately when
  `AUTO_MODE=1` (set by `--auto`); no interactive prompts are issued anywhere in the
  build/install/compile/reboot flow.  Safe to invoke from cron.
- **run.sh: boot-environment check skipped in AUTO_MODE** — `check_boot_environment()`
  returns early when `AUTO_MODE=1`; the operator is responsible for the safety net in
  unattended operation.
- **run.sh: already-tested guard** — `auto_mode()` reads
  `/var/db/ibs-autotest-last-commit`.  If the file exists and its content equals
  `git -C $SRC_DIR rev-parse HEAD`, the source tree has not changed since the last run:
  a notification email (`[AMD CI] Auto-test skipped — already tested — …`) is sent and
  the script exits without building or rebooting.
- **run.sh: `AUTOTEST_SRC_COMMIT` in sentinel** — `write_autotest_sentinel()` captures
  the pre-reboot git HEAD and writes it as `AUTOTEST_SRC_COMMIT` into
  `/var/db/ibs-autotest-sentinel` so the value survives the reboot.
- **run.sh: rc.d records last-tested commit** — After emailing the post-reboot report,
  the `ibs_autotest` rc.d service writes `$AUTOTEST_SRC_COMMIT` to
  `/var/db/ibs-autotest-last-commit`.  This is the file the next `--auto` invocation
  reads for the already-tested check.
- **run.sh `--auto` help text** — Updated to document step 0 (already-tested guard),
  the removal of confirmation prompts, and the new sentinel fields.
- **dev-docs/modules/run-sh.md** — New module file created; added to `dev-docs/index.jsonl`.

## Remaining Gaps (2026-05-11 — after round 3)

| # | Gap | Section | Action Required |
|---|-----|---------|----------------|
| G1 | No L3 test suite code | §4.1, §7.3 | Create `tests/sys/amd/l3/` — the caller placeholder (`call-l3-full.yml`) is ready; only the test code is missing |
| G2 | `PMC_LOG_UPLOAD_KEY` external upload not wired | §6.1 | Upload step added but uses GitHub artifact storage; external upload (S3/NFS via PMC_LOG_UPLOAD_KEY) not implemented — backend unknown |
| G3 | Parallel execution matrix | §4.5 | Design + implement; blocked on runner capacity |
| G4 | Runner not registered | §5 | Generate fresh token at GitHub Settings → Actions → Runners |
| G5 | E2E validation (Story 4.1/4.2) | §11 | Blocked on G4 |
| G6 | Confluence pages not published | §0, §4.3, §4.4 | External edit; all content exists in dev-docs/ |
| G7 | SMTP secrets not configured | §6.1 | `NOTIFY_EMAIL` step requires `SMTP_SERVER`, `SMTP_PORT`, `SMTP_USERNAME`, `SMTP_PASSWORD` secrets in addition to `NOTIFY_EMAIL` |
| G8 | `--suite ALL` not fully implemented | run.sh | `suite_install_dir("ALL")` falls through to IBS default; sequential multi-suite run in `run_all_tests()` not implemented |
