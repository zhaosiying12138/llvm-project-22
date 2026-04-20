#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

MODE="full"
SELF_TEST=0
while [[ "$#" -gt 0 ]]; do
  case "$1" in
  --quick)
    MODE="quick"
    shift
    ;;
  --self-test)
    SELF_TEST=1
    shift
    ;;
  *)
    break
    ;;
  esac
done

if [[ "$#" -ne 0 ]]; then
  echo "usage: $0 [--quick] [--self-test]" >&2
  exit 2
fi

PYTHON="${PYTHON:-python3}"
ARGS=(
  --repo-root "${REPO_ROOT}" \
  --bench-root "${SCRIPT_DIR}" \
  --mode "${MODE}"
)
if [[ "${SELF_TEST}" -eq 1 ]]; then
  ARGS+=(--self-test)
fi

exec "${PYTHON}" "${SCRIPT_DIR}/scripts/compare_compile.py" "${ARGS[@]}"
