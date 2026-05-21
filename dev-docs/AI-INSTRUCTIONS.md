# freebsd-ci-actions — AI Agent Instructions
# dev-docs v3.4 Team Edition

This file is the authoritative instruction set for any AI agent working on this
project. It is referenced by all agent-specific rule files. When in conflict,
this file takes precedence.

Version: dev-docs v3.4 (Team Edition)
Last modified: 2026-05-21

---

## 1. Identity Detection Protocol

Before making any save, edit, or append, the AI must identify the current actor.

1. Check `dev-docs/team/users.jsonl` for known users.
2. If operating as an AI agent, use actor_id `agent_claude` and actor_type `ai-agent`.
3. Record `requested_by` as the human user who issued the prompt (if known).
4. Every operation must carry: `actor_id`, `actor_name`, `actor_type`, `session`.
5. Generate a session id for the work session: `sess_<YYYY-MM-DD>_<HHMM>`.

Human identity (when a human is interacting directly):
- Display name: Osvaldo J. Filho
- actor_id: usr_osvaldo
- role: owner

---

## 2. Audit Logging Protocol

Append an event to `dev-docs/team/audit-log.jsonl` for every:
- Entry created or edited
- File saved or generated
- Decision status changed
- Review performed
- Conflict resolved
- AI-generated change

Event format (one JSON object per line):
```
{"event_id":"evt_<YYYYMMDD>_<NNN>","ts":"<ISO8601>","actor_id":"<id>","actor_name":"<name>","actor_type":"<type>","action":"<verb>.<noun>","target":"<file>#<entry-id>","source":"<source>","session":"<sess_id>","requested_by":"<user_id>","summary":"<one sentence>"}
```

Common action verbs: `entry.create`, `entry.edit`, `file.save`, `decision.status`,
`review.approve`, `conflict.resolve`, `bootstrap.start`, `bootstrap.complete`,
`user.onboarded`, `identity.confirmed`.

---

## 3. Memory System

This project uses a structured developer memory system in `dev-docs/`.
The system persists decisions, bugs, patterns, and learned knowledge across
sessions and across different AI agents.

### How to look up a module

`dev-docs/index.jsonl` is a newline-delimited JSON index.
Each line describes one module, api, or infra component.
Use grep to find the relevant entry, then read only that file.

Examples:
  grep "ibs"      dev-docs/index.jsonl
  grep "runner"   dev-docs/index.jsonl
  grep "jira"     dev-docs/index.jsonl
  grep "keyword"  dev-docs/index.jsonl

Each entry has this shape:
```json
{
  "id": "module-name",
  "type": "module | api | infra",
  "file": "dev-docs/modules/module-name.md",
  "keywords": ["dirname", "ClassName", "function_name"],
  "description": "one sentence",
  "status": "active | paused | deprecated",
  "last_modified": "YYYY-MM-DD"
}
```

### File to module mapping

| Path pattern or filename                | Module id              |
|-----------------------------------------|------------------------|
| tests/sys/amd/ibs/                      | ibs-test-suite         |
| ibs_utils.h, ibs_decode.h              | ibs-test-suite         |
| tests/sys/amd/pmc/                      | pmc-tests              |
| .github/workflows/, actions/           | ci-actions-workflows   |
| caller-examples/                        | ci-actions-workflows   |
| scripts/install-runner-freebsd.sh       | runner-setup           |
| ci/tools/ci.conf, ci/tools/freebsdci   | ci-vm-infra            |
| Linuxulator, linux.ko, nullfs mounts   | linuxulator            |
| GitHub API, upload-artifact, config.sh | github-actions         |
| JIRA_*, gajira, atlassian              | jira                   |
| file not listed above                   | grep index.jsonl first; if no match, create new module |

---

## 4. Startup Protocol

At the start of every task:
1. Read this file if you have not already done so this session.
2. Check `dev-docs/repos.jsonl` — confirm you are working in the root repo scope.
3. grep `dev-docs/index.jsonl` for the module you are working on.
4. Read that single module file — do not load other modules.
5. If the module file does not exist, create it using the New Module Template
   in `dev-docs/_global.md` before starting work.
6. Establish your session id: `sess_<YYYY-MM-DD>_<HHMM>`.
7. Begin the task.

---

## 5. Shutdown Protocol

Before ending any task:
1. Append new entries to the relevant module file using the v3.4 formats below.
2. Append one line to that module's Learning Log section.
3. Update `last_modified` in the module frontmatter to today.
4. If you created a new module, append its entry to `dev-docs/index.jsonl`.
5. If a pattern now appears in 3 or more modules, append it to
   `dev-docs/_global.md` under the appropriate section.
6. Append audit events to `dev-docs/team/audit-log.jsonl` for every change made.

---

## 6. Entry Formats (v3.4)

Use these formats when appending to any module file.
Always append — never edit or delete past entries.
Every entry must include: ID, Author, Actor type, Source, Session.

### Decision

```markdown
## [DECISION] [#xxxxxx] <title>
**Date:** YYYY-MM-DD
**Status:** PROPOSED | ACTIVE | BLOCKED | SUPERSEDED | INVALID
**Author:** <display name>
**Actor type:** human | ai-agent | migration | import | system
**Source:** dashboard | ai-prompt | code-scan | review | incident | meeting
**Session:** <sess_id>
**Reviewed by:** <optional>
**Approved by:** <optional, name on YYYY-MM-DD>
**Related commit:** <sha or pending>

**Context:** why this decision was needed
**Decision:** what was chosen and implemented
**Rejected alternatives:** what was considered and why rejected
**Consequences:** files or modules affected
```

### Bug

```markdown
## [BUG] [#xxxxxx] <short description>
**Found:** YYYY-MM-DD
**Status:** ACTIVE | RESOLVED | SUPERSEDED | INVALID
**Author:** <display name>
**Actor type:** human | ai-agent | migration | import | system
**Source:** dashboard | ai-prompt | code-scan | review | incident
**Session:** <sess_id>
**Owner:** <person/team>
**Reviewed by:** <optional>

**Symptom:** observable behavior
**Root cause:** underlying reason
**Fix:** what resolves it
**Files affected:** <paths>
**Related commit:** <sha or pending>
```

### TODO

```markdown
## [TODO] [#xxxxxx] <title>
**Created:** YYYY-MM-DD
**Status:** pending | in-progress | blocked | review | done
**Priority:** high | medium | low
**Author:** <display name>
**Actor type:** human | ai-agent
**Source:** dashboard | ai-prompt | code-comment | review
**Session:** <sess_id>
**Owner:** <person/team>
**Due:** <optional>

**Task:** what must be done
**Acceptance criteria:** how we know it is complete
**Files affected:** <paths>
```

### Snippet

```markdown
## [SNIPPET] [#xxxxxx] <pattern name>
**Author:** <display name>
**Actor type:** human | ai-agent
**Source:** <source>
**Session:** <sess_id>
**Use:** when to apply this pattern
\`\`\`lang
code here
\`\`\`
```

### Learning Log entry format

```
YYYY-MM-DD | observation about the module or system | <module-id> | <author>
```

---

## 7. Status State Machine

Entries progress through these statuses:

```
DRAFT → PROPOSED → ACTIVE → SUPERSEDED
                 → BLOCKED → ACTIVE
                 → INVALID
```

- AI agents may NOT move a decision from PROPOSED → ACTIVE without explicit human approval.
- AI agents may NOT move ACTIVE → INVALID or ACTIVE → SUPERSEDED autonomously.
- These restricted transitions must be logged and confirmed.

---

## 8. Repo Topology Protocol

1. Always load the root `dev-docs/AI-INSTRUCTIONS.md` first.
2. Read `dev-docs/repos.jsonl` to understand repo boundaries.
3. If editing a file that belongs to a subrepo overlay, load that overlay's
   instructions before making changes.
4. Do not edit submodule contents unless `write_policy` permits it.
5. Flag any unregistered nested `.git` directory found during work.

---

## 9. Team Locks and Conflict Handling

Before editing a file referenced in `dev-docs/team/locks.jsonl`:
1. Check if an active lock exists for the target file.
2. If locked by another actor, warn and do not proceed without confirmation.
3. If the lock is stale (older than `stale_lock_hours` in policies.yaml), note it
   and ask the human whether to take over.
4. Log all conflict resolutions to the audit log.

---

## 10. Project Stack

- **Languages:** POSIX sh, bash, C (ATF), YAML (GitHub Actions)
- **Build:** FreeBSD make, kyua (ATF test runner)
- **Frameworks:** ATF (Automated Test Framework), GitHub Actions
- **Platform:** FreeBSD 16.0-CURRENT amd64, AMD EPYC (Zen 2–5)
- **Hardware:** AMD IBS (Instruction-Based Sampling), hwpmc.ko, cpuctl.ko
- **CI:** GitHub Actions self-hosted runner via Linuxulator
- **Integration:** GitHub API, Atlassian Jira

---

## 11. Conventions

- All dev-docs content in English
- Obsidian-compatible: YAML frontmatter, [[wiki-links]], #tags
- Append only — never edit past entries, only add new dated ones
- Filenames: kebab-case, no accents or spaces
- index.jsonl: one valid JSON object per line, no surrounding array
- Every entry must carry Author, Actor type, Source, Session (v3.4 requirement)
- Audit log is append-only — never edit or delete past events

---

## 12. AI Agent Behavior Rules (from policies.yaml)

- may_append: true — agents may add new entries
- may_annotate: true — agents may add annotations to existing entries
- may_edit_existing: false — agents must NOT silently edit existing decisions/bugs
- must_log_requested_by: true — always record who requested the AI action
- Restricted status transitions require human confirmation before execution
