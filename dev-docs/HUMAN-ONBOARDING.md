---
scope: onboarding
last_modified: 2026-05-21
---

# Human Onboarding — dev-docs v3.4 Team Edition

Welcome to the freebsd-ci-actions project. This document explains the project
memory system so you can find information quickly and contribute correctly.

## What is dev-docs?

`dev-docs/` is a plain-Markdown knowledge base that AI agents and human
contributors use to share context across sessions. It records decisions, bugs,
TODOs, code patterns, and infrastructure notes.

## First steps

1. Tell any AI agent your display name before it saves changes on your behalf.
   It will record your identity in `dev-docs/team/users.jsonl`.
2. Read `dev-docs/AI-INSTRUCTIONS.md` to understand the protocol the AI follows.
3. Browse `dev-docs/index.jsonl` to see all modules.

## Finding information

```sh
# Find the module covering the topic you care about
grep "ibs"    dev-docs/index.jsonl
grep "runner" dev-docs/index.jsonl

# Then read that module file
# e.g.: dev-docs/modules/ibs-test-suite.md
```

## Contributing

- All entries are **append-only** — never edit or delete past entries.
- New entries must include: Date, Author, Actor type, Source, Session.
- Use the templates in `AI-INSTRUCTIONS.md` Section 6.
- Decisions that affect architecture, security, or infrastructure should go
  through the review workflow defined in `dev-docs/team/policies.yaml`.

## Key files

| File | Purpose |
|------|---------|
| `dev-docs/AI-INSTRUCTIONS.md` | Agent protocol — read this first |
| `dev-docs/index.jsonl` | Module index — grep to find modules |
| `dev-docs/_global.md` | Cross-cutting patterns and decisions |
| `dev-docs/team/users.jsonl` | Contributor identity registry |
| `dev-docs/team/audit-log.jsonl` | Append-only event log |
| `dev-docs/team/ownership.yaml` | Module owners and reviewers |
| `dev-docs/team/policies.yaml` | Review and collaboration policy |
| `dev-docs/repos.jsonl` | Repository topology registry |

## Project context

- Sponsored by AMD; tracked in Jira story SWLSVROS-6363
- Platform: FreeBSD 16.0-CURRENT, AMD EPYC bare metal
- CI: GitHub Actions self-hosted runner via Linuxulator
