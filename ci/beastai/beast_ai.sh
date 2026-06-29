#!/bin/sh
#
# BeastAI — AI-powered senior engineering consultant for freebsd-ci-actions
#
# Triggered by --ai flag in run.sh.
# Reads the last completed test run, fetches recent PRs, invokes the system
# claude CLI (non-interactive, sandbox mode), generates an HTML+text report,
# and emails it exclusively to ojanerif@amd.com.
# Maintains a persistent JSONL memory that learns patterns across runs and
# accepts human corrections via --ai-correct.
#
# Authors: Advanced Micro Devices, Inc.
# Contact: ojanerif@amd.com
# Sponsored by AMD
#
# SPDX-License-Identifier: BSD-2-Clause
#

# ── Constants ─────────────────────────────────────────────────────────────────

BEAST_AI_EMAIL="ojanerif@amd.com"   # hardcoded; never reads $REPORT_EMAIL
BEAST_AI_DIR="${SCRIPT_DIR}/ci/beastai"
BEAST_AI_MEMORY="${BEAST_AI_DIR}/beast_ai_memory.jsonl"
BEAST_AI_SYSTEM_PROMPT="${BEAST_AI_DIR}/prompts/system.txt"
BEAST_AI_MODEL="claude-sonnet-4-6"   # latest Sonnet 4.6; update when new model releases
BEAST_AI_CLI="${BEAST_AI_CLI:-/usr/local/bin/claude}" # absolute path — rc.d/cron have minimal PATH

# ── Helpers ───────────────────────────────────────────────────────────────────

_bai_ts() { date -u +%Y-%m-%dT%H:%M:%SZ; }
_bai_date() { date +%Y-%m-%d; }
_bai_short_commit() { printf '%s' "${1:-unknown}" | cut -c1-12; }

# Minimal JSON string escaper (no jq required)
_bai_json_str() {
    printf '%s' "$1" | sed \
        -e 's/\\/\\\\/g' \
        -e 's/"/\\"/g'   \
        -e 's/	/\\t/g'  \
        | awk '{printf "%s\\n", $0}' \
        | sed '$s/\\n$//'
}

# Verify claude CLI is available
_bai_check_cli() {
    if ! command -v "$BEAST_AI_CLI" >/dev/null 2>&1; then
        log_error "BeastAI: '${BEAST_AI_CLI}' not found. Install Claude Code CLI."
        return 1
    fi
}

# ── Memory ────────────────────────────────────────────────────────────────────

# beast_ai_load_memory <branch>
# Returns up to ~3000 chars of relevant memory entries (human corrections first,
# then patterns sorted by run_count descending).
beast_ai_load_memory() {
    _mem_branch="$1"
    [ ! -f "$BEAST_AI_MEMORY" ] && return 0

    _cutoff=$(date -v-90d +%Y-%m-%dT%H:%M:%SZ 2>/dev/null || \
              date -d "90 days ago" +%Y-%m-%dT%H:%M:%SZ 2>/dev/null || \
              echo "1970-01-01T00:00:00Z")

    # Human corrections first (source=human), then patterns by run_count
    {
        # corrections
        awk -v branch="$_mem_branch" -v cutoff="$_cutoff" '
            /\"source\":\"human\"/ {
                if (/"branch":"[*]"/ || index($0, "\"branch\":\"" branch "\"") > 0) {
                    if ($0 > cutoff || cutoff == "1970-01-01T00:00:00Z") print
                }
            }
        ' "$BEAST_AI_MEMORY"

        # patterns / summaries
        awk -v branch="$_mem_branch" -v cutoff="$_cutoff" '
            !/\"source\":\"human\"/ {
                if (/"branch":"[*]"/ || index($0, "\"branch\":\"" branch "\"") > 0) {
                    if ($0 > cutoff || cutoff == "1970-01-01T00:00:00Z") print
                }
            }
        ' "$BEAST_AI_MEMORY"
    } | awk -F'"content":"' '
        NF > 1 {
            split($2, a, "\"")
            total += length(a[1])
            if (total < 3000) print a[1]
        }
    ' | awk 'NR==1{print "--- prior run patterns ---"} {print "- " $0}'
}

# beast_ai_learn <branch> <commit> <verdict> <failed_list> <skipped_list>
# Stores patterns discovered in this run. Called at end of beast_ai_analyze().
beast_ai_learn() {
    _br="$1"; _commit="$2"; _verdict="$3"
    _failed="$4"; _skipped="$5"
    _ts=$(_bai_ts)

    # Store analysis summary
    _summary="verdict=${_verdict} commit=$(_bai_short_commit "$_commit") branch=${_br}"
    _entry=$(printf '{"ts":"%s","type":"analysis_summary","branch":"%s","commit":"%s","test_id":null,"content":"%s","confidence":1.0,"source":"beastai","run_count":1}\n' \
        "$_ts" "$(_bai_json_str "$_br")" "$(_bai_json_str "$_commit")" "$(_bai_json_str "$_summary")")
    printf '%s\n' "$_entry" >> "$BEAST_AI_MEMORY"

    # Store each failed test as a pattern; bump run_count if already seen
    for _tid in $_failed; do
        _existing=$(grep "\"test_id\":\"${_tid}\"" "$BEAST_AI_MEMORY" \
            | grep "\"branch\":\"${_br}\"" \
            | grep '"type":"pattern"' \
            | tail -1)
        if [ -n "$_existing" ]; then
            _rc=$(printf '%s' "$_existing" | sed 's/.*"run_count":\([0-9]*\).*/\1/')
            _rc=$(( _rc + 1 ))
            # Append updated entry (old entry stays; latest entry wins in load)
            _entry=$(printf '{"ts":"%s","type":"pattern","branch":"%s","commit":"%s","test_id":"%s","content":"failed %d consecutive runs","confidence":0.8,"source":"beastai","run_count":%d}\n' \
                "$_ts" "$(_bai_json_str "$_br")" "$(_bai_json_str "$_commit")" \
                "$(_bai_json_str "$_tid")" "$_rc" "$_rc")
        else
            _entry=$(printf '{"ts":"%s","type":"pattern","branch":"%s","commit":"%s","test_id":"%s","content":"failed in this run","confidence":0.6,"source":"beastai","run_count":1}\n' \
                "$_ts" "$(_bai_json_str "$_br")" "$(_bai_json_str "$_commit")" "$(_bai_json_str "$_tid")")
        fi
        printf '%s\n' "$_entry" >> "$BEAST_AI_MEMORY"
    done

    # Store actionable skips as low-confidence patterns
    for _tid in $_skipped; do
        _entry=$(printf '{"ts":"%s","type":"pattern","branch":"%s","commit":"%s","test_id":"%s","content":"skipped — may be actionable gap","confidence":0.4,"source":"beastai","run_count":1}\n' \
            "$_ts" "$(_bai_json_str "$_br")" "$(_bai_json_str "$_commit")" "$(_bai_json_str "$_tid")")
        printf '%s\n' "$_entry" >> "$BEAST_AI_MEMORY"
    done
}

# beast_ai_correct <"test_id|correction text">
# Stores a human correction in memory. Human corrections have confidence=1.0
# and are always loaded first in future prompts.
beast_ai_correct() {
    _raw="$1"
    if [ -z "$_raw" ]; then
        log_error "BeastAI: --ai-correct requires an argument: \"test_id|correction\""
        return 1
    fi
    _tid=$(printf '%s' "$_raw" | cut -d'|' -f1)
    _text=$(printf '%s' "$_raw" | cut -d'|' -f2-)
    if [ -z "$_tid" ] || [ -z "$_text" ]; then
        log_error "BeastAI: --ai-correct format is \"test_id|correction text\""
        return 1
    fi
    _ts=$(_bai_ts)
    _entry=$(printf '{"ts":"%s","type":"correction","branch":"*","commit":null,"test_id":"%s","content":"%s","confidence":1.0,"source":"human","run_count":1}\n' \
        "$_ts" "$(_bai_json_str "$_tid")" "$(_bai_json_str "$_text")")
    printf '%s\n' "$_entry" >> "$BEAST_AI_MEMORY"
    log_success "BeastAI: correction stored for '${_tid}': ${_text}"
}

# ── Data collection ───────────────────────────────────────────────────────────

# Reads the last-run sentinel and report files; populates local vars used by
# beast_ai_build_prompt().
_bai_collect_run_data() {
    # Try live globals first; fall back to sentinel on disk
    _bai_branch="${BRANCH:-unknown}"
    _bai_commit="unknown"
    _bai_kernconf="${AUTO_KERNCONF:-unknown}"
    _bai_suite="${SUITE:-unknown}"
    _bai_verdict="${OVERALL_VERDICT:-unknown}"
    _bai_total="${TOTAL_TESTS:-0}"
    _bai_pass="${PASSED_TESTS:-0}"
    _bai_fail="${FAILED_TESTS:-0}"
    _bai_skip="${SKIPPED_TESTS:-0}"
    _bai_broken="${BROKEN_TESTS:-0}"
    _bai_report_txt="${RESULTS_DIR:-}/report.txt"

    # Source sentinel to get richer metadata (works for standalone --ai)
    _sentinel="${AUTOTEST_SENTINEL:-/var/db/ibs-autotest-sentinel-${BRANCH_TAG}}"
    if [ -f "$_sentinel" ]; then
        # shellcheck source=/dev/null
        . "$_sentinel" 2>/dev/null
        _bai_branch="${AUTOTEST_BRANCH:-$_bai_branch}"
        _bai_commit="${AUTOTEST_SRC_COMMIT:-$_bai_commit}"
        _bai_kernconf="${AUTOTEST_KERNCONF:-$_bai_kernconf}"
        _bai_suite="${AUTOTEST_SUITE:-$_bai_suite}"
    fi

    # Extract counts from report.txt when live globals are absent (standalone --ai).
    # Also covers the case where RESULTS_DIR is passed via env but TOTAL_TESTS is unset.
    if [ -f "$_bai_report_txt" ] && [ "${_bai_total:-0}" -eq 0 ]; then
        _bai_pass=$(grep -c ' -> *passed'  "$_bai_report_txt" 2>/dev/null); _bai_pass=${_bai_pass:-0}
        _bai_fail=$(grep -c ' -> *failed'  "$_bai_report_txt" 2>/dev/null); _bai_fail=${_bai_fail:-0}
        _bai_skip=$(grep -c ' -> *skipped' "$_bai_report_txt" 2>/dev/null); _bai_skip=${_bai_skip:-0}
        _bai_broken=$(grep -c ' -> *broken' "$_bai_report_txt" 2>/dev/null); _bai_broken=${_bai_broken:-0}
        _bai_total=$(( _bai_pass + _bai_fail + _bai_skip + _bai_broken ))
    fi

    # If standalone --ai (no live run), find the most recent results dir
    # that corresponds to _bai_branch (matched via BRANCH_TAG in dir name or
    # sentinel embedded in the results dir). Fall back to absolute latest.
    if [ ! -f "$_bai_report_txt" ]; then
        # Derive a safe tag from the branch for directory matching
        _bai_btag=$(printf '%s' "$_bai_branch" | tr '/' '-' | tr -cd 'a-zA-Z0-9-')
        # Try branch-specific dir first (contains branch tag in name)
        _bai_report_txt=$(ls -t /usr/local/www/darkhttpd/results-*/report.txt 2>/dev/null \
            | while IFS= read -r _r; do
                _dir=$(dirname "$_r")
                # Accept if branch tag appears in dir name OR dir has matching sentinel ref
                if printf '%s' "$_dir" | grep -q "$_bai_btag" 2>/dev/null; then
                    printf '%s\n' "$_r"; break
                fi
              done)
        # No branch-specific match — use the absolute latest non-skipped result
        if [ -z "$_bai_report_txt" ]; then
            _bai_report_txt=$(ls -t /usr/local/www/darkhttpd/results-*/report.txt 2>/dev/null \
                | grep -v skipped | head -1)
        fi
        # Try last-run log as last resort
        [ -z "$_bai_report_txt" ] && _bai_report_txt="/var/log/ibs-last-run.log"

        # Extract counts from the report
        if [ -f "$_bai_report_txt" ]; then
            _bai_pass=$(grep -c ' -> *passed'  "$_bai_report_txt" 2>/dev/null); _bai_pass=${_bai_pass:-0}
            _bai_fail=$(grep -c ' -> *failed'  "$_bai_report_txt" 2>/dev/null); _bai_fail=${_bai_fail:-0}
            _bai_skip=$(grep -c ' -> *skipped' "$_bai_report_txt" 2>/dev/null); _bai_skip=${_bai_skip:-0}
            _bai_broken=$(grep -c ' -> *broken' "$_bai_report_txt" 2>/dev/null); _bai_broken=${_bai_broken:-0}
            _bai_total=$(( ${_bai_pass:-0} + ${_bai_fail:-0} + ${_bai_skip:-0} + ${_bai_broken:-0} ))
        fi
    fi

    # Flag whether there is enough data to perform a meaningful analysis
    # Sufficient = at least 1 test result recorded
    _bai_has_data=0
    [ "${_bai_total:-0}" -gt 0 ] && _bai_has_data=1

    # Matrix (pipe-delimited: tc|cat|sev|status|dur)
    _bai_matrix=""
    if [ -n "${TMP_MATRIX:-}" ] && [ -f "$TMP_MATRIX" ]; then
        _bai_matrix=$(cat "$TMP_MATRIX" 2>/dev/null)
    fi

    # Diagnostic blocks for non-passed tests (from report.txt)
    _bai_diag_failed=""
    _bai_diag_skipped=""
    _bai_diag_detail=""
    if [ -f "$_bai_report_txt" ]; then
        _bai_diag_failed=$(grep -E "^[a-z_].*:.*->  (failed|broken):" "$_bai_report_txt" \
            | sed 's/  ->  / → /' | head -40)
        _bai_diag_skipped=$(grep -E "^[a-z_].*:.*->  skipped:" "$_bai_report_txt" \
            | sed 's/  ->  skipped: / → skip: /' | head -40)
        # Rich per-failure detail: the actual ATF_CHECK_MSG output (*** Check
        # failed: file:line: msg) and the report's own Root cause: analysis.
        # Without this, BeastAI only sees "1 checks failed" and wrongly
        # concludes tests lack descriptive messages.
        _bai_diag_detail=$({
            grep -E "^\*\*\* Check failed:" "$_bai_report_txt" 2>/dev/null | head -40
            grep -E "^[[:space:]]*Root cause:" "$_bai_report_txt" 2>/dev/null | head -40
        })
    fi

    # Lists for learning (space-separated test IDs)
    _bai_failed_ids=$(printf '%s' "$_bai_diag_failed" | awk -F' →' '{print $1}' | tr '\n' ' ')
    _bai_skipped_ids=$(printf '%s' "$_bai_diag_skipped" | awk -F' →' '{print $1}' | tr '\n' ' ')

    # Determine verdict if not known
    if [ "$_bai_verdict" = "unknown" ] && [ -f "$_bai_report_txt" ]; then
        _bai_verdict=$(grep -m1 'APPROVED\|NOT APPROVED\|CONDITIONAL' "$_bai_report_txt" \
            | sed 's/.*Verdict.*: //' | tr -d '[:space:]' | head -c 15)
        [ -z "$_bai_verdict" ] && _bai_verdict="unknown"
    fi
}

# Fetch recent PRs from GitHub (ojanerif/freebsd-src) and sos-git delta
_bai_collect_prs() {
    _bai_prs=""

    # GitHub PRs via gh CLI
    if command -v gh >/dev/null 2>&1; then
        _bai_prs=$(gh pr list \
            --repo ojanerif/freebsd-src \
            --limit 8 \
            --json number,title,state,additions,deletions,createdAt \
            --template '{{range .}}PR #{{.number}}: {{.title}} ({{.state}}, +{{.additions}}/-{{.deletions}}, {{.createdAt}})
{{end}}' 2>/dev/null || echo "(gh pr list unavailable)")
    fi

    # sos-git commit delta between dev and main
    _bai_sos_delta=""
    if [ -d "${SRC_DIR:-}/.git" ] || [ -d "${SCRIPT_DIR}/freebsd-src/.git" ]; then
        _src="${SRC_DIR:-${SCRIPT_DIR}/freebsd-src}"
        _bai_sos_delta=$(git -C "$_src" log \
            sos-git/main..sos-git/dev \
            --oneline --no-walk=unsorted \
            --max-count=15 2>/dev/null \
            | head -15)
    fi
}

# ── Prompt assembly ───────────────────────────────────────────────────────────

beast_ai_build_prompt() {
    cat <<PROMPT
## Run Context
Branch   : ${_bai_branch}
Commit   : ${_bai_commit}
KernConf : ${_bai_kernconf}
Suite    : ${_bai_suite}
Date     : $(_bai_date)
Verdict  : ${_bai_verdict}
Total    : ${_bai_total}  Pass: ${_bai_pass}  Fail: ${_bai_fail}  Skip: ${_bai_skip}  Broken: ${_bai_broken}

## Test Matrix (tc|category|severity|status|duration_s)
${_bai_matrix:-"(matrix not available — using report.txt summary below)"}

## Failed / Broken Tests
${_bai_diag_failed:-"(none)"}

## Failure Diagnostics — ATF check messages + report root-cause
${_bai_diag_detail:-"(no detailed diagnostics captured — do NOT assume tests lack ATF_CHECK_MSG; absence here means the detail was not collected, not that the test is non-compliant)"}

## Skipped Tests
${_bai_diag_skipped:-"(none)"}

## Recent PRs — ojanerif/freebsd-src (GitHub)
${_bai_prs:-"(not available)"}

## Commit Delta — sos-git dev vs main
${_bai_sos_delta:-"(not available)"}

## Prior Decisions & Human Corrections (your memory)
${_bai_memory:-"(no prior memory for this branch)"}

---
Analyze this run. Use the "Failure Diagnostics" section as the authoritative source for each failure's actual ATF message — do NOT claim a test lacks a descriptive message unless that section confirms it.

Before finalizing, review the "Prior Decisions & Human Corrections" section:
- Human corrections (source=human) OVERRIDE your own prior assessments — honor them explicitly and say so.
- If a finding contradicts a past call you made, note what changed and why ("Revised from prior run: ...").
- Do not repeat an assessment a human already corrected.

1. **Executive Summary** — overall verdict confidence, key findings from this run
2. **Failures Analysis** — per failed/broken test: root cause (cite the exact ATF message from Failure Diagnostics), Scope (QA/Dev/Kernel-Dev/CI), genuine FreeBSD style(9)/ATF compliance issues only, priority (P1/P2/P3)
3. **Skips Analysis** — classify each skip: expected-hardware-limit | actionable-gap | regression-risk; assign Scope to actionable skips
4. **PR Review** — for each PR: FreeBSD style(9) compliance, ATF conventions, AMD PMU subsystem patterns; flag any red lines
5. **Prioritized Action List** — top 5 items with columns: Priority | Scope | Owner | Ticket Type | Action
6. **Self-Review vs Memory** — explicitly list any prior decisions honored, revised, or superseded by human corrections this run
PROMPT
}

# ── Claude CLI call ───────────────────────────────────────────────────────────

# beast_ai_call_cli <prompt_file>
# Pipes the prompt into the system claude CLI in non-interactive (--print) mode.
# Uses --allowedTools "" to prevent any tool use (pure text generation).
# --bare strips hooks/LSP/memory so the call is lightweight and self-contained.
beast_ai_call_cli() {
    _prompt_file="$1"
    _system_msg=$(cat "$BEAST_AI_SYSTEM_PROMPT" 2>/dev/null || \
        printf 'You are BeastAI, a senior FreeBSD/AMD PMU engineering consultant. Write in English only.')

    "$BEAST_AI_CLI" \
        --print \
        --model "$BEAST_AI_MODEL" \
        --system-prompt "$_system_msg" \
        --allowedTools "" \
        --bare \
        --output-format text \
        < "$_prompt_file"
}

# ── HTML report generation ────────────────────────────────────────────────────

beast_ai_html_report() {
    _ai_text="$1"
    _out_html="${RESULTS_DIR}/beast-ai-report.html"

    # Convert minimal Markdown to HTML (headers, bold, lists, line breaks)
    _body_html=$(printf '%s\n' "$_ai_text" | awk '
        /^## / { gsub(/^## /,""); printf "<h2>%s</h2>\n", $0; next }
        /^\*\*/ {
            line = $0
            while (match(line, /\*\*[^*]+\*\*/)) {
                pre  = substr(line, 1, RSTART-1)
                mid  = substr(line, RSTART+2, RLENGTH-4)
                line = pre "<strong>" mid "</strong>" substr(line, RSTART+RLENGTH)
            }
            print line; next
        }
        /^[0-9]+\. / { printf "<li>%s</li>\n", substr($0, index($0," ")+1); next }
        /^- /        { printf "<li>%s</li>\n", substr($0, 3); next }
        /^$/         { print "<br>"; next }
        { print $0 "<br>" }
    ')

    _commit_short=$(_bai_short_commit "$_bai_commit")
    _run_date=$(date)

    cat > "$_out_html" <<HTML
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>BeastAI Report — ${_bai_branch} @ ${_commit_short}</title>
<style>
  :root{--bg:#0d1117;--surface:#161b22;--border:#30363d;--text:#c9d1d9;--muted:#8b949e;
        --green:#3fb950;--red:#f85149;--yellow:#d29922;--blue:#388bfd;--accent:#58a6ff}
  body{background:var(--bg);color:var(--text);font-family:ui-monospace,monospace;
       font-size:13px;margin:0;padding:24px}
  h1{font-size:1.4em;border-bottom:1px solid var(--border);padding-bottom:8px;
     color:var(--accent);margin-top:0}
  h2{font-size:1.1em;color:var(--blue);border-left:3px solid var(--accent);
     padding-left:10px;margin-top:24px}
  .badge{display:inline-block;padding:2px 8px;border-radius:4px;font-size:0.85em;font-weight:700}
  .approved{background:#1a4a2e;color:var(--green);border:1px solid var(--green)}
  .rejected{background:#4a1a1a;color:var(--red);border:1px solid var(--red)}
  .conditional{background:#4a3a1a;color:var(--yellow);border:1px solid var(--yellow)}
  .unknown{background:#1c2230;color:var(--muted);border:1px solid var(--border)}
  .meta{display:flex;gap:24px;flex-wrap:wrap;background:var(--surface);
        border:1px solid var(--border);border-radius:6px;padding:12px;margin:16px 0}
  .meta-item{display:flex;flex-direction:column}
  .meta-label{color:var(--muted);font-size:0.8em;text-transform:uppercase;letter-spacing:.05em}
  .meta-value{color:var(--text);font-weight:600}
  .content{background:var(--surface);border:1px solid var(--border);border-radius:6px;
           padding:16px;line-height:1.6;white-space:pre-wrap;word-break:break-word}
  .footer{margin-top:24px;color:var(--muted);font-size:0.8em;border-top:1px solid var(--border);
          padding-top:8px}
  a{color:var(--accent)}
  li{margin:2px 0}
</style>
</head>
<body>
<h1>&#129302; BeastAI — Senior Engineering Consultant Report</h1>

<div class="meta">
  <div class="meta-item">
    <span class="meta-label">Branch</span>
    <span class="meta-value">${_bai_branch}</span>
  </div>
  <div class="meta-item">
    <span class="meta-label">Commit</span>
    <span class="meta-value">${_commit_short}</span>
  </div>
  <div class="meta-item">
    <span class="meta-label">KernConf</span>
    <span class="meta-value">${_bai_kernconf}</span>
  </div>
  <div class="meta-item">
    <span class="meta-label">Suite</span>
    <span class="meta-value">${_bai_suite}</span>
  </div>
  <div class="meta-item">
    <span class="meta-label">Results</span>
    <span class="meta-value">${_bai_pass}✓ ${_bai_fail}✗ ${_bai_skip}⊘ / ${_bai_total}</span>
  </div>
  <div class="meta-item">
    <span class="meta-label">CI Verdict</span>
    <span class="badge $(printf '%s' "$_bai_verdict" | tr '[:upper:]' '[:lower:]' | tr ' ' '-')">${_bai_verdict}</span>
  </div>
</div>

<div class="content">
${_body_html}
</div>

<div class="footer">
  BeastAI report generated ${_run_date} &nbsp;|&nbsp;
  Model: ${BEAST_AI_MODEL} &nbsp;|&nbsp;
  <a href="report.html">CI Report</a> &nbsp;|&nbsp;
  <a href="report.txt">report.txt</a>
</div>
</body>
</html>
HTML

    log_success "BeastAI HTML report: ${_out_html}"
    printf '%s' "$_out_html"
}

# ── Markdown → plain text (flowing paragraphs for email body) ────────────────

# Strips markdown symbols and produces clean, readable plain text.
# Tables become indented lines. Bullets become "- text". Headers become
# ALL-CAPS underlined with dashes. Bold/italic markers removed.
_bai_md_to_plain() {
    printf '%s\n' "$1" | awk '
    function strip_inline(s,    out, i, c, bold) {
        # Remove **bold**, *italic*, `code`, and emoji sequences (&#..;)
        gsub(/\*\*/, "", s)
        gsub(/\*/, "", s)
        gsub(/`/, "", s)
        gsub(/&#[0-9]+;/, "", s)
        gsub(/:[a-z_]+:/, "", s)
        return s
    }
    # Emit a buffered markdown table with every column padded to its widest
    # cell, so the "|" separators line up across all rows.
    function flush_table(   i, j, line, cell, pad) {
        for (i = 1; i <= nrows; i++) {
            line = "  "
            for (j = 1; j <= ncols; j++) {
                cell = tbl[i, j]
                line = line cell
                if (j < ncols) {
                    pad = maxw[j] - length(cell)
                    while (pad-- > 0) line = line " "
                    line = line "  |  "
                }
            }
            print line
        }
        for (i = 1; i <= nrows; i++)
            for (j = 1; j <= ncols; j++) delete tbl[i, j]
        for (j = 1; j <= ncols; j++) delete maxw[j]
        nrows = 0; ncols = 0
    }
    # Table rows: buffer them so columns can be width-aligned on flush.
    /^\|/ {
        if ($0 ~ /^\| *[-:| ]+\|/) next   # separator row (|---|---|)
        line = $0
        gsub(/^\|/, "", line)
        gsub(/\|$/, "", line)
        n = split(line, cols, "|")
        nrows++
        if (n > ncols) ncols = n
        for (i = 1; i <= n; i++) {
            cell = cols[i]
            gsub(/^ +| +$/, "", cell)
            tbl[nrows, i] = strip_inline(cell)
            if (length(tbl[nrows, i]) > maxw[i]) maxw[i] = length(tbl[nrows, i])
        }
        next
    }
    # Any non-table line: flush a pending table (aligned), then fall through.
    { if (nrows > 0) flush_table() }
    /^---+$/ { print ""; next }
    /^## / {                            # section header
        title = substr($0, 4)
        title = strip_inline(title)
        printf "\n%s\n", toupper(title)
        printf "%s\n", substr("------------------------------------------------------------", 1, length(title))
        next
    }
    /^### / {
        title = substr($0, 5)
        title = strip_inline(title)
        printf "\n%s:\n", title
        next
    }
    /^> / {                             # blockquote
        print "  " strip_inline(substr($0, 3))
        next
    }
    /^[0-9]+\. / {                      # numbered list
        sub(/^[0-9]+\. /, "")
        print "  " strip_inline($0)
        next
    }
    /^- / {
        print "- " strip_inline(substr($0, 3))
        next
    }
    /^\*\* / {
        print strip_inline($0)
        next
    }
    /^$/ { print ""; next }
    { print strip_inline($0) }
    END { if (nrows > 0) flush_table() }
    ' | awk '
    # Collapse 3+ consecutive blank lines to 1
    /^$/ { blank++; if (blank <= 1) print ""; next }
    { blank = 0; print }
    '
}

# ── Email ─────────────────────────────────────────────────────────────────────

beast_ai_email() {
    _ai_html="$1"
    _ai_text_body="$2"
    _plain_body=$(_bai_md_to_plain "$_ai_text_body")
    _commit_short=$(_bai_short_commit "$_bai_commit")
    _subject="[BeastAI] ${_bai_branch} @ ${_commit_short}: ${_bai_verdict} — $(_bai_date)"
    _report_txt="${RESULTS_DIR:-}/report.txt"
    _boundary="----=_BeastAIPart_$(date +%s)_$$"
    _sep="--${_boundary}"

    if [ "$DRY_RUN" -eq 1 ]; then
        log_info "BeastAI (dry run): would email '${_subject}' to '${BEAST_AI_EMAIL}'"
        return 0
    fi

    {
        printf 'From: %s\n' "$SENDER_EMAIL"
        printf 'To: %s\n' "$BEAST_AI_EMAIL"
        printf 'Subject: %s\n' "$_subject"
        printf 'MIME-Version: 1.0\n'
        printf 'Content-Type: multipart/mixed; boundary="%s"\n\n' "$_boundary"
        printf '%s\n' "$_sep"
        printf 'Content-Type: text/plain; charset=utf-8\n\n'
        printf '%s\n\n' "$_plain_body"
        if [ -f "$_ai_html" ]; then
            printf '%s\n' "$_sep"
            printf 'Content-Type: text/html; name="beast-ai-report.html"\n'
            printf 'Content-Disposition: attachment; filename="beast-ai-report.html"\n\n'
            cat "$_ai_html"
            printf '\n'
        fi
        if [ -f "$_report_txt" ]; then
            printf '%s\n' "$_sep"
            printf 'Content-Type: text/plain; name="report.txt"\n'
            printf 'Content-Disposition: attachment; filename="report.txt"\n\n'
            cat "$_report_txt"
            printf '\n'
        fi
        printf '%s--\n' "$_sep"
    } | sendmail -f "$SENDER_EMAIL" "$BEAST_AI_EMAIL"

    log_success "BeastAI email sent to: ${BEAST_AI_EMAIL}"
}

# ── Main entry point ──────────────────────────────────────────────────────────

beast_ai_analyze() {
    log_info "BeastAI: starting analysis..."

    _bai_check_cli || return 1

    # 1. Collect run data (sets _bai_has_data)
    _bai_collect_run_data
    _bai_collect_prs

    # 1b. Load prior decisions + human corrections so BeastAI reviews its own
    #     past calls, honors corrections, and avoids repeating mistakes.
    _bai_memory=$(beast_ai_load_memory "$_bai_branch")

    # 2. Skip analysis if there is no test data for this branch
    if [ "${_bai_has_data:-0}" -eq 0 ]; then
        log_warning "BeastAI: no test data found for branch '${_bai_branch}' — sending skip notice"
        if [ "$DRY_RUN" -eq 0 ]; then
            _skip_subject="[BeastAI] ${_bai_branch}: analysis skipped — no test data"
            _skip_body=$(printf 'BeastAI analysis skipped for branch: %s\nDate: %s\n\nNo test results were found for this branch. BeastAI requires at least one completed test run to perform analysis. Run the test suite first (--run-all or --auto), then invoke --ai.\n' \
                "$_bai_branch" "$(date)")
            printf 'From: %s\nTo: %s\nSubject: %s\n\n%s\n' \
                "$SENDER_EMAIL" "$BEAST_AI_EMAIL" "$_skip_subject" "$_skip_body" \
                | sendmail -f "$SENDER_EMAIL" "$BEAST_AI_EMAIL"
            log_success "BeastAI skip notice sent to: ${BEAST_AI_EMAIL}"
        else
            log_info "BeastAI (dry run): no data — would send skip notice to ${BEAST_AI_EMAIL}"
        fi
        return 0
    fi

    # 3. Build prompt file
    _prompt_tmp=$(mktemp /tmp/beastai_prompt_$$.XXXXXX)
    beast_ai_build_prompt > "$_prompt_tmp"

    if [ "$DRY_RUN" -eq 1 ]; then
        log_info "BeastAI (dry run): prompt written to ${_prompt_tmp}"
        log_info "BeastAI (dry run): would invoke: ${BEAST_AI_CLI} --print --model ${BEAST_AI_MODEL} --bare"
        cat "$_prompt_tmp"
        rm -f "$_prompt_tmp"
        return 0
    fi

    # Ensure RESULTS_DIR exists for output
    if [ -z "${RESULTS_DIR:-}" ] || [ ! -d "$RESULTS_DIR" ]; then
        RESULTS_DIR=$(ls -td /usr/local/www/darkhttpd/results-* 2>/dev/null | grep -v skipped | head -1)
        [ -z "$RESULTS_DIR" ] && RESULTS_DIR="/tmp/beastai-report-$$" && mkdir -p "$RESULTS_DIR"
    fi

    # 4. Call claude CLI (non-interactive, sandbox mode)
    log_info "BeastAI: invoking claude CLI (model: ${BEAST_AI_MODEL}, bare+no-tools)..."
    _ai_text=$(beast_ai_call_cli "$_prompt_tmp")
    _cli_rc=$?
    rm -f "$_prompt_tmp"
    if [ $_cli_rc -ne 0 ] || [ -z "$_ai_text" ]; then
        log_error "BeastAI: claude CLI returned no output (rc=${_cli_rc})"
        return 1
    fi

    # 5. Generate HTML report
    _ai_html=$(beast_ai_html_report "$_ai_text")

    # 6. Send email (BEAST_AI_EMAIL only — never $REPORT_EMAIL)
    beast_ai_email "$_ai_html" "$_ai_text"

    # 7. Auto-learn from this run
    beast_ai_learn "$_bai_branch" "$_bai_commit" "$_bai_verdict" \
        "$_bai_failed_ids" "$_bai_skipped_ids"

    log_success "BeastAI: analysis complete. Report: ${_ai_html}"
}
