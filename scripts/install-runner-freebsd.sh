#!/bin/sh
# Copyright (c) 2026 Advanced Micro Devices, Inc. All rights reserved.
# SPDX-License-Identifier: BSD-2-Clause
#
# Author: Osvaldo J. Filho <ojanerif@amd.com>
#
# install-runner-freebsd.sh — Automated GitHub Actions self-hosted runner
# setup on FreeBSD via Linuxulator.
#
# Usage:
#   sh install-runner-freebsd.sh \
#       --repo    https://github.com/ojanerif/freebsd-src \
#       --token   RUNNER_REGISTRATION_TOKEN \
#       [--runner-version 2.334.0] \
#       [--runner-user   gh-runner] \
#       [--runner-dir    /home/gh-runner/actions-runner]
#
# Must be run as root.
#
# FreeBSD Linuxulator notes (why this script does what it does):
#
# 1. linux_base-rl9: Rocky Linux 9 userland required because the GitHub
#    Actions runner binary needs GLIBCXX_3.4.20+. CentOS 7 (linux_base-c7)
#    only ships GLIBCXX_3.4.19 — runner crashes at startup.
#
# 2. linux-rl9-icu: .NET runtime requires the ICU (International Components
#    for Unicode) library.  Without it runner exits 134 with
#    "Couldn't find a valid ICU package".
#
# 3. Nullfs mount at /usr/home/<runner-user>: The Linuxulator's readlink(2)
#    resolves /home as a symlink to /usr/home (FreeBSD base-system legacy
#    symlink), but on this machine /home is a separate ZFS dataset.
#    Consequently .NET's realpath() walks /usr/home/<user> — which doesn't
#    exist — and returns an empty path, triggering exit 133 ("Failed to
#    resolve full path of the current executable []").
#    Fix: nullfs-bind /home/<user> → /usr/home/<user> so realpath succeeds.
#    The bind is persisted in /etc/fstab.

set -eu

# ---------------------------------------------------------------------------
# Defaults
# ---------------------------------------------------------------------------
RUNNER_VERSION="2.334.0"
RUNNER_USER="gh-runner"
RUNNER_DIR="/home/gh-runner/actions-runner"
REPO_URL=""
REG_TOKEN=""
RUNNER_LABELS="self-hosted,freebsd,amd-ibs"

# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------
while [ $# -gt 0 ]; do
	case "$1" in
		--repo)            REPO_URL="$2";      shift 2 ;;
		--token)           REG_TOKEN="$2";     shift 2 ;;
		--runner-version)  RUNNER_VERSION="$2"; shift 2 ;;
		--runner-user)     RUNNER_USER="$2";   shift 2 ;;
		--runner-dir)      RUNNER_DIR="$2";    shift 2 ;;
		*)
			printf 'Unknown argument: %s\n' "$1" >&2
			exit 1
			;;
	esac
done

log_info() { printf '[install-runner] INFO:  %s\n' "$*"; }
log_err()  { printf '[install-runner] ERROR: %s\n' "$*" >&2; }
die()  { log_err "$*"; exit 1; }

# ---------------------------------------------------------------------------
# Validate
# ---------------------------------------------------------------------------
[ "$(id -u)" -eq 0 ] || die "This script must be run as root."
[ -n "$REPO_URL" ]   || die "--repo is required."
[ -n "$REG_TOKEN" ]  || die "--token is required."

log_info "Repository:     $REPO_URL"
log_info "Runner version: $RUNNER_VERSION"
log_info "Runner user:    $RUNNER_USER"
log_info "Runner dir:     $RUNNER_DIR"

# ---------------------------------------------------------------------------
# 1. Enable Linuxulator
# ---------------------------------------------------------------------------
log_info "Enabling Linuxulator..."
sysrc linux_enable="YES"
if ! kldstat -n linux 2>/dev/null; then
	kldload linux || die "Failed to load linux.ko"
fi
if ! kldstat -n linux64 2>/dev/null; then
	kldload linux64 2>/dev/null || true
fi

# ---------------------------------------------------------------------------
# 2. Install packages
# ---------------------------------------------------------------------------
log_info "Installing required packages..."
pkg install -y \
	linux_base-rl9 \
	linux-rl9-icu \
	kyua \
	atf \
	llvm \
	gmake \
	git \
	curl \
	bash \
	sudo

# ---------------------------------------------------------------------------
# 3. Mount Linux pseudo-filesystems (idempotent)
# ---------------------------------------------------------------------------
log_info "Mounting Linuxulator filesystems..."

mount_if_needed() {
	local mnt="$1" fstype="$2" opts="${3:-rw}"
	if ! mount | grep -q " $mnt "; then
		mount -t "$fstype" "$fstype" "$mnt" -o "$opts"
	fi
}

mkdir -p /compat/linux/proc /compat/linux/sys /compat/linux/dev /compat/linux/dev/shm
mount_if_needed /compat/linux/proc linprocfs
mount_if_needed /compat/linux/sys  linsysfs
mount_if_needed /compat/linux/dev  devfs
mount_if_needed /compat/linux/dev/shm tmpfs "rw,mode=1777"

# Persist in fstab if not already present
add_fstab() {
	local line="$1"
	grep -qF "$line" /etc/fstab || printf '%s\n' "$line" >> /etc/fstab
}
add_fstab "linprocfs   /compat/linux/proc   linprocfs   rw   0   0"
add_fstab "linsysfs    /compat/linux/sys    linsysfs    rw   0   0"
add_fstab "devfs       /compat/linux/dev    devfs        rw   0   0"
add_fstab "tmpfs       /compat/linux/dev/shm  tmpfs     rw,mode=1777  0  0"

# ---------------------------------------------------------------------------
# 4. Create /bin/bash compatibility symlink
#
# run.sh and run-helper.sh inside the runner archive have #!/bin/bash shebangs.
# FreeBSD installs bash at /usr/local/bin/bash. Without this symlink the
# runner starts but immediately exits:
#   run-helper.sh: /bin/bash: bad interpreter: No such file or directory
# ---------------------------------------------------------------------------
if [ ! -e /bin/bash ]; then
	log_info "Creating /bin/bash → /usr/local/bin/bash symlink"
	ln -s /usr/local/bin/bash /bin/bash
fi

# ---------------------------------------------------------------------------
# 5. Create runner user
# ---------------------------------------------------------------------------
if ! id "$RUNNER_USER" > /dev/null 2>&1; then
	log_info "Creating user: $RUNNER_USER"
	pw useradd -n "$RUNNER_USER" -m -s /bin/sh \
		-c "GitHub Actions Runner"
else
	log_info "User $RUNNER_USER already exists."
fi

USER_HOME="$(getent passwd "$RUNNER_USER" | cut -d: -f6)"

# ---------------------------------------------------------------------------
# 6. Nullfs bind: /home/<user> → /usr/home/<user>
#
# The Linuxulator follows the base-system symlink /home → /usr/home when
# resolving process paths. Because /home is a ZFS dataset here (not the
# symlink), /usr/home/<user> is empty. The nullfs bind makes it visible
# so that .NET's realpath() succeeds.
# ---------------------------------------------------------------------------
NULLFS_TARGET="/usr/home/${RUNNER_USER}"
log_info "Creating nullfs bind: $USER_HOME → $NULLFS_TARGET"
mkdir -p "$NULLFS_TARGET"
if ! mount | grep -q " $NULLFS_TARGET "; then
	mount_nullfs "$USER_HOME" "$NULLFS_TARGET"
fi
FSTAB_NULLFS="${USER_HOME}  ${NULLFS_TARGET}  nullfs  rw  0  0"
grep -qF "$FSTAB_NULLFS" /etc/fstab || printf '%s\n' "$FSTAB_NULLFS" >> /etc/fstab

# ---------------------------------------------------------------------------
# 6. Configure passwordless sudo for kldload/kldunload
# ---------------------------------------------------------------------------
SUDOERS_FILE="/usr/local/etc/sudoers.d/gh-runner-kld"
log_info "Writing sudoers: $SUDOERS_FILE"
cat > "$SUDOERS_FILE" << EOF
# GitHub Actions runner: allow kldload/kldunload for IBS testing
$RUNNER_USER ALL=(root) NOPASSWD: /sbin/kldload, /sbin/kldunload
EOF
chmod 440 "$SUDOERS_FILE"

# ---------------------------------------------------------------------------
# 7. Download runner archive
# ---------------------------------------------------------------------------
RUNNER_ARCHIVE="actions-runner-linux-x64-${RUNNER_VERSION}.tar.gz"
RUNNER_URL="https://github.com/actions/runner/releases/download/v${RUNNER_VERSION}/${RUNNER_ARCHIVE}"

log_info "Downloading runner $RUNNER_VERSION..."
mkdir -p "$RUNNER_DIR"
chown -R "$RUNNER_USER" "$RUNNER_DIR"

if [ ! -f "${RUNNER_DIR}/${RUNNER_ARCHIVE}" ]; then
	su -m "$RUNNER_USER" -c \
		"curl -sSL '$RUNNER_URL' -o '${RUNNER_DIR}/${RUNNER_ARCHIVE}'"
fi

log_info "Verifying checksum..."
RUNNER_SHA256="048024cd2c848eb6f14d5646d56c13a4def2ae7ee3ad12122bee960c56f3d271"
echo "${RUNNER_SHA256}  ${RUNNER_DIR}/${RUNNER_ARCHIVE}" | sha256sum -c - \
    || die "Checksum verification failed"

log_info "Extracting runner..."
su -m "$RUNNER_USER" -c \
	"cd '$RUNNER_DIR' && tar xzf '${RUNNER_ARCHIVE}'"

# ---------------------------------------------------------------------------
# 8. Register runner with GitHub
# ---------------------------------------------------------------------------
RUNNER_NAME="freebsd-amd-$(hostname)"
log_info "Registering runner: $RUNNER_NAME"

# config.sh is a bash script; use the Linuxulator bash to execute it
# (FreeBSD sh cannot run the runner's bash scripts directly)
su -m "$RUNNER_USER" -c \
	"/usr/local/bin/bash '${RUNNER_DIR}/config.sh' \
		--url '$REPO_URL' \
		--token '$REG_TOKEN' \
		--labels '$RUNNER_LABELS' \
		--name '$RUNNER_NAME' \
		--unattended"

# ---------------------------------------------------------------------------
# 9. Install rc.d service
#
# Uses daemon(8) to daemonize run.sh with PID tracking as root (gh-runner
# user cannot write to /var/run or /var/log directly).
# ---------------------------------------------------------------------------
RC_SCRIPT="/usr/local/etc/rc.d/gh_runner"
log_info "Installing rc.d service: $RC_SCRIPT"

touch /var/log/gh-runner.log
chown "$RUNNER_USER" /var/log/gh-runner.log

cat > "$RC_SCRIPT" << RCEOF
#!/bin/sh
# PROVIDE: gh_runner
# REQUIRE: NETWORKING
# KEYWORD: shutdown

. /etc/rc.subr

name="gh_runner"
rcvar="gh_runner_enable"
gh_runner_user="${RUNNER_USER}"
gh_runner_dir="${RUNNER_DIR}"
pidfile="/var/run/gh_runner.pid"
logfile="/var/log/gh-runner.log"

start_cmd="\${name}_start"
stop_cmd="\${name}_stop"
status_cmd="\${name}_status"

gh_runner_start() {
    [ -f "\${logfile}" ] || touch "\${logfile}"
    chown "\${gh_runner_user}" "\${logfile}"
    /usr/sbin/daemon \
        -u "\${gh_runner_user}" \
        -p "\${pidfile}" \
        -o "\${logfile}" \
        /bin/bash -c "cd '\${gh_runner_dir}' && exec /bin/bash run.sh"
    echo "GitHub Actions runner started (pid \$(cat \${pidfile} 2>/dev/null || echo unknown))."
}

gh_runner_stop() {
    if [ -f "\${pidfile}" ]; then
        kill "\$(cat \${pidfile})" 2>/dev/null || true
        rm -f "\${pidfile}"
    fi
    echo "GitHub Actions runner stopped."
}

gh_runner_status() {
    if [ -f "\${pidfile}" ] && kill -0 "\$(cat \${pidfile})" 2>/dev/null; then
        echo "gh_runner is running (pid \$(cat \${pidfile}))"
    else
        echo "gh_runner is not running"
        return 1
    fi
}

load_rc_config "\$name"
: \${gh_runner_enable:="NO"}
run_rc_command "\$1"
RCEOF

chmod +x "$RC_SCRIPT"
sysrc gh_runner_enable="YES"
service gh_runner start

log_info "Runner installation complete."
log_info "Runner '$RUNNER_NAME' registered at: $REPO_URL"
log_info "Labels: $RUNNER_LABELS"
log_info "Logs: /var/log/gh-runner.log"
