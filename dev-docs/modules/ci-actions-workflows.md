---
module: ci-actions-workflows
type: module
status: active
stack: YAML (GitHub Actions), bash
last_modified: 2026-04-30
related: [[[ibs-test-suite]], [[runner-setup]], [[github-actions-api]], [[jira]]]
tags: [freebsd-ci-actions, module, ci, github-actions]
---

# CI Actions & Workflows
> Reusable GitHub Actions workflow + 4 composite actions that orchestrate build, test, and reporting for the IBS test suite on the self-hosted FreeBSD runner.

## Overview

Entry point is `.github/workflows/ibs-full-test.yml` (reusable workflow).
Callers in `caller-examples/` call it via `workflow_call`. The pipeline has
4 sequential stages, each a composite action under `actions/`:

1. `setup-freebsd-build` — collect system info (dmesg, sysctl, kernel config)
2. `build-kernel` — compile hwpmc.ko with a 600s timeout guard
3. `run-atf-tests` — load hwpmc.ko, run kyua, emit JUnit XML + HTML
4. `report-results` — parse XML, post Markdown summary, upload 6 artifact sets

Optional Jira integration is secrets-gated; no-op if secrets absent.

## Main Files

- `.github/workflows/ibs-full-test.yml` (112 LOC) — Reusable workflow: triggers, inputs, calls 4 composite actions, Jira login/comment/transition
- `actions/setup-freebsd-build/action.yml` — Composite: verify tools, export env vars, collect dmesg/sysctl/kernel-config artifact
- `actions/build-kernel/action.yml` — Composite: calls build.sh
- `actions/build-kernel/scripts/build.sh` (80 LOC) — `timeout 600 make` with error reporting
- `actions/run-atf-tests/action.yml` — Composite: calls build-tests.sh + run-tests.sh
- `actions/run-atf-tests/scripts/build-tests.sh` — Compile ATF test programs
- `actions/run-atf-tests/scripts/run-tests.sh` (99 LOC) — `kldload hwpmc.ko`, kyua test, `kyua report-junit`, `kldunload`, error handling
- `actions/report-results/action.yml` — Composite: calls generate-report.sh
- `actions/report-results/scripts/generate-report.sh` (85 LOC) — XML parse, Markdown GITHUB_STEP_SUMMARY, upload artifacts
- `caller-examples/call-ibs-full.yml` — Full test caller (on changes to IBS/hwpmc source)
- `caller-examples/call-smoke.yml` — Smoke test caller (on every push to main)

## Dependencies

- modules: [[ibs-test-suite]]
- infra: [[runner-setup]]
- apis: [[github-actions-api]], [[jira]]
- runner labels: `self-hosted`, `freebsd`, `amd-ibs`

## Decisions

## [DECISION] Reusable workflow + composite actions over monolithic job
**Date:** 2026-04-30
**Context:** Multiple repos (freebsd-src, future callers) need the same
pipeline. Jira must be optional. Maintenance should be centralized.
**Decision:** Single reusable workflow in `ojanerif/freebsd-ci-actions`.
Callers are thin 10-line YAML files. Jira steps gated on secrets presence.
**Discarded alternatives:** Monolithic per-repo workflow (duplication),
always-on Jira (blocks callers without secrets).
**Impact:** Any caller repo needs only a `workflow_call` reference.

## [DECISION] 30-day artifact retention on all uploads
**Date:** 2026-04-30
**Context:** Balance storage costs vs debugging window (typically one sprint).
**Decision:** `retention-days: 30` on every `actions/upload-artifact@v4`.
Six artifact sets per run: system-info, kernel-build-log (failure only),
kyua-junit-xml, kyua-html-report, ibs-junit-results, ibs-html-report.
**Impact:** github-actions-api

## [DECISION] 600s build timeout + 900s job ceiling
**Date:** 2026-04-30
**Context:** A hung kernel module build would block the single runner
indefinitely; full kernel would take 1–2 hours.
**Decision:** `timeout 600` wrapper in build.sh; job-level `timeout-minutes: 15`
in the workflow.
**Impact:** build-kernel action, run-atf-tests action.

## Known Bugs

## [BUG] Jira transition fires even on partial test failures
**Found:** 2026-04-30
**Symptom:** Jira story transitions to "Done" if overall workflow succeeds,
even if some test cases were skipped or some asserts soft-failed.
**Root cause:** Transition condition checks workflow success, not zero-failure
count in JUnit XML.
**Fix/Workaround:** Acceptable for now — skips are expected on non-Zen-4 hardware.
**Status:** open

## TODOs

## [TODO] Add KASAN CI job
**Priority:** low
**Context:** Run tests with `KERNCONF=GENERIC-KASAN` to catch memory safety
issues in hwpmc. Tracked in docs/TODO.md Phase 3.
**Status:** pending

## [TODO] Status badge wired to README
**Priority:** low
**Context:** Badge image URL should reflect latest run of call-ibs-full.yml.
Tracked in dev-docs/phases-plan.md Phase 1.
**Status:** pending

## [TODO] E2E green-path validation (Phase 2 gate)
**Priority:** high
**Context:** Blocked on runner registration. Target: ≥6 PASS, 1 SKIP on known-good
EPYC hardware. Deadline: 2026-05-09.
**Status:** pending

## Snippets

## [SNIPPET] Secrets-gated Jira step
**Use:** Any optional third-party integration in GitHub Actions.
```yaml
- name: Jira login
  if: ${{ secrets.JIRA_API_TOKEN != '' }}
  uses: atlassian/gajira-login@v3
  env:
    JIRA_BASE_URL: ${{ secrets.JIRA_BASE_URL }}
    JIRA_USER_EMAIL: ${{ secrets.JIRA_USER_EMAIL }}
    JIRA_API_TOKEN: ${{ secrets.JIRA_API_TOKEN }}
```

## [SNIPPET] Reusable workflow call pattern
**Use:** Any repo that wants to run the IBS test pipeline.
```yaml
jobs:
  ibs-tests:
    uses: ojanerif/freebsd-ci-actions/.github/workflows/ibs-full-test.yml@main
    secrets: inherit
    with:
      test-suite: full   # or: smoke
```

## Learning Log

2026-04-30 | First read. 2 decisions, 1 open bug (Jira transition granularity), 3 TODOs. Pipeline structure is clean; main blocker is runner registration. | ci-actions-workflows
