# SPDX-FileCopyrightText: 2026 Pavel Švec
# SPDX-License-Identifier: GPL-2.0-or-later
"""GMO (PSP model) chunk parser — extracts wave control-point grid."""

from __future__ import annotations

import struct
from dataclasses import dataclass, field
from enum import Enum


GMO_MAGIC = b"OMG.00.1PSP"
CONTAINER_TYPES = {0x0002, 0x0003}
NESTED_TYPES = {0x0002, 0x0003, 0x0006}


class IndexTopology(Enum):
    GRID_FALLBACK = 0
    LIST = 1
    STRIP = 2


@dataclass
class GmoMesh:
    positions: list[tuple[float, float, float]]
    uvs: list[tuple[float, float]]
    grid_cols: int
    grid_rows: int
    indices: list[int] = field(default_factory=list)
    index_topology: IndexTopology = IndexTopology.GRID_FALLBACK
    winding_cw: bool = False
    face_count: int = 0

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


def _scan_chunks(data: bytes, gmo_start: int, region_end: int) -> list[tuple[int, int, int, int]]:
    chunks: list[tuple[int, int, int, int]] = []
    off = gmo_start
    while off + 8 <= region_end:
        hdr = _read_chunk_header(data, off)
        if hdr is None:
            off += 2
            continue
        ctype, hdr_size, data_size, total = hdr
        chunks.append((off, ctype, hdr_size, data_size))
        if off + total > off + 8:
            off = off + total - 2
        else:
            off += 2
    return chunks


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
    for i in range(0, len(body) - 11, 12):
        x, y, z = struct.unpack_from("<3f", body, i)
        if abs(x) > 80 or abs(y) > 80 or abs(z) > 80:
            continue
        if abs(x) < 1e-6 and abs(y) < 1e-6 and abs(z) < 1e-6:
            continue
        positions.append((x, y, z))

    if len(positions) >= 100:
        return positions

    positions.clear()
    for i in range(0, len(body) - 19, 20):
        x, y, z = struct.unpack_from("<3f", body, i)
        if abs(x) > 80 or abs(y) > 80 or abs(z) > 80:
            continue
        positions.append((x, y, z))

    return positions


def _infer_grid(n: int) -> tuple[int, int]:
    """Prefer wide grid: cols >= rows, largest cols first."""
    for cols in range(n // 2, 4, -1):
        if n % cols != 0:
            continue
        rows = n // cols
        if rows >= 2 and cols >= rows:
            return cols, rows
    best = (1, n)
    best_score = n
    for cols in range(5, n // 2 + 1):
        if n % cols != 0:
            continue
        rows = n // cols
        if rows < 2:
            continue
        if cols >= rows:
            return cols, rows
        score = abs(cols * rows - n)
        if score < best_score:
            best_score = score
            best = (cols, rows)
    c, r = best
    if c < r:
        c, r = r, c
    return c, r


def _try_read_grid_from_mesh_info(data: bytes, off: int, hdr_size: int, data_size: int, vertex_count: int) -> tuple[int, int]:
    body_off = off + hdr_size
    body_end = body_off + data_size
    for p in range(body_off, body_end - 7, 4):
        a, b = struct.unpack_from("<II", data, p)
        if 2 <= a <= 64 and 2 <= b <= 64 and a * b == vertex_count:
            return max(a, b), min(a, b)
    for p in range(body_off, body_end - 3, 2):
        a, b = struct.unpack_from("<HH", data, p)
        if 2 <= a <= 64 and 2 <= b <= 64 and a * b == vertex_count:
            return max(a, b), min(a, b)
    return 0, 0


def _is_sequential_table(indices: list[int]) -> bool:
    if len(indices) < 10:
        return False
    return all(indices[i] == i for i in range(len(indices)))


def _find_index_offset(data: bytes, body_off: int, body_end: int, vertex_count: int) -> int:
    for base in (224, 192, 160, 128, 96, 64, 32, 0):
        start = body_off + base
        if start + 6 > body_end:
            continue
        v0, v1, v2 = struct.unpack_from("<HHH", data, start)
        if v0 < vertex_count and v1 < vertex_count and v2 < vertex_count:
            return start
    for p in range(body_off, body_end - 5, 2):
        v0, v1, v2 = struct.unpack_from("<HHH", data, p)
        if v0 < vertex_count and v1 < vertex_count and v2 < vertex_count:
            return p
    return body_off


def _parse_index_chunk(data: bytes, off: int, hdr_size: int, data_size: int, vertex_count: int) -> tuple[list[int], IndexTopology]:
    body_off = off + hdr_size
    body_end = body_off + data_size
    idx_start = _find_index_offset(data, body_off, body_end, vertex_count)
    raw: list[int] = []
    p = idx_start
    while p + 2 <= body_end:
        raw.append(struct.unpack_from("<H", data, p)[0])
        p += 2
        if len(raw) > 50000:
            break

    if not raw or _is_sequential_table(raw):
        return [], IndexTopology.GRID_FALLBACK

    if not all(v < vertex_count for v in raw):
        return [], IndexTopology.GRID_FALLBACK

    if len(raw) % 3 == 0:
        return raw, IndexTopology.LIST

    tris: list[int] = []
    if len(raw) >= 3:
        i0, i1 = raw[0], raw[1]
        for k in range(2, len(raw)):
            i2 = raw[k]
            if i0 == i1 or i1 == i2 or i0 == i2:
                i1 = i2
                continue
            if (k - 2) % 2 == 0:
                tris.extend((i0, i1, i2))
            else:
                tris.extend((i0, i2, i1))
            i0, i1 = i1, i2
        if len(tris) >= 3:
            return tris, IndexTopology.STRIP

    return [], IndexTopology.GRID_FALLBACK


def _transpose_grid(positions: list[tuple[float, float, float]], uvs: list[tuple[float, float]], old_cols: int, old_rows: int):
    new_cols, new_rows = old_rows, old_cols
    n = len(positions)
    new_pos: list[tuple[float, float, float]] = [(0.0, 0.0, 0.0)] * n
    new_uv: list[tuple[float, float]] = [(0.0, 0.0)] * n if uvs else []
    for r in range(old_rows):
        for c in range(old_cols):
            old_idx = r * old_cols + c
            new_idx = c * new_cols + r
            if old_idx < n and new_idx < n:
                new_pos[new_idx] = positions[old_idx]
                if uvs and old_idx < len(uvs):
                    new_uv[new_idx] = uvs[old_idx]
    positions[:] = new_pos
    if uvs:
        uvs[:] = new_uv


def _apply_orientation(positions: list[tuple[float, float, float]], uvs: list[tuple[float, float]], cols: int, rows: int) -> tuple[int, int]:
    old_cols, old_rows = cols, rows
    if cols < rows:
        cols, rows = rows, cols
        _transpose_grid(positions, uvs, old_cols, old_rows)

    if positions:
        xs = [p[0] for p in positions]
        ys = [p[1] for p in positions]
        span_x = max(xs) - min(xs)
        span_y = max(ys) - min(ys)
        if span_y > span_x and span_x > 1e-6:
            for i, (x, y, z) in enumerate(positions):
                positions[i] = (y, x, z)

    return cols, rows


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

    vertex_count = len(positions)
    region_end = min(len(data), gmo_offset + 150_000)
    chunks = _scan_chunks(data, gmo_offset, region_end)

    cols, rows = 0, 0
    for off, ctype, hdr_size, data_size in chunks:
        if ctype == 0x0003:
            cols, rows = _try_read_grid_from_mesh_info(data, off, hdr_size, data_size, vertex_count)
            if cols > 0:
                break

    indices: list[int] = []
    index_topo = IndexTopology.GRID_FALLBACK
    for off, ctype, hdr_size, data_size in chunks:
        if ctype == 0x0006:
            indices, index_topo = _parse_index_chunk(data, off, hdr_size, data_size, vertex_count)
            break

    if cols <= 0 or rows <= 0:
        cols, rows = _infer_grid(vertex_count)

    grid_count = cols * rows
    if len(positions) > grid_count:
        positions = positions[:grid_count]

    uvs: list[tuple[float, float]] = []
    cols, rows = _apply_orientation(positions, uvs, cols, rows)

    if not uvs:
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
        index_topology=index_topo,
        face_count=len(indices) // 3 if indices else 0,
    )
