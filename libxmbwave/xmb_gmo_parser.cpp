// SPDX-FileCopyrightText: 2026 Pavel Švec
// SPDX-License-Identifier: GPL-2.0-or-later

#include "xmb_gmo_parser.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <set>
#include <sstream>
#include <unordered_set>

namespace xmb {
namespace {

constexpr uint8_t kGmoMagic[] = {'O', 'M', 'G', '.', '0', '0', '.', '1', 'P', 'S', 'P', 0};
constexpr size_t kGmoMagicLen = sizeof(kGmoMagic) - 1;

struct GmoChunkHdr {
    uint16_t type = 0;
    uint16_t headerSize = 0;
    uint32_t dataSize = 0;
    size_t offset = 0;

    size_t bodyOffset() const { return offset + headerSize; }
    size_t endOffset() const { return offset + headerSize + dataSize; }
    bool validIn(const std::vector<uint8_t> &d) const
    {
        return headerSize >= 8 && endOffset() <= d.size();
    }
};

struct MeshInfoFields {
    int faceCount = 0;
    uint16_t primType = 0;
};

struct IndexParseResult {
    std::vector<uint32_t> indices;
    IndexTopology topology = IndexTopology::GridFallback;
    bool windingCw = false;
    int faceCount = 0;
    bool valid = false;
};

bool canRead(const std::vector<uint8_t> &d, size_t off, size_t n)
{
    return off <= d.size() && n <= d.size() - off;
}

bool readU16(const std::vector<uint8_t> &d, size_t off, uint16_t &out)
{
    if (!canRead(d, off, 2))
        return false;
    std::memcpy(&out, d.data() + off, 2);
    return true;
}

bool readU32(const std::vector<uint8_t> &d, size_t off, uint32_t &out)
{
    if (!canRead(d, off, 4))
        return false;
    std::memcpy(&out, d.data() + off, 4);
    return true;
}

bool readF32(const std::vector<uint8_t> &d, size_t off, float &out)
{
    if (!canRead(d, off, 4))
        return false;
    std::memcpy(&out, d.data() + off, 4);
    return true;
}

bool readChunkHdr(const std::vector<uint8_t> &d, size_t off, GmoChunkHdr &hdr)
{
    if (!readU16(d, off, hdr.type) || !readU16(d, off + 2, hdr.headerSize) || !readU32(d, off + 4, hdr.dataSize))
        return false;
    hdr.offset = off;
    return hdr.validIn(d);
}

std::vector<GmoChunkHdr> scanGmoChunks(const std::vector<uint8_t> &data, size_t gmoStart, size_t regionEnd)
{
    std::vector<GmoChunkHdr> chunks;
    for (size_t off = gmoStart; off + 8 <= regionEnd; off += 2) {
        GmoChunkHdr hdr;
        if (!readChunkHdr(data, off, hdr))
            continue;
        chunks.push_back(hdr);
        if (hdr.endOffset() > off + 8)
            off = hdr.endOffset() - 2;
    }
    return chunks;
}

const GmoChunkHdr *findChunk(const std::vector<GmoChunkHdr> &chunks, uint16_t type)
{
    for (const auto &c : chunks) {
        if (c.type == type)
            return &c;
    }
    return nullptr;
}

bool validPosition(float x, float y, float z, float bound)
{
    if (std::abs(x) > bound || std::abs(y) > bound || std::abs(z) > bound)
        return false;
    if (std::abs(x) < 1e-6f && std::abs(y) < 1e-6f && std::abs(z) < 1e-6f)
        return false;
    return true;
}

std::vector<Vec3> extractPositionsStride12(const std::vector<uint8_t> &body, float bound)
{
    std::vector<Vec3> positions;
    for (size_t i = 0; i + 11 < body.size(); i += 12) {
        float x = 0, y = 0, z = 0;
        if (!readF32(body, i, x) || !readF32(body, i + 4, y) || !readF32(body, i + 8, z))
            break;
        if (!validPosition(x, y, z, bound))
            continue;
        positions.push_back({x, y, z});
    }
    return positions;
}

std::pair<std::vector<Vec3>, std::vector<Vec2>> extractPositionsStride20(const std::vector<uint8_t> &body, float bound)
{
    std::vector<Vec3> positions;
    std::vector<Vec2> uvs;
    for (size_t i = 0; i + 19 < body.size(); i += 20) {
        float x = 0, y = 0, z = 0, u = 0, v = 0;
        if (!readF32(body, i, x) || !readF32(body, i + 4, y) || !readF32(body, i + 8, z))
            break;
        if (!readF32(body, i + 12, u) || !readF32(body, i + 16, v))
            break;
        if (!validPosition(x, y, z, bound))
            continue;
        positions.push_back({x, y, z});
        uvs.push_back({u, v});
    }
    return {positions, uvs};
}

std::pair<int, int> inferGrid(int n)
{
    for (int cols = n / 2; cols >= 5; --cols) {
        if (n % cols != 0)
            continue;
        const int rows = n / cols;
        if (rows >= 2 && cols >= rows)
            return {cols, rows};
    }
    int bestC = 1, bestR = n, bestScore = n;
    for (int cols = 5; cols <= n / 2; ++cols) {
        if (n % cols != 0)
            continue;
        const int rows = n / cols;
        if (rows < 2)
            continue;
        if (cols >= rows)
            return {cols, rows};
        const int score = std::abs(cols * rows - n);
        if (score < bestScore) {
            bestScore = score;
            bestC = cols;
            bestR = rows;
        }
    }
    if (bestC < bestR)
        std::swap(bestC, bestR);
    return {bestC, bestR};
}

std::pair<int, int> tryReadGridFromMeshInfo(const std::vector<uint8_t> &data,
                                            const GmoChunkHdr &meshInfo,
                                            int vertexCount,
                                            MeshInfoFields *fields)
{
    const size_t bodyOff = meshInfo.bodyOffset();
    const size_t bodyEnd = meshInfo.endOffset();
    if (bodyEnd <= bodyOff + 8)
        return {0, 0};

    if (fields && bodyEnd >= bodyOff + 16) {
        uint32_t fc = 0;
        uint16_t pt = 0;
        if (readU32(data, bodyOff, fc))
            fields->faceCount = int(fc);
        if (readU16(data, bodyOff + 12, pt))
            fields->primType = pt;
    }

    for (size_t off = bodyOff; off + 8 <= bodyEnd; off += 4) {
        uint32_t a = 0, b = 0;
        if (!readU32(data, off, a) || !readU32(data, off + 4, b))
            continue;
        if (a < 2 || a > 64 || b < 2 || b > 64)
            continue;
        if (int(a) * int(b) == vertexCount) {
            const int cols = int(std::max(a, b));
            const int rows = int(std::min(a, b));
            return {cols, rows};
        }
    }

    for (size_t off = bodyOff; off + 4 <= bodyEnd; off += 2) {
        uint16_t a = 0, b = 0;
        if (!readU16(data, off, a) || !readU16(data, off + 2, b))
            continue;
        if (a >= 2 && a <= 64 && b >= 2 && b <= 64 && int(a) * int(b) == vertexCount) {
            const int cols = int(std::max(a, b));
            const int rows = int(std::min(a, b));
            return {cols, rows};
        }
    }
    return {0, 0};
}

void computeBbox(const std::vector<Vec3> &positions, float &minX, float &maxX, float &minY, float &maxY)
{
    minX = minY = std::numeric_limits<float>::max();
    maxX = maxY = std::numeric_limits<float>::lowest();
    for (const auto &p : positions) {
        minX = std::min(minX, p.x);
        maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y);
        maxY = std::max(maxY, p.y);
    }
}

void transposeGrid(std::vector<Vec3> &positions, std::vector<Vec2> &uvs, int oldCols, int oldRows)
{
    const int newCols = oldRows;
    const int newRows = oldCols;
    const size_t n = positions.size();
    std::vector<Vec3> newPos(n);
    std::vector<Vec2> newUv(n);
    for (int r = 0; r < oldRows; ++r) {
        for (int c = 0; c < oldCols; ++c) {
            const size_t oldIdx = size_t(r * oldCols + c);
            const size_t newIdx = size_t(c * newCols + r);
            if (oldIdx < n && newIdx < n) {
                newPos[newIdx] = positions[oldIdx];
                if (oldIdx < uvs.size() && newIdx < newUv.size())
                    newUv[newIdx] = uvs[oldIdx];
            }
        }
    }
    positions = std::move(newPos);
    if (!uvs.empty())
        uvs = std::move(newUv);
}

void swapXYCoords(std::vector<Vec3> &positions)
{
    for (auto &p : positions)
        std::swap(p.x, p.y);
}

bool isSequentialAttributeTable(const std::vector<uint32_t> &indices)
{
    if (indices.size() < 10)
        return false;
    for (size_t i = 0; i < indices.size(); ++i) {
        if (indices[i] != i)
            return false;
    }
    return true;
}

bool looksLikeTriangleStrip(const std::vector<uint32_t> &raw)
{
    if (raw.size() < 4)
        return false;
    int repeats = 0;
    for (size_t i = 1; i < raw.size(); ++i) {
        if (raw[i] == raw[i - 1])
            ++repeats;
    }
    return repeats >= 2;
}

std::vector<uint32_t> stripToTriangles(const std::vector<uint32_t> &strip)
{
    std::vector<uint32_t> tris;
    if (strip.size() < 3)
        return tris;
    uint32_t i0 = strip[0], i1 = strip[1];
    for (size_t k = 2; k < strip.size(); ++k) {
        const uint32_t i2 = strip[k];
        if (i0 == i1 || i1 == i2 || i0 == i2) {
            i1 = i2;
            continue;
        }
        if ((k - 2) % 2 == 0) {
            tris.push_back(i0);
            tris.push_back(i1);
            tris.push_back(i2);
        } else {
            tris.push_back(i0);
            tris.push_back(i2);
            tris.push_back(i1);
        }
        i0 = i1;
        i1 = i2;
    }
    return tris;
}

bool isDegenerateTri(uint32_t a, uint32_t b, uint32_t c)
{
    return a == b || b == c || a == c;
}

Vec3 triNormal(const Vec3 &a, const Vec3 &b, const Vec3 &c)
{
    const float ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z;
    const float vx = c.x - a.x, vy = c.y - a.y, vz = c.z - a.z;
    return Vec3 {uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx};
}

void appendLog(GmoParseInfo *info, const std::string &line)
{
    if (!info)
        return;
    if (!info->log.empty())
        info->log += "; ";
    info->log += line;
}

bool validateAndFixWinding(std::vector<uint32_t> &indices,
                           const std::vector<Vec3> &positions,
                           bool &windingCw,
                           GmoParseInfo *info)
{
    if (indices.size() < 3 || indices.size() % 3 != 0)
        return false;

    int positiveY = 0, negativeY = 0, valid = 0;
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        const uint32_t ia = indices[i], ib = indices[i + 1], ic = indices[i + 2];
        if (isDegenerateTri(ia, ib, ic))
            continue;
        if (ia >= positions.size() || ib >= positions.size() || ic >= positions.size())
            continue;
        const Vec3 n = triNormal(positions[ia], positions[ib], positions[ic]);
        const float len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
        if (len < 1e-8f)
            continue;
        ++valid;
        if (n.y / len > 0.f)
            ++positiveY;
        else
            ++negativeY;
    }

    if (valid == 0)
        return true;

    if (negativeY > positiveY) {
        for (size_t i = 0; i + 2 < indices.size(); i += 3)
            std::swap(indices[i + 1], indices[i + 2]);
        windingCw = true;
        appendLog(info, "winding=flipped");
    }
    return true;
}

size_t findIndexDataOffset(const std::vector<uint8_t> &data, size_t bodyOff, size_t bodyEnd, int vertexCount)
{
    const size_t candidates[] = {224, 192, 160, 128, 96, 64, 32, 0};
    for (size_t base : candidates) {
        const size_t start = bodyOff + base;
        if (start + 6 > bodyEnd)
            continue;
        uint16_t v0 = 0, v1 = 0, v2 = 0;
        if (!readU16(data, start, v0) || !readU16(data, start + 2, v1) || !readU16(data, start + 4, v2))
            continue;
        if (v0 < uint16_t(vertexCount) && v1 < uint16_t(vertexCount) && v2 < uint16_t(vertexCount))
            return start;
    }

    for (size_t p = bodyOff; p + 6 <= bodyEnd; p += 2) {
        uint16_t v0 = 0, v1 = 0, v2 = 0;
        if (!readU16(data, p, v0) || !readU16(data, p + 2, v1) || !readU16(data, p + 4, v2))
            break;
        if (v0 < uint16_t(vertexCount) && v1 < uint16_t(vertexCount) && v2 < uint16_t(vertexCount))
            return p;
    }
    return bodyOff;
}

IndexParseResult parseIndexChunk(const std::vector<uint8_t> &data,
                                 const GmoChunkHdr &chunk,
                                 int vertexCount,
                                 const std::vector<Vec3> &positions,
                                 GmoParseInfo *info)
{
    IndexParseResult result;
    const size_t bodyOff = chunk.bodyOffset();
    const size_t bodyEnd = chunk.endOffset();
    if (bodyEnd <= bodyOff || vertexCount <= 0)
        return result;

    uint16_t primType = 0, indexFormat = 16;
    uint32_t indexCount = 0;
    if (chunk.headerSize >= 24) {
        readU16(data, chunk.offset + 8, primType);
        readU16(data, chunk.offset + 10, indexFormat);
        readU32(data, chunk.offset + 12, indexCount);
    }

    const size_t idxStart = findIndexDataOffset(data, bodyOff, bodyEnd, vertexCount);
    std::vector<uint32_t> raw;
    for (size_t p = idxStart; p + 2 <= bodyEnd; p += 2) {
        uint16_t v = 0;
        if (!readU16(data, p, v))
            break;
        raw.push_back(v);
        if (raw.size() > 50000)
            break;
    }

    if (raw.empty())
        return result;

    if (isSequentialAttributeTable(raw)) {
        appendLog(info, "indices_rejected_sequential_table");
        return result;
    }

    std::vector<uint32_t> tris;
    IndexTopology topo = IndexTopology::GridFallback;

    const bool allInRange = std::all_of(raw.begin(), raw.end(), [vertexCount](uint32_t v) {
        return v < uint32_t(vertexCount);
    });

    if (!allInRange) {
        appendLog(info, "indices_out_of_range");
        return result;
    }

    if (raw.size() % 3 == 0 && !looksLikeTriangleStrip(raw)) {
        tris = raw;
        topo = IndexTopology::List;
    } else if (looksLikeTriangleStrip(raw) || primType == 5) {
        tris = stripToTriangles(raw);
        topo = IndexTopology::Strip;
    } else if (raw.size() >= 3) {
        tris = stripToTriangles(raw);
        if (tris.size() >= 3)
            topo = IndexTopology::Strip;
    }

    if (tris.size() < 3 || tris.size() % 3 != 0)
        return result;

    for (size_t i = 0; i + 2 < tris.size(); i += 3) {
        if (isDegenerateTri(tris[i], tris[i + 1], tris[i + 2]))
            continue;
    }

    result.indices = std::move(tris);
    result.topology = topo;
    result.faceCount = int(result.indices.size() / 3);
    result.valid = true;
    validateAndFixWinding(result.indices, positions, result.windingCw, info);
    return result;
}

float indexCoverage(const std::vector<uint32_t> &indices, int vertexCount)
{
    if (vertexCount <= 0 || indices.empty())
        return 0.f;
    std::unordered_set<uint32_t> used;
    for (uint32_t idx : indices)
        used.insert(idx);
    return float(used.size()) / float(vertexCount);
}

bool reorderByIndexWalk(std::vector<Vec3> &positions,
                        std::vector<Vec2> &uvs,
                        const std::vector<uint32_t> &indices,
                        int vertexCount)
{
    if (indexCoverage(indices, vertexCount) < 0.8f)
        return false;

    std::vector<Vec3> newPos;
    std::vector<Vec2> newUv;
    std::unordered_set<uint32_t> seen;
    newPos.reserve(positions.size());
    newUv.reserve(uvs.size());

    for (uint32_t idx : indices) {
        if (idx >= positions.size() || seen.count(idx))
            continue;
        seen.insert(idx);
        newPos.push_back(positions[idx]);
        if (idx < uvs.size())
            newUv.push_back(uvs[idx]);
    }

    for (size_t i = 0; i < positions.size(); ++i) {
        if (!seen.count(uint32_t(i))) {
            newPos.push_back(positions[i]);
            if (i < uvs.size())
                newUv.push_back(uvs[i]);
        }
    }

    if (newPos.size() != positions.size())
        return false;
    positions = std::move(newPos);
    if (!uvs.empty() && newUv.size() == uvs.size())
        uvs = std::move(newUv);
    return true;
}

void applyOrientationFix(std::vector<Vec3> &positions,
                         std::vector<Vec2> &uvs,
                         int &cols,
                         int &rows,
                         GmoParseInfo *info)
{
    const int oldCols = cols, oldRows = rows;

    if (cols < rows && cols > 0 && rows > 0) {
        std::swap(cols, rows);
        transposeGrid(positions, uvs, oldCols, oldRows);
        appendLog(info, "Swapped grid orientation: was " + std::to_string(oldCols) + "x" + std::to_string(oldRows)
                          + ", now " + std::to_string(cols) + "x" + std::to_string(rows));
    }

    float minX, maxX, minY, maxY;
    computeBbox(positions, minX, maxX, minY, maxY);
    const float spanX = maxX - minX;
    const float spanY = maxY - minY;
    if (spanY > spanX && spanX > 1e-6f) {
        swapXYCoords(positions);
        appendLog(info, "Swapped X/Y coords: spanY > spanX");
    }
}

const char *topologyLabel(IndexTopology t)
{
    switch (t) {
    case IndexTopology::List:
        return "list";
    case IndexTopology::Strip:
        return "strip";
    default:
        return "grid_fallback";
    }
}

} // namespace

GmoResult parseGmoResult(const std::vector<uint8_t> &data, int gmoOffset, GmoParseInfo *info)
{
    return parseGmoResult(data, gmoOffset, GmoParseOptions{}, info);
}

GmoResult parseGmoResult(const std::vector<uint8_t> &data,
                         int gmoOffset,
                         const GmoParseOptions &opts,
                         GmoParseInfo *info)
{
    GmoResult result;
    if (gmoOffset < 0) {
        result.error = RcoError::GmoNotFound;
        appendLog(info, "gmo_offset_invalid");
        return result;
    }

    if (!canRead(data, size_t(gmoOffset), kGmoMagicLen)
        || std::memcmp(data.data() + gmoOffset, kGmoMagic, kGmoMagicLen) != 0) {
        result.error = RcoError::GmoNotFound;
        appendLog(info, "gmo_magic_mismatch");
        return result;
    }

    const size_t regionEnd = std::min(data.size(), size_t(gmoOffset) + 150000);
    const auto chunks = scanGmoChunks(data, size_t(gmoOffset), regionEnd);

    const GmoChunkHdr *vertChunk = nullptr;
    for (const auto &c : chunks) {
        if (c.type == 0x0007 && c.headerSize == 28 && c.dataSize >= 500 && c.dataSize <= 20000) {
            vertChunk = &c;
            break;
        }
    }

    if (!vertChunk) {
        for (size_t off = size_t(gmoOffset); off + 8 <= regionEnd; off += 2) {
            GmoChunkHdr hdr;
            if (!readChunkHdr(data, off, hdr))
                continue;
            if (hdr.type == 0x0007 && hdr.headerSize == 28 && hdr.dataSize >= 500 && hdr.dataSize <= 20000) {
                vertChunk = &hdr;
                break;
            }
        }
    }

    if (!vertChunk) {
        result.error = RcoError::VertexChunkNotFound;
        appendLog(info, "vertex_chunk_0x0007_not_found");
        return result;
    }

    const size_t bodyOff = vertChunk->bodyOffset();
    const size_t bodyEnd = vertChunk->endOffset();
    if (bodyEnd <= bodyOff) {
        result.error = RcoError::VertexChunkNotFound;
        return result;
    }

    std::vector<uint8_t> body(data.begin() + bodyOff, data.begin() + bodyEnd);
    std::vector<Vec3> positions = extractPositionsStride12(body, opts.positionBound);
    std::vector<Vec2> uvsFromGmo;

    if (positions.size() < opts.minControlPoints) {
        auto pair = extractPositionsStride20(body, opts.positionBound);
        positions = std::move(pair.first);
        uvsFromGmo = std::move(pair.second);
        if (!uvsFromGmo.empty())
            appendLog(info, "vertex_stride20");
    } else {
        appendLog(info, "vertex_stride12");
    }

    result.value.controlPointCount = int(positions.size());
    if (info)
        info->controlPointCount = int(positions.size());

    if (positions.size() < opts.minControlPoints) {
        result.error = RcoError::TooFewControlPoints;
        appendLog(info, "count=" + std::to_string(positions.size()));
        return result;
    }

    const int vertexCount = int(positions.size());
    appendLog(info, "[GMO] vertices: " + std::to_string(vertexCount));

    MeshInfoFields meshFields;
    int cols = 0, rows = 0;
    if (const GmoChunkHdr *meshInfo = findChunk(chunks, 0x0003)) {
        auto grid = tryReadGridFromMeshInfo(data, *meshInfo, vertexCount, &meshFields);
        cols = grid.first;
        rows = grid.second;
        if (cols > 0 && rows > 0) {
            if (info) {
                info->gridSource = GridDimensionSource::GmoMetadata;
                result.value.gridSource = GridDimensionSource::GmoMetadata;
            }
            appendLog(info, "[GMO] grid from metadata: " + std::to_string(cols) + "x" + std::to_string(rows));
        }
    }

    IndexParseResult indexResult;
    if (const GmoChunkHdr *idxChunk = findChunk(chunks, 0x0006))
        indexResult = parseIndexChunk(data, *idxChunk, vertexCount, positions, info);

    if (indexResult.valid) {
        appendLog(info, "[GMO] indices: " + std::to_string(indexResult.indices.size())
                          + ", faces: " + std::to_string(indexResult.faceCount));
    }

    if (cols <= 0 || rows <= 0) {
        auto grid = inferGrid(vertexCount);
        cols = grid.first;
        rows = grid.second;
        if (info) {
            info->gridSource = GridDimensionSource::Inferred;
            result.value.gridSource = GridDimensionSource::Inferred;
        }
        appendLog(info, "[GMO] inferred: " + std::to_string(cols) + "x" + std::to_string(rows));
    }

    const size_t gridCount = size_t(cols) * size_t(rows);
    if (positions.size() > gridCount)
        positions.resize(gridCount);

    if (opts.vertexOrder == VertexOrder::FollowIndices && indexResult.valid
        && indexResult.topology != IndexTopology::GridFallback) {
        if (reorderByIndexWalk(positions, uvsFromGmo, indexResult.indices, vertexCount)) {
            appendLog(info, "vertex_order=follow_indices");
            auto grid = inferGrid(int(positions.size()));
            cols = grid.first;
            rows = grid.second;
        }
    }

    applyOrientationFix(positions, uvsFromGmo, cols, rows, info);

    float minX, maxX, minY, maxY;
    computeBbox(positions, minX, maxX, minY, maxY);
    float minZ = std::numeric_limits<float>::max();
    float maxZ = std::numeric_limits<float>::lowest();
    for (const auto &p : positions) {
        minZ = std::min(minZ, p.z);
        maxZ = std::max(maxZ, p.z);
    }
    appendLog(info, "[MESH] bbox: min(" + std::to_string(minX) + "," + std::to_string(minY) + "," + std::to_string(minZ)
                      + ") max(" + std::to_string(maxX) + "," + std::to_string(maxY) + "," + std::to_string(maxZ) + ")");

    result.value.positions = std::move(positions);
    result.value.gridCols = cols;
    result.value.gridRows = rows;

    if (indexResult.valid) {
        result.value.indices = std::move(indexResult.indices);
        result.value.indexTopology = indexResult.topology;
        result.value.windingCw = indexResult.windingCw;
        result.value.faceCount = indexResult.faceCount;
        result.value.usesGmoIndices = true;
        if (info)
            info->usedGmoIndices = true;
        appendLog(info, std::string("[GMO] index type: ") + topologyLabel(indexResult.topology)
                          + ", winding: " + (indexResult.windingCw ? "cw" : "ccw"));
    } else {
        appendLog(info, "[GMO] triangulation: grid_fallback");
        result.value.indexTopology = IndexTopology::GridFallback;
    }

    if (!uvsFromGmo.empty() && uvsFromGmo.size() == result.value.positions.size()) {
        result.value.uvs = std::move(uvsFromGmo);
        if (info) {
            info->usedGmoUvs = true;
            result.value.usesGmoUvs = true;
        }
        appendLog(info, "uv_from_gmo");
    } else {
        result.value.uvs.reserve(result.value.positions.size());
        for (size_t i = 0; i < result.value.positions.size(); ++i) {
            const int c = static_cast<int>(i) % cols;
            const int r = static_cast<int>(i) / cols;
            result.value.uvs.push_back({static_cast<float>(c) / std::max(cols - 1, 1),
                                        static_cast<float>(r) / std::max(rows - 1, 1)});
        }
        appendLog(info, "uv_synthetic");
    }

    return result;
}

} // namespace xmb
