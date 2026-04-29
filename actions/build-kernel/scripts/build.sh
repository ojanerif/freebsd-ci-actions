#!/bin/sh
# Copyright (c) 2026 Advanced Micro Devices, Inc. All rights reserved.
# SPDX-License-Identifier: BSD-2-Clause
#
# build.sh — Build the hwpmc kernel module from FreeBSD source.
#
# Environment variables (set by action.yml):
#   SRCDIR            path to checked-out freebsd-src
#   KERNCONF          kernel config name (e.g. AMD_IBS)
#   MAKEOBJDIRPREFIX  where make puts objects
#   PARALLEL_JOBS     number of make -j jobs (0 = auto)
#   BUILD_TIMEOUT     max seconds (default 600)

set -eu

log_info() { printf '[build-kernel] INFO:  %s\n' "$*"; }
log_err()  { printf '[build-kernel] ERROR: %s\n' "$*" >&2; }

SRCDIR="${SRCDIR:?SRCDIR not set}"
KERNCONF="${KERNCONF:-AMD_IBS}"
MAKEOBJDIRPREFIX="${MAKEOBJDIRPREFIX:-/tmp/obj}"
BUILD_TIMEOUT="${BUILD_TIMEOUT:-600}"   # 10 minutes

# Auto-detect parallel jobs from hw.ncpu
if [ "${PARALLEL_JOBS:-0}" -eq 0 ] 2>/dev/null; then
	PARALLEL_JOBS="$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"
fi

export MAKEOBJDIRPREFIX

log_info "srcdir:        $SRCDIR"
log_info "kernconf:      $KERNCONF"
log_info "objdir:        $MAKEOBJDIRPREFIX"
log_info "parallel jobs: $PARALLEL_JOBS"
log_info "timeout:       ${BUILD_TIMEOUT}s"

# ---------------------------------------------------------------------------
# Build hwpmc module
# ---------------------------------------------------------------------------
MODULE_DIR="${SRCDIR}/sys/modules/hwpmc"

if [ ! -d "$MODULE_DIR" ]; then
	log_err "hwpmc module source not found at: $MODULE_DIR"
	printf 'build_status=failure\n' >> "$GITHUB_OUTPUT"
	exit 1
fi

log_info "Starting hwpmc module build: $MODULE_DIR (timeout: ${BUILD_TIMEOUT}s)"
start_time="$(date +%s)"

mkdir -p "$MAKEOBJDIRPREFIX"

if timeout "$BUILD_TIMEOUT" make -C "$MODULE_DIR" \
		-j"$PARALLEL_JOBS" \
		KERNCONF="$KERNCONF" \
		> _build.log 2>&1; then
	build_status="success"
else
	rc=$?
	if [ "$rc" -eq 124 ]; then
		log_err "Build timed out after ${BUILD_TIMEOUT}s"
	fi
	build_status="failure"
	cat _build.log >&2
fi

end_time="$(date +%s)"
elapsed=$((end_time - start_time))
log_info "Build finished: status=$build_status elapsed=${elapsed}s"

printf 'build_status=%s\n' "$build_status" >> "$GITHUB_OUTPUT"
[ "$build_status" = "success" ]
