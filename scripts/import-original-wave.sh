#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Pavel Švec
# SPDX-License-Identifier: GPL-2.0-or-later
# Extract the ORIGINAL Sony XMB wave (flash0:/vsh/resource/system_plugin_bg.rco) from an official
# PSP system update (EBOOT.PBP, e.g. the 6.61 updater) and add it to the wave collection.
#
#   bash scripts/import-original-wave.sh [EBOOT.PBP] [collection folder]
#
# Official updaters: https://archive.org/details/psp_ofw_firmwares
# Decryption uses pspdecrypt (GPL-3, https://github.com/John-K/pspdecrypt), built into
# ~/.cache/vlnky/tools on first use (needs git, gcc/g++, zlib-devel, openssl-devel).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

PBP="${1:-$ROOT/../EBOOT.PBP}"
COLLECTION="${2:-}"
if [[ -z "$COLLECTION" ]]; then
  for candidate in \
      "$(cat "$HOME/.local/share/plasma/wallpapers/org.psvec.vlnky/contents/config/default_collection.path" 2>/dev/null || true)" \
      "$ROOT/../176 XMB WAVES for 5.00/176 XMB WAVES"; do
    if [[ -n "$candidate" && -d "$candidate" ]]; then
      COLLECTION="$candidate"
      break
    fi
  done
fi
[[ -f "$PBP" ]] || { echo "error: updater not found: $PBP" >&2; exit 1; }
[[ -d "$COLLECTION" ]] || { echo "error: collection folder not found (pass it as 2nd argument)" >&2; exit 1; }

TOOLS="$HOME/.cache/vlnky/tools"
PSPDECRYPT="$TOOLS/pspdecrypt/pspdecrypt"
if [[ ! -x "$PSPDECRYPT" ]]; then
  mkdir -p "$TOOLS"
  rm -rf "$TOOLS/pspdecrypt"
  git clone --depth 1 https://github.com/John-K/pspdecrypt.git "$TOOLS/pspdecrypt"
  make -C "$TOOLS/pspdecrypt" -j"$(nproc)" CC=gcc CXX=g++ >/dev/null
fi

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
"$PSPDECRYPT" -A -O "$WORK/fw" "$PBP" >"$WORK/log.txt" 2>&1 || true

RCO="$WORK/fw/F0/vsh/resource/system_plugin_bg.rco"
if [[ ! -f "$RCO" ]]; then
  echo "error: system_plugin_bg.rco not found in the updater. pspdecrypt log:" >&2
  tail -20 "$WORK/log.txt" >&2
  exit 2
fi

VERSION="$(strings -n 6 "$PBP" | sed -n 's/.*Update ver \([0-9.]*\).*/\1/p' | head -1)"
DEST="$COLLECTION/PSP Original (OFW ${VERSION:-unknown})"
mkdir -p "$DEST"
cp -f "$RCO" "$DEST/system_plugin_bg.rco"
# The wave colour on the console comes from the XMB theme colour: pick "XMB colour" in the
# wallpaper settings (Original = current month, or any of the 12 month colours).
echo "Original wave installed: $DEST/system_plugin_bg.rco"
echo "Select \"$(basename "$DEST")\" in the wallpaper settings and choose an XMB colour."
