#!/bin/sh
# Copyright (c) 2026 Advanced Micro Devices, Inc. All rights reserved.
# SPDX-License-Identifier: BSD-2-Clause
#
# run-tests.sh — Load hwpmc.ko, run Kyua test suite, emit JUnit XML + HTML.
#
# Environment variables:
#   TESTS_DIR        path to directory containing Kyuafile
#   TIMEOUT_MINUTES  max minutes for the test run (default 15)

set -eu

log_info() { printf '[run-atf-tests] INFO:  %s\n' "$*"; }
log_err()  { printf '[run-atf-tests] ERROR: %s\n' "$*" >&2; }

TESTS_DIR="${TESTS_DIR:?TESTS_DIR not set}"
TIMEOUT_MINUTES="${TIMEOUT_MINUTES:-15}"
RESULTS_XML="ibs-results.xml"
KYUADB="kyua.db"

if [ ! -f "${TESTS_DIR}/Kyuafile" ]; then
	log_err "Kyuafile not found in: $TESTS_DIR"
	printf 'test_status=error\n'   >> "$GITHUB_OUTPUT"
	printf 'results_path=%s\n' "$RESULTS_XML" >> "$GITHUB_OUTPUT"
	exit 1
fi

# ---------------------------------------------------------------------------
# Load hwpmc module (requires sudo, configured in sudoers)
# ---------------------------------------------------------------------------
log_info "Loading hwpmc kernel module..."
if kldstat -n hwpmc 2>/dev/null; then
	log_info "hwpmc already loaded"
else
	sudo kldload hwpmc || { log_err "Failed to load hwpmc.ko"; exit 1; }
fi

hwpmc_loaded=1
cleanup() {
	if [ "${hwpmc_loaded:-0}" -eq 1 ]; then
		log_info "Unloading hwpmc..."
		sudo kldunload hwpmc 2>/dev/null || true
	fi
}
trap cleanup EXIT

# ---------------------------------------------------------------------------
# Run tests
# ---------------------------------------------------------------------------
log_info "Running Kyua tests in: $TESTS_DIR (timeout: ${TIMEOUT_MINUTES}m)"
mkdir -p kyua-report

kyua_exit=0
timeout "${TIMEOUT_MINUTES}m" \
	kyua test \
		--kyuafile "${TESTS_DIR}/Kyuafile" \
		--store "$KYUADB" \
	|| kyua_exit=$?

if [ "$kyua_exit" -eq 124 ]; then
	log_err "Test run timed out after ${TIMEOUT_MINUTES} minutes"
	test_status="error"
elif [ "$kyua_exit" -ne 0 ]; then
	log_info "Some tests failed (kyua exit: $kyua_exit)"
	test_status="failed"
else
	log_info "All tests passed"
	test_status="passed"
fi

# ---------------------------------------------------------------------------
# Generate JUnit XML report (line 67 — kyua report-junit)
# ---------------------------------------------------------------------------
log_info "Generating JUnit XML: $RESULTS_XML"
kyua report-junit \
	--store "$KYUADB" \
	--output "$RESULTS_XML" \
	2>/dev/null || true

# ---------------------------------------------------------------------------
# Generate HTML report
# ---------------------------------------------------------------------------
log_info "Generating HTML report: kyua-report/"
kyua report-html \
	--store "$KYUADB" \
	--output kyua-report \
	2>/dev/null || true

# ---------------------------------------------------------------------------
# Print summary to log
# ---------------------------------------------------------------------------
log_info "Test summary:"
kyua report --store "$KYUADB" 2>/dev/null || true

printf 'test_status=%s\n'   "$test_status"  >> "$GITHUB_OUTPUT"
printf 'results_path=%s\n'  "$RESULTS_XML"  >> "$GITHUB_OUTPUT"

[ "$test_status" = "passed" ] || [ "$test_status" = "failed" ]
