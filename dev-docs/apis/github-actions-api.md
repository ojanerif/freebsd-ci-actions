---
module: github-actions-api
type: api
status: active
stack: YAML, bash
last_modified: 2026-04-30
related: [[[ci-actions-workflows]], [[runner-setup]]]
tags: [freebsd-ci-actions, api, github]
---

# GitHub Actions API
> GitHub Actions API integration — runner registration token flow, artifact upload, step summary posting, and workflow trigger patterns.

## Overview

Three distinct integration points:
1. **Runner registration:** `POST /actions/runner-registration` via `config.sh`; token expires in 60 min
2. **Artifact upload:** `actions/upload-artifact@v4` — 6 artifact sets per run, 30-day retention
3. **Step summary:** `$GITHUB_STEP_SUMMARY` — Markdown report posted to workflow run page by `generate-report.sh`

## Main Files

- `actions/report-results/scripts/generate-report.sh` — Posts Markdown to `$GITHUB_STEP_SUMMARY`, calls `upload-artifact`
- `actions/run-atf-tests/scripts/run-tests.sh` — Uploads JUnit XML artifact
- `.github/workflows/ibs-full-test.yml` — Defines artifact upload steps with `retention-days: 30`
- `scripts/install-runner-freebsd.sh` — Step 8–9: download runner binary, call `config.sh`

## Artifact Sets (per run)

| Name | Contents | Condition |
|------|----------|-----------|
| system-info | dmesg, sysctl -a, kernel config | always |
| kernel-build-log | make output | on build failure only |
| kyua-junit-xml | ibs-results.xml | always |
| kyua-html-report | kyua-report/ dir | always |
| ibs-junit-results | consolidated XML | always |
| ibs-html-report | consolidated HTML | always |

## Dependencies

- modules: [[ci-actions-workflows]]
- infra: [[runner-setup]]

## Decisions

## [DECISION] upload-artifact@v4 with 30-day retention on all uploads
**Date:** 2026-04-30
**Author:** Osvaldo J. Filho
**Actor type:** human
**Source:** ai-prompt
**Session:** sess_2026-04-30_0000
**Context:** v3 was deprecated; v4 is required. Retention set uniformly to
match one sprint debugging window.
**Decision:** Pin to `actions/upload-artifact@v4`, `retention-days: 30` on
all 6 artifact upload calls.
**Impact:** ci-actions-workflows

## [DECISION] GITHUB_STEP_SUMMARY for Markdown test report
**Date:** 2026-04-30
**Author:** Osvaldo J. Filho
**Actor type:** human
**Source:** ai-prompt
**Session:** sess_2026-04-30_0000
**Context:** Results need to be visible without downloading artifacts.
**Decision:** `generate-report.sh` parses JUnit XML and appends a Markdown
table to `$GITHUB_STEP_SUMMARY`. Visible on the workflow run page immediately.
**Impact:** report-results action.

## Known Bugs

## [BUG] Registration token 60-minute TTL
**Found:** 2026-04-30
**Symptom:** `config.sh` fails if >60 min elapsed since token generation.
**Root cause:** GitHub design — registration tokens are short-lived.
**Fix/Workaround:** Generate token immediately before running install script.
**Status:** open (GitHub limitation, documented in runner-setup)

## TODOs

## [TODO] Wire status badge to README
**Priority:** low
**Context:** Badge URL from call-ibs-full.yml workflow run status.
Format: `https://github.com/ojanerif/freebsd-src/actions/workflows/call-ibs-full.yml/badge.svg`
**Status:** pending

## Snippets

## [SNIPPET] Markdown step summary with test table
**Use:** Posting a structured test result table to the GitHub Actions run page.
```bash
{
  echo "## IBS Test Results"
  echo "| Test | Result |"
  echo "|------|--------|"
  # parse ibs-results.xml with grep/awk and emit rows
} >> "$GITHUB_STEP_SUMMARY"
```

## Learning Log

2026-04-30 | First read. Clean integration — upload-artifact@v4, step summary, registration token. Main operational gotcha is the 60-min token TTL. | github-actions-api
