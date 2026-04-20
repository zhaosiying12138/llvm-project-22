#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

MODE="full"
if [[ "${1:-}" == "--quick" ]]; then
  MODE="quick"
  shift
fi

if [[ "$#" -ne 0 ]]; then
  echo "usage: $0 [--quick]" >&2
  exit 2
fi

PYTHON="${PYTHON:-python3}"
exec "${PYTHON}" "${SCRIPT_DIR}/scripts/compare_compile.py" \
  --repo-root "${REPO_ROOT}" \
  --bench-root "${SCRIPT_DIR}" \
  --mode "${MODE}"
