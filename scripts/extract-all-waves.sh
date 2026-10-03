#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Pavel Švec
# SPDX-License-Identifier: GPL-2.0-or-later
# Generate wave_texture.png for every wave folder in a collection.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
COLLECTION="${1:-${WAVE_COLLECTION:-}}"
if [[ -z "$COLLECTION" ]]; then
  echo "usage: $(basename "$0") <collection folder>" >&2
  exit 1
fi
PY="${ROOT}/plasma-wallpaper/contents/code/xmb_extract.py"
if [[ ! -f "$PY" ]]; then
  PY="${HOME}/.local/share/plasma/wallpapers/org.psvec.vlnky/contents/code/xmb_extract.py"
fi
if [[ ! -d "$COLLECTION" ]]; then
  echo "Collection not found: $COLLECTION" >&2
  exit 1
fi
count=0
fail=0
while IFS= read -r -d '' rco; do
  if python3 "$PY" "$rco" >/dev/null; then
    count=$((count + 1))
    printf '\rExtracted %d: %s' "$count" "$(dirname "$rco")"
  else
    fail=$((fail + 1))
  fi
done < <(find "$COLLECTION" -mindepth 2 -maxdepth 2 -name system_plugin_bg.rco -print0)
echo ""
echo "Done: $count textures, $fail failed."
