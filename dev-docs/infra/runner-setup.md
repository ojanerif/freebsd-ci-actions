---
module: runner-setup
type: infra
status: active
stack: sh, FreeBSD rc.d
last_modified: 2026-04-30
related: [[[linuxulator]], [[ci-actions-workflows]], [[github-actions-api]]]
tags: [freebsd-ci-actions, infra, runner, self-hosted]
---

# Runner Setup
> Automated 10-step FreeBSD self-hosted GitHub Actions runner installation — Linuxulator, gh-runner user, nullfs bind, sudo, rc.d service.

## Overview

`scripts/install-runner-freebsd.sh` (300+ LOC) installs and configures a
GitHub Actions self-hosted runner on a bare-metal FreeBSD AMD EPYC host.
The runner binary is a Linux ELF; it executes via Linuxulator.
Runner labels: `self-hosted`, `freebsd`, `amd-ibs`.
Concurrency: single job at a time (self-hosted default).

## Main Files

- `scripts/install-runner-freebsd.sh` — Full 10-step install script
- `dev-docs/runner-and-cicd-guide.md` (740 LOC) — Comprehensive setup guide + 14 FAQs

## 10 Installation Steps

1. Enable Linuxulator (`kldload linux.ko linux64.ko`, add to `/boot/loader.conf`)
2. Install packages: `linux_base-rl9 linux-rl9-icu kyua atf gmake git bash sudo`
3. Mount Linux pseudo-filesystems (`linprocfs`, `linsysfs`, `devfs`, `tmpfs`)
4. Create `/bin/bash` symlink → `/usr/local/bin/bash`
5. Create `gh-runner` user (unprivileged)
6. Nullfs bind-mount `/home/gh-runner` → `/usr/home/gh-runner` (persist via `/etc/fstab`)
7. Configure sudo: `gh-runner ALL=(root) NOPASSWD: /sbin/kldload, /sbin/kldunload`
8. Download runner binary (`actions-runner-linux-x64-2.334.0.tar.gz`) + SHA256 verify
9. Register with GitHub: `./config.sh --url ... --token <TOKEN> --labels ...`
10. Install rc.d service `/usr/local/etc/rc.d/gh_runner` for auto-start

## Dependencies

- infra: [[linuxulator]]
- apis: [[github-actions-api]]

## Decisions

## [DECISION] Bare-metal EPYC, not VM or container
**Date:** 2026-04-30
**Context:** IBS requires writing to AMD MSRs via `/dev/cpuctl` and loading
`hwpmc.ko`. VMs restrict MSR access; containers share the host kernel.
**Decision:** Bare-metal AMD EPYC Zen 4 host. Runner user has sudo for
`kldload`/`kldunload` only — minimal privilege expansion.
**Discarded alternatives:** FreeBSD jail (still needs delegated kldload on host),
GitHub-hosted Linux (no FreeBSD kernel or AMD hardware).
**Impact:** linuxulator, github-actions-api

## [DECISION] Single concurrent job (--max-parallel-jobs 1)
**Date:** 2026-04-30
**Context:** MSR writes are per-CPU global state; concurrent jobs would race
on MSR values and corrupt each other's test results.
**Decision:** `--max-parallel-jobs 1` during `config.sh` registration.
**Impact:** All CI workflows queue behind a single job on this runner.

## [DECISION] rc.d service for runner auto-start
**Date:** 2026-04-30
**Context:** Runner must survive host reboots without manual intervention.
**Decision:** Custom `/usr/local/etc/rc.d/gh_runner` RC script.
Logs to `/var/log/gh-runner.log`. Uses `daemon(8)` for daemonization.
**Impact:** Runner comes up automatically on boot.

## Known Bugs

## [BUG] Registration token expires in 60 minutes
**Found:** 2026-04-30
**Symptom:** `config.sh` fails with "Token is not valid" if more than 60 min
elapse between token generation and script execution.
**Root cause:** GitHub Actions registration tokens have a 60-minute TTL by design.
**Fix/Workaround:** Generate token immediately before running install script.
Do not pre-generate tokens in advance.
**Status:** open (GitHub limitation)

## TODOs

## [TODO] Register EPYC runner (critical path blocker)
**Priority:** high
**Context:** All E2E CI validation is blocked until runner is online.
Fresh token needed (60-min expiry). Tracked in dev-docs/ci-status.md Story 1.
**Status:** pending

## [TODO] Disk space monitoring for runner host
**Priority:** low
**Context:** linux_base-rl9 (~700 MB) + runner (~200 MB) + build artifacts
(~2 GB) + 30-day CI artifacts. No automated cleanup today.
**Status:** pending

## Snippets

## [SNIPPET] Runner registration one-liner
**Use:** After all 10 setup steps are complete. TOKEN expires in 60 min.
```sh
cd /home/gh-runner/actions-runner
./config.sh \
  --url https://github.com/ojanerif/freebsd-src \
  --token <TOKEN> \
  --labels self-hosted,freebsd,amd-ibs \
  --name freebsd-epyc-01 \
  --unattended \
  --replace
```

## [SNIPPET] fstab entry for nullfs bind (survives reboot)
**Use:** Add to `/etc/fstab` after step 6.
```
/home/gh-runner   /usr/home/gh-runner   nullfs   rw   0   0
```

## Learning Log

2026-04-30 | First read. Critical blocker: runner not yet registered (token expiry). All E2E tests blocked. 3 decisions, 1 open bug, 2 TODOs. | runner-setup
