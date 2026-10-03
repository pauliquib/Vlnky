# SPDX-FileCopyrightText: 2026 Pavel Švec
# SPDX-License-Identifier: GPL-2.0-or-later
"""GMO (PSP model) chunk parser — extracts wave control-point grid."""

from __future__ import annotations

import struct
from dataclasses import dataclass

GMO_MAGIC = b"OMG.00.1PSP"
CONTAINER_TYPES = {0x0002, 0x0003}
NESTED_TYPES = {0x0002, 0x0003, 0x0006}


@dataclass
class GmoMesh:
    positions: list[tuple[float, float, float]]
    uvs: list[tuple[float, float]]
    grid_cols: int
    grid_rows: int
    indices: list[int]

    def control_points_flat(self) -> list[float]:
        out: list[float] = []
        for x, y, z in self.positions:
            out.extend((x, y, z))
        return out

    def to_cache_bin(self) -> bytes:
        """Binary cache: cols(u32) rows(u32) n(u32) then vec3 floats."""
        n = len(self.positions)
        buf = struct.pack("<III", self.grid_cols, self.grid_rows, n)
        for x, y, z in self.positions:
            buf += struct.pack("<3f", x, y, z)
        for u, v in self.uvs:
            buf += struct.pack("<2f", u, v)
        return buf


def _read_chunk_header(data: bytes, pos: int) -> tuple[int, int, int, int] | None:
    if pos + 8 > len(data):
        return None
    ctype, hdr_size = struct.unpack_from("<HH", data, pos)
    data_size = struct.unpack_from("<I", data, pos + 4)[0]
    total = hdr_size + data_size
    if hdr_size < 8 or total < 8:
        return None
    return ctype, hdr_size, data_size, total


def _find_vertex_chunk(data: bytes, gmo_start: int) -> tuple[int, int, int] | None:
    """Scan GMO region for valid 0x0007 vertex array chunk."""
    region_end = min(len(data), gmo_start + 150_000)
    for off in range(gmo_start, region_end - 8, 2):
        hdr = _read_chunk_header(data, off)
        if hdr is None:
            continue
        ctype, hdr_size, data_size, total = hdr
        if ctype != 0x0007 or hdr_size != 28:
            continue
        if not (500 < data_size < 20_000):
            continue
        if off + total > len(data):
            continue
        return off, hdr_size, data_size
    return None


def _extract_positions_from_vertex(data: bytes, body: bytes) -> list[tuple[float, float, float]]:
    """Extract vec3 positions — wave mesh uses mostly fixed X (~−36) with Y/Z deformation."""
    positions: list[tuple[float, float, float]] = []
    # Try stride-12 vec3 scan (dominant pattern in XMB waves)
    for i in range(0, len(body) - 11, 12):
        x, y, z = struct.unpack_from("<3f", body, i)
        if abs(x) > 80 or abs(y) > 80 or abs(z) > 80:
            continue
        if abs(x) < 1e-6 and abs(y) < 1e-6 and abs(z) < 1e-6:
            continue
        positions.append((x, y, z))

    if len(positions) >= 100:
        return positions

    # Fallback: stride-20 (pos + uv)
    positions.clear()
    uvs: list[tuple[float, float]] = []
    for i in range(0, len(body) - 19, 20):
        x, y, z = struct.unpack_from("<3f", body, i)
        u, v = struct.unpack_from("<2f", body, i + 12)
        if abs(x) > 80 or abs(y) > 80 or abs(z) > 80:
            continue
        positions.append((x, y, z))
        uvs.append((u, v))

    return positions


def _infer_grid(n: int) -> tuple[int, int]:
    """Infer control-point grid dimensions from vertex count."""
    best = (1, n)
    best_score = n
    for cols in range(8, 40):
        for rows in (n // cols, n // cols + 1):
            if rows < 2:
                continue
            score = abs(cols * rows - n)
            if score < best_score:
                best_score = score
                best = (cols, rows)
    return best


def _extract_index_buffer(data: bytes, gmo_start: int, mesh_end: int) -> list[int]:
    """Find sequential index buffer inside mesh chunk (0..N-1 ushort list)."""
    for off in range(gmo_start + 200, mesh_end):
        if off + 6 > len(data):
            break
        if struct.unpack_from("<H", data, off)[0] != 0:
            continue
        if struct.unpack_from("<H", data, off + 2)[0] != 1:
            continue
        # Count consecutive indices
        indices: list[int] = []
        p = off
        while p + 2 <= mesh_end:
            v = struct.unpack_from("<H", data, p)[0]
            if indices and v != indices[-1] + 1 and v != 0:
                break
            indices.append(v)
            p += 2
            if len(indices) > 2000:
                break
        if len(indices) >= 100:
            return indices
    return []


def parse_gmo(data: bytes, gmo_offset: int | None = None) -> GmoMesh:
    if gmo_offset is None:
        gmo_offset = data.find(GMO_MAGIC)
    if gmo_offset < 0:
        raise ValueError("GMO magic not found")

    vert = _find_vertex_chunk(data, gmo_offset)
    if vert is None:
        raise ValueError("GMO vertex chunk (0x0007) not found")

    vpos, vhdr, vdata_size = vert
    body = data[vpos + vhdr : vpos + vhdr + vdata_size]
    positions = _extract_positions_from_vertex(data, body)

    if len(positions) < 50:
        raise ValueError(f"Too few control points extracted: {len(positions)}")

    cols, rows = _infer_grid(len(positions))
    mesh_end = vpos + vhdr + vdata_size
    indices = _extract_index_buffer(data, gmo_offset, mesh_end)

    # UVs from grid parametric coords
    uvs = []
    for i in range(len(positions)):
        c = i % cols
        r = i // cols
        uvs.append((c / max(cols - 1, 1), r / max(rows - 1, 1)))

    return GmoMesh(
        positions=positions,
        uvs=uvs,
        grid_cols=cols,
        grid_rows=rows,
        indices=indices,
    )
