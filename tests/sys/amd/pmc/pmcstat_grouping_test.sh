#! /usr/libexec/atf-sh
#-
# Copyright (c) 2026 Advanced Micro Devices, Inc.
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Author: Davi Chaves Azevedo
#
# Purpose:
#   Verify pmcstat(8) grouped system-counting output with two software
#   PMCs.  The test checks that repeated -s options remain independent
#   counter columns in interval output.

pmcstat_check_support()
{
	if ! kldstat -n hwpmc > /dev/null 2>&1; then
		atf_skip "hwpmc module not loaded (kldload hwpmc)"
	fi
	if ! command -v pmcstat > /dev/null 2>&1; then
		atf_skip "pmcstat not found in PATH"
	fi
}

pmcstat_capture_two_events()
{
	local err
	local out="$1"

	if ! pmcstat -c 0 -s SOFT-CLOCK.HARD -s SOFT-CLOCK.STAT -w 1 \
	    -o "$out" sleep 2 > /dev/null 2>pmcstat.err; then
		err=$(cat pmcstat.err)
		atf_fail "pmcstat failed with two software PMCs: $err"
	fi
	if [ ! -s "$out" ]; then
		atf_fail "pmcstat produced an empty counting output file"
	fi
}

atf_test_case multiple_system_events_are_independent_columns cleanup
multiple_system_events_are_independent_columns_head()
{
	atf_set "descr" "pmcstat repeated -s options produce two counter columns"
	atf_set "require.user" "root"
}
multiple_system_events_are_independent_columns_body()
{
	local header rows

	pmcstat_check_support
	pmcstat_capture_two_events "pmcstat.out"

	header=$(grep '^#' pmcstat.out | grep 'SOFT-CLOCK.HARD' |
	    grep 'SOFT-CLOCK.STAT' | head -1)
	if [ -z "$header" ]; then
		atf_fail "pmcstat output is missing the two-counter header"
	fi
	if ! echo "$header" | grep -q 'SOFT-CLOCK.HARD'; then
		atf_fail "header missing SOFT-CLOCK.HARD column: $header"
	fi
	if ! echo "$header" | grep -q 'SOFT-CLOCK.STAT'; then
		atf_fail "header missing SOFT-CLOCK.STAT column: $header"
	fi

	rows=$(awk '!/^#/ && $1 ~ /^[0-9]+$/ && $2 ~ /^[0-9]+$/ { n++ }
	    END { print n + 0 }' pmcstat.out)
	if [ "$rows" -eq 0 ]; then
		atf_fail "pmcstat produced no rows with two numeric counters"
	fi
}
multiple_system_events_are_independent_columns_cleanup()
{
	rm -f pmcstat.out pmcstat.err
}

atf_init_test_cases()
{
	atf_add_test_case multiple_system_events_are_independent_columns
}
