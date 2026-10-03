#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Pavel Švec
# SPDX-License-Identifier: GPL-2.0-or-later
# Parse every system_plugin_bg.rco in a wave collection; report failures.
set -euo pipefail

COLLECTION="${1:-}"
BIN="${2:-}"

if [[ -z "$COLLECTION" ]]; then
  echo "Usage: $0 <wave-collection-dir> [vlnky-extract-binary]" >&2
  exit 1
fi

if [[ -z "$BIN" ]]; then
  SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  BIN="$SCRIPT_DIR/../build/vlnky-extract"
fi

if [[ ! -x "$BIN" ]]; then
  echo "error: missing $BIN (build the project first)" >&2
  exit 2
fi

ok=0
fail=0
csv="$(mktemp)"
echo "wave,status,detail" >"$csv"

while IFS= read -r -d '' rco; do
  wave="$(basename "$(dirname "$rco")")"
  if out="$("$BIN" "$rco" 2>&1)"; then
    echo "OK  $wave"
    echo "$wave,ok," >>"$csv"
    ok=$((ok + 1))
  else
    echo "FAIL $wave: $out"
    echo "$wave,fail,\"${out//\"/\"\"}\"" >>"$csv"
    fail=$((fail + 1))
  fi
done < <(find "$COLLECTION" -name 'system_plugin_bg.rco' -print0)

echo "---"
echo "OK: $ok  FAIL: $fail"
echo "Report: $csv"
exit $(( fail > 0 ? 1 : 0 ))
