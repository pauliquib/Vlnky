# SPDX-FileCopyrightText: 2026 Pavel Švec
# SPDX-License-Identifier: GPL-2.0-or-later
"""PRF/RCO container reader — extracts GMO, TGA texture, and fcurve blobs."""

from __future__ import annotations

import hashlib
import json
import re
import struct
from dataclasses import dataclass, field
from pathlib import Path
from typing import BinaryIO

PRF_MAGIC = b"\x00PRF"
GMO_MAGIC = b"OMG.00.1PSP"
TEX_DIM_MARKER = b"\x00\x80\x00\x80\x00\x08"  # 128×128 8-bit (typical wave reflection)


@dataclass
class PrfFile:
    path: Path
    data: bytes
    version: int = 0
    gmo_offset: int = -1
    gmo_data: bytes = b""
    texture: bytes = b""
    texture_width: int = 128
    texture_height: int = 128
    fcurve_blobs: dict[str, bytes] = field(default_factory=dict)
    gmo_hash: str = ""

    @property
    def md5(self) -> str:
        return hashlib.md5(self.data).hexdigest()


def _read_file(path: Path) -> bytes:
    return path.read_bytes()


def _find_gmo(data: bytes) -> int:
    return data.find(GMO_MAGIC)


def _extract_texture(data: bytes) -> tuple[bytes, int, int]:
    """Locate embedded 8-bit grayscale reflection texture."""
    # Primary: dimension marker used across most 146 KB RCO files
    idx = data.find(TEX_DIM_MARKER)
    if idx >= 0:
        img_off = idx + len(TEX_DIM_MARKER)
        while img_off < len(data) and data[img_off] == 0:
            img_off += 1
        w, h = 128, 128
        need = w * h
        if img_off + need <= len(data):
            return data[img_off : img_off + need], w, h

    # Secondary: after .tga filename, look for 8bpp marker then raw pixels
    tga_ref = data.find(b".tga")
    if tga_ref >= 0:
        scan_start = tga_ref + 4
        scan_end = min(len(data), scan_start + 256)
        for off in range(scan_start, scan_end):
            if data[off : off + 2] != b"\x08\x08":
                continue
            for w, h in ((128, 128), (256, 128), (128, 64), (256, 64), (64, 64)):
                need = w * h
                img_off = off + 2
                while img_off < len(data) and data[img_off] == 0:
                    img_off += 1
                if img_off + need > len(data):
                    continue
                block = data[img_off : img_off + need]
                if len(set(block[:64])) >= 4 and max(block) > 0:
                    return block, w, h

    # Fallback: scan for standard uncompressed TGA type 2
    for off in range(len(data) - 18):
        if data[off + 2] != 2:
            continue
        w, h = struct.unpack_from("<HH", data, off + 12)
        bpp = data[off + 16]
        if bpp != 8 or w not in (64, 128, 256) or h not in (32, 64, 128, 256):
            continue
        img_size = w * h
        img_off = off + 18
        if img_off + img_size <= len(data):
            return data[img_off : img_off + img_size], w, h

    return b"", 0, 0


def _extract_fcurves(data: bytes) -> dict[str, bytes]:
    blobs: dict[str, bytes] = {}
    for m in re.finditer(rb"fcurve-\d+\x00", data):
        name = m.group()[:-1].decode()
        start = m.start()
        p = m.end()
        while p % 4:
            p += 1
        # Grab up to 512 bytes of payload (enough for keyframes)
        blobs[name] = data[p : p + 512]
    return blobs


def load_rco(path: str | Path) -> PrfFile:
    path = Path(path)
    data = _read_file(path)

    if not data.startswith(PRF_MAGIC):
        raise ValueError(f"Not a PRF/RCO file: {path}")

    version = struct.unpack_from("<I", data, 4)[0]
    gmo_off = _find_gmo(data)
    gmo_data = data[gmo_off:] if gmo_off >= 0 else b""

    tex, tw, th = _extract_texture(data)
    fcurves = _extract_fcurves(data)

    gmo_hash = ""
    if gmo_data:
        # Hash vertex chunk region (stable across texture-only variants)
        try:
            from .gmo_parser import parse_gmo

            mesh = parse_gmo(data, gmo_off)
            gmo_hash = hashlib.md5(mesh.to_cache_bin()).hexdigest()
        except Exception:
            gmo_hash = hashlib.md5(gmo_data[:8192]).hexdigest()

    return PrfFile(
        path=path,
        data=data,
        version=version,
        gmo_offset=gmo_off,
        gmo_data=gmo_data,
        texture=tex,
        texture_width=tw,
        texture_height=th,
        fcurve_blobs=fcurves,
        gmo_hash=gmo_hash,
    )


def write_cache(prf: PrfFile, cache_dir: Path) -> Path:
    """Write extracted assets to ~/.cache/vlnky/<md5>/."""
    out = cache_dir / prf.md5
    out.mkdir(parents=True, exist_ok=True)

    meta = {
        "source": str(prf.path),
        "version": prf.version,
        "gmo_offset": prf.gmo_offset,
        "gmo_hash": prf.gmo_hash,
        "texture_width": prf.texture_width,
        "texture_height": prf.texture_height,
        "fcurve_names": list(prf.fcurve_blobs.keys()),
    }
    (out / "meta.json").write_text(json.dumps(meta, indent=2))

    if prf.texture:
        (out / "texture.raw").write_bytes(prf.texture)
        try:
            from PIL import Image

            img = Image.frombytes("L", (prf.texture_width, prf.texture_height), prf.texture)
            img.save(out / "texture.png")
        except ImportError:
            pass

    for name, blob in prf.fcurve_blobs.items():
        safe = name.replace("-", "_")
        (out / f"{safe}.bin").write_bytes(blob)

    return out
