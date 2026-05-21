#!/bin/sh
# dev-docs/view.sh — dev-docs v3.4 Team Edition interactive viewer
# POSIX sh, FreeBSD compatible, no external dependencies beyond less/more

# ─── Paths ────────────────────────────────────────────────────────────────────
DD="$(cd "$(dirname "$0")" && pwd)"   # dev-docs/ directory (this script lives here)
ROOT="$(dirname "$DD")"               # project root

# ─── Temp directory ───────────────────────────────────────────────────────────
TMPD="/tmp/devdocs_view_$$"
mkdir -p "$TMPD"
trap 'rm -rf "$TMPD"' EXIT INT TERM HUP

# ─── Colors (only when stdout is a terminal) ──────────────────────────────────
if [ -t 1 ]; then
    B='\033[1m'   D='\033[2m'   R='\033[0m'
    C0='\033[0m'  C1='\033[32m' C2='\033[33m' C3='\033[31m'
    C4='\033[36m' C5='\033[37m' C6='\033[34m' C7='\033[35m'
    CYN='\033[36m' GRN='\033[32m' YLW='\033[33m'
    RED='\033[31m' BLU='\033[34m' WHT='\033[37m' MAG='\033[35m'
else
    B='' D='' R=''
    C0='' C1='' C2='' C3='' C4='' C5='' C6='' C7=''
    CYN='' GRN='' YLW='' RED='' BLU='' WHT='' MAG=''
fi

# ─── Pager ────────────────────────────────────────────────────────────────────
PAGER="${PAGER:-less}"
command -v "$PAGER" >/dev/null 2>&1 || PAGER="more"
export LESS="-R --quit-if-one-screen"

# ─── Separator ────────────────────────────────────────────────────────────────
SEP="────────────────────────────────────────────────────────────────────────────"

# ─── JSONL field extractor (no jq required) ───────────────────────────────────
# Usage: jf <field> <json_line>
jf() {
    local field="$1" line="$2"
    printf '%s' "$line" | sed -n "s/.*\"$field\":\"\([^\"]*\)\".*/\1/p" | head -1
}

# ─── UI primitives ────────────────────────────────────────────────────────────
cls()   { printf '\033[2J\033[H'; }
pause() { printf "\n${D}  Press Enter to continue...${R}"; read _r; }

hdr() {
    printf "${B}${CYN}"
    printf '╔══════════════════════════════════════════════════════╗\n'
    printf '║       dev-docs v3.4  ·  freebsd-ci-actions          ║\n'
    printf '╚══════════════════════════════════════════════════════╝\n'
    printf "${R}"
}

section() {
    printf "\n${B}${YLW}── %s ${R}\n" "$1"
}

# ─── Stats bar ────────────────────────────────────────────────────────────────
stats() {
    local m=0 dec=0 bug=0 todo=0 snip=0 u=0 ev=0
    [ -f "$DD/index.jsonl" ]          && m=$(grep -c .   "$DD/index.jsonl"          2>/dev/null || echo 0)
    [ -f "$DD/team/users.jsonl" ]     && u=$(grep -c .   "$DD/team/users.jsonl"     2>/dev/null || echo 0)
    [ -f "$DD/team/audit-log.jsonl" ] && ev=$(grep -c .  "$DD/team/audit-log.jsonl" 2>/dev/null || echo 0)
    dec=$(grep  -rl "\[DECISION\]" "$DD/modules" "$DD/infra" "$DD/apis" "$DD/_global.md" 2>/dev/null | \
          xargs grep -h "\[DECISION\]" 2>/dev/null | wc -l | tr -d ' ')
    bug=$(grep  -rl "\[BUG\]"      "$DD/modules" "$DD/infra" "$DD/apis" "$DD/_global.md" 2>/dev/null | \
          xargs grep -h "\[BUG\]"      2>/dev/null | wc -l | tr -d ' ')
    todo=$(grep -rl "\[TODO\]"     "$DD/modules" "$DD/infra" "$DD/apis" "$DD/_global.md" 2>/dev/null | \
           xargs grep -h "\[TODO\]"    2>/dev/null | wc -l | tr -d ' ')
    snip=$(grep -rl "\[SNIPPET\]"  "$DD/modules" "$DD/infra" "$DD/apis" "$DD/_global.md" 2>/dev/null | \
           xargs grep -h "\[SNIPPET\]" 2>/dev/null | wc -l | tr -d ' ')

    printf "  ${D}modules${R} ${B}%s${R}  " "$m"
    printf "${D}decisions${R} ${B}${YLW}%s${R}  " "$dec"
    printf "${D}bugs${R} ${B}${RED}%s${R}  " "$bug"
    printf "${D}todos${R} ${B}${CYN}%s${R}  " "$todo"
    printf "${D}snippets${R} ${B}%s${R}  " "$snip"
    printf "${D}users${R} ${B}${GRN}%s${R}  " "$u"
    printf "${D}audit events${R} ${B}${BLU}%s${R}\n" "$ev"
}

# ─── Validate numeric choice ──────────────────────────────────────────────────
is_num() {
    case "$1" in ''|*[!0-9]*) return 1 ;; esac
    return 0
}

# ─── 1. Modules & Files ───────────────────────────────────────────────────────
view_modules() {
    local idx="$DD/index.jsonl"
    while true; do
        cls; hdr
        section "Modules & Files"

        if [ ! -f "$idx" ]; then
            printf "  ${RED}index.jsonl not found${R}\n"; pause; return
        fi

        # Build numbered list; store file paths in temp file (line N = choice N)
        local listf="$TMPD/mod_list"
        : > "$listf"
        local i=0

        while IFS= read -r line; do
            [ -z "$line" ] && continue
            i=$((i+1))
            local id type desc file status modified
            id=$(jf id "$line"); type=$(jf type "$line")
            desc=$(jf description "$line"); file=$(jf file "$line")
            status=$(jf status "$line"); modified=$(jf last_modified "$line")

            case "$type" in
                module) tc="$GRN" ;;
                infra)  tc="$BLU" ;;
                api)    tc="$YLW" ;;
                *)      tc="$WHT" ;;
            esac
            case "$status" in
                active)     sc="$GRN" ;;
                paused)     sc="$YLW" ;;
                deprecated) sc="$RED" ;;
                *)          sc="$WHT" ;;
            esac

            printf "  ${B}%2d)${R}  ${tc}%-28s${R}  ${D}[%-6s]${R}  ${D}%-10s${R}  %s\n" \
                "$i" "$id" "$type" "$modified" "$desc"
            printf '%s\n' "$ROOT/$file" >> "$listf"
        done < "$idx"

        printf "\n  ${B}${D} 0)${R}  Back\n"
        printf "\n${B}Select: ${R}"; read choice

        [ "$choice" = "0" ] || [ -z "$choice" ] && return
        is_num "$choice" || continue
        [ "$choice" -lt 1 ] || [ "$choice" -gt "$i" ] && continue

        local fpath
        fpath=$(sed -n "${choice}p" "$listf")
        if [ -f "$fpath" ]; then
            $PAGER "$fpath"
        else
            printf "\n  ${RED}File not found: %s${R}\n" "$fpath"; pause
        fi
    done
}

# ─── 2–5. Entry browser (DECISION, BUG, TODO, SNIPPET) ───────────────────────
view_entries() {
    local etype="$1" color="$2" label="$3"

    # Build a temp file listing all markdown source files to scan
    local scanlist="$TMPD/scan_list"
    find "$DD/modules" "$DD/infra" "$DD/apis" -name "*.md" 2>/dev/null | sort > "$scanlist"
    [ -f "$DD/_global.md" ] && printf '%s\n' "$DD/_global.md" >> "$scanlist"

    while true; do
        cls; hdr
        section "$label"

        local entryf="$TMPD/entry_files"
        : > "$entryf"
        local i=0

        while IFS= read -r mdfile; do
            [ -f "$mdfile" ] || continue
            local modname
            modname=$(basename "$mdfile" .md)

            while IFS= read -r mdline; do
                case "$mdline" in
                    "## [$etype]"*)
                        i=$((i+1))
                        local title
                        title=$(printf '%s' "$mdline" | sed "s|## \[$etype\] *||")
                        printf "  ${B}%2d)${R}  ${color}%-24s${R}  %s\n" \
                            "$i" "$modname" "$title"
                        printf '%s\n' "$mdfile" >> "$entryf"
                        ;;
                esac
            done < "$mdfile"
        done < "$scanlist"

        if [ "$i" -eq 0 ]; then
            printf "\n  ${D}No %s entries found.${R}\n" "$label"; pause; return
        fi

        printf "\n  ${B}${D} 0)${R}  Back\n"
        printf "\n${B}Select to open source file: ${R}"; read choice

        [ "$choice" = "0" ] || [ -z "$choice" ] && return
        is_num "$choice" || continue
        [ "$choice" -lt 1 ] || [ "$choice" -gt "$i" ] && continue

        local mf
        mf=$(sed -n "${choice}p" "$entryf")
        [ -f "$mf" ] && $PAGER "$mf" || { printf "\n  ${RED}Not found: %s${R}\n" "$mf"; pause; }
    done
}

# ─── 6. Team & Users ─────────────────────────────────────────────────────────
view_team() {
    cls; hdr
    section "Team & Users"

    local ufile="$DD/team/users.jsonl"
    if [ ! -f "$ufile" ]; then
        printf "\n  ${RED}team/users.jsonl not found${R}\n"; pause; return
    fi

    printf "\n  ${B}%-22s  %-14s  %-14s  %-12s  %s${R}\n" \
        "Display Name" "Handle" "Role" "Actor Type" "Team"
    printf "  ${D}%s${R}\n" "$SEP"

    while IFS= read -r line; do
        [ -z "$line" ] && continue
        local name handle role atype team last
        name=$(jf display_name "$line"); handle=$(jf username "$line")
        role=$(jf role "$line"); atype=$(jf actor_type "$line")
        team=$(jf team "$line"); last=$(jf last_seen "$line")
        [ -z "$handle" ] && handle="-"
        [ -z "$team"   ] && team="-"
        [ -z "$last"   ] && last="-"

        case "$atype" in
            human)    ac="$GRN" ;;
            ai-agent) ac="$CYN" ;;
            *)        ac="$WHT" ;;
        esac

        printf "  ${B}${ac}%-22s${R}  %-14s  ${ac}%-14s${R}  ${D}%-12s${R}  %s\n" \
            "$name" "$handle" "$role" "$atype" "$team"
    done < "$ufile"

    section "Policies  (team/policies.yaml)"
    if [ -f "$DD/team/policies.yaml" ]; then
        grep -v '^#' "$DD/team/policies.yaml" | grep -v '^[[:space:]]*$' | \
            sed 's/^/  /'
    else
        printf "  ${D}Not found${R}\n"
    fi

    pause
}

# ─── 7. Audit Log ────────────────────────────────────────────────────────────
view_audit() {
    local afile="$DD/team/audit-log.jsonl"
    if [ ! -f "$afile" ] || ! grep -q . "$afile" 2>/dev/null; then
        cls; hdr
        printf "\n  ${D}No audit events yet.${R}\n"; pause; return
    fi

    {
        printf "${B}%-22s  %-22s  %-22s  %-24s  %s${R}\n" \
            "Timestamp" "Actor" "Action" "Target" "Summary"
        printf "${D}%s%s${R}\n" "$SEP" "$SEP"
        while IFS= read -r line; do
            [ -z "$line" ] && continue
            local ts actor action target summary
            ts=$(jf ts "$line" | cut -c1-19)
            actor=$(jf actor_name "$line")
            action=$(jf action "$line")
            target=$(jf target "$line")
            summary=$(jf summary "$line")
            printf "%-22s  %-22s  %-22s  %-24s  %s\n" \
                "$ts" "$actor" "$action" "$target" "$summary"
        done < "$afile"
    } | $PAGER
}

# ─── 8. Ownership Map ────────────────────────────────────────────────────────
view_ownership() {
    local f="$DD/team/ownership.yaml"
    if [ -f "$f" ]; then
        $PAGER "$f"
    else
        cls; hdr
        printf "\n  ${RED}team/ownership.yaml not found${R}\n"; pause
    fi
}

# ─── 9. Repo Map ─────────────────────────────────────────────────────────────
view_repos() {
    cls; hdr
    section "Repository Map  (repos.jsonl)"

    local rfile="$DD/repos.jsonl"
    if [ ! -f "$rfile" ]; then
        printf "\n  ${RED}repos.jsonl not found${R}\n"; pause; return
    fi

    printf "\n  ${B}%-14s  %-14s  %-8s  %-24s  %-18s  %s${R}\n" \
        "Repo ID" "Type" "Branch" "Path" "Scope" "Owner"
    printf "  ${D}%s${R}\n" "$SEP"

    while IFS= read -r line; do
        [ -z "$line" ] && continue
        local rid rtype branch path scope owner status
        rid=$(jf repo_id "$line"); rtype=$(jf type "$line")
        branch=$(jf branch "$line"); path=$(jf path "$line")
        scope=$(jf dev_docs_scope "$line"); owner=$(jf owner "$line")
        status=$(jf status "$line")
        [ -z "$branch" ] && branch="-"

        case "$rtype" in
            root)              tc="$GRN" ;;
            workspace-package) tc="$CYN" ;;
            submodule)         tc="$YLW" ;;
            nested-git)        tc="$MAG" ;;
            *)                 tc="$WHT" ;;
        esac

        printf "  ${B}%-14s${R}  ${tc}%-14s${R}  %-8s  %-24s  ${D}%-18s${R}  %s\n" \
            "$rid" "$rtype" "$branch" "$path" "$scope" "$owner"
    done < "$rfile"

    pause
}

# ─── 10. Global Patterns ─────────────────────────────────────────────────────
view_global() {
    local f="$DD/_global.md"
    if [ -f "$f" ]; then
        $PAGER "$f"
    else
        cls; hdr
        printf "\n  ${RED}_global.md not found${R}\n"; pause
    fi
}

# ─── 11. Search ───────────────────────────────────────────────────────────────
view_search() {
    while true; do
        cls; hdr
        section "Search"
        printf "\n  ${D}Searches all .md, .jsonl, .yaml files in dev-docs/${R}\n"
        printf "\n  Query (or Enter to cancel): "; read query
        [ -z "$query" ] && return

        {
            printf "${B}Search results for: %s${R}\n" "$query"
            printf "${D}%s${R}\n\n" "$SEP"
            grep -r --include="*.md" --include="*.jsonl" --include="*.yaml" \
                -n -- "$query" "$DD/" 2>/dev/null | \
                sed "s|$DD/||" | \
                while IFS= read -r result; do
                    printf "  %s\n" "$result"
                done
        } | $PAGER

        printf "\n  ${D}Search again?${R} (Enter for new query, 0 to back): "
        read again
        [ "$again" = "0" ] && return
    done
}

# ─── 12. All files ────────────────────────────────────────────────────────────
view_all_files() {
    while true; do
        cls; hdr
        section "All Files in dev-docs/"

        local listf="$TMPD/allfiles"
        find "$DD" -type f | sort | grep -v '/\.' > "$listf" 2>/dev/null

        local i=0
        while IFS= read -r fpath; do
            i=$((i+1))
            local rel
            rel=$(printf '%s' "$fpath" | sed "s|$DD/||")
            local ext
            ext=$(printf '%s' "$fpath" | sed 's/.*\.//')

            case "$ext" in
                md)    fc="$GRN" ;;
                jsonl) fc="$YLW" ;;
                yaml)  fc="$CYN" ;;
                sh)    fc="$MAG" ;;
                *)     fc="$WHT" ;;
            esac

            # Size
            local sz
            sz=$(wc -c < "$fpath" 2>/dev/null | tr -d ' ')
            printf "  ${B}%2d)${R}  ${fc}%-42s${R}  ${D}%6s bytes${R}\n" \
                "$i" "$rel" "$sz"
        done < "$listf"

        printf "\n  ${B}${D} 0)${R}  Back\n"
        printf "\n${B}Select to open: ${R}"; read choice

        [ "$choice" = "0" ] || [ -z "$choice" ] && return
        is_num "$choice" || continue
        [ "$choice" -lt 1 ] || [ "$choice" -gt "$i" ] && continue

        local fpath
        fpath=$(sed -n "${choice}p" "$listf")
        [ -f "$fpath" ] && $PAGER "$fpath" || { printf "\n  ${RED}Not found${R}\n"; pause; }
    done
}

# ─── Main menu ────────────────────────────────────────────────────────────────
while true; do
    cls
    hdr
    printf "\n"
    stats
    printf "\n"
    printf "  ${B}${GRN} 1)${R}  Modules & Files\n"
    printf "  ${B}${YLW} 2)${R}  Decisions\n"
    printf "  ${B}${RED} 3)${R}  Bugs\n"
    printf "  ${B}${CYN} 4)${R}  TODOs\n"
    printf "  ${B}${WHT} 5)${R}  Snippets\n"
    printf "  ${B}${GRN} 6)${R}  Team & Users\n"
    printf "  ${B}${BLU} 7)${R}  Audit Log\n"
    printf "  ${B}${YLW} 8)${R}  Ownership Map\n"
    printf "  ${B}${CYN} 9)${R}  Repo Map\n"
    printf "  ${B}${WHT}10)${R}  Global Patterns & Cross-cutting Decisions\n"
    printf "  ${B}${MAG}11)${R}  Search\n"
    printf "  ${B}${D}  12)${R}  All Files\n"
    printf "\n  ${B}${D}  0)${R}  Exit\n"
    printf "\n${B}Select: ${R}"; read choice

    case "$choice" in
         1) view_modules ;;
         2) view_entries "DECISION" "$YLW" "Decisions" ;;
         3) view_entries "BUG"      "$RED" "Bugs" ;;
         4) view_entries "TODO"     "$CYN" "TODOs" ;;
         5) view_entries "SNIPPET"  "$WHT" "Snippets" ;;
         6) view_team ;;
         7) view_audit ;;
         8) view_ownership ;;
         9) view_repos ;;
        10) view_global ;;
        11) view_search ;;
        12) view_all_files ;;
         0|q|Q) cls; exit 0 ;;
    esac
done
