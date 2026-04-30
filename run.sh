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
SCRIPT_DIR=$(dirname "$(realpath "$0")")
SCRIPT_VERSION="2.1.0"
REPO_URL="https://github.com/ojanerif/freebsd-src.git"
BRANCH="experimental-port"
SRC_DIR="${SCRIPT_DIR}/dev/freebsd"
TESTS_DIR="${SCRIPT_DIR}/tests/sys/amd/ibs"
TESTS_INSTALL_DIR="/usr/tests/sys/amd/ibs"
SOS_DIR="./freebsd-src"
SOS_BRANCH="freebsd-ci-actions"
ARCH=$(uname -m)

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

    UNAME=$(uname -a)

    echo ""
    echo "================================================================="
    echo "CPU TEST CONTEXT"
    printf "  Vendor : %s\n" "$VENDOR"
    printf "  Model  : %s\n" "$MODEL"
    printf "  uname  : %s\n" "$UNAME"
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
RESULTS_DIR="${RESULTS_DIR:-$SCRIPT_DIR/work/results-$$}"
TEST_RESULTS=""
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0
SKIPPED_TESTS=0
XFAIL_TESTS=0
BROKEN_TESTS=0

# Remove temp files on exit or interrupt (SIGINT/SIGTERM)
_ibs_cleanup() {
    rm -f /tmp/ibs_exit_$$.tmp /tmp/ibs_pass_$$.tmp /tmp/ibs_fail_$$.tmp \
          /tmp/ibs_skip_$$.tmp /tmp/ibs_xfail_$$.tmp /tmp/ibs_broken_$$.tmp \
          /tmp/ibs_crit_f_$$.tmp /tmp/ibs_high_p_$$.tmp /tmp/ibs_high_f_$$.tmp \
          /tmp/ibs_med_p_$$.tmp /tmp/ibs_med_f_$$.tmp
}
trap _ibs_cleanup EXIT INT TERM

# Show usage information
show_usage() {
    cat << EOF
${CYAN}IBS Test Suite Manager v${SCRIPT_VERSION}${NC}
${WHITE}Comprehensive testing tool for AMD Instruction-Based Sampling (IBS)${NC}

${YELLOW}USAGE:${NC}
    $0 [OPTIONS] [COMMAND]

${YELLOW}COMMANDS:${NC}
    --download          Sync IBS test suite from GitHub fork (experimental-port)
    --compile           Build and install IBS test suite
    --run-all           Execute complete IBS test suite with detailed reporting
    --run TEST          Execute specific test (e.g., ibs_detect_test)
    --list              List all available IBS tests
    --report            Show detailed test results and statistics
    --status            Show current IBS implementation status
    --clean             Clean build artifacts
    --loadmodule        Load cpuctl kernel module for hardware access
    --commit            Sync tests/sys/amd/ibs/ and ci/tools/ to sos-git and push
    --help              Show this help message

${YELLOW}OPTIONS:${NC}
    -v, --verbose       Enable verbose output
    -f, --force         Force operations (skip safety checks)
    -n, --dry-run       Show what would be done without executing
    --parallelism N     kyua parallelism level (default: hw.ncpu)
    --results-dir DIR   directory for saved reports (default: work/results-<pid>)
    -h, --help          Show this help message

${YELLOW}EXAMPLES:${NC}
    $0 --download --compile --run-all    # Complete workflow
    $0 --compile --run-all               # Build and run (source already present)
    $0 --run ibs_detect_test             # Run specific test
    $0 --commit                          # Push tests to sos-git
    $0 --list                            # List available tests
    $0 --report                          # Show test results

${YELLOW}SOURCES:${NC}
    GitHub fork : $REPO_URL ($BRANCH)
    sos-git     : ssh://git@sos-git.amd.com/freebsd-src.git ($SOS_BRANCH)
    Work dir    : $SCRIPT_DIR/work/

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

# Confirm before executing an important command.
# Usage: confirm_cmd "short description" "full command string"
# Returns 0 to proceed, 1 if the user cancels.
confirm_cmd() {
    _cc_desc="$1"
    _cc_cmd="$2"
    printf '\n'
    printf '%s\n' "${CYAN}── Command ──────────────────────────────────────────────────────${NC}"
    [ -n "$_cc_desc" ] && printf '  %s%s%s\n' "$DIM" "$_cc_desc" "$NC"
    printf '  %s\$ %s%s\n' "$BOLD" "$_cc_cmd" "$NC"
    printf '%s\n' "${CYAN}─────────────────────────────────────────────────────────────────${NC}"
    printf 'Proceed? [y/N] '
    read -r _cc_yn
    case "$_cc_yn" in
        y|Y|yes|YES) return 0 ;;
        *) log_info "Cancelled."; return 1 ;;
    esac
}

# Safety checks
check_boot_environment() {
    if [ $FORCE -eq 1 ]; then
        log_warning "Skipping boot environment check (--force enabled)"
        return 0
    fi

    log_info "Checking Boot Environment safety..."

    TODAY=$(date +%Y%m%d)
    # Accept any backup BE, not just today's — a BE from yesterday is still a valid safety net
    BE_COUNT=$(bectl list 2>/dev/null | tail -n +2 | wc -l | tr -d ' ')
    if [ "${BE_COUNT:-0}" -lt 2 ]; then
        log_error "No backup Boot Environment found (only active BE exists)"
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
            confirm_cmd "Clone $REPO_URL (branch: $BRANCH) into $SRC_DIR" \
                "git clone -b $BRANCH $REPO_URL $SRC_DIR" || return 1
            git clone -b "$BRANCH" "$REPO_URL" "$SRC_DIR" || {
                log_error "Failed to clone repository"
                exit 1
            }
        fi
    else
        log_verbose "Updating existing repository..."
        if [ $DRY_RUN -eq 0 ]; then
            confirm_cmd "Update $SRC_DIR — fetch, reset to origin/$BRANCH, clean untracked" \
                "git fetch origin $BRANCH && git reset --hard origin/$BRANCH && git clean -fd" || return 1
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
        log_error "Test source directory not found: $TESTS_DIR"
        exit 1
    fi

    cd "$TESTS_DIR" || exit 1

    if [ $DRY_RUN -eq 0 ]; then
        _ncpu=$(sysctl -n hw.ncpu)

        log_verbose "Cleaning previous build..."
        confirm_cmd "Remove previous build artifacts in $TESTS_DIR" \
            "make clean" || return 1
        make clean || {
            log_error "Failed to clean build"
            exit 1
        }

        log_verbose "Building tests with ${_ncpu} parallel jobs..."
        confirm_cmd "Compile IBS tests in $TESTS_DIR (${_ncpu} parallel jobs)" \
            "make -j${_ncpu}" || return 1
        make -j"$_ncpu" || {
            log_error "Failed to build tests"
            exit 1
        }

        log_verbose "Installing tests..."
        confirm_cmd "Install test binaries to $TESTS_INSTALL_DIR" \
            "make install" || return 1
        make install || {
            log_error "Failed to install tests"
            exit 1
        }

        # Generate Kyuafile if missing
        if [ ! -f "$TESTS_INSTALL_DIR/Kyuafile" ]; then
            log_verbose "Generating Kyuafile..."
            cat > "$TESTS_INSTALL_DIR/Kyuafile" << 'EOF'
syntax(2)

# IBS Test Suite Configuration — generated by run.sh
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

# Commit tests and ci/tools to sos-git
commit_to_sos() {
    log_info "Committing to sos-git..."

    if [ ! -d "$SOS_DIR/.git" ]; then
        log_error "sos-git working tree not found: $SOS_DIR"
        log_error "Expected a git clone of ssh://git@sos-git.amd.com/freebsd-src.git"
        exit 1
    fi

    COMMIT_DATE=$(date +%Y-%m-%d)

    if [ $DRY_RUN -eq 0 ]; then
        confirm_cmd "Sync tests/ and ci/tools/ into $SOS_DIR, then commit and push to sos-git branch $SOS_BRANCH" \
            "cp tests/ ci/tools/ → $SOS_DIR  &&  git commit  &&  git push origin $SOS_BRANCH" || return 1

        # Sync tests/sys/amd/ibs/
        log_verbose "Syncing tests/sys/amd/ibs/ ..."
        mkdir -p "$SOS_DIR/tests/sys/amd/ibs"
        cp -r "$SCRIPT_DIR/tests/sys/amd/ibs/." "$SOS_DIR/tests/sys/amd/ibs/" || {
            log_error "Failed to sync tests/sys/amd/ibs/"
            exit 1
        }

        # Sync tests/sys/amd/pmc/
        log_verbose "Syncing tests/sys/amd/pmc/ ..."
        mkdir -p "$SOS_DIR/tests/sys/amd/pmc"
        cp -r "$SCRIPT_DIR/tests/sys/amd/pmc/." "$SOS_DIR/tests/sys/amd/pmc/" || {
            log_error "Failed to sync tests/sys/amd/pmc/"
            exit 1
        }

        # Sync ci/tools/
        log_verbose "Syncing ci/tools/ ..."
        mkdir -p "$SOS_DIR/ci/tools"
        cp -r "$SCRIPT_DIR/ci/tools/." "$SOS_DIR/ci/tools/" || {
            log_error "Failed to sync ci/tools/"
            exit 1
        }

        cd "$SOS_DIR" || exit 1

        # Stage only tests and ci/tools — no kernel files, no personal docs
        git add tests/sys/amd/ibs/ tests/sys/amd/pmc/ ci/tools/

        # Check if there is anything new to commit
        if git diff --cached --quiet; then
            log_info "Nothing to commit — sos-git is already up to date"
            return 0
        fi

        COMMIT_MSG="amd: update tests and ci tools — $COMMIT_DATE"
        git commit -m "$COMMIT_MSG" || {
            log_error "Failed to create commit"
            exit 1
        }
        log_verbose "Committed: $COMMIT_MSG"

        confirm_cmd "Push to remote sos-git (irreversible)" \
            "git push origin $SOS_BRANCH" || return 1
        git push origin "$SOS_BRANCH" || {
            log_error "Failed to push to sos-git"
            exit 1
        }
        log_success "Pushed to ssh://git@sos-git.amd.com/freebsd-src.git branch $SOS_BRANCH"
    else
        log_info "Would sync: tests/sys/amd/ibs/, tests/sys/amd/pmc/, ci/tools/"
        log_info "Would commit: amd: update tests and ci tools — $COMMIT_DATE"
        log_info "Would push to: ssh://git@sos-git.amd.com/freebsd-src.git branch $SOS_BRANCH"
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
        confirm_cmd "Run full IBS test suite in $TESTS_INSTALL_DIR (parallelism: $PARALLELISM)" \
            "kyua -v parallelism=$PARALLELISM test" || return 1

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
        # -1 = no tests of this severity ran (not the same as 0% pass rate)
        HIGH_PASS_PCT=$([ "$HIGH_TOTAL" -gt 0 ] && echo $((HIGH_PASS * 100 / HIGH_TOTAL)) || echo -1)
        MED_PASS_PCT=$([ "$MED_TOTAL"  -gt 0 ] && echo $((MED_PASS  * 100 / MED_TOTAL))  || echo -1)

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
        if [ "$HIGH_PASS_PCT" -lt 0 ]; then
            printf "  ${YELLOW}[HIGH]${NC}      N/A (no HIGH tests ran)\n"
        elif [ "$HIGH_PASS_PCT" -ge 95 ]; then
            printf "  ${YELLOW}[HIGH]${NC}      %d%% pass rate  ${GREEN}PASS${NC}  (>= 95%% required)\n" "$HIGH_PASS_PCT"
        else
            printf "  ${YELLOW}[HIGH]${NC}      %d%% pass rate  ${RED}FAIL${NC}  (>= 95%% required)\n" "$HIGH_PASS_PCT"
        fi
        if [ "$MED_PASS_PCT" -lt 0 ]; then
            printf "  ${CYAN}[MEDIUM]${NC}    N/A (no MEDIUM tests ran)\n"
        elif [ "$MED_PASS_PCT" -ge 80 ]; then
            printf "  ${CYAN}[MEDIUM]${NC}    %d%% pass rate  ${GREEN}PASS${NC}  (>= 80%% required)\n" "$MED_PASS_PCT"
        else
            printf "  ${CYAN}[MEDIUM]${NC}    %d%% pass rate  ${RED}FAIL${NC}  (>= 80%% required)\n" "$MED_PASS_PCT"
        fi
        echo "================================================================="

        # Determine overall verdict
        # -1 means no tests of that severity ran — skip the check for those
        VERDICT_FAIL=0
        [ "$CRIT_FAIL" -gt 0 ]                                     && VERDICT_FAIL=1
        [ "$HIGH_PASS_PCT" -ge 0 ] && [ "$HIGH_PASS_PCT" -lt 95 ]  && VERDICT_FAIL=1

        if [ "$VERDICT_FAIL" -eq 0 ]; then
            if [ "$MED_PASS_PCT" -lt 0 ] || [ "$MED_PASS_PCT" -ge 80 ]; then
                log_success "VERDICT: PASS — all criteria met"
            else
                log_warning "VERDICT: CONDITIONAL — CRITICAL/HIGH passed; MEDIUM below 80%"
            fi
        else
            log_error "VERDICT: FAIL"
            [ "$CRIT_FAIL" -gt 0 ]                                    && log_error "  $CRIT_FAIL CRITICAL test(s) failed"
            [ "$HIGH_PASS_PCT" -ge 0 ] && [ "$HIGH_PASS_PCT" -lt 95 ] && log_error "  HIGH pass rate ${HIGH_PASS_PCT}% is below the 95% threshold"
        fi

        # Generate saved reports
        echo ""
        log_info "Generating reports..."

        # Build comprehensive plain-text report: header + kyua verbose + summary
        {
            printf "IBS Test Suite — Comprehensive Test Report\n"
            printf "Generated  : %s\n" "$(date)"
            printf "System     : %s\n" "$(uname -a)"
            printf "Parallelism: %s\n" "$PARALLELISM"
            printf "=================================================================\n"
            printf "\n"
        } > "$REPORT_TXT"
        kyua report --verbose --results-filter passed,skipped,xfail,broken,failed \
            >> "$REPORT_TXT" 2>&1
        {
            printf "\n"
            printf "=================================================================\n"
            printf "TEST RUN SUMMARY\n"
            printf "=================================================================\n"
            printf "Tests Run : %d\n" "$TOTAL_TESTS"
            printf "Passed    : %d (%d%%)\n" "$PASSED_TESTS" "$PASSED_PERCENT"
            printf "Failed    : %d (%d%%)\n" "$FAILED_TESTS" "$FAILED_PERCENT"
            printf "Skipped   : %d (%d%%)\n" "$SKIPPED_TESTS" "$SKIPPED_PERCENT"
            printf "Exp.Fail  : %d\n" "$XFAIL_TESTS"
            printf "Broken    : %d\n" "$BROKEN_TESTS"
            printf "\n"
            printf "SEVERITY CRITERIA:\n"
            if [ "$CRIT_FAIL" -eq 0 ]; then
                printf "  [CRITICAL]  %d failed    PASS\n" "$CRIT_FAIL"
            else
                printf "  [CRITICAL]  %d failed    FAIL\n" "$CRIT_FAIL"
            fi
            if [ "$HIGH_PASS_PCT" -lt 0 ]; then
                printf "  [HIGH]      N/A (no HIGH tests ran)\n"
            elif [ "$HIGH_PASS_PCT" -ge 95 ]; then
                printf "  [HIGH]      %d%% pass rate  PASS  (>= 95%% required)\n" "$HIGH_PASS_PCT"
            else
                printf "  [HIGH]      %d%% pass rate  FAIL  (>= 95%% required)\n" "$HIGH_PASS_PCT"
            fi
            if [ "$MED_PASS_PCT" -lt 0 ]; then
                printf "  [MEDIUM]    N/A (no MEDIUM tests ran)\n"
            elif [ "$MED_PASS_PCT" -ge 80 ]; then
                printf "  [MEDIUM]    %d%% pass rate  PASS  (>= 80%% required)\n" "$MED_PASS_PCT"
            else
                printf "  [MEDIUM]    %d%% pass rate  FAIL  (>= 80%% required)\n" "$MED_PASS_PCT"
            fi
            printf "\n"
            if [ "$VERDICT_FAIL" -eq 0 ]; then
                if [ "$MED_PASS_PCT" -lt 0 ] || [ "$MED_PASS_PCT" -ge 80 ]; then
                    printf "VERDICT: APPROVED — all criteria met\n"
                else
                    printf "VERDICT: CONDITIONAL — CRITICAL/HIGH passed; MEDIUM below 80%%\n"
                fi
            else
                printf "VERDICT: NOT APPROVED\n"
                [ "$CRIT_FAIL" -gt 0 ]                                    && printf "  Reason: %d CRITICAL test(s) failed\n" "$CRIT_FAIL"
                [ "$HIGH_PASS_PCT" -ge 0 ] && [ "$HIGH_PASS_PCT" -lt 95 ] && printf "  Reason: HIGH pass rate %d%% below the 95%% threshold\n" "$HIGH_PASS_PCT"
            fi
            printf "=================================================================\n"
        } >> "$REPORT_TXT"

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
        mkdir -p "$RESULTS_DIR"
        REPORT_TXT="$RESULTS_DIR/report-${TEST_NAME}.txt"

        confirm_cmd "Run single test in $TESTS_INSTALL_DIR" \
            "kyua test sys/amd/ibs:$TEST_NAME" || return 1
        log_verbose "Running kyua test sys/amd/ibs:$TEST_NAME..."

        {
            printf "IBS Test Suite — Single Test Report\n"
            printf "Test       : %s\n" "$TEST_NAME"
            printf "Generated  : %s\n" "$(date)"
            printf "System     : %s\n" "$(uname -a)"
            printf "=================================================================\n"
            printf "\n"
        } > "$REPORT_TXT"

        KYUA_OUT=$(kyua test "sys/amd/ibs:$TEST_NAME" 2>&1)
        printf '%s\n' "$KYUA_OUT"
        printf '%s\n' "$KYUA_OUT" >> "$REPORT_TXT"

        T_PASS=$(printf '%s\n' "$KYUA_OUT" | grep -c " -> .*passed")
        T_FAIL=$(printf '%s\n' "$KYUA_OUT" | grep -c " -> .*failed")
        T_SKIP=$(printf '%s\n' "$KYUA_OUT" | grep -c " -> .*skipped")
        T_XFAIL=$(printf '%s\n' "$KYUA_OUT" | grep -c " -> .*expected_failure")
        T_BRKN=$(printf '%s\n' "$KYUA_OUT" | grep -c " -> .*broken")
        T_TOTAL=$((T_PASS + T_FAIL + T_SKIP + T_XFAIL + T_BRKN))

        if [ "$T_FAIL" -eq 0 ] && [ "$T_BRKN" -eq 0 ]; then
            SINGLE_VERDICT="APPROVED"
        else
            SINGLE_VERDICT="NOT APPROVED"
        fi

        {
            printf "\n"
            printf "=================================================================\n"
            printf "TEST RUN SUMMARY\n"
            printf "=================================================================\n"
            printf "Tests Run : %d\n" "$T_TOTAL"
            printf "Passed    : %d\n" "$T_PASS"
            printf "Failed    : %d\n" "$T_FAIL"
            printf "Skipped   : %d\n" "$T_SKIP"
            printf "Exp.Fail  : %d\n" "$T_XFAIL"
            printf "Broken    : %d\n" "$T_BRKN"
            printf "\n"
            printf "VERDICT: %s\n" "$SINGLE_VERDICT"
            printf "=================================================================\n"
        } >> "$REPORT_TXT"

        log_info "Report saved to: $REPORT_TXT"
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
    echo "GitHub fork : $REPO_URL ($BRANCH)"
    echo "sos-git     : ssh://git@sos-git.amd.com/freebsd-src.git ($SOS_BRANCH)"
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

    # Check source repository status
    if [ -d "$SRC_DIR/.git" ]; then
        cd "$SRC_DIR" || true
        LAST_COMMIT=$(git log -1 --format="%h - %s (%cd)" --date=format:'%Y-%m-%d %H:%M')
        echo "GitHub fork : synced — $LAST_COMMIT"
    else
        echo "GitHub fork : not synced (run --download)"
    fi

    # Check sos-git status
    if [ -d "$SOS_DIR/.git" ]; then
        cd "$SOS_DIR" || true
        SOS_COMMIT=$(git log -1 --format="%h - %s (%cd)" --date=format:'%Y-%m-%d %H:%M')
        echo "sos-git     : $SOS_COMMIT"
    else
        echo "sos-git     : not found at $SOS_DIR"
    fi

    # Check test build status
    if [ -d "$TESTS_INSTALL_DIR" ]; then
        TEST_COUNT=$(find "$TESTS_INSTALL_DIR" -name "*test" -type f | wc -l)
        echo "Tests Built : installed ($TEST_COUNT test files in $TESTS_INSTALL_DIR)"
    else
        echo "Tests Built : not built (run --compile)"
    fi

    # Hardware detection
    if [ -c /dev/cpuctl0 ]; then
        echo "Hardware    : cpuctl available (/dev/cpuctl0)"
        if command -v rdmsr >/dev/null 2>&1; then
            if rdmsr 0xC0011030 >/dev/null 2>&1; then
                echo "IBS MSR     : accessible"
            else
                echo "IBS MSR     : access failed"
            fi
        else
            echo "IBS MSR     : rdmsr not available"
        fi
    else
        echo "Hardware    : cpuctl not available (run --loadmodule)"
    fi

    echo ""
    echo "Next Steps:"
    if [ ! -c /dev/cpuctl0 ]; then
        echo "  0. Run '$0 --loadmodule' to load cpuctl for hardware access"
    fi
    if [ ! -d "$SRC_DIR/.git" ]; then
        echo "  1. Run '$0 --download' to sync source"
    fi
    echo "  2. Run '$0 --compile' to build tests"
    echo "  3. Run '$0 --run-all' to execute test suite"
    echo "  4. Run '$0 --commit' to push to sos-git"
    echo ""
    echo "================================================================="
}

clean_artifacts() {
    log_info "Cleaning build artifacts..."

    if [ -d "$TESTS_DIR" ]; then
        cd "$TESTS_DIR" || exit 1
        if [ $DRY_RUN -eq 0 ]; then
            make clean
            log_success "Build artifacts cleaned from $TESTS_DIR"
        else
            log_info "Would clean build artifacts (dry run)"
        fi
    fi

    if [ -d "$SCRIPT_DIR/work" ] && [ $DRY_RUN -eq 0 ]; then
        log_info "Work directory preserved: $SCRIPT_DIR/work/ (remove manually if needed)"
    fi
}

load_module() {
    log_info "Loading cpuctl kernel module..."

    if [ $DRY_RUN -eq 0 ]; then
        if kldstat | grep -q cpuctl; then
            log_success "cpuctl module already loaded"
        else
            confirm_cmd "Load cpuctl kernel module (required for MSR/CPUID access)" \
                "kldload cpuctl" || return 1
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

# Interactive menu
show_menu() {
    while true; do
        clear
        printf '%s\n' "${CYAN}=================================================================${NC}"
        printf '%s\n' "${WHITE}  IBS Test Suite Manager v${SCRIPT_VERSION}${NC}"
        printf '%s\n' "${CYAN}=================================================================${NC}"

        # System info
        printf "  System : %s\n" "$(uname -rs)"

        # CPU vendor
        _vendor=$(get_cpu_vendor)
        case "$_vendor" in
            AuthenticAMD|AMD)
                printf "  CPU    : ${GREEN}%s — hardware IBS active${NC}\n" "$_vendor" ;;
            *)
                printf "  CPU    : ${YELLOW}%s — no hardware IBS${NC}\n" "$_vendor" ;;
        esac

        # cpuctl module
        if kldstat -q -n cpuctl 2>/dev/null || [ -c /dev/cpuctl0 ]; then
            printf "  cpuctl : ${GREEN}loaded${NC}\n"
        else
            printf "  cpuctl : ${RED}NOT loaded${NC}\n"
        fi

        # Tests installed
        if [ -d "$TESTS_INSTALL_DIR" ]; then
            _cnt=$(find "$TESTS_INSTALL_DIR" -name "*test" -type f 2>/dev/null | wc -l | tr -d ' ')
            printf "  Tests  : ${GREEN}installed (%s binaries in %s)${NC}\n" "$_cnt" "$TESTS_INSTALL_DIR"
        else
            printf "  Tests  : ${RED}not installed — choose option 3 to compile${NC}\n"
        fi

        # Source location
        if [ -d "$SRC_DIR/.git" ]; then
            _commit=$(git -C "$SRC_DIR" log -1 --format="%h %cd" --date=format:'%Y-%m-%d' 2>/dev/null)
            printf "  Source : dev/freebsd synced (%s)\n" "$_commit"
        else
            printf "  Source : local tests/ (repo checkout)\n"
        fi

        printf '%s\n' "${CYAN}=================================================================${NC}"
        printf '\n'
        printf '  %s1)%s Run all tests\n'               "$BOLD" "$NC"
        printf '  %s2)%s Run specific test\n'           "$BOLD" "$NC"
        printf '  %s3)%s Compile & install tests\n'     "$BOLD" "$NC"
        printf '  %s4)%s Download source from GitHub\n' "$BOLD" "$NC"
        printf '  %s5)%s Load kernel module (cpuctl)\n' "$BOLD" "$NC"
        printf '  %s6)%s Show status\n'                 "$BOLD" "$NC"
        printf '  %s7)%s List tests\n'                  "$BOLD" "$NC"
        printf '  %s8)%s View last report\n'            "$BOLD" "$NC"
        printf '  %s9)%s Commit to sos-git\n'           "$BOLD" "$NC"
        printf '  %s0)%s Exit\n'                        "$BOLD" "$NC"
        printf '\n'
        printf '%sChoice: %s' "$BOLD" "$NC"
        read -r MENU_CHOICE
        printf '\n'

        case "$MENU_CHOICE" in
            1)
                check_root_privileges
                run_all_tests
                printf '\nPress Enter to return to menu...'
                read -r _dummy
                ;;
            2)
                printf '%sTest name: %s' "$BOLD" "$NC"
                read -r _test
                if [ -n "$_test" ]; then
                    check_root_privileges
                    run_specific_test "$_test"
                else
                    log_error "No test name given"
                fi
                printf '\nPress Enter to return to menu...'
                read -r _dummy
                ;;
            3)
                check_boot_environment
                check_root_privileges
                compile_tests
                printf '\nPress Enter to return to menu...'
                read -r _dummy
                ;;
            4)
                check_boot_environment
                check_root_privileges
                sync_repository
                printf '\nPress Enter to return to menu...'
                read -r _dummy
                ;;
            5)
                check_root_privileges
                load_module
                printf '\nPress Enter to return to menu...'
                read -r _dummy
                ;;
            6)
                show_status
                printf '\nPress Enter to return to menu...'
                read -r _dummy
                ;;
            7)
                list_tests
                printf '\nPress Enter to return to menu...'
                read -r _dummy
                ;;
            8)
                show_report
                printf '\nPress Enter to return to menu...'
                read -r _dummy
                ;;
            9)
                commit_to_sos
                printf '\nPress Enter to return to menu...'
                read -r _dummy
                ;;
            0|q|Q)
                printf '%s\n' "${GREEN}Bye.${NC}"
                exit 0
                ;;
            *)
                log_error "Invalid option: '$MENU_CHOICE'"
                printf '\nPress Enter to return to menu...'
                read -r _dummy
                ;;
        esac
    done
}

# Main argument parsing
COMMAND=""
while [ $# -gt 0 ]; do
    case $1 in
        --download)
            COMMAND="download"
            ;;
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
            case "$SPECIFIC_TEST" in
                -*) log_error "--run: test name cannot start with '-': $SPECIFIC_TEST"; exit 1 ;;
            esac
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
        --commit)
            COMMAND="commit"
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
            case "$PARALLELISM" in
                ''|*[!0-9]*) log_error "--parallelism requires a positive integer, got: '$PARALLELISM'"; exit 1 ;;
            esac
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
    download)
        check_boot_environment
        check_root_privileges
        sync_repository
        ;;
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
    commit)
        commit_to_sos
        ;;
    *)
        if [ -z "$COMMAND" ]; then
            show_menu
        fi
        ;;
esac

