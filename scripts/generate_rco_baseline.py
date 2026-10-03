#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pavel Švec
# SPDX-License-Identifier: GPL-2.0-or-later
"""Generate RCO baseline JSON for golden waves (requires rcomage in PATH)."""
from __future__ import annotations

import hashlib
import json
import re
import struct
import subprocess
import sys
from pathlib import Path

GOLDEN = ["Alice", "aquadark", "Blank", "1up", "Blue Wire"]


def md5_hex(data: bytes) -> str:
    return hashlib.md5(data).hexdigest()


def find_fcurves(data: bytes, max_blob: int = 4096) -> list[dict]:
    out: list[dict] = []
    pos = 0
    while True:
        i = data.find(b"fcurve-", pos)
        if i < 0:
            break
        end = data.find(b"\x00", i)
        name = data[i:end].decode("ascii", "replace")
        p = end + 1
        while p % 4:
            p += 1
        blob = data[p : p + min(max_blob, len(data) - p)]
        num_keys = struct.unpack_from("<I", blob, 0)[0] if len(blob) >= 4 else 0
        channel = struct.unpack_from("<I", blob, 4)[0] if len(blob) >= 8 else 0
        duration = struct.unpack_from("<I", blob, 8)[0] if len(blob) >= 12 else 0
        out.append(
            {
                "name": name,
                "file_offset": i,
                "blob_offset": p,
                "num_keys": num_keys,
                "channel": channel,
                "duration": duration,
            }
        )
        pos = end + 1
    return out


def parse_model_object(xml: str) -> dict | None:
    m = re.search(
        r'<ModelObject[^>]+name="([^"]+)"[^>]+scaleWidth="([^"]+)"[^>]+scaleHeight="([^"]+)"[^>]+scaleDepth="([^"]+)"[^>]+model="model:([^"]+)"',
        xml,
        re.S,
    )
    if not m:
        return None
    return {
        "name": m.group(1),
        "scaleWidth": float(m.group(2)),
        "scaleHeight": float(m.group(3)),
        "scaleDepth": float(m.group(4)),
        "model": m.group(5),
    }


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    if len(sys.argv) < 2:
        print("usage: generate_rco_baseline.py <collection folder>", file=sys.stderr)
        return 1
    collection = Path(sys.argv[1])
    out_dir = root / "docs" / "rco_baseline"
    dump_dir = out_dir / "dumps"
    out_dir.mkdir(parents=True, exist_ok=True)
    dump_dir.mkdir(parents=True, exist_ok=True)

    rcomage = "rcomage"

    for wave in GOLDEN:
        safe = wave.replace(" ", "_")
        rco = collection / wave / "system_plugin_bg.rco"
        if not rco.is_file():
            print(f"skip missing {rco}", file=sys.stderr)
            continue
        wave_dump = dump_dir / safe
        res_dir = wave_dump / "res"
        wave_dump.mkdir(parents=True, exist_ok=True)
        res_dir.mkdir(parents=True, exist_ok=True)
        subprocess.run(
            [rcomage, "dump", str(rco), str(wave_dump / "wave.xml"), "--resdir", str(res_dir)],
            check=False,
            capture_output=True,
        )
        data = rco.read_bytes()
        gmo_path = res_dir / "mdl_bg.gmo"
        gmo_md5 = md5_hex(gmo_path.read_bytes()) if gmo_path.is_file() else None
        xml = (wave_dump / "wave.xml").read_text(errors="replace") if (wave_dump / "wave.xml").exists() else ""
        entry = {
            "wave": wave,
            "rco_size": len(data),
            "rco_version": struct.unpack_from("<I", data, 4)[0],
            "gmo_md5": gmo_md5,
            "gmo_size": gmo_path.stat().st_size if gmo_path.is_file() else 0,
            "model_object": parse_model_object(xml),
            "image_count": xml.count("<Image "),
            "model_count": xml.count("<Model "),
            "fcurves_in_rco": find_fcurves(data),
            "fcurves_in_gmo": find_fcurves(gmo_path.read_bytes()) if gmo_path.is_file() else [],
        }
        (out_dir / f"{safe}.json").write_text(json.dumps(entry, indent=2) + "\n")
        print(f"wrote {safe}.json gmo={gmo_md5[:8] if gmo_md5 else '?'}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
