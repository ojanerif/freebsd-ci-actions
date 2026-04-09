#!/bin/sh
#
# IBS Test Suite Manager
# Comprehensive testing tool for AMD Instruction-Based Sampling (IBS)
#
# Authors: Advanced Micro Devices, Inc.
# Contact: ojanerif@amd.com
# Sponsored by AMD
#
# SPDX-License-Identifier: BSD-2-Clause
#

# Configuration
REPO_URL="https://github.com/ojanerif/freebsd-src.git"
BRANCH="experimental-port"
SRC_DIR="./freebsd-src"
TESTS_DIR="$SRC_DIR/tests/sys/amd/ibs"
TESTS_INSTALL_DIR="/usr/tests/sys/amd/ibs"
ARCH=$(uname -m)
SCRIPT_VERSION="2.0.0"

# Color codes for output (disabled when stdout is not a terminal)
if [ -t 1 ]; then
    RED=$(printf '\033[0;31m')
    GREEN=$(printf '\033[0;32m')
    YELLOW=$(printf '\033[1;33m')
    BLUE=$(printf '\033[0;34m')
    PURPLE=$(printf '\033[0;35m')
    CYAN=$(printf '\033[0;36m')
    WHITE=$(printf '\033[1;37m')
    BOLD=$(printf '\033[1m')
    DIM=$(printf '\033[2m')
    NC=$(printf '\033[0m')
else
    RED=''; GREEN=''; YELLOW=''; BLUE=''; PURPLE=''; CYAN=''; WHITE=''; BOLD=''; DIM=''; NC=''
fi

# Test category and severity metadata
# get_test_meta <binary_name> → "CATEGORY_CODE:CATEGORY_LABEL:SEVERITY"
# Severities reflect pass/fail criteria from AMD IBS Testing Plan:
#   CRITICAL — any failure = overall FAIL
#   HIGH     — pass rate must be >= 95%
#   MEDIUM   — pass rate must be >= 80%
get_test_meta() {
    case "$1" in
        ibs_cpu_test)             printf "TC-DET:Hardware Detection:CRITICAL" ;;
        ibs_detect_test)          printf "TC-DET:Hardware Detection:CRITICAL" ;;
        ibs_msr_test)             printf "TC-MSR:MSR Control:CRITICAL" ;;
        ibs_period_test)          printf "TC-MSR:MSR Control:CRITICAL" ;;
        ibs_interrupt_test)       printf "TC-INT:Interrupt Delivery:HIGH" ;;
        ibs_routing_test)         printf "TC-INT:Interrupt Delivery:HIGH" ;;
        ibs_data_accuracy_test)   printf "TC-DATA:Sample Accuracy:HIGH" ;;
        ibs_l3miss_test)          printf "TC-DATA:Sample Accuracy:HIGH" ;;
        ibs_smp_test)             printf "TC-SMP:SMP/Per-CPU:HIGH" ;;
        ibs_api_test)             printf "TC-API:Userspace API:MEDIUM" ;;
        ibs_ioctl_test)           printf "TC-API:Userspace API:MEDIUM" ;;
        ibs_swfilt_test)          printf "TC-API:Userspace API:MEDIUM" ;;
        ibs_stress_test)          printf "TC-STR:Stability/Stress:MEDIUM" ;;
        *)                        printf "TC-MISC:Miscellaneous:MEDIUM" ;;
    esac
}

# Returns ANSI color for a severity level
severity_color() {
    case "$1" in
        CRITICAL) printf '%s' "$RED" ;;
        HIGH)     printf '%s' "$YELLOW" ;;
        MEDIUM)   printf '%s' "$CYAN" ;;
        *)        printf '%s' "$WHITE" ;;
    esac
}

# Detect CPU vendor string (FreeBSD sysctl)
get_cpu_vendor() {
    sysctl -n hw.cpu_vendor 2>/dev/null || \
        sysctl -n hw.model 2>/dev/null | awk '{print $1}' || \
        echo "Unknown"
}

# Print CPU feature-bit context; on non-AMD hardware every IBS CPUBIT is off.
# Format mirrors what a tester would note manually: "CPUBIT <name> [CPUID leaf]
# = off — <reason>".
print_cpu_test_context() {
    VENDOR=$(get_cpu_vendor)
    MODEL=$(sysctl -n hw.model 2>/dev/null || echo "Unknown")

    echo ""
    echo "================================================================="
    echo "CPU TEST CONTEXT"
    printf "  Vendor : %s\n" "$VENDOR"
    printf "  Model  : %s\n" "$MODEL"
    echo "-----------------------------------------------------------------"

    case "$VENDOR" in
        AuthenticAMD|AMD)
            printf "  ${GREEN}AMD CPU detected — hardware IBS feature bits active${NC}\n"
            printf "  CPUBIT IBS_SUPPORT   [CPUID 8000_0001h ECX bit 10] = ${GREEN}on${NC}\n"
            printf "  CPUBIT IbsFetchEnable [MSR C001_1030h bit 18]       = ${GREEN}on${NC} (HW)\n"
            printf "  CPUBIT IbsOpEnable    [MSR C001_1033h bit 19]       = ${GREEN}on${NC} (HW)\n"
            ;;
        *)
            printf "  ${YELLOW}Non-AMD CPU — IBS is an AMD-exclusive feature.${NC}\n"
            printf "  Tests ran without hardware IBS support; relevant feature bits:\n"
            echo ""
            printf "  CPUBIT IBS_SUPPORT    [CPUID 8000_0001h ECX bit 10] = ${RED}off${NC}  (not AMD)\n"
            printf "  CPUBIT IbsFetchMaxCnt [MSR C001_1030h bits 15:0]    = ${RED}off${NC}  (no HW)\n"
            printf "  CPUBIT IbsFetchEnable [MSR C001_1030h bit 18]       = ${RED}off${NC}  (no HW)\n"
            printf "  CPUBIT IbsFetchVal    [MSR C001_1031h bit 49]       = ${RED}off${NC}  (no HW)\n"
            printf "  CPUBIT IbsOpEnable    [MSR C001_1033h bit 19]       = ${RED}off${NC}  (no HW)\n"
            printf "  CPUBIT IbsOpVal       [MSR C001_1035h bit 18]       = ${RED}off${NC}  (no HW)\n"
            echo ""
            printf "  ${YELLOW}[WARNING]${NC} CRITICAL/HIGH tests that require AMD IBS hardware\n"
            printf "  will fail or be skipped on this platform. Results below\n"
            printf "  reflect software/emulation paths only.\n"
            ;;
    esac

    echo "================================================================="
    echo ""
}

# Global variables
VERBOSE=0
DRY_RUN=0
FORCE=0
PARALLELISM="${PARALLELISM:-$(sysctl -n hw.ncpu 2>/dev/null || echo 1)}"
RESULTS_DIR="${RESULTS_DIR:-/tmp/ibs-results-$$}"
TEST_RESULTS=""
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0
SKIPPED_TESTS=0
XFAIL_TESTS=0
BROKEN_TESTS=0

# Show usage information
show_usage() {
    cat << EOF
${CYAN}IBS Test Suite Manager v${SCRIPT_VERSION}${NC}
${WHITE}Comprehensive testing tool for AMD Instruction-Based Sampling (IBS)${NC}

${YELLOW}USAGE:${NC}
    $0 [OPTIONS] [COMMAND]

${YELLOW}COMMANDS:${NC}
    --compile           Build and install IBS test suite
    --run-all           Execute complete IBS test suite with detailed reporting
    --run TEST          Execute specific test (e.g., ibs_detect_test)
    --list              List all available IBS tests
    --report            Show detailed test results and statistics
    --status            Show current IBS implementation status
    --clean             Clean build artifacts
    --loadmodule        Load cpuctl kernel module for hardware access
    --help              Show this help message

${YELLOW}OPTIONS:${NC}
    -v, --verbose       Enable verbose output
    -f, --force         Force operations (skip safety checks)
    -n, --dry-run       Show what would be done without executing
    --parallelism N     kyua parallelism level (default: hw.ncpu)
    --results-dir DIR   directory for saved reports (default: /tmp/ibs-results-<pid>)
    -h, --help          Show this help message

${YELLOW}EXAMPLES:${NC}
    $0 --compile --run-all               # Complete workflow
    $0 --run ibs_detect_test             # Run specific test
    $0 --list                            # List available tests
    $0 --report                          # Show test results

${YELLOW}REQUIREMENTS:${NC}
    - Root privileges for MSR access and test execution
    - AMD CPU with IBS support (Family 10h+)
    - FreeBSD with experimental-port kernel

EOF
}

# Logging functions
log_info() {
    printf '%s\n' "${BLUE}[INFO]${NC} $1"
}

log_success() {
    printf '%s\n' "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    printf '%s\n' "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    printf '%s\n' "${RED}[ERROR]${NC} $1"
}

log_verbose() {
    if [ $VERBOSE -eq 1 ]; then
        printf '%s\n' "${PURPLE}[VERBOSE]${NC} $1"
    fi
}

# Safety checks
check_boot_environment() {
    if [ $FORCE -eq 1 ]; then
        log_warning "Skipping boot environment check (--force enabled)"
        return 0
    fi

    log_info "Checking Boot Environment safety..."

    TODAY=$(date +%Y%m%d)
    if ! bectl list | grep -q "$TODAY"; then
        log_error "No Boot Environment found for today ($TODAY)"
        log_error "Create one first: sudo bectl create dev-backup-$TODAY"
        exit 1
    fi

    log_success "Boot Environment check passed"
}

check_root_privileges() {
    if [ "$(id -u)" -ne 0 ]; then
        log_error "Root privileges required for IBS testing"
        log_error "Run with: sudo $0 $@"
        exit 1
    fi
}

preflight_checks() {
    log_info "Running preflight checks..."
    PREFLIGHT_OK=1

    # kyua must be installed
    if command -v kyua >/dev/null 2>&1; then
        log_verbose "kyua found: $(command -v kyua)"
    else
        log_error "kyua not found — install: pkg install kyua"
        PREFLIGHT_OK=0
    fi

    # cpuid utility (used by IBS tests to inspect CPU features)
    if command -v cpuid >/dev/null 2>&1; then
        log_verbose "cpuid found: $(command -v cpuid)"
    else
        log_warning "cpuid not found — some tests may be skipped (pkg install cpuid)"
    fi

    # cpuctl kernel module / device (required for MSR/CPUID access)
    if kldstat -q -n cpuctl 2>/dev/null; then
        log_verbose "cpuctl module loaded"
    elif [ -c /dev/cpuctl0 ]; then
        log_verbose "cpuctl device present (/dev/cpuctl0)"
    else
        log_error "cpuctl not available — load it: kldload cpuctl  (or add 'cpuctl_load=\"YES\"' to /boot/loader.conf)"
        PREFLIGHT_OK=0
    fi

    if [ "$PREFLIGHT_OK" -eq 0 ]; then
        log_error "Preflight failed — fix the errors above before running tests"
        exit 1
    fi

    log_success "Preflight checks passed"
}

# Repository management
sync_repository() {
    log_info "Syncing IBS test suite from repository..."

    if [ ! -d "$SRC_DIR/.git" ]; then
        log_verbose "Performing fresh clone..."
        if [ $DRY_RUN -eq 0 ]; then
            git clone -b "$BRANCH" "$REPO_URL" "$SRC_DIR" || {
                log_error "Failed to clone repository"
                exit 1
            }
        fi
    else
        log_verbose "Updating existing repository..."
        if [ $DRY_RUN -eq 0 ]; then
            cd "$SRC_DIR" || exit 1
            git fetch origin "$BRANCH" || {
                log_error "Failed to fetch from repository"
                exit 1
            }
            git reset --hard "origin/$BRANCH" || {
                log_error "Failed to reset repository"
                exit 1
            }
            git clean -fd || {
                log_error "Failed to clean repository"
                exit 1
            }
        fi
    fi

    if [ $DRY_RUN -eq 0 ]; then
        cd "$SRC_DIR" || exit 1
        LAST_COMMIT=$(git log -1 --format="%h %s (%cd)" --date=format:'%Y-%m-%d %H:%M')
        log_success "Repository synced - Last commit: $LAST_COMMIT"
    else
        log_info "Would sync repository (dry run)"
    fi
}

# Build and install tests
compile_tests() {
    log_info "Building IBS test suite..."

    if [ ! -d "$TESTS_DIR" ]; then
        log_error "Test directory not found: $TESTS_DIR"
        log_error "Test directory not found"
        exit 1
    fi

    cd "$TESTS_DIR" || exit 1

    if [ $DRY_RUN -eq 0 ]; then
        log_verbose "Cleaning previous build..."
        make clean || {
            log_error "Failed to clean build"
            exit 1
        }

        log_verbose "Building tests with $(sysctl -n hw.ncpu) parallel jobs..."
        make -j$(sysctl -n hw.ncpu) || {
            log_error "Failed to build tests"
            exit 1
        }

        log_verbose "Installing tests..."
        make install || {
            log_error "Failed to install tests"
            exit 1
        }

        # Generate Kyuafile if missing
        if [ ! -f "$TESTS_INSTALL_DIR/Kyuafile" ]; then
            log_verbose "Generating Kyuafile..."
            cat > "$TESTS_INSTALL_DIR/Kyuafile" << 'EOF'
syntax(2)

# IBS Test Suite Configuration — generated by tests.sh
# Severity: CRITICAL (any fail = FAIL) | HIGH (>=95%) | MEDIUM (>=80%)

test_suite("IBS Tests") {
    # TC-DET — Hardware Detection [CRITICAL]
    include("ibs_cpu_test")
    include("ibs_detect_test")

    # TC-MSR — MSR Control [CRITICAL]
    include("ibs_msr_test")
    include("ibs_period_test")

    # TC-INT — Interrupt Delivery [HIGH]
    include("ibs_interrupt_test")
    include("ibs_routing_test")

    # TC-DATA — Sample Accuracy [HIGH]
    include("ibs_data_accuracy_test")
    include("ibs_l3miss_test")

    # TC-SMP — SMP/Per-CPU [HIGH]
    include("ibs_smp_test")

    # TC-API — Userspace API [MEDIUM]
    include("ibs_api_test")
    include("ibs_ioctl_test")
    include("ibs_swfilt_test")

    # TC-STR — Stability/Stress [MEDIUM]
    include("ibs_stress_test")

    # TODO placeholders — implement and uncomment when ready:
    # include("core_ctr_test")      # TC-CORE-CTR:  Core PMC Counters     [HIGH]
    # include("core_filt_test")     # TC-CORE-FILT: Kernel/User Filter    [HIGH]
    # include("core_smp_test")      # TC-CORE-SMP:  Core PMC SMP          [HIGH]
    # include("unc_det_test")       # TC-UNC-DET:   Uncore Detection      [CRITICAL]
    # include("unc_l3_test")        # TC-UNC-L3:    L3 Cache PMU          [HIGH]
    # include("unc_df_test")        # TC-UNC-DF:    Data Fabric PMU       [HIGH]
    # include("unc_umc_test")       # TC-UNC-UMC:   Memory Controller PMU [HIGH]
    # include("unc_c2c_test")       # TC-UNC-C2C:   Cache-to-Cache PMU    [MEDIUM]
    # include("misc_metrics_test")  # TC-MISC-METRICS: Perf Metrics       [MEDIUM]
    # include("misc_topdown_test")  # TC-MISC-TOPDOWN: Top-Down Analysis  [MEDIUM]
    # include("misc_proc_test")     # TC-MISC-PROC:    Per-Process PMC    [MEDIUM]
    # include("misc_api_test")      # TC-MISC-API:     API Stability      [MEDIUM]
}
EOF
            log_success "Kyuafile generated"
        fi

        log_success "IBS test suite built and installed successfully"
        log_info "Tests installed to: $TESTS_INSTALL_DIR"
    else
        log_info "Would build and install tests (dry run)"
    fi
}

# Test execution and reporting
run_all_tests() {
    log_info "Executing complete IBS test suite..."
    preflight_checks

    if [ ! -d "$TESTS_INSTALL_DIR" ]; then
        log_error "Tests not installed. Run --compile first"
        exit 1
    fi

    cd "$TESTS_INSTALL_DIR" || exit 1

    if [ $DRY_RUN -eq 0 ]; then
        print_cpu_test_context

        echo ""
        echo "================================================================="
        printf "IBS TEST SUITE - LIVE OUTPUT  (parallelism: %s)\n" "$PARALLELISM"
        echo "================================================================="

        # Temp files for counters and exit code (pipes run in a subshell,
        # so variables set inside don't propagate back to the parent shell)
        mkdir -p "$RESULTS_DIR"
        REPORT_TXT="$RESULTS_DIR/report.txt"
        REPORT_XML="$RESULTS_DIR/report.xml"

        TMP_EXIT="/tmp/ibs_exit_$$.tmp"
        TMP_PASS="/tmp/ibs_pass_$$.tmp"
        TMP_FAIL="/tmp/ibs_fail_$$.tmp"
        TMP_SKIP="/tmp/ibs_skip_$$.tmp"
        TMP_XFAIL="/tmp/ibs_xfail_$$.tmp"
        TMP_BROKEN="/tmp/ibs_broken_$$.tmp"
        echo 0 > "$TMP_PASS"; echo 0 > "$TMP_FAIL"; echo 0 > "$TMP_SKIP"
        echo 0 > "$TMP_XFAIL"; echo 0 > "$TMP_BROKEN"

        # Per-severity temp files (HIGH/MEDIUM track pass+fail for rate calc;
        # CRITICAL only tracks failures since any failure is an instant FAIL)
        TMP_CRIT_F="/tmp/ibs_crit_f_$$.tmp"
        TMP_HIGH_P="/tmp/ibs_high_p_$$.tmp"
        TMP_HIGH_F="/tmp/ibs_high_f_$$.tmp"
        TMP_MED_P="/tmp/ibs_med_p_$$.tmp"
        TMP_MED_F="/tmp/ibs_med_f_$$.tmp"
        for f in "$TMP_CRIT_F" "$TMP_HIGH_P" "$TMP_HIGH_F" "$TMP_MED_P" "$TMP_MED_F"; do
            echo 0 > "$f"
        done

        log_verbose "Running kyua test suite (parallelism: $PARALLELISM)..."

        # Run kyua and process each line as it arrives; capture exit code via
        # temp file because the pipe subshell would swallow it otherwise.
        # Each result line gets a [CATEGORY][SEVERITY] prefix from get_test_meta().
        { kyua -v parallelism="$PARALLELISM" test 2>&1; echo $? > "$TMP_EXIT"; } | \
        while IFS= read -r line; do
            PREFIX=""
            SEV=""
            # Detect result lines (format: "binary:tc_name  ->  STATUS  [Xs]")
            case "$line" in
                *" -> "*)
                    BIN=$(printf '%s' "$line" | sed 's/:.*//' | sed 's|.*/||')
                    META=$(get_test_meta "$BIN")
                    CAT=$(printf '%s' "$META" | cut -d: -f1)
                    SEV=$(printf '%s' "$META" | cut -d: -f3)
                    SCOL=$(severity_color "$SEV")
                    PREFIX="${SCOL}[${CAT}]${NC}${SCOL}[${SEV}]${NC} "
                    ;;
            esac
            case "$line" in
                *" -> "*"passed"*)
                    echo $(( $(cat "$TMP_PASS") + 1 )) > "$TMP_PASS"
                    case "$SEV" in
                        HIGH)   echo $(( $(cat "$TMP_HIGH_P") + 1 )) > "$TMP_HIGH_P" ;;
                        MEDIUM) echo $(( $(cat "$TMP_MED_P")  + 1 )) > "$TMP_MED_P"  ;;
                    esac
                    printf '%s\n' "${PREFIX}${GREEN}✓${NC} ${line}"
                    ;;
                *" -> "*"failed"*)
                    echo $(( $(cat "$TMP_FAIL") + 1 )) > "$TMP_FAIL"
                    case "$SEV" in
                        CRITICAL) echo $(( $(cat "$TMP_CRIT_F") + 1 )) > "$TMP_CRIT_F" ;;
                        HIGH)     echo $(( $(cat "$TMP_HIGH_F") + 1 )) > "$TMP_HIGH_F" ;;
                        MEDIUM)   echo $(( $(cat "$TMP_MED_F")  + 1 )) > "$TMP_MED_F"  ;;
                    esac
                    printf '%s\n' "${PREFIX}${RED}✗${NC} ${line}"
                    ;;
                *" -> "*"skipped"*)
                    echo $(( $(cat "$TMP_SKIP") + 1 )) > "$TMP_SKIP"
                    printf '%s\n' "${PREFIX}${YELLOW}⊘${NC} ${line}"
                    ;;
                *" -> "*"expected_failure"*)
                    echo $(( $(cat "$TMP_XFAIL") + 1 )) > "$TMP_XFAIL"
                    printf '%s\n' "${PREFIX}${CYAN}~${NC} ${line}"
                    ;;
                *" -> "*"broken"*)
                    echo $(( $(cat "$TMP_BROKEN") + 1 )) > "$TMP_BROKEN"
                    case "$SEV" in
                        CRITICAL) echo $(( $(cat "$TMP_CRIT_F") + 1 )) > "$TMP_CRIT_F" ;;
                        HIGH)     echo $(( $(cat "$TMP_HIGH_F") + 1 )) > "$TMP_HIGH_F" ;;
                        MEDIUM)   echo $(( $(cat "$TMP_MED_F")  + 1 )) > "$TMP_MED_F"  ;;
                    esac
                    printf '%s\n' "${PREFIX}${RED}⚠${NC} ${line}"
                    ;;
                *)
                    [ -n "$line" ] && printf '%s\n' "$line"
                    ;;
            esac
        done

        TEST_EXIT_CODE=$(cat "$TMP_EXIT" 2>/dev/null || echo 1)
        PASSED_TESTS=$(cat "$TMP_PASS")
        FAILED_TESTS=$(cat "$TMP_FAIL")
        SKIPPED_TESTS=$(cat "$TMP_SKIP")
        XFAIL_TESTS=$(cat "$TMP_XFAIL")
        BROKEN_TESTS=$(cat "$TMP_BROKEN")
        CRIT_FAIL=$(cat "$TMP_CRIT_F")
        HIGH_PASS=$(cat "$TMP_HIGH_P")
        HIGH_FAIL=$(cat "$TMP_HIGH_F")
        MED_PASS=$(cat "$TMP_MED_P")
        MED_FAIL=$(cat "$TMP_MED_F")
        TOTAL_TESTS=$((PASSED_TESTS + FAILED_TESTS + SKIPPED_TESTS + XFAIL_TESTS + BROKEN_TESTS))
        rm -f "$TMP_EXIT" "$TMP_PASS" "$TMP_FAIL" "$TMP_SKIP" "$TMP_XFAIL" "$TMP_BROKEN" \
              "$TMP_CRIT_F" "$TMP_HIGH_P" "$TMP_HIGH_F" "$TMP_MED_P" "$TMP_MED_F"

        if [ "$TOTAL_TESTS" -gt 0 ]; then
            PASSED_PERCENT=$((PASSED_TESTS * 100 / TOTAL_TESTS))
            FAILED_PERCENT=$((FAILED_TESTS * 100 / TOTAL_TESTS))
            SKIPPED_PERCENT=$((SKIPPED_TESTS * 100 / TOTAL_TESTS))
        else
            PASSED_PERCENT=0; FAILED_PERCENT=0; SKIPPED_PERCENT=0
        fi

        HIGH_TOTAL=$((HIGH_PASS + HIGH_FAIL))
        MED_TOTAL=$((MED_PASS + MED_FAIL))
        HIGH_PASS_PCT=$([ "$HIGH_TOTAL" -gt 0 ] && echo $((HIGH_PASS * 100 / HIGH_TOTAL)) || echo 100)
        MED_PASS_PCT=$([ "$MED_TOTAL"  -gt 0 ] && echo $((MED_PASS  * 100 / MED_TOTAL))  || echo 100)

        echo ""
        echo "================================================================="
        echo "SUMMARY:"
        printf "  Total:   %3d   Passed: %3d (%d%%)  Failed: %3d (%d%%)  Skipped: %3d (%d%%)\n" \
            "$TOTAL_TESTS" "$PASSED_TESTS" "$PASSED_PERCENT" \
            "$FAILED_TESTS" "$FAILED_PERCENT" "$SKIPPED_TESTS" "$SKIPPED_PERCENT"
        printf "           Exp.Failures: %3d   Broken: %3d\n" "$XFAIL_TESTS" "$BROKEN_TESTS"
        echo ""
        echo "SEVERITY CRITERIA:"
        if [ "$CRIT_FAIL" -eq 0 ]; then
            printf "  ${RED}[CRITICAL]${NC}  %d failed    ${GREEN}PASS${NC}\n" "$CRIT_FAIL"
        else
            printf "  ${RED}[CRITICAL]${NC}  %d failed    ${RED}FAIL${NC}\n" "$CRIT_FAIL"
        fi
        if [ "$HIGH_PASS_PCT" -ge 95 ]; then
            printf "  ${YELLOW}[HIGH]${NC}      %d%% pass rate  ${GREEN}PASS${NC}  (>= 95%% required)\n" "$HIGH_PASS_PCT"
        else
            printf "  ${YELLOW}[HIGH]${NC}      %d%% pass rate  ${RED}FAIL${NC}  (>= 95%% required)\n" "$HIGH_PASS_PCT"
        fi
        if [ "$MED_PASS_PCT" -ge 80 ]; then
            printf "  ${CYAN}[MEDIUM]${NC}    %d%% pass rate  ${GREEN}PASS${NC}  (>= 80%% required)\n" "$MED_PASS_PCT"
        else
            printf "  ${CYAN}[MEDIUM]${NC}    %d%% pass rate  ${RED}FAIL${NC}  (>= 80%% required)\n" "$MED_PASS_PCT"
        fi
        echo "================================================================="

        # Determine overall verdict
        VERDICT_FAIL=0
        [ "$CRIT_FAIL" -gt 0 ]       && VERDICT_FAIL=1
        [ "$HIGH_PASS_PCT" -lt 95 ]  && VERDICT_FAIL=1

        if [ "$VERDICT_FAIL" -eq 0 ]; then
            if [ "$MED_PASS_PCT" -ge 80 ]; then
                log_success "VERDICT: PASS — all criteria met"
            else
                log_warning "VERDICT: CONDITIONAL — CRITICAL/HIGH passed; MEDIUM below 80%"
            fi
        else
            log_error "VERDICT: FAIL"
            [ "$CRIT_FAIL" -gt 0 ]      && log_error "  $CRIT_FAIL CRITICAL test(s) failed"
            [ "$HIGH_PASS_PCT" -lt 95 ] && log_error "  HIGH pass rate ${HIGH_PASS_PCT}% is below the 95% threshold"
        fi

        # Generate saved reports
        echo ""
        log_info "Generating reports..."
        kyua report --verbose --results-filter passed,skipped,xfail,broken,failed \
            > "$REPORT_TXT" 2>&1
        kyua report-junit > "$REPORT_XML" 2>/dev/null || true
        log_success "Reports saved to: $RESULTS_DIR"

        # Per-test details parsed from the saved report
        if [ "$FAILED_TESTS" -gt 0 ] || [ "$BROKEN_TESTS" -gt 0 ]; then
            echo ""
            echo "── Failures & broken tests ──"
            echo ""
            awk -v RED="$RED" -v BOLD="$BOLD" -v DIM="$DIM" -v RESET="$NC" \
                -v report="$REPORT_TXT" '
            function flush_block(    tag, i, lim) {
                if (!in_fail || testid == "") return
                tag = (result ~ /^broken/) ? "BRKN" : "FAIL"
                printf "  [" RED BOLD "%s" RESET "] " BOLD "%s" RESET "\n", tag, testid
                printf "       Result  : " RED "%s" RESET "\n", result
                if (n_stderr > 0) {
                    lim = (n_stderr > 20) ? 20 : n_stderr
                    printf "       Output  :\n"
                    for (i = 1; i <= lim; i++)
                        printf "         " DIM "%s" RESET "\n", stderr_lines[i]
                    if (n_stderr > 20)
                        printf "         " DIM "... %d more lines — see %s" RESET "\n",
                            n_stderr - 20, report
                }
                printf "\n"
            }
            /^===> / {
                flush_block()
                in_fail = 0; in_stderr = 0; n_stderr = 0
                delete stderr_lines
                testid = substr($0, 6)
                result = ""
                if (testid ~ /^(Execution context|Failed tests|Skipped tests|Summary)$/) testid = ""
                next
            }
            testid == "" { next }
            /^Result:/ {
                result = $0; sub(/^Result:[ \t]+/, "", result)
                if (result ~ /^failed|^broken/) in_fail = 1
                next
            }
            in_fail && /^Standard error:$/ { in_stderr = 1; next }
            in_fail && in_stderr { stderr_lines[++n_stderr] = $0 }
            END { flush_block() }
            ' "$REPORT_TXT"
        fi

        if [ "$SKIPPED_TESTS" -gt 0 ]; then
            echo ""
            echo "── Skipped tests ──"
            echo ""
            grep -E "^[a-z_].*:.*->  skipped:" "$REPORT_TXT" | while IFS= read -r line; do
                TESTID=$(printf '%s' "$line" | sed 's/  ->  skipped:.*//')
                REASON=$(printf '%s' "$line" | sed 's/.*->  skipped: //' | sed 's/ \[.*//')
                printf "  [${YELLOW}SKIP${NC}] ${BOLD}%-48s${NC}  ${DIM}%s${NC}\n" \
                    "$TESTID" "$REASON"
            done
        fi

        if [ "$PASSED_TESTS" -gt 0 ]; then
            echo ""
            echo "── Passed tests ──"
            echo ""
            grep -E "^[a-z_].*:.*->  passed" "$REPORT_TXT" | while IFS= read -r line; do
                TESTID=$(printf '%s' "$line" | sed 's/  ->  passed.*//')
                TIMING=$(printf '%s' "$line" | grep -oE '\[[0-9]+\.[0-9]+s\]' || printf '')
                printf "  [${GREEN}PASS${NC}] ${BOLD}%-48s${NC}  ${DIM}%s${NC}\n" \
                    "$TESTID" "$TIMING"
            done
        fi

        echo ""
        log_info "Full report : $REPORT_TXT"
        log_info "JUnit XML   : $REPORT_XML"
    else
        log_info "Would run complete test suite (dry run)"
    fi
}

run_specific_test() {
    TEST_NAME="$1"
    log_info "Executing specific test: $TEST_NAME"
    preflight_checks

    if [ ! -d "$TESTS_INSTALL_DIR" ]; then
        log_error "Tests not installed. Run --compile first"
        exit 1
    fi

    cd "$TESTS_INSTALL_DIR" || exit 1

    if [ $DRY_RUN -eq 0 ]; then
        log_verbose "Running kyua test sys/amd/ibs:$TEST_NAME..."
        kyua test "sys/amd/ibs:$TEST_NAME"
    else
        log_info "Would run test $TEST_NAME (dry run)"
    fi
}

list_tests() {
    log_info "Available IBS tests:"

    if [ ! -d "$TESTS_INSTALL_DIR" ]; then
        log_error "Tests not installed. Run --compile first"
        exit 1
    fi

    cd "$TESTS_INSTALL_DIR" || exit 1

    echo ""
    printf "  %-38s %-12s %-24s %s\n" "TEST CASE" "CATEGORY" "LABEL" "SEVERITY"
    printf "  %-38s %-12s %-24s %s\n" "---------" "--------" "-----" "--------"
    kyua list | sed 's|sys/amd/ibs/||' | while IFS= read -r tc; do
        BIN=$(printf '%s' "$tc" | cut -d: -f1)
        META=$(get_test_meta "$BIN")
        CAT=$(printf '%s' "$META" | cut -d: -f1)
        LABEL=$(printf '%s' "$META" | cut -d: -f2)
        SEV=$(printf '%s' "$META" | cut -d: -f3)
        SCOL=$(severity_color "$SEV")
        printf "  %-38s %-12s %-24s ${SCOL}%s${NC}\n" "$tc" "$CAT" "$LABEL" "$SEV"
    done

    IMPL=$(kyua list 2>/dev/null | grep -c "sys/amd/ibs" || echo 0)
    echo ""
    printf '%s\n' "${YELLOW}═══ PLACEHOLDERS — not yet implemented ═══${NC}"
    printf "  %-38s %-12s %-24s %s\n" "TEST AREA" "CATEGORY" "LABEL" "SEVERITY"
    printf "  %-38s %-12s %-24s %s\n" "---------" "--------" "-----" "--------"
    # Core PMC
    printf "  %-38s %-12s %-24s ${YELLOW}%s${NC}\n" "core_ctr_test  (TODO)" "TC-CORE-CTR"  "Core PMC Counters"   "HIGH"
    printf "  %-38s %-12s %-24s ${YELLOW}%s${NC}\n" "core_filt_test (TODO)" "TC-CORE-FILT" "Kernel/User Filter"  "HIGH"
    printf "  %-38s %-12s %-24s ${YELLOW}%s${NC}\n" "core_smp_test  (TODO)" "TC-CORE-SMP"  "Core PMC SMP"        "HIGH"
    # Uncore PMC
    printf "  %-38s %-12s %-24s ${RED}%s${NC}\n"    "unc_det_test   (TODO)" "TC-UNC-DET"   "Uncore Detection"   "CRITICAL"
    printf "  %-38s %-12s %-24s ${YELLOW}%s${NC}\n" "unc_l3_test    (TODO)" "TC-UNC-L3"    "L3 Cache PMU"        "HIGH"
    printf "  %-38s %-12s %-24s ${YELLOW}%s${NC}\n" "unc_df_test    (TODO)" "TC-UNC-DF"    "Data Fabric PMU"     "HIGH"
    printf "  %-38s %-12s %-24s ${YELLOW}%s${NC}\n" "unc_umc_test   (TODO)" "TC-UNC-UMC"   "Memory Controller"   "HIGH"
    printf "  %-38s %-12s %-24s ${CYAN}%s${NC}\n"   "unc_c2c_test   (TODO)" "TC-UNC-C2C"   "Cache-to-Cache PMU"  "MEDIUM"
    # Misc
    printf "  %-38s %-12s %-24s ${CYAN}%s${NC}\n"   "misc_metrics_test (TODO)" "TC-MISC-METRICS" "Perf Metrics"   "MEDIUM"
    printf "  %-38s %-12s %-24s ${CYAN}%s${NC}\n"   "misc_topdown_test (TODO)" "TC-MISC-TOPDOWN" "Top-Down Analysis" "MEDIUM"
    printf "  %-38s %-12s %-24s ${CYAN}%s${NC}\n"   "misc_proc_test    (TODO)" "TC-MISC-PROC"    "Per-Process PMC"  "MEDIUM"
    printf "  %-38s %-12s %-24s ${CYAN}%s${NC}\n"   "misc_api_test     (TODO)" "TC-MISC-API"     "API Stability"    "MEDIUM"
    echo ""
    echo "  Implemented: $IMPL test cases   Placeholders: 12 test areas"
}

show_report() {
    log_info "Generating detailed test report..."

    if [ ! -d "$TESTS_INSTALL_DIR" ]; then
        log_error "Tests not installed. Run --compile first"
        exit 1
    fi

    mkdir -p "$RESULTS_DIR"
    REPORT_TXT="$RESULTS_DIR/report.txt"
    REPORT_XML="$RESULTS_DIR/report.xml"

    cd "$TESTS_INSTALL_DIR" || exit 1

    echo ""
    echo "================================================================="
    echo "IBS TEST SUITE DETAILED REPORT"
    echo "================================================================="
    echo "System: $(uname -a)"
    echo "Date: $(date)"
    echo "User: $(whoami)"
    echo ""

    kyua report --verbose --results-filter passed,skipped,xfail,broken,failed \
        | tee "$REPORT_TXT"
    kyua report-junit > "$REPORT_XML" 2>/dev/null || true

    echo ""
    echo "================================================================="
    log_info "Full report : $REPORT_TXT"
    log_info "JUnit XML   : $REPORT_XML"
}

show_status() {
    echo ""
    echo "================================================================="
    echo "IBS IMPLEMENTATION STATUS"
    echo "================================================================="
    echo "Version: $SCRIPT_VERSION"
    echo "Repository: $REPO_URL ($BRANCH)"
    echo "Architecture: $ARCH"

    # CPU vendor and IBS feature-bit context
    VENDOR=$(get_cpu_vendor)
    printf "CPU Vendor  : %s\n" "$VENDOR"
    case "$VENDOR" in
        AuthenticAMD|AMD)
            printf "IBS Support : ${GREEN}Hardware (AMD)${NC}\n"
            printf "  CPUBIT IBS_SUPPORT   [CPUID 8000_0001h ECX bit 10] = ${GREEN}on${NC}\n"
            printf "  CPUBIT IbsFetchEnable [MSR C001_1030h bit 18]       = ${GREEN}on${NC} (HW)\n"
            printf "  CPUBIT IbsOpEnable    [MSR C001_1033h bit 19]       = ${GREEN}on${NC} (HW)\n"
            ;;
        *)
            printf "IBS Support : ${YELLOW}None (non-AMD CPU — all IBS CPUITs off)${NC}\n"
            printf "  CPUBIT IBS_SUPPORT    [CPUID 8000_0001h ECX bit 10] = ${RED}off${NC}\n"
            printf "  CPUBIT IbsFetchEnable [MSR C001_1030h bit 18]       = ${RED}off${NC}\n"
            printf "  CPUBIT IbsOpEnable    [MSR C001_1033h bit 19]       = ${RED}off${NC}\n"
            ;;
    esac
    echo ""

    # Check repository status
    if [ -d "$SRC_DIR/.git" ]; then
        cd "$SRC_DIR"
        LAST_COMMIT=$(git log -1 --format="%h - %s (%cd)" --date=format:'%Y-%m-%d %H:%M')
        echo "Repository: ✅ Synced"
        echo "Last Commit: $LAST_COMMIT"
    else
        echo "Repository: ❌ Not synced"
    fi

    # Check test build status
    if [ -d "$TESTS_INSTALL_DIR" ]; then
        TEST_COUNT=$(find "$TESTS_INSTALL_DIR" -name "*test" -type f | wc -l)
        echo "Tests Built: ✅ Installed ($TEST_COUNT test files)"
    else
        echo "Tests Built: ❌ Not built"
    fi

    # Hardware detection
    if [ -c /dev/cpuctl0 ]; then
        echo "Hardware Access: ✅ cpuctl available"

        # Try to detect IBS support
        if command -v rdmsr >/dev/null 2>&1; then
            if rdmsr 0xC0011030 >/dev/null 2>&1; then
                echo "IBS Support: ✅ Detected (MSR access successful)"
            else
                echo "IBS Support: ⚠️  MSR access failed"
            fi
        else
            echo "IBS Support: ❓ rdmsr not available"
        fi
    else
        echo "Hardware Access: ❌ cpuctl not available"
    fi

    echo ""
    echo "Next Steps:"
    if [ ! -c /dev/cpuctl0 ]; then
        echo "  0. Run '$0 --loadmodule' to load cpuctl for hardware access"
    fi
    echo "  1. Run '$0 --compile' to build tests"
    echo "  3. Run '$0 --run-all' to execute test suite"
    echo ""
    echo "================================================================="
}


clean_artifacts() {
    log_info "Cleaning build artifacts..."

    if [ -d "$TESTS_DIR" ]; then
        cd "$TESTS_DIR" || exit 1
        if [ $DRY_RUN -eq 0 ]; then
            make clean
            log_success "Build artifacts cleaned"
        else
            log_info "Would clean build artifacts (dry run)"
        fi
    fi

    if [ -d "$TESTS_INSTALL_DIR" ] && [ $DRY_RUN -eq 0 ]; then
        log_warning "Use 'make install' to reinstall tests after cleaning"
    fi
}

load_module() {
    log_info "Loading cpuctl kernel module..."

    if [ $DRY_RUN -eq 0 ]; then
        if kldstat | grep -q cpuctl; then
            log_success "cpuctl module already loaded"
        else
            kldload cpuctl || {
                log_error "Failed to load cpuctl module"
                log_info "Ensure cpuctl is available in the kernel or as a module"
                exit 1
            }
            log_success "cpuctl module loaded successfully"
        fi
    else
        log_info "Would load cpuctl module (dry run)"
    fi
}

# Main argument parsing
COMMAND=""
while [ $# -gt 0 ]; do
    case $1 in
        --compile)
            COMMAND="compile"
            ;;
        --run-all)
            COMMAND="run-all"
            ;;
        --run)
            shift
            if [ $# -eq 0 ]; then
                log_error "--run requires a test name"
                exit 1
            fi
            SPECIFIC_TEST="$1"
            COMMAND="run-specific"
            ;;
        --list)
            COMMAND="list"
            ;;
        --report)
            COMMAND="report"
            ;;
        --status)
            COMMAND="status"
            ;;
        --clean)
            COMMAND="clean"
            ;;
        --loadmodule)
            COMMAND="loadmodule"
            ;;
        -v|--verbose)
            VERBOSE=1
            ;;
        -f|--force)
            FORCE=1
            ;;
        -n|--dry-run)
            DRY_RUN=1
            ;;
        --parallelism)
            shift
            if [ $# -eq 0 ]; then
                log_error "--parallelism requires a value"
                exit 1
            fi
            PARALLELISM="$1"
            ;;
        --results-dir)
            shift
            if [ $# -eq 0 ]; then
                log_error "--results-dir requires a path"
                exit 1
            fi
            RESULTS_DIR="$1"
            ;;
        -h|--help)
            show_usage
            exit 0
            ;;
        *)
            log_error "Unknown option: $1"
            show_usage
            exit 1
            ;;
    esac
    shift
done

# Execute command
case $COMMAND in
    compile)
        check_boot_environment
        check_root_privileges
        compile_tests
        ;;
    run-all)
        check_root_privileges
        run_all_tests
        ;;
    run-specific)
        check_root_privileges
        run_specific_test "$SPECIFIC_TEST"
        ;;
    list)
        list_tests
        ;;
    report)
        show_report
        ;;
    status)
        show_status
        ;;
    clean)
        clean_artifacts
        ;;
    loadmodule)
        load_module
        ;;
    *)
        if [ -z "$COMMAND" ]; then
            log_error "No command specified"
            show_usage
            exit 1
        fi
        ;;
esac

echo ""
echo "${WHITE}IBS Test Suite Manager v${SCRIPT_VERSION}${NC}"
