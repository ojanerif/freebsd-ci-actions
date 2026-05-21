---
module: jira
type: api
status: active
stack: YAML (GitHub Actions), Atlassian gajira
last_modified: 2026-04-30
related: [[[ci-actions-workflows]]]
tags: [freebsd-ci-actions, api, jira, atlassian]
---

# Jira Integration
> Atlassian Jira integration via GitHub Actions gajira — conditionally comments on CI failure and transitions story SWLSVROS-6363 on success.

## Overview

Optional integration in `ibs-full-test.yml`. Three steps, all gated on
`JIRA_API_TOKEN` secret presence. If secrets are absent the steps no-op
and the workflow continues normally. Target story: SWLSVROS-6363 on
`amd.atlassian.net`.

Smart commits (commit message syntax) also link automatically via the
GitHub-for-Jira app installed on `ojanerif/freebsd-src`.

## Main Files

- `.github/workflows/ibs-full-test.yml` — Contains the 3 Jira steps (login, comment, transition)

## Secrets Required (in ojanerif/freebsd-src repo settings)

| Secret | Value |
|--------|-------|
| `JIRA_BASE_URL` | `https://amd.atlassian.net` |
| `JIRA_USER_EMAIL` | AMD Atlassian email |
| `JIRA_API_TOKEN` | From `id.atlassian.com/manage-profile/security/api-tokens` |

## Smart Commit Syntax

```
SWLSVROS-6363 #comment CI run passed — 8 PASS, 1 SKIP
SWLSVROS-6363 #done
SWLSVROS-6363 #in-progress
```

## Dependencies

- modules: [[ci-actions-workflows]]

## Decisions

## [DECISION] Secrets-gated Jira integration (optional, not required)
**Date:** 2026-04-30
**Context:** Not all caller repos will have Jira. Integration must not block
workflows in repos without Jira secrets configured.
**Decision:** All three Jira steps use `if: ${{ secrets.JIRA_API_TOKEN != '' }}`.
If secret is absent, steps are skipped silently.
**Impact:** ci-actions-workflows

## Known Bugs

## [BUG] Jira transition fires on workflow success regardless of failure count
**Found:** 2026-04-30
**Symptom:** Story transitions to "Done" even when test cases SKIPped.
**Root cause:** Transition condition is `job.status == 'success'`, not
"zero failed test cases in XML".
**Fix/Workaround:** Acceptable — SKIPs are expected on non-Zen-4 hardware.
Re-evaluate if false-Done transitions become a problem.
**Status:** open

## TODOs

## [TODO] Configure Jira secrets in ojanerif/freebsd-src repo
**Priority:** medium
**Context:** Jira integration is implemented but not yet activated.
Secrets need to be added to repo settings.
**Status:** pending

## Snippets

## [SNIPPET] Full 3-step Jira integration block
**Use:** Copy into any reusable workflow that needs Jira status updates.
```yaml
- name: Jira login
  if: ${{ secrets.JIRA_API_TOKEN != '' }}
  uses: atlassian/gajira-login@v3
  env:
    JIRA_BASE_URL: ${{ secrets.JIRA_BASE_URL }}
    JIRA_USER_EMAIL: ${{ secrets.JIRA_USER_EMAIL }}
    JIRA_API_TOKEN: ${{ secrets.JIRA_API_TOKEN }}

- name: Comment on failure
  if: ${{ failure() && secrets.JIRA_API_TOKEN != '' }}
  uses: atlassian/gajira-comment@v3
  with:
    issue: SWLSVROS-6363
    comment: "CI run failed: ${{ github.server_url }}/${{ github.repository }}/actions/runs/${{ github.run_id }}"

- name: Transition on success
  if: ${{ success() && secrets.JIRA_API_TOKEN != '' }}
  uses: atlassian/gajira-transition@v3
  with:
    issue: SWLSVROS-6363
    transition: Done
```

## Learning Log

2026-04-30 | First read. Integration is implemented but secrets not yet set. One open bug (transition granularity). Low risk — fully no-op without secrets. | jira
