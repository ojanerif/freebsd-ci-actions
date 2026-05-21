---
id: run-sh
type: module
file: dev-docs/modules/run-sh.md
keywords: [run.sh, auto_mode, confirm_cmd, AUTOTEST_SENTINEL, LAST_COMMIT_FILE, ibs_autotest, rc.d, cron, email, sentinel, kernel, build, install, reboot, AUTO_MODE]
description: Local lifecycle manager for AMD PMU test suites — compile, run, report, commit, and --auto (kernel build + post-reboot test + email) with cron-safe no-prompt execution
status: active
last_modified: 2026-05-18
---

# run.sh — Local Test Suite Lifecycle Manager

Manages the full lifecycle of AMD PMU test suites on bare-metal FreeBSD.

## Key paths

| Constant | Value |
|---|---|
| `AUTOTEST_SENTINEL` | `/var/db/ibs-autotest-sentinel` |
| `LAST_COMMIT_FILE` | `/var/db/ibs-autotest-last-commit` |
| `RCD_SERVICE` | `/usr/local/etc/rc.d/ibs_autotest` |
| Log | `/var/log/ibs-autotest.log` |

## --auto workflow

1. Check if `$SRC_DIR` git HEAD == `LAST_COMMIT_FILE` → if yes, skip and email
2. `make buildkernel` in `$SRC_DIR`
3. `make installkernel` + `nextboot`
4. Pre-compile test suite
5. Write sentinel (includes `AUTOTEST_SRC_COMMIT`)
6. Install + enable `ibs_autotest` rc.d service
7. Reboot

After reboot, `ibs_autotest` rc.d:
- Sources sentinel, removes it, disables itself
- Runs tests, emails verdict
- Writes tested commit hash to `LAST_COMMIT_FILE`

## AUTO_MODE flag

`AUTO_MODE=1` is set when `--auto` is parsed from argv. Effects:
- `confirm_cmd()` returns 0 immediately (no interactive prompt)
- `check_boot_environment()` is skipped

## [DECISION] Cron-safe --auto: no confirmations, already-tested guard
**Date:** 2026-05-11
**Context:** `--auto` was previously interactive — it called `confirm_cmd()` for
kernel build, install, compile, and reboot. This made it unusable from cron.
There was also no protection against repeated builds when the source hadn't
changed (cron would rebuild and reboot on every invocation).
**Decision:**
- Set `AUTO_MODE=1` when `--auto` is parsed; `confirm_cmd()` returns 0 in that mode.
- `check_boot_environment()` is a no-op in AUTO_MODE.
- `auto_mode()` compares `git -C $SRC_DIR rev-parse HEAD` with `LAST_COMMIT_FILE`
  at startup; if equal, sends a "already tested" email and exits cleanly.
- After tests complete, the rc.d service writes `AUTOTEST_SRC_COMMIT` (from the
  sentinel) to `LAST_COMMIT_FILE`.
- `AUTOTEST_SRC_COMMIT` is captured in `write_autotest_sentinel()` so it survives
  the reboot inside the sentinel file.
**Discarded alternatives:**
- Timestamp-based dedup: fragile (time drift, manual re-tests).
- Kernel version string (`uname -v`): only available post-reboot, not pre-build.
**Impact:** run.sh only

## Learning Log

2026-05-11 | AUTO_MODE flag + already-tested commit guard added; rc.d now records AUTOTEST_SRC_COMMIT to LAST_COMMIT_FILE post-test | run-sh
2026-05-12 | --auto up-to-date path now calls generate_html_skipped_report: creates work/results-TIMESTAMP-skipped/ with report.txt (VERDICT: SKIPPED) + report.html; index shows blue SKIPPED badge | run-sh
2026-05-18 | generate_html_report: _sys_esc changed from uname -srm to uname -a; header now shows full kernel version string on a second .meta line | run-sh
2026-05-18 | generate_html_index: added Kernel column (release field from "System     :" line in report.txt, awk field 5); added --reindex flag to regenerate index.html without a full test run | run-sh
2026-05-18 | generate_html_index: Kernel column now shows full "FreeBSD 16.0-CURRENT #N branch-slug" instead of only field 5; sed strips "FreeBSD hostname release" prefix, awk -F': ' cuts at build date | run-sh
