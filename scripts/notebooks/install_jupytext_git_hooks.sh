#!/usr/bin/env bash
set -euo pipefail

repo_root="$(git rev-parse --show-toplevel)"
cd "$repo_root"

chmod +x .githooks/pre-commit
chmod +x .githooks/post-merge
chmod +x .githooks/post-checkout
chmod +x .githooks/post-rewrite
chmod +x scripts/notebooks/jupytext_sync.py

git config core.hooksPath .githooks

echo "jupytext hooks installed at .githooks"
