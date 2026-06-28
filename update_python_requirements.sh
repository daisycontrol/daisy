#!/usr/bin/env bash
set -euo pipefail

bazel run //python:requirements.update

cd "$(dirname "$0")"

python3 -m venv .venv
source .venv/bin/activate
pip install -r python/requirements_lock.txt