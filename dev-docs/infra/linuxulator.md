---
module: linuxulator
type: infra
status: active
stack: FreeBSD kernel, sh
last_modified: 2026-04-30
related: [[[runner-setup]], [[ci-actions-workflows]]]
tags: [freebsd-ci-actions, infra, linuxulator, linux-compat]
---

# Linuxulator
> FreeBSD Linuxulator layer that runs the Linux GitHub Actions runner binary — kernel module loading, pseudo-filesystem mounts, and four known quirks with fixes.

## Overview

The GitHub Actions runner is a Linux x86-64 ELF binary (.NET runtime).
FreeBSD runs it via Linuxulator (Linux syscall translation layer). This
requires kernel modules, pseudo-filesystem mounts, and three workarounds
for .NET and runner quirks. All fixes are documented in `ci-infra-fixes.patch`
and `dev-docs/runner-and-cicd-guide.md`.

## Main Files

- `scripts/install-runner-freebsd.sh` — Steps 1–4 configure Linuxulator
- `dev-docs/runner-and-cicd-guide.md` (740 LOC) — Deep dive + 14 FAQs on quirks
- `ci-infra-fixes.patch` — Patch capturing the three Linuxulator fixes

## Setup Sequence

```sh
# Load kernel modules
kldload linux.ko
kldload linux64.ko

# Mount pseudo-filesystems
mount -t linprocfs linprocfs /compat/linux/proc
mount -t linsysfs  linsysfs  /compat/linux/sys
mount -t devfs     devfs     /compat/linux/dev
mount -t tmpfs     tmpfs     /compat/linux/dev/shm

# Persist in /etc/fstab and /boot/loader.conf
```

## Dependencies

- infra: [[runner-setup]]

## Decisions

## [DECISION] Rocky Linux 9 (rl9) over CentOS 7
**Date:** 2026-04-30
**Author:** Osvaldo J. Filho
**Actor type:** human
**Source:** ai-prompt
**Session:** sess_2026-04-30_0000
**Context:** .NET runtime needs GLIBCXX 3.4.20+. Default `linux_base-c7`
(CentOS 7) ships only 3.4.19; runner binary crashes on startup.
**Decision:** `pkg install linux_base-rl9 linux-rl9-icu`. Rocky Linux 9 meets
all .NET and runner binary requirements.
**Discarded alternatives:** CentOS 7 (`linux_base-c7`) — GLIBCXX too old.
**Impact:** runner-setup step 2.

## Known Bugs

## [BUG] .NET realpath() resolves /home as /usr/home symlink
**Found:** 2026-04-30
**Symptom:** Runner binary fails to find its own working directory.
.NET's `realpath()` follows the `/home → /usr/home` FreeBSD symlink,
so it looks for files at `/usr/home/gh-runner` while they live at `/home/gh-runner`.
**Root cause:** FreeBSD ships `/home` as a symlink to `/usr/home`. Linux
programs using realpath() get the resolved path; their file I/O then fails.
**Fix/Workaround:** Nullfs bind-mount: `mount -t nullfs /home/gh-runner /usr/home/gh-runner`.
Persist in `/etc/fstab`:
```
/home/gh-runner   /usr/home/gh-runner   nullfs   rw   0   0
```
**Status:** workaround (upstream .NET won't fix; FreeBSD symlink is intentional)

## [BUG] Runner scripts use #!/bin/bash shebang, FreeBSD has none there
**Found:** 2026-04-30
**Symptom:** Runner startup scripts fail immediately: `/bin/bash: not found`.
**Root cause:** FreeBSD ships bash at `/usr/local/bin/bash`; runner scripts
have `#!/bin/bash` hardcoded (Linux convention).
**Fix/Workaround:** `ln -sf /usr/local/bin/bash /bin/bash`.
Must re-apply after pkg upgrades that refresh base system symlinks.
**Status:** workaround

## [BUG] .NET crashes without linux-rl9-icu package
**Found:** 2026-04-30
**Symptom:** Runner startup fails with ICU data not found / globalization exception.
**Root cause:** .NET runtime requires ICU (International Components for Unicode)
for globalization. Not included in `linux_base-rl9` base.
**Fix/Workaround:** `pkg install linux-rl9-icu`. Must be installed alongside
`linux_base-rl9`.
**Status:** resolved (package install)

## [BUG] GLIBCXX 3.4.19 on CentOS 7 too old for runner binary
**Found:** 2026-04-30
**Symptom:** Runner binary exits with "version GLIBCXX_3.4.20 not found".
**Root cause:** `linux_base-c7` (CentOS 7) ships GLIBCXX 3.4.19; runner
requires 3.4.20+.
**Fix/Workaround:** Switch to `linux_base-rl9` (Rocky Linux 9).
**Status:** resolved (switch to rl9)

## TODOs

## [TODO] Automate /bin/bash symlink check post-pkg-upgrade
**Priority:** low
**Context:** `pkg upgrade` occasionally removes the `/bin/bash` symlink.
Should be checked in runner health-check script or rc.d service start hook.
**Status:** pending

## Snippets

## [SNIPPET] Full Linuxulator bootstrap (one block)
**Use:** Fresh FreeBSD host setup for GitHub Actions runner.
```sh
# Packages
pkg install -y linux_base-rl9 linux-rl9-icu bash

# Kernel modules (persist in loader.conf)
echo 'linux_load="YES"'   >> /boot/loader.conf
echo 'linux64_load="YES"' >> /boot/loader.conf
kldload linux.ko linux64.ko 2>/dev/null || true

# Pseudo-filesystems (persist in /etc/fstab)
cat >> /etc/fstab <<'EOF'
linprocfs  /compat/linux/proc  linprocfs  rw  0  0
linsysfs   /compat/linux/sys   linsysfs   rw  0  0
devfs      /compat/linux/dev   devfs      rw  0  0
tmpfs      /compat/linux/dev/shm  tmpfs   rw  0  0
/home/gh-runner  /usr/home/gh-runner  nullfs  rw  0  0
EOF
mount -al

# Bash shebang fix
ln -sf /usr/local/bin/bash /bin/bash
```

## Learning Log

2026-04-30 | First read. 4 bugs documented — all are workarounds for FreeBSD/Linux ABI mismatches. Rocky Linux 9 is the correct base. Nullfs bind is critical and easy to forget. | linuxulator
