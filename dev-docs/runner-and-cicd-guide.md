# CI/CD on FreeBSD: GitHub Actions Self-Hosted Runner Guide
<!-- Author: ojanerif | Last updated: 2026-04-29 | v2 — Jira integration added -->

A complete reference for setting up the AMD IBS CI/CD pipeline on a FreeBSD
bare-metal EPYC host, from zero to a running self-hosted GitHub Actions runner.
Written for internal knowledge transfer and colleague onboarding.

---

## Table of Contents

1. [Architecture overview](#1-architecture-overview)
2. [Prerequisites](#2-prerequisites)
3. [Step-by-step installation](#3-step-by-step-installation)
4. [What the install script does](#4-what-the-install-script-does)
5. [Configuration files reference](#5-configuration-files-reference)
6. [How the CI/CD pipeline runs](#6-how-the-cicd-pipeline-runs)
7. [Jira integration](#7-jira-integration)
8. [Workflow diagram](#8-workflow-diagram)
9. [FAQ](#9-faq)

---

## 1. Architecture overview

```
GitHub (ojanerif account)
├── ojanerif/freebsd-src        — FreeBSD kernel fork with AMD IBS patches
│   └── .github/workflows/      — Caller workflow: triggers the CI pipeline
│
└── ojanerif/freebsd-ci-actions — Reusable CI actions & workflow library
    ├── workflows/
    │   └── ibs-full-test.yml   — Reusable workflow: build → install → test
    └── actions/
        ├── setup-freebsd-build/  — Collect system info, check build env
        ├── build-kernel/         — Build IBS-patched kernel (with timeout)
        ├── run-atf-tests/        — Run ATF/Kyua tests, emit JUnit XML
        └── report-results/       — Parse XML, post summary, upload artifacts

Bare-metal EPYC host (FreeBSD 16-CURRENT)
└── gh-runner user
    └── /home/gh-runner/actions-runner/
        └── run.sh              — Runner daemon (started by rc.d service)
```

**Flow**: A push to `ojanerif/freebsd-src` triggers the caller workflow, which
dispatches to the reusable workflow in `freebsd-ci-actions`. The job lands on
the self-hosted runner (`self-hosted,freebsd,amd-ibs` labels). The runner
builds the kernel, loads the IBS kernel module, runs ATF tests via Kyua,
and uploads JUnit XML + HTML reports as GitHub Actions artifacts.

**Why self-hosted?** AMD IBS requires MSR access (`/dev/cpuctl`) and kernel
module loading (`kldload`). GitHub-hosted runners are Linux VMs with no access
to hardware performance counters or FreeBSD kernel interfaces.

---

## 2. Prerequisites

| Item | Requirement |
|------|------------|
| Hardware | AMD EPYC (Zen 3 or Zen 4) — IBS requires AMD Family 10h+ |
| OS | FreeBSD 16.0-CURRENT, amd64 |
| Root access | The install script must run as root |
| Internet | Outbound HTTPS to github.com (runner registration + artifact upload) |
| GitHub token | Runner registration token from the repo settings page |
| Disk | ~3 GB free (linux_base-rl9 ≈ 700 MB, runner ≈ 200 MB, build artifacts ≈ 2 GB) |

### Getting the registration token

1. Go to: `https://github.com/ojanerif/freebsd-src/settings/actions/runners/new`
2. Select **Linux / x64**
3. Copy the `--token XXXXXXX` value from the curl download block
4. Tokens expire in **1 hour** — run the script promptly after copying

---

## 3. Step-by-step installation

### 3a. One-shot install (recommended)

```sh
# As root on the FreeBSD host:
sh scripts/install-runner-freebsd.sh \
    --repo  https://github.com/ojanerif/freebsd-src \
    --token <PASTE_TOKEN_HERE>
```

Optional overrides:

```sh
sh scripts/install-runner-freebsd.sh \
    --repo           https://github.com/ojanerif/freebsd-src \
    --token          XXXXXXXXX \
    --runner-version 2.334.0 \
    --runner-user    gh-runner \
    --runner-dir     /home/gh-runner/actions-runner
```

### 3b. Manual steps (for understanding or troubleshooting)

If you need to run each step by hand:

```sh
# 1. Enable Linuxulator
sysrc linux_enable="YES"
kldload linux
kldload linux64

# 2. Install packages
pkg install -y linux_base-rl9 linux-rl9-icu kyua atf llvm gmake git curl bash sudo

# 3. Mount Linux pseudo-filesystems
mount -t linprocfs linprocfs /compat/linux/proc
mount -t linsysfs  linsysfs  /compat/linux/sys
mount -t tmpfs tmpfs /compat/linux/dev/shm -o rw,mode=1777

# 4. /bin/bash symlink (run.sh has #!/bin/bash hardcoded)
ln -s /usr/local/bin/bash /bin/bash

# 5. Create runner user
pw useradd -n gh-runner -m -s /bin/sh -c "GitHub Actions Runner"

# 6. Nullfs bind (critical — see FAQ #3)
mkdir -p /usr/home/gh-runner
mount_nullfs /home/gh-runner /usr/home/gh-runner

# 7. Configure sudo
cat > /usr/local/etc/sudoers.d/gh-runner-kld << 'EOF'
gh-runner ALL=(root) NOPASSWD: /sbin/kldload, /sbin/kldunload
EOF
chmod 440 /usr/local/etc/sudoers.d/gh-runner-kld

# 7. Download and extract runner
mkdir -p /home/gh-runner/actions-runner
chown -R gh-runner /home/gh-runner/actions-runner
su -m gh-runner -c "curl -sSL https://github.com/actions/runner/releases/download/v2.334.0/actions-runner-linux-x64-2.334.0.tar.gz \
    -o /home/gh-runner/actions-runner/actions-runner-linux-x64-2.334.0.tar.gz"
su -m gh-runner -c "cd /home/gh-runner/actions-runner && tar xzf actions-runner-linux-x64-2.334.0.tar.gz"

# 8. Register
su -m gh-runner -c "/usr/local/bin/bash /home/gh-runner/actions-runner/config.sh \
    --url https://github.com/ojanerif/freebsd-src \
    --token <TOKEN> \
    --labels self-hosted,freebsd,amd-ibs \
    --name freebsd-amd-$(hostname) \
    --unattended"

# 9. Start
service gh_runner start
```

### 3c. Verify the runner is online

After registration:

```sh
service gh_runner status
tail -f /var/log/gh-runner.log
```

Then open: `https://github.com/ojanerif/freebsd-src/settings/actions/runners`  
The runner should appear as **Online** with labels `self-hosted`, `freebsd`, `amd-ibs`.

---

## 4. What the install script does

The script (`scripts/install-runner-freebsd.sh`) performs 9 steps:

### Step 1 — Enable Linuxulator

The GitHub Actions runner is a **Linux x86-64 binary** (a .NET 8 application).
FreeBSD runs Linux binaries via the **Linuxulator** — a kernel subsystem that
translates Linux system calls to FreeBSD equivalents. Two kernel modules are
loaded:

- `linux.ko` — 32-bit Linux compat (required by rl9 base package)
- `linux64.ko` — 64-bit Linux compat (runs the runner itself)

`sysrc linux_enable=YES` makes this survive reboots.

### Step 2 — Install packages

| Package | Why it's needed |
|---------|----------------|
| `linux_base-rl9` | Rocky Linux 9.7 userland: glibc, libstdc++, dynamic linker. Required for GLIBCXX ≥ 3.4.20 (runner crashes on CentOS 7 which only has 3.4.19). |
| `linux-rl9-icu` | ICU (Unicode library) for .NET globalization. Without it: exit 134 "Couldn't find a valid ICU package". |
| `kyua` | ATF test runner — executes `Kyuafile` test suites |
| `atf` | Automated Testing Framework libraries and headers |
| `llvm` | Compiler for the IBS kernel module and tests |
| `gmake` | GNU make (FreeBSD `make` is BSD make; Makefiles use GNU extensions) |
| `git` | Checkout in workflow steps |
| `curl` | Download runner archive |
| `bash` | config.sh and run.sh are bash scripts; FreeBSD ships `/bin/sh` (ash) |
| `sudo` | Allow `gh-runner` to kldload/kldunload without a password |

### Step 3 — Mount Linux pseudo-filesystems

| Mount | Purpose |
|-------|---------|
| `linprocfs → /compat/linux/proc` | Linux `/proc` — the runner reads `/proc/self/exe`, `/proc/meminfo`, etc. |
| `linsysfs → /compat/linux/sys` | Linux `/sys` — sysfs interface for device enumeration |
| `tmpfs → /compat/linux/dev/shm` | Shared memory for IPC |

These are also written to `/etc/fstab` for persistence.

### Step 4 — `/bin/bash` compatibility symlink

`run.sh` and `run-helper.sh` inside the runner archive have `#!/bin/bash`
shebangs. FreeBSD installs bash at `/usr/local/bin/bash`. Without a compat
symlink the runner starts, then immediately exits:

```
run-helper.sh: /bin/bash: bad interpreter: No such file or directory
Exiting runner...
```

Fix (one line, survives reboots because it's a filesystem symlink):
```sh
ln -s /usr/local/bin/bash /bin/bash
```

### Step 5 — Create runner user

A dedicated `gh-runner` user is created with `pw useradd`. Running the runner
as root would be a security risk and GitHub explicitly blocks `config.sh` from
running under root/sudo.

### Step 6 — Nullfs bind mount (the tricky part)

**Why this is needed** (full explanation in [FAQ #3](#faq-3-why-the-nullfs-bind-mount)):

On this machine `/home` is a ZFS dataset. FreeBSD's base filesystem has a
legacy symlink `/home → /usr/home`. When the Linuxulator resolves Linux
process paths, it follows this symlink — so it looks for `/usr/home/gh-runner`
rather than `/home/gh-runner`.

The fix: bind-mount `/home/gh-runner` at `/usr/home/gh-runner` using nullfs:

```sh
mount_nullfs /home/gh-runner /usr/home/gh-runner
```

This is also persisted in `/etc/fstab`.

### Step 7 — Sudo configuration

```
gh-runner ALL=(root) NOPASSWD: /sbin/kldload, /sbin/kldunload
```

The IBS test workflow needs to load/unload `hwpmc.ko` between test runs to
reset MSR state. This sudoers entry allows that without a password prompt.

### Step 8 — Download and verify runner

Runner v2.334.0 is downloaded from GitHub Releases and verified against its
SHA256 checksum before extraction.

### Step 9 — Register with GitHub

`config.sh` is the runner's registration script. It calls the GitHub API
(`POST /actions/runner-registration`) with the one-time token and stores
the runner credentials (OAuth token, runner ID) in `.credentials` and
`runner.json` inside the runner directory.

After registration, the runner shows up in the GitHub UI at:
`https://github.com/ojanerif/freebsd-src/settings/actions/runners`

### Step 10 — rc.d service

An rc.d script at `/usr/local/etc/rc.d/gh_runner` is installed. It:
- Starts `run.sh` as the `gh-runner` user via `su -l`
- Writes logs to `/var/log/gh-runner.log`
- Stores PID at `/var/run/gh_runner.pid`

Commands:
```sh
service gh_runner start
service gh_runner stop
service gh_runner status
```

---

## 5. Configuration files reference

### `/etc/fstab` additions

```
# Linuxulator pseudo-filesystems
linprocfs   /compat/linux/proc     linprocfs  rw           0  0
linsysfs    /compat/linux/sys      linsysfs   rw           0  0
tmpfs       /compat/linux/dev/shm  tmpfs      rw,mode=1777 0  0

# Nullfs bind: .NET realpath() fix (see dev-docs/runner-and-cicd-guide.md §4 Step 5)
/home/gh-runner  /usr/home/gh-runner  nullfs  rw  0  0
```

### `/usr/local/etc/sudoers.d/gh-runner-kld`

```
# GitHub Actions runner: allow kldload/kldunload for IBS testing
gh-runner ALL=(root) NOPASSWD: /sbin/kldload, /sbin/kldunload
```

### `/usr/local/etc/rc.d/gh_runner` (generated)

Auto-starts the runner at boot, writes logs to `/var/log/gh-runner.log`.

### `/home/gh-runner/actions-runner/runner.json`

Written by `config.sh` after registration. Contains runner name, labels,
repository URL, and the runner's internal ID. Do not edit manually.

### `/home/gh-runner/actions-runner/.credentials`

OAuth credentials written by `config.sh`. Keep secret. Not committed.

### `/home/gh-runner/actions-runner/.env`

Environment overrides that apply to every job. Can be used to set
`DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1` if ICU is not installed (fallback).

---

## 6. How the CI/CD pipeline runs

When a commit is pushed to `ojanerif/freebsd-src`:

```
Push event
  │
  ▼
.github/workflows/call-ibs-full.yml  (caller, in freebsd-src repo)
  │   uses: ojanerif/freebsd-ci-actions/.github/workflows/ibs-full-test.yml
  │
  ▼
ibs-full-test.yml  (reusable workflow, in freebsd-ci-actions repo)
  │   runs-on: [self-hosted, freebsd, amd-ibs]
  │   timeout-minutes: 30
  │
  ├── action: setup-freebsd-build
  │     • Collects dmesg, sysctl hw.model, kernel config
  │     • Uploads "system-info" artifact (retained 30 days)
  │
  ├── action: build-kernel
  │     • Runs: make buildkernel KERNCONF=AMD_IBS
  │     • Hard timeout: 600 seconds (exits 1 if exceeded)
  │     • Uploads "kernel-build" artifact on failure
  │
  ├── action: run-atf-tests
  │     • Loads hwpmc.ko via sudo kldload
  │     • Runs: kyua test --kyuafile tests/sys/amd/ibs/Kyuafile
  │     • Generates JUnit XML: kyua report-junit
  │     • Unloads hwpmc.ko
  │     • Uploads "test-results" artifact (JUnit XML + Kyua HTML, 30 days)
  │
  └── action: report-results
        • Parses JUnit XML
        • Posts pass/fail/skip summary to workflow run page
        • Uploads "test-summary" artifact (30 days)
```

### Expected test results (good commit)

| Test | Expected |
|------|----------|
| ibs_fetch_basic | PASS |
| ibs_op_basic | PASS |
| ibs_fetch_filter | PASS |
| ibs_op_filter | PASS |
| ibs_multiplex | PASS |
| ibs_stress | PASS |
| ibs_uncore | SKIP (not yet implemented) |

---

## 7. Jira integration

There are three levels of integration. They can be combined.

---

### 7a. Smart Commits (zero config)

Reference the Jira ticket key in any commit message. Jira picks it up
automatically once the GitHub-for-Jira app is connected to the repo.

```
git commit -m "SWLSVROS-6363 fix IBS fetch filter on Zen4"
```

Supported inline commands:

| Syntax | Effect |
|--------|--------|
| `TICKET-123 #comment text` | Posts a comment on the issue |
| `TICKET-123 #done` | Transitions issue to Done |
| `TICKET-123 #in-progress` | Transitions issue to In Progress |
| `TICKET-123 #time 2h 30m` | Logs work time |

**One-time setup:** Install the
[GitHub for Jira](https://marketplace.atlassian.com/apps/1219592/github-for-jira)
app from Atlassian Marketplace → connect `ojanerif/freebsd-src`. No workflow
changes needed.

---

### 7b. Workflow steps via Atlassian Actions

The CI workflow (`.github/workflows/ibs-full-test.yml` in
`ojanerif/freebsd-ci-actions`) includes optional Jira steps that run after the
test result is known. They are gated on secrets — if the secrets are not set,
the steps are skipped gracefully.

**Steps added to the pipeline:**

| Step | Trigger | Action |
|------|---------|--------|
| Jira login | always | Authenticates using API token |
| Comment on failure | `test_status == 'failed'` | Posts run URL + failure count to ticket |
| Transition on success | `test_status == 'passed'` | Moves Story 4.1 to Done |

**Secrets required** (set in `ojanerif/freebsd-src` → Settings → Secrets):

| Secret | Value |
|--------|-------|
| `JIRA_BASE_URL` | `https://amd.atlassian.net` |
| `JIRA_USER_EMAIL` | Your AMD Atlassian email |
| `JIRA_API_TOKEN` | Token from `id.atlassian.com/manage-profile/security/api-tokens` |

**Workflow snippet** (already in `ibs-full-test.yml`):

```yaml
- name: Jira login
  if: ${{ secrets.JIRA_API_TOKEN != '' }}
  uses: atlassian/gajira-login@v3
  env:
    JIRA_BASE_URL:   ${{ secrets.JIRA_BASE_URL }}
    JIRA_USER_EMAIL: ${{ secrets.JIRA_USER_EMAIL }}
    JIRA_API_TOKEN:  ${{ secrets.JIRA_API_TOKEN }}

- name: Comment on Jira on failure
  if: steps.test.outputs.test_status == 'failed' && secrets.JIRA_API_TOKEN != ''
  uses: atlassian/gajira-comment@v3
  with:
    issue: SWLSVROS-6363
    comment: |
      ❌ IBS CI failed on FreeBSD runner
      Run: ${{ github.server_url }}/${{ github.repository }}/actions/runs/${{ github.run_id }}

- name: Transition Jira story on success
  if: steps.test.outputs.test_status == 'passed' && secrets.JIRA_API_TOKEN != ''
  uses: atlassian/gajira-transition@v3
  with:
    issue: SWLSVROS-6363
    transition: "Done"
```

---

### 7c. Direct Jira REST API

For custom payloads (attach artifacts, bulk-update, structured comments):

```yaml
- name: Post detailed result to Jira
  if: always() && secrets.JIRA_API_TOKEN != ''
  shell: sh
  env:
    JIRA_TOKEN: ${{ secrets.JIRA_API_TOKEN }}
    JIRA_EMAIL: ${{ secrets.JIRA_USER_EMAIL }}
    TEST_STATUS: ${{ steps.test.outputs.test_status }}
    RUN_URL: ${{ github.server_url }}/${{ github.repository }}/actions/runs/${{ github.run_id }}
  run: |
    curl -s -X POST \
      -u "${JIRA_EMAIL}:${JIRA_TOKEN}" \
      -H "Content-Type: application/json" \
      "https://amd.atlassian.net/rest/api/3/issue/SWLSVROS-6363/comment" \
      -d "{\"body\":{\"type\":\"doc\",\"version\":1,\"content\":[{\"type\":\"paragraph\",\"content\":[{\"type\":\"text\",\"text\":\"CI ${TEST_STATUS}: ${RUN_URL}\"}]}]}}"
```

---

### 7d. Available Atlassian GitHub Actions

| Action | Purpose |
|--------|---------|
| `atlassian/gajira-login@v3` | Authenticate (required by all others) |
| `atlassian/gajira-comment@v3` | Post a comment on a ticket |
| `atlassian/gajira-transition@v3` | Move ticket through workflow states |
| `atlassian/gajira-create@v3` | Create a new issue |
| `atlassian/gajira-find-issue-key@v3` | Extract ticket key from branch/commit message |

---

## 8. Workflow diagram

```
ojanerif/freebsd-src                    ojanerif/freebsd-ci-actions
─────────────────────────               ───────────────────────────
push / workflow_dispatch
        │
        ▼
call-ibs-full.yml ──uses:──────────────► ibs-full-test.yml
                                                  │
                                          job: build-and-test
                                          runs-on: [self-hosted,
                                                    freebsd,
                                                    amd-ibs]
                                                  │
                              ┌───────────────────┤
                              │                   │
                    FreeBSD bare-metal             │
                    (EPYC Zen4 host)               │
                              │                   │
                    gh_runner (rc.d)               │
                              │ picks up job       │
                              ▼                   │
                    Runner.Listener ◄──────────────┘
                    (Linux ELF via Linuxulator)
                              │
                    ┌─────────┼──────────┐
                    ▼         ▼          ▼
               setup-    build-     run-atf-    report-
               freebsd-  kernel     tests       results
               build     (make)     (kyua)      (XML→MD)
                    │         │          │          │
                    └─────────┴──────────┴──────────┘
                                   │
                          GitHub Actions artifacts
                          (system-info, kernel-build,
                           test-results, test-summary)
                          retained 30 days
```

---

## 9. FAQ

### FAQ 1: Why FreeBSD instead of a Linux runner?

AMD IBS (Instruction-Based Sampling) requires:
- Writing MSRs via `/dev/cpuctl0` (FreeBSD-specific interface)
- Loading `hwpmc.ko` to expose the PMC framework to userland
- The IBS patches live in the FreeBSD kernel tree (`ojanerif/freebsd-src`)

There is no Linux port of the test suite. FreeBSD is the target platform.

### FAQ 2: Why not use a FreeBSD container or jail?

Jails share the host kernel. The tests need to `kldload` and `kldunload`
kernel modules — operations that require root in the global zone. Running the
runner in a jail would still need delegated `kldload` via the host, adding
complexity without benefit. Running on the bare-metal host directly is simpler
and gives full hardware access.

### FAQ 3: Why the nullfs bind mount?

**Root cause**: On this machine, `/home` is a ZFS dataset (`zroot/home`).
FreeBSD's base filesystem has a legacy symlink entry `/home → /usr/home`. The
Linuxulator's `readlink(2)` implementation follows that symlink, so Linux
processes see `/home` as a symlink to `/usr/home`.

The GitHub runner is a .NET 8 single-file bundle. On startup, the .NET AppHost
calls `realpath("/proc/self/exe")` to find its own location. `realpath`
resolves symlinks component-by-component:

```
/proc/self/exe → /home/gh-runner/actions-runner/bin/Runner.Listener
realpath:
  /home → /usr/home          (Linuxulator sees this as a symlink)
  /usr/home/gh-runner → ???  (ENOENT — doesn't exist in root FS!)
```

Result: `realpath` returns an empty string. .NET aborts with:

```
Failed to resolve full path of the current executable []
exit: 133
```

**Fix**: Create `/usr/home/gh-runner` as a nullfs bind of `/home/gh-runner`.
Now `realpath` walks `/usr/home/gh-runner/actions-runner/...` successfully
because the directory physically exists (as a nullfs view of the ZFS path).

### FAQ 4: Runner starts then immediately exits with "bad interpreter"

Symptom in `/var/log/gh-runner.log`:
```
run.sh: /home/gh-runner/actions-runner/run-helper.sh: /bin/bash: bad interpreter: No such file or directory
Exiting runner...
```

The scripts shipped inside the runner archive (`run.sh`, `run-helper.sh`) have
`#!/bin/bash` shebangs. FreeBSD installs bash at `/usr/local/bin/bash`.

Fix:
```sh
ln -s /usr/local/bin/bash /bin/bash
```

This is a filesystem symlink so it survives reboots. The install script handles
this automatically.

### FAQ 6: Why Rocky Linux 9, not CentOS 7?

The runner binary requires `GLIBCXX_3.4.20` and `GLIBCXX_3.4.21` from
`libstdc++`. CentOS 7 (linux_base-c7) ships GCC 4.8, which only provides up
to `GLIBCXX_3.4.19`. Rocky Linux 9 ships GCC 11 (`GLIBCXX_3.4.29`). The
runner crashes at load time on CentOS 7.

### FAQ 7: What if the runner token expires?

Registration tokens are valid for **60 minutes** from the time they appear on
the GitHub settings page. If the token expires before `config.sh` runs, you
will see:

```
Http response code: NotFound from 'POST .../runner-registration'
Response status code does not indicate success: 404 (Not Found)
```

Generate a fresh token at:
`https://github.com/ojanerif/freebsd-src/settings/actions/runners/new`

Then re-run only the registration step:

```sh
sudo -u gh-runner /usr/local/bin/bash /home/gh-runner/actions-runner/config.sh \
    --url   https://github.com/ojanerif/freebsd-src \
    --token <NEW_TOKEN> \
    --labels self-hosted,freebsd,amd-ibs \
    --name  freebsd-amd-$(hostname) \
    --replace \
    --unattended
```

### FAQ 8: How do I check if the runner is working?

```sh
# Service status
service gh_runner status

# Live log
tail -f /var/log/gh-runner.log

# Trigger a manual run
# Go to: Actions tab in ojanerif/freebsd-src → select workflow → Run workflow
```

### FAQ 9: The runner shows Offline in GitHub — how to fix?

1. Check if the process is alive: `service gh_runner status`
2. If dead, check the log: `tail -50 /var/log/gh-runner.log`
3. Common causes:
   - Linuxulator mounts lost after reboot → `mount -a` or verify `/etc/fstab`
   - Nullfs bind lost → `mount_nullfs /home/gh-runner /usr/home/gh-runner`
   - Runner updated itself (auto-update) and the new binary has an issue
4. Restart: `service gh_runner start`

### FAQ 10: How do I add more test cases?

Test files live in `tests/sys/amd/ibs/` in the root repo (`freebsd-ci-actions`).
Each test is a `.c` file implementing the ATF C API. After adding:

1. Add the test to `Kyuafile`
2. Add a build rule in `Makefile`
3. Push — the CI picks it up automatically on the next run

### FAQ 11: Can multiple jobs run concurrently?

No. The runner is configured as a single-instance runner. A second job queues
and waits until the first finishes. This is intentional — concurrent IBS test
jobs would conflict on PMC MSRs and produce unreliable results.

GitHub Actions self-hosted runners run one job at a time by default. If
multiple runners were registered on the same host, they would compete. Keep
exactly one runner per EPYC host.

### FAQ 13: CI steps fail with `/dev/null: Permission denied`

Symptom in GitHub Actions job log:
```
/tmp/runner-script.sh: line N: /dev/null: Permission denied
```

The Linuxulator needs a real `devfs` mounted at `/compat/linux/dev/`. Without
it, `/compat/linux/dev/null` is either missing or a regular file (not a char
device), so any `2>/dev/null` redirect in Linux-context scripts fails.

Fix (one-time, persists across reboots via fstab):
```sh
mount -t devfs devfs /compat/linux/dev
# Persist:
echo 'devfs  /compat/linux/dev  devfs  rw  0  0' >> /etc/fstab
```

The install script handles this automatically (step 3).

---

### FAQ 14: How do I set up Jira integration?

See [§7 Jira integration](#7-jira-integration) for the full guide. Quick path:

1. Install the [GitHub for Jira](https://marketplace.atlassian.com/apps/1219592/github-for-jira) app
2. Add three secrets to `ojanerif/freebsd-src`: `JIRA_BASE_URL`, `JIRA_USER_EMAIL`, `JIRA_API_TOKEN`
3. The CI workflow already has the Jira steps — they activate automatically once
   the secrets are present

From then on, commit messages like `SWLSVROS-6363 #done` also transition tickets.

---

### FAQ 12: How do I re-register the runner after a reinstall?

```sh
# Remove old registration
sudo -u gh-runner /usr/local/bin/bash /home/gh-runner/actions-runner/config.sh remove \
    --token <REMOVE_TOKEN>

# Re-register with a fresh token
sudo -u gh-runner /usr/local/bin/bash /home/gh-runner/actions-runner/config.sh \
    --url https://github.com/ojanerif/freebsd-src \
    --token <NEW_TOKEN> \
    --labels self-hosted,freebsd,amd-ibs \
    --name freebsd-amd-$(hostname) \
    --replace \
    --unattended
```

Remove tokens are obtained from the runner's "..." menu on the GitHub runners
settings page.
