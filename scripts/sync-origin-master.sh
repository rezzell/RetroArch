#!/usr/bin/env bash

set -euo pipefail

usage() {
  cat <<'EOF'
Usage: scripts/sync-origin-master.sh [fork-local-commit...]

Rebases the repository's master branch onto upstream/master while preserving
fork-local commits.

With explicit commit arguments (or FORK_LOCAL_COMMITS), the script rebases
master and then cherry-picks those commits if they are not already part of the
local-only stack.

Without explicit commits, the script detects the local-only commits currently on
master (the range upstream/master..master), prints them, and relies on rebase to
carry them forward automatically.

Examples:
  scripts/sync-origin-master.sh e5f233f65a abcdef1234
  FORK_LOCAL_COMMITS="e5f233f65a abcdef1234" scripts/sync-origin-master.sh
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  usage
  exit 0
fi

if ! git diff --quiet || ! git diff --cached --quiet; then
  echo "Worktree must be clean before syncing master." >&2
  exit 1
fi

if ! git show-ref --verify --quiet refs/heads/master; then
  echo "Local master branch is required." >&2
  exit 1
fi

git fetch upstream refs/heads/master:refs/remotes/upstream/master
git fetch origin refs/heads/master:refs/remotes/origin/master

auto_detect=0
mapfile -t fork_local_commits < <(
  if [[ "$#" -gt 0 ]]; then
    printf '%s\n' "$@"
  elif [[ -n "${FORK_LOCAL_COMMITS:-}" ]]; then
    printf '%s\n' ${FORK_LOCAL_COMMITS}
  else
    git rev-list --reverse upstream/master..master
  fi
)

if [[ "$#" -eq 0 && -z "${FORK_LOCAL_COMMITS:-}" ]]; then
  auto_detect=1
fi

if [[ "${#fork_local_commits[@]}" -gt 0 ]]; then
  echo "Fork-local commits to preserve:"
  for commit in "${fork_local_commits[@]}"; do
    git show --quiet --format='%h %s' "$commit"
  done
else
  echo "No fork-local commits detected or supplied."
fi

git checkout --quiet master
git rebase upstream/master

if [[ "${#fork_local_commits[@]}" -eq 0 ]]; then
  echo "Master is now rebased onto upstream/master."
  exit 0
fi

if [[ "${auto_detect}" -eq 1 ]]; then
  echo "Detected fork-local commits were carried forward by rebase."
  exit 0
fi

for commit in "${fork_local_commits[@]}"; do
  git cherry-pick "$commit"
done
