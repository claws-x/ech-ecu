#!/usr/bin/env bash
set -e

REPO_DIR="$HOME/work/ech-ecu"
cd "$REPO_DIR"

MSG="${1:-auto: update from OpenClaw}"

git status
git add .
git commit -m "$MSG" || {
  echo "Nothing to commit."
  exit 0
}
git push
