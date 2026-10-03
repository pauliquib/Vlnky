#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Pavel Švec
# SPDX-License-Identifier: GPL-2.0-or-later
# Extract wave_texture.png next to system_plugin_bg.rco (for Plasma wallpaper).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
RCO="${1:-}"
if [[ -z "$RCO" ]]; then
  echo "usage: $0 /path/to/system_plugin_bg.rco" >&2
  exit 1
fi
PY="${ROOT}/plasma-wallpaper/contents/code/xmb_extract.py"
if [[ ! -f "$PY" ]]; then
  PY="${HOME}/.local/share/plasma/wallpapers/org.psvec.vlnky/contents/code/xmb_extract.py"
fi
exec python3 "$PY" "$RCO"
