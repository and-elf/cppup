#!/usr/bin/env bash
# Coverage threshold gate for the optional pre-commit coverage check.
#
# Standalone so the threshold/parsing logic is unit-testable without paying
# for a real coverage build (see scripts/tests/test_check_coverage.sh).
#
# Normally invoked with no arguments: runs `cppup test --coverage`, captures
# its output, and extracts the summary line it prints on success, shaped
# like:
#   [INFO] Coverage: 87.34% line coverage across 12 files; reports in ...
#
# For tests (or to gate on a coverage run captured elsewhere) pass a path to
# a text file containing that same kind of output as $1 — the file's
# content is parsed directly and cppup is never invoked.
#
# Usage:
#   scripts/check_coverage.sh [log-file]
#
# Env:
#   CPPUP_MIN_COVERAGE   minimum acceptable total line-coverage percentage
#                        (default: 70)
#   CPPUP_COVERAGE_BIN   path to the cppup binary to run when no log-file is
#                        given (default: resolved like .githooks/pre-commit —
#                        PATH, then bootstrap_build/, then build/)
#
# Exit codes:
#   0  coverage >= threshold
#   1  coverage < threshold, or `cppup test --coverage` itself failed
#   2  usage error: bad args, missing binary, or no coverage line found

set -uo pipefail

min_coverage="${CPPUP_MIN_COVERAGE:-70}"

usage() {
    echo "usage: $(basename "$0") [log-file]" >&2
}

if [[ $# -gt 1 ]]; then
    usage
    exit 2
fi

repo_root="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"

if [[ $# -eq 1 ]]; then
    log_file="$1"
    if [[ ! -f "$log_file" ]]; then
        echo "[check-coverage] log file not found: $log_file" >&2
        exit 2
    fi
    coverage_output="$(cat "$log_file")"
else
    cppup_bin="${CPPUP_COVERAGE_BIN:-}"
    if [[ -z "$cppup_bin" ]]; then
        if command -v cppup >/dev/null 2>&1; then
            cppup_bin="$(command -v cppup)"
        elif [[ -x "$repo_root/bootstrap_build/cppup" ]]; then
            cppup_bin="$repo_root/bootstrap_build/cppup"
        elif [[ -x "$repo_root/build/cppup" ]]; then
            cppup_bin="$repo_root/build/cppup"
        fi
    fi

    if [[ -z "$cppup_bin" ]]; then
        echo "[check-coverage] cppup binary not found (looked in PATH, bootstrap_build/, build/)." >&2
        echo "                 Run ./bootstrap.sh build first, or set CPPUP_COVERAGE_BIN." >&2
        exit 2
    fi

    echo "[check-coverage] Running: $cppup_bin test --coverage" >&2
    if ! coverage_output="$("$cppup_bin" test --coverage 2>&1)"; then
        echo "$coverage_output" >&2
        echo "[check-coverage] cppup test --coverage failed." >&2
        exit 1
    fi
    echo "$coverage_output" >&2
fi

# Take the last match in case the string appears more than once.
total_pct="$(printf '%s\n' "$coverage_output" \
    | grep -Eo 'Coverage:[[:space:]]+[0-9]+(\.[0-9]+)?%' \
    | tail -n1 \
    | grep -Eo '[0-9]+(\.[0-9]+)?')"

if [[ -z "$total_pct" ]]; then
    echo "[check-coverage] could not find a 'Coverage: NN.NN%' line in the test output." >&2
    echo "                 Was the project built with --coverage?" >&2
    exit 2
fi

# Floating point comparison: bash only does integers, so hand off to awk.
if awk -v c="$total_pct" -v m="$min_coverage" 'BEGIN { exit !(c + 0 >= m + 0) }'; then
    echo "[check-coverage] total coverage ${total_pct}% >= threshold ${min_coverage}% - OK"
    exit 0
else
    echo "[check-coverage] total coverage ${total_pct}% is BELOW threshold ${min_coverage}%" >&2
    exit 1
fi
