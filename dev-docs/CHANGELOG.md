---
scope: changelog
last_modified: 2026-05-21
---

# dev-docs CHANGELOG

## v3.4 — 2026-05-21
- Upgraded to dev-docs v3.4 Team Edition
- Added `dev-docs/team/` directory with users.jsonl, audit-log.jsonl, ownership.yaml, policies.yaml, locks.jsonl, reviews.jsonl, sessions/
- Added `dev-docs/repos.jsonl` repository topology registry
- Added `dev-docs/repo-overlays/` directory for subrepo overlays
- Created `dev-docs/index.jsonl` (was missing from previous bootstrap)
- Updated `AI-INSTRUCTIONS.md` with v3.4 full protocol: identity detection, audit logging, repo topology, v3.4 entry formats with Author/Actor type/Source/Session, status state machine, team locks, AI behavior rules
- Updated `CLAUDE.md` pointer to reference v3.4 rules
- Author: Osvaldo J. Filho | Actor type: human | Session: sess_2026-05-21_0000

## v3.3 — prior
- AI-first memory system: modules, apis, infra, global patterns
- Startup/shutdown protocol, entry formats (DECISION, BUG, TODO, SNIPPET)
- Append-only discipline, Obsidian-compatible Markdown
