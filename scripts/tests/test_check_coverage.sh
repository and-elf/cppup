#!/usr/bin/env bash
# Tests for scripts/check_coverage.sh — the coverage-threshold gate behind
# the optional pre-commit coverage check (CPPUP_CHECK_COVERAGE=1).
#
# Feeds check_coverage.sh fixture "cppup test --coverage" log snippets via
# its log-file argument, so the threshold/parsing logic is exercised without
# paying for a real coverage build. Written before scripts/check_coverage.sh
# existed (red), then kept as the regression suite (green).
#
# Run directly: scripts/tests/test_check_coverage.sh

set -uo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../.." && pwd)"
check_coverage="$repo_root/scripts/check_coverage.sh"

work_dir="$(mktemp -d)"
trap 'rm -rf "$work_dir"' EXIT

failures=0
tests_run=0

# assert_status <expected_exit_code> <description> -- <command...>
assert_status() {
    local expected="$1"
    shift
    local description="$1"
    shift
    if [[ "${1:-}" == "--" ]]; then
        shift
    fi

    tests_run=$((tests_run + 1))
    "$@" >"$work_dir/stdout" 2>"$work_dir/stderr"
    local actual=$?

    if [[ "$actual" -eq "$expected" ]]; then
        echo "ok   - $description"
    else
        echo "FAIL - $description (expected exit $expected, got $actual)"
        echo "       stdout: $(cat "$work_dir/stdout")"
        echo "       stderr: $(cat "$work_dir/stderr")"
        failures=$((failures + 1))
    fi
}

write_fixture() {
    local name="$1" pct="$2"
    cat >"$work_dir/$name" <<EOF
[INFO] Running tests...
[INFO] Test summary: 42 passed, 0 failed
[INFO] Coverage: ${pct}% line coverage across 17 files; reports in build/coverage
EOF
}

write_fixture "high.log" "87.50"
write_fixture "low.log" "42.00"
write_fixture "exact.log" "70.00"

cat >"$work_dir/no_coverage.log" <<'EOF'
[INFO] Running tests...
[INFO] Test summary: 42 passed, 0 failed
EOF

# 1. Coverage comfortably above the default threshold (70%) -> pass.
assert_status 0 "coverage above default threshold (70%) passes" -- \
    env -u CPPUP_MIN_COVERAGE "$check_coverage" "$work_dir/high.log"

# 2. Coverage below the default threshold -> fail.
assert_status 1 "coverage below default threshold (70%) fails" -- \
    env -u CPPUP_MIN_COVERAGE "$check_coverage" "$work_dir/low.log"

# 3. Coverage exactly at the threshold -> pass (>=, not >).
assert_status 0 "coverage exactly at threshold passes" -- \
    env -u CPPUP_MIN_COVERAGE "$check_coverage" "$work_dir/exact.log"

# 4. CPPUP_MIN_COVERAGE override raises the bar past what would otherwise pass.
assert_status 1 "custom CPPUP_MIN_COVERAGE overrides the default" -- \
    env CPPUP_MIN_COVERAGE=90 "$check_coverage" "$work_dir/high.log"

# 5. Custom threshold low enough that normally-failing coverage now passes.
assert_status 0 "custom CPPUP_MIN_COVERAGE can lower the bar" -- \
    env CPPUP_MIN_COVERAGE=10 "$check_coverage" "$work_dir/low.log"

# 6. Missing coverage line in the log -> parse error, not a silent pass.
assert_status 2 "missing coverage line is a parse error, not a pass" -- \
    env -u CPPUP_MIN_COVERAGE "$check_coverage" "$work_dir/no_coverage.log"

# 7. Nonexistent log file -> usage error.
assert_status 2 "nonexistent log file is a usage error" -- \
    "$check_coverage" "$work_dir/does_not_exist.log"

# 8. Too many arguments -> usage error.
assert_status 2 "extra arguments are a usage error" -- \
    "$check_coverage" "$work_dir/high.log" "extra-arg"

echo
if [[ "$failures" -eq 0 ]]; then
    echo "All $tests_run check_coverage.sh tests passed."
    exit 0
else
    echo "$failures/$tests_run check_coverage.sh tests FAILED."
    exit 1
fi
