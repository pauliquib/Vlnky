#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Pavel Švec
# SPDX-License-Identifier: GPL-2.0-or-later
# Remove the per-user installation made by scripts/install.sh.
#   bash scripts/uninstall.sh            keep cache (~/.cache/vlnky)
#   bash scripts/uninstall.sh --purge    also delete cache, screenshots and helper tools
set -euo pipefail

kpackagetool6 -t Plasma/Wallpaper -r org.psvec.vlnky 2>/dev/null || true
rm -rf "$HOME/.local/share/plasma/wallpapers/org.psvec.vlnky"
for GLOBAL_QML in \
    "$HOME/.local/lib64/qml/org/psvec/vlnky" \
    "$HOME/.local/lib64/qt6/qml/org/psvec/vlnky"; do
  rm -rf "$GLOBAL_QML"
done
# Legacy install from before the rename (psp-xmb-wallpaper / org.psvec.pspxmbwave)
kpackagetool6 -t Plasma/Wallpaper -r org.psvec.pspxmbwave 2>/dev/null || true
rm -rf "$HOME/.local/share/plasma/wallpapers/org.psvec.pspxmbwave"
for GLOBAL_QML in \
    "$HOME/.local/lib64/qml/org/psvec/pspxmbwave" \
    "$HOME/.local/lib64/qt6/qml/org/psvec/pspxmbwave"; do
  rm -rf "$GLOBAL_QML"
done
if [[ "${1:-}" == "--purge" ]]; then
  rm -rf "$HOME/.cache/vlnky" "$HOME/.config/vlnky" "$HOME/.cache/psp-xmb-wave" "$HOME/.config/psp-xmb-wave"
fi
echo "Removed. Pick another wallpaper plugin if Vlnky was active, then restart Plasma:"
echo "  systemctl --user restart plasma-plasmashell.service"
