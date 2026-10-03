#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pavel Švec
# SPDX-License-Identifier: GPL-2.0-or-later
"""Extract wave texture from system_plugin_bg.rco → PNG on stdout path (for Plasma wallpaper)."""
from __future__ import annotations

import hashlib
import json
import sys
from pathlib import Path

# Use prototype parsers from repo when run from dev tree
SCRIPT = Path(__file__).resolve()
sys.path.insert(0, str(SCRIPT.parent))

from xmb_wave.fcurve import parse_fcurves
from xmb_wave.gmo_parser import parse_gmo
from xmb_wave.prf_reader import load_rco, write_cache


def main() -> int:
    if len(sys.argv) < 2:
        print("usage: xmb_extract.py /path/to/system_plugin_bg.rco", file=sys.stderr)
        return 1

    rco = Path(sys.argv[1]).resolve()
    if not rco.is_file():
        print(f"error: not found: {rco}", file=sys.stderr)
        return 2

    prf = load_rco(rco)
    mesh = None
    try:
        mesh = parse_gmo(prf.data, prf.gmo_offset)
    except Exception:
        pass
    cache_root = Path.home() / ".cache" / "vlnky"
    out = write_cache(prf, cache_root)
    if mesh is not None:
        (out / "control_points.bin").write_bytes(mesh.to_cache_bin())
        anim = parse_fcurves(prf.fcurve_blobs)
        (out / "anim.json").write_text(anim.to_anim_json())

    png = out / "texture.png"
    sidecar = rco.parent / "wave_texture.png"
    if not png.is_file() and not sidecar.is_file():
        print("error: texture.png not written", file=sys.stderr)
        return 3

    texture = sidecar if sidecar.is_file() else png
    print(json.dumps({"ok": True, "texture": str(texture), "cache": str(out)}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
