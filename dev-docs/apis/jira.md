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
**Author:** Osvaldo J. Filho
**Actor type:** human
**Source:** ai-prompt
**Session:** sess_2026-04-30_0000
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

## AMD Jira REST API — Field Reference (learned 2026-06-11)

All direct Jira REST API work uses `/rest/api/3/search/jql` (POST) and `/rest/api/3/issue/<KEY>` (GET/PUT).

### Critical: shell quoting with curl

Always write JSON to a temp file and use `curl -d @file`. Shell interpolation
inside `-d '...'` breaks with complex JQL. Pattern:

```python
import tempfile, json, subprocess, os
with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
    json.dump(payload, f); tmp = f.name
res = subprocess.run(['curl','-s','-u','USER:TOKEN','-H','Accept: application/json',
    '-H','Content-Type: application/json','-X','POST','URL','-d','@'+tmp],
    capture_output=True, text=True)
os.unlink(tmp)
```

### Pagination

`/rest/api/3/search/jql` uses **cursor-based pagination** (`nextPageToken`), not offset.
Loop until `nextPageToken` is absent or batch is empty.

### Custom field IDs

| Field | ID | Format |
|-------|----|--------|
| Start date | `customfield_10022` | `YYYY-MM-DD` |
| End date | `customfield_10023` | `YYYY-MM-DD` |
| Target Start | `customfield_10257` | datetime — send `T12:00:00.000+0000` |
| Resolve By Target | `customfield_10217` | datetime — send `T12:00:00.000+0000` |
| Implemented Date | `customfield_10183` | datetime — send `T12:00:00.000+0000` |
| Analyzed Date | `customfield_10220` | datetime — send `T12:00:00.000+0000` |
| Issue Resolution (exec) | `customfield_10171` | option `{id}` — Done=`30835`, Fixed=`30834`-ish |
| Epic Link | `customfield_10014` | issue key string |

**Datetime timezone gotcha:** Always use `T12:00:00.000+0000` for datetime fields.
`T00:00:00.000+0000` shifts the stored date back by 1 day (server is UTC-5).

### AMD priorities

`P1 (Gating)`, `P2 (Must Solve)`, `P3 (Solution Desired)`, `P4 (No Impact/Notify)`, `Undefined`.
Use `{"priority": {"name": "P3 (Solution Desired)"}"` — not `"Minor"` (invalid).

### Exec status (`customfield_10171`)

Closure-only field. No "In Progress" option exists. Valid values: `Done`, `Fixed`,
`Completed`, `Cancelled`, `Duplicate`, `Cannot Reproduce`. Set `{"id": "30835"}` for Done.

### Transition: Move to Implemented

Transition id=71 on SWLSVROS tickets. Requires **Schedule Issues** Jira permission.
If blocked with 400/permission error → use Jira UI or ask project admin.

### Compliance checklist (SWLSVROS tickets)

- start date = target start date
- end date = target end date (resolve by target)
- Priority ≠ Undefined
- Analyzed Date set on Analyzed/Implemented/Closed
- Implemented Date set on Implemented/Closed
- Exec Resolution set on Closed tickets
- Weekly comment on every open ticket

## Learning Log

2026-04-30 | First read. Integration is implemented but secrets not yet set. One open bug (transition granularity). Low risk — fully no-op without secrets. | jira
2026-06-11 | Full AMD Jira REST API field reference added. Learned: datetime tz offset bug (use T12:00), AMD priority names, exec status options, pagination pattern, transition permission requirements, compliance rules for SWLSVROS tickets. | ojanerif
