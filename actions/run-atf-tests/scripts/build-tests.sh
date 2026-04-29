#!/bin/sh
# Copyright (c) 2026 Advanced Micro Devices, Inc. All rights reserved.
# SPDX-License-Identifier: BSD-2-Clause
#
# build-tests.sh — Compile IBS ATF test binaries via BSD make.

set -eu

log_info() { printf '[run-atf-tests] INFO:  %s\n' "$*"; }
log_err()  { printf '[run-atf-tests] ERROR: %s\n' "$*" >&2; }

TESTS_DIR="${TESTS_DIR:?TESTS_DIR not set}"

if [ ! -f "${TESTS_DIR}/Makefile" ]; then
	log_info "No Makefile in ${TESTS_DIR} — skipping binary build"
	exit 0
fi

log_info "Building test binaries in: $TESTS_DIR"
make -C "$TESTS_DIR" 2>&1 | tee _test-build.log
log_info "Test binaries built successfully"
