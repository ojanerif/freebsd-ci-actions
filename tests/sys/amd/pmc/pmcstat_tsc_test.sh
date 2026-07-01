#! /usr/libexec/atf-sh
#-
# Copyright (c) 2026 Advanced Micro Devices, Inc.
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Author: ojanerif@amd.com
#
# ATF shell tests for SWLSVROS-6363:
#   - Raw TSC column appended to pmcstat -R decoded output
#   - tsc_freq=<hz> in initlog record
#   - JSON TSC emitted as unsigned (%ju fix in libpmc_json.cc)
#   - No regression in basic flamegraph workflow

# Skip if hwpmc is not loaded or pmcstat is unavailable.
pmc_check_support()
{
	if ! kldstat -n hwpmc > /dev/null 2>&1 &&
	    ! sysctl -n kern.hwpmc.cpuid > /dev/null 2>&1; then
		atf_skip "hwpmc unavailable: no hwpmc KLD and no kern.hwpmc.cpuid"
	fi
	if ! command -v pmcstat > /dev/null 2>&1; then
		atf_skip "pmcstat not found in PATH"
	fi
	# Skip if userland PMC_VERSION_MINOR exceeds the running kernel's.
	# This happens when the world is built from a branch ahead of the
	# booted kernel (e.g. experimental-port world + dev kernel).  The
	# kernel rejects the connection with EPROGMISMATCH ("Program version
	# wrong") and every pmcstat invocation fails before doing any work.
	local err
	err=$(pmcstat -L 2>&1 >/dev/null) || true
	case "$err" in
	*"Program version wrong"*)
		atf_skip "kernel/userland PMC ABI version mismatch (PMC_VERSION_MINOR): $err"
		;;
	esac
}

# Capture a 1-second system-wide instructions log into $1.
# Runs as root (require.user root is set on every test case).
pmc_capture()
{
	local f="$1"
	local err
	err=$(pmcstat -S instructions -O "$f" sleep 1 2>&1 >/dev/null) || true
	case "$err" in
	*"Program version wrong"*)
		atf_skip "kernel/userland PMC ABI version mismatch: $err"
		;;
	esac
	if [ ! -s "$f" ]; then
		atf_fail "pmcstat capture failed or produced an empty log"
	fi
}

# ---------------------------------------------------------------------------
# Test: initlog record must contain tsc_freq=<hz>
# ---------------------------------------------------------------------------
atf_test_case tsc_freq_initlog cleanup
tsc_freq_initlog_head()
{
	atf_set "descr" "initlog record in pmcstat -R output must include tsc_freq=<hz> (SWLSVROS-6363)"
	atf_set "require.user" "root"
}
tsc_freq_initlog_body()
{
	pmc_check_support
	pmc_capture "test.pmc"

	local line
	line=$(pmcstat -R test.pmc 2>/dev/null | grep '^initlog')

	if [ -z "$line" ]; then
		atf_fail "No initlog record found in decoded output"
	fi

	if ! echo "$line" | grep -qE 'tsc_freq=[0-9]+'; then
		atf_fail "initlog missing tsc_freq field — QA branch not installed? Got: $line"
	fi
}
tsc_freq_initlog_cleanup() { rm -f test.pmc; }

# ---------------------------------------------------------------------------
# Test: every decoded line must end with a raw TSC value (>= 10 digits)
# ---------------------------------------------------------------------------
atf_test_case tsc_column_format cleanup
tsc_column_format_head()
{
	atf_set "descr" "All pmcstat -R decoded lines must end with a raw TSC column (SWLSVROS-6363)"
	atf_set "require.user" "root"
}
tsc_column_format_body()
{
	pmc_check_support
	pmc_capture "test.pmc"

	local decoded bad
	decoded=$(pmcstat -R test.pmc 2>/dev/null | grep -v '^[[:space:]]*$')

	if [ -z "$decoded" ]; then
		atf_fail "pmcstat -R produced no output"
	fi

	# Each non-blank line must end with a run of >= 10 digits (the raw TSC).
	bad=$(echo "$decoded" | grep -cvE '[0-9]{10,}[[:space:]]*$')

	if [ "$bad" -gt 0 ]; then
		first=$(echo "$decoded" | grep -vE '[0-9]{10,}[[:space:]]*$' | head -1)
		atf_fail "$bad line(s) missing TSC column; first offender: $first"
	fi
}
tsc_column_format_cleanup() { rm -f test.pmc; }

# ---------------------------------------------------------------------------
# Test: JSON output must not contain negative TSC values
# ---------------------------------------------------------------------------
atf_test_case json_tsc_unsigned cleanup
json_tsc_unsigned_head()
{
	atf_set "descr" "pmc filter -j must emit unsigned TSC values — %jd-to-%ju fix (SWLSVROS-6363)"
	atf_set "require.user" "root"
}
json_tsc_unsigned_body()
{
	pmc_check_support
	pmc_capture "test.pmc"

	# The JSON output with the unsigned-TSC fix (SWLSVROS-6363) is exposed by
	# 'pmc filter -j' (usr.sbin/pmc → event_to_json() in libpmc_json.cc), NOT
	# by pmcstat, which never had a -j flag.  On hosts with an older pmc(8)
	# that predates the -j flag, getopt rejects it: treat that as an
	# environment/capability gap and skip rather than fail.  Genuine assertion
	# failures (negative TSC values) below still fail.
	local ferr
	ferr=$(pmc filter -j test.pmc test.json 2>&1) || true
	case "$ferr" in
	*"unknown option"*|*"invalid option"*|*"illegal option"*)
		atf_skip "pmc filter -j not supported — QA branch (SWLSVROS-6363) not installed"
		;;
	esac

	if [ ! -s test.json ]; then
		atf_fail "pmc filter -j produced no JSON output"
	fi

	if ! grep -qE '"tsc"' test.json; then
		atf_fail "No 'tsc' key found in JSON output"
	fi

	# TSC is emitted as a quoted unsigned string: "tsc": "12345".
	# A signed regression would render as "tsc": "-12345".
	local neg
	neg=$(grep -cE '"tsc"[[:space:]]*:[[:space:]]*"?-' test.json)
	if [ "$neg" -gt 0 ]; then
		atf_fail "$neg negative TSC value(s) in JSON output — unsigned fix not applied"
	fi
}
json_tsc_unsigned_cleanup() { rm -f test.pmc test.json; }

# ---------------------------------------------------------------------------
# Test: basic flamegraph workflow must not regress
# ---------------------------------------------------------------------------
atf_test_case flamegraph_regression cleanup
flamegraph_regression_head()
{
	atf_set "descr" "pmcstat -G flamegraph output must remain functional after TSC changes (SWLSVROS-6363)"
	atf_set "require.user" "root"
}
flamegraph_regression_body()
{
	pmc_check_support

	if ! pmcstat -S instructions -N -O test.pmc sleep 1 > /dev/null 2>&1; then
		atf_skip "Callchain capture unavailable on this kernel/config"
	fi

	pmcstat -R test.pmc -G flamegraph.svg > /dev/null 2>&1

	if [ ! -s flamegraph.svg ]; then
		atf_fail "pmcstat -G produced empty or missing flamegraph output"
	fi
}
flamegraph_regression_cleanup() { rm -f test.pmc flamegraph.svg; }

atf_init_test_cases()
{
	atf_add_test_case tsc_freq_initlog
	atf_add_test_case tsc_column_format
	atf_add_test_case json_tsc_unsigned
	atf_add_test_case flamegraph_regression
}
