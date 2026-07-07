#!/usr/bin/env bash
# Wire up the tracked .githooks/ directory as the git hooks path for this
# clone. Idempotent — run as many times as you like.

set -euo pipefail

repo_root="$(git -C "$(dirname "$0")" rev-parse --show-toplevel)"
cd "$repo_root"

chmod +x .githooks/* scripts/check_coverage.sh 2>/dev/null || true
git config core.hooksPath .githooks

echo "Git hooks enabled (core.hooksPath = .githooks)."
echo "Pre-commit will run: gitleaks (if installed), cppup format --check, cppup tidy."
echo "Escape hatches: CPPUP_SKIP_HOOKS=1, CPPUP_SKIP_FORMAT=1, CPPUP_SKIP_TIDY=1, CPPUP_SKIP_GITLEAKS=1."
echo "Optional coverage gate (off by default): CPPUP_CHECK_COVERAGE=1 to enable,"
echo "CPPUP_SKIP_COVERAGE=1 to skip for one commit, CPPUP_MIN_COVERAGE=N for the threshold."
