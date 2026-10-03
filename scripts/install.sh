#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Pavel Švec
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

if ! ldd build/libvlnkywave.so >/dev/null 2>&1; then
  cmake -B build -DCMAKE_BUILD_TYPE=Release
  cmake --build build
else
  cmake --build build
fi

chmod +x "$ROOT/scripts/extract-wave.sh" "$ROOT/plasma-wallpaper/contents/code/extract-all-waves.sh"
kpackagetool6 -t Plasma/Wallpaper -i build/plasma-wallpaper/package 2>/dev/null || \
    kpackagetool6 -t Plasma/Wallpaper -u build/plasma-wallpaper/package

# Optional global paths (versioned import); wallpaper uses relative import in contents/ui/.
for GLOBAL_QML in \
    "${HOME}/.local/lib64/qml/org/psvec/vlnky" \
    "${HOME}/.local/lib64/qt6/qml/org/psvec/vlnky"; do
    mkdir -p "$GLOBAL_QML"
    cp -f build/libvlnkywave.so "$GLOBAL_QML/"
    cp -f build/qmldir.vlnkywave "$GLOBAL_QML/qmldir"
done

if [[ -z "${WAVE_COLLECTION:-}" ]]; then
  for candidate in \
      "$ROOT/../176 XMB WAVES for 5.00/176 XMB WAVES" \
      "$HOME/XMB waves/176 XMB WAVES for 5.00/176 XMB WAVES" \
      "$HOME/Downloads/176 XMB WAVES for 5.00/176 XMB WAVES"; do
    if [[ -f "$candidate/Blue Wire/system_plugin_bg.rco" || -f "$candidate/Alice/system_plugin_bg.rco" ]]; then
      WAVE_COLLECTION="$candidate"
      break
    fi
  done
fi

INSTALL_CONFIG="${HOME}/.local/share/plasma/wallpapers/org.psvec.vlnky/contents/config"
if [[ -n "${WAVE_COLLECTION:-}" ]]; then
  mkdir -p "$INSTALL_CONFIG"
  printf '%s\n' "$WAVE_COLLECTION" > "$INSTALL_CONFIG/default_collection.path"
fi

if [[ "${SKIP_TEXTURE_EXTRACT:-0}" != "1" && -n "${WAVE_COLLECTION:-}" ]]; then
    echo "Extracting PNG sidecars (optional fallback): $WAVE_COLLECTION"
    bash "$ROOT/plasma-wallpaper/contents/code/extract-all-waves.sh" "$WAVE_COLLECTION"
fi

echo ""
echo "Installed: ~/.local/share/plasma/wallpapers/org.psvec.vlnky/"
echo "QML module: ~/.local/share/plasma/wallpapers/.../contents/ui/org/psvec/vlnky/"
if [[ -n "${WAVE_COLLECTION:-}" ]]; then
  echo "Wave collection: $WAVE_COLLECTION"
else
  echo "Set wave collection in wallpaper settings (folder with per-wave system_plugin_bg.rco)."
fi
echo "Restart Plasma:"
echo "  systemctl --user restart plasma-plasmashell.service"
