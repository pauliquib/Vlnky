#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pavel Švec
# SPDX-License-Identifier: GPL-2.0-or-later
"""Run PySide6 XMB wave preview from an RCO file or wave collection folder."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

# Allow running from repo without install
ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from xmb_wave.fcurve import parse_fcurves
from xmb_wave.gmo_parser import parse_gmo
from xmb_wave.prf_reader import load_rco, write_cache


DEFAULT_COLLECTION = (
    Path(__file__).resolve().parents[3] / "176 XMB WAVES for 5.00" / "176 XMB WAVES"
)


def list_waves(collection: Path) -> list[Path]:
    waves = []
    if not collection.is_dir():
        return waves
    for d in sorted(collection.iterdir()):
        rco = d / "system_plugin_bg.rco"
        if rco.is_file():
            waves.append(d)
    return waves


def main() -> int:
    ap = argparse.ArgumentParser(description="Vlnky wave preview")
    ap.add_argument("--rco", type=Path, help="Path to system_plugin_bg.rco")
    ap.add_argument("--collection", type=Path, default=DEFAULT_COLLECTION, help="Wave collection root")
    ap.add_argument("--wave", type=str, help="Wave folder name inside collection")
    ap.add_argument("--cache", action="store_true", help="Write cache to ~/.cache/vlnky/")
    ap.add_argument("--list", action="store_true", help="List available waves")
    ap.add_argument("--no-gui", action="store_true", help="Parse only, no preview window")
    args = ap.parse_args()

    if args.list:
        waves = list_waves(args.collection)
        print(f"{len(waves)} waves in {args.collection}:")
        for w in waves:
            print(f"  {w.name}")
        return 0

    rco_path = args.rco
    if rco_path is None:
        if args.wave:
            rco_path = args.collection / args.wave / "system_plugin_bg.rco"
        else:
            waves = list_waves(args.collection)
            if not waves:
                ap.error(f"No waves found in {args.collection}")
            rco_path = waves[0] / "system_plugin_bg.rco"

    if not rco_path.is_file():
        ap.error(f"RCO not found: {rco_path}")

    print(f"Loading {rco_path} ...")
    prf = load_rco(rco_path)
    mesh = parse_gmo(prf.data, prf.gmo_offset)
    anim = parse_fcurves(prf.fcurve_blobs)

    print(
        f"  PRF v{prf.version:#x}  GMO@{prf.gmo_offset}  "
        f"grid {mesh.grid_cols}×{mesh.grid_rows} ({len(mesh.positions)} pts)  "
        f"tex {prf.texture_width}×{prf.texture_height} ({len(prf.texture)} B)  "
        f"tracks {len(anim.tracks)} loop={anim.loop_duration}"
    )

    if args.cache:
        cache_root = Path.home() / ".cache" / "vlnky"
        out = write_cache(prf, cache_root)
        mesh_bin = out / "control_points.bin"
        mesh_bin.write_bytes(mesh.to_cache_bin())
        (out / "anim.json").write_text(anim.to_anim_json())
        print(f"  Cache → {out}")

    if args.no_gui:
        return 0

    try:
        from xmb_wave.preview_window import show_preview
        show_preview(prf, mesh, anim)
    except ImportError as e:
        print(f"PySide6 required for GUI preview: {e}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
