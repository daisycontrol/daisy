#!/usr/bin/env bash
set -euo pipefail

PIP_CONSTRAINT=$(pwd)/build-constraints.txt bazel run //python:requirements.update

cd "$(dirname "$0")"

python3 -m venv .venv
source .venv/bin/activate
PIP_CONSTRAINT=$(pwd)/build-constraints.txt pip install -r python/requirements_lock.txt