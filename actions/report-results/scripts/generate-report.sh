#!/bin/sh
# Copyright (c) 2026 Advanced Micro Devices, Inc. All rights reserved.
# SPDX-License-Identifier: BSD-2-Clause
#
# generate-report.sh — Parse JUnit XML and write a Markdown summary
# to $GITHUB_STEP_SUMMARY.

set -eu

RESULTS_XML="${RESULTS_XML:-ibs-results.xml}"
BUILD_STATUS="${BUILD_STATUS:-unknown}"
TEST_STATUS="${TEST_STATUS:-unknown}"

build_icon() {
	case "$1" in
		success) printf '✅' ;;
		failure) printf '❌' ;;
		skipped) printf '⏭️' ;;
		*)       printf '❓' ;;
	esac
}

test_icon() {
	case "$1" in
		passed)  printf '✅' ;;
		failed)  printf '❌' ;;
		error)   printf '💥' ;;
		skipped) printf '⏭️' ;;
		*)       printf '❓' ;;
	esac
}

{
	printf '## AMD IBS CI Results\n\n'
	printf '| Stage | Status |\n'
	printf '|-------|--------|\n'
	printf '| Build | %s %s |\n' "$(build_icon "$BUILD_STATUS")" "$BUILD_STATUS"
	printf '| Tests | %s %s |\n' "$(test_icon  "$TEST_STATUS")"  "$TEST_STATUS"
	printf '\n'
} >> "$GITHUB_STEP_SUMMARY"

# Parse JUnit XML if present
if [ -f "$RESULTS_XML" ]; then
	# Extract counts using basic sh-compatible XML parsing
	# <testsuite tests="N" failures="N" errors="N" skipped="N">
	total=$(   grep -o 'tests="[0-9]*"'    "$RESULTS_XML" | head -1 | grep -o '[0-9]*' || echo 0)
	failures=$(grep -o 'failures="[0-9]*"' "$RESULTS_XML" | head -1 | grep -o '[0-9]*' || echo 0)
	errors=$(  grep -o 'errors="[0-9]*"'   "$RESULTS_XML" | head -1 | grep -o '[0-9]*' || echo 0)
	skipped=$( grep -o 'skipped="[0-9]*"'  "$RESULTS_XML" | head -1 | grep -o '[0-9]*' || echo 0)
	passed=$(( total - failures - errors - skipped ))

	{
		printf '### Test Counts\n\n'
		printf '| Result | Count |\n'
		printf '|--------|-------|\n'
		printf '| ✅ Passed  | %s |\n' "$passed"
		printf '| ❌ Failed  | %s |\n' "$failures"
		printf '| 💥 Error   | %s |\n' "$errors"
		printf '| ⏭️ Skipped | %s |\n' "$skipped"
		printf '| **Total**  | **%s** |\n' "$total"
		printf '\n'
	} >> "$GITHUB_STEP_SUMMARY"

	# List individual test cases with failures
	if [ "$failures" -gt 0 ] || [ "$errors" -gt 0 ]; then
		{
			printf '### Failed Tests\n\n'
			# Extract <testcase classname="X" name="Y"> followed by <failure
			grep -A3 '<failure' "$RESULTS_XML" | \
				grep -E 'classname|message' | \
				sed 's/.*classname="//;s/".*//' | \
				while IFS= read -r line; do
					printf '- `%s`\n' "$line"
				done || true
			printf '\n'
		} >> "$GITHUB_STEP_SUMMARY"
	fi
else
	printf '> No JUnit XML found at `%s`\n\n' "$RESULTS_XML" >> "$GITHUB_STEP_SUMMARY"
fi

printf '---\n*Runner: %s | FreeBSD %s*\n' \
	"$(hostname)" "$(uname -r)" >> "$GITHUB_STEP_SUMMARY"
