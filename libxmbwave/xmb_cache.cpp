// SPDX-FileCopyrightText: 2026 Pavel Švec
// SPDX-License-Identifier: GPL-2.0-or-later

#include "xmb_cache.hpp"

#include "xmb_crc32.hpp"
#include "xmbwave.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace xmb {
namespace {

constexpr char kCacheMagicV1[4] = {'X', 'M', 'B', '1'};
constexpr char kCacheMagicV2[4] = {'X', 'M', 'B', '2'};
constexpr uint32_t kCacheVersionV1 = 1;
constexpr uint32_t kCacheVersionV2 = 2;
constexpr size_t kMaxFcurveKeys = 4096;

bool canRead(const std::vector<uint8_t> &d, size_t off, size_t n)
{
    return off <= d.size() && n <= d.size() - off;
}

bool readU32(const std::vector<uint8_t> &d, size_t off, uint32_t &out)
{
    if (!canRead(d, off, 4))
        return false;
    std::memcpy(&out, d.data() + off, 4);
    return true;
}

bool readU64(const std::vector<uint8_t> &d, size_t off, uint64_t &out)
{
    if (!canRead(d, off, 8))
        return false;
    std::memcpy(&out, d.data() + off, 8);
    return true;
}

bool readF32(const std::vector<uint8_t> &d, size_t off, float &out)
{
    if (!canRead(d, off, 4))
        return false;
    std::memcpy(&out, d.data() + off, 4);
    return true;
}

void writeU32(std::vector<uint8_t> &buf, uint32_t v)
{
    uint8_t b[4];
    std::memcpy(b, &v, 4);
    buf.insert(buf.end(), b, b + 4);
}

void writeU64(std::vector<uint8_t> &buf, uint64_t v)
{
    uint8_t b[8];
    std::memcpy(b, &v, 8);
    buf.insert(buf.end(), b, b + 8);
}

void writeF32(std::vector<uint8_t> &buf, float v)
{
    uint8_t b[4];
    std::memcpy(b, &v, 4);
    buf.insert(buf.end(), b, b + 4);
}

void writeString(std::vector<uint8_t> &buf, const std::string &s)
{
    writeU32(buf, static_cast<uint32_t>(s.size()));
    buf.insert(buf.end(), s.begin(), s.end());
}

bool readString(const std::vector<uint8_t> &d, size_t &off, std::string &out)
{
    uint32_t len = 0;
    if (!readU32(d, off, len))
        return false;
    off += 4;
    if (!canRead(d, off, len))
        return false;
    out.assign(reinterpret_cast<const char *>(d.data() + off), len);
    off += len;
    return true;
}

std::filesystem::path homeCacheDir()
{
    const char *home = std::getenv("HOME");
    return std::filesystem::path(home ? home : "/tmp") / ".cache" / "vlnky";
}

bool readFileBytes(const std::string &path, std::vector<uint8_t> &out)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return false;
    in.seekg(0, std::ios::end);
    const auto sz = in.tellg();
    if (sz < 0)
        return false;
    in.seekg(0);
    out.resize(static_cast<size_t>(sz));
    in.read(reinterpret_cast<char *>(out.data()), sz);
    return bool(in);
}

RcoError atomicWriteFile(const std::filesystem::path &dest, const std::vector<uint8_t> &buf)
{
    const auto tmp = dest.string() + ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out)
            return RcoError::CacheWriteFailed;
        out.write(reinterpret_cast<const char *>(buf.data()), std::streamsize(buf.size()));
        if (!out)
            return RcoError::CacheWriteFailed;
    }
    std::error_code ec;
    std::filesystem::rename(tmp, dest, ec);
    if (ec) {
        std::filesystem::remove(tmp, ec);
        return RcoError::CacheWriteFailed;
    }
    return RcoError::Ok;
}

} // namespace

std::string cacheRootForHash(const std::string &hash)
{
    return (homeCacheDir() / hash).string();
}

RcoSourceInfo rcoSourceInfo(const std::string &path)
{
    RcoSourceInfo info;
    std::error_code ec;
    const auto ft = std::filesystem::last_write_time(path, ec);
    if (!ec) {
        const auto sctp = std::chrono::time_point_cast<std::chrono::seconds>(
            ft - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
        info.mtime = static_cast<uint64_t>(sctp.time_since_epoch().count());
    }
    info.size = std::filesystem::file_size(path, ec);
    if (ec)
        info.size = 0;
    return info;
}

bool loadMeshFromLegacyCache(const std::string &cacheDir, GmoMesh &mesh)
{
    std::ifstream in(std::filesystem::path(cacheDir) / "control_points.bin", std::ios::binary);
    if (!in)
        return false;

    in.seekg(0, std::ios::end);
    const auto sz = in.tellg();
    in.seekg(0);
    if (sz < 12)
        return false;

    std::vector<uint8_t> data(static_cast<size_t>(sz));
    in.read(reinterpret_cast<char *>(data.data()), sz);
    if (!in)
        return false;

    size_t off = 0;
    uint32_t cols = 0, rows = 0, count = 0;
    if (!readU32(data, off, cols) || !readU32(data, off + 4, rows) || !readU32(data, off + 8, count))
        return false;
    off += 12;

    if (cols == 0 || rows == 0 || count == 0)
        return false;

    const size_t posBytes = size_t(count) * 12;
    const size_t uvBytes = size_t(count) * 8;
    if (!canRead(data, off, posBytes + uvBytes))
        return false;

    mesh.positions.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        float x = 0, y = 0, z = 0;
        if (!readF32(data, off + i * 12, x) || !readF32(data, off + i * 12 + 4, y)
            || !readF32(data, off + i * 12 + 8, z))
            return false;
        mesh.positions[i] = {x, y, z};
    }
    off += posBytes;

    mesh.uvs.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        float u = 0, v = 0;
        if (!readF32(data, off + i * 8, u) || !readF32(data, off + i * 8 + 4, v))
            return false;
        mesh.uvs[i] = {u, v};
    }

    mesh.gridCols = static_cast<int>(cols);
    mesh.gridRows = static_cast<int>(rows);
    return !mesh.positions.empty();
}

static bool serializeAnim(const FCurveSet &anim, std::vector<uint8_t> &out)
{
    out.clear();
    writeU32(out, static_cast<uint32_t>(anim.tracks.size()));
    writeF32(out, anim.loopDuration);

    for (const auto &tr : anim.tracks) {
        writeString(out, tr.name);
        writeU32(out, static_cast<uint32_t>(tr.channel));
        writeF32(out, tr.duration);
        const uint32_t nKeys = static_cast<uint32_t>(std::min(tr.keys.size(), kMaxFcurveKeys));
        writeU32(out, nKeys);
        for (uint32_t i = 0; i < nKeys; ++i) {
            const auto &k = tr.keys[i];
            writeF32(out, k.time);
            writeF32(out, k.value);
            writeF32(out, k.inTangent);
            writeF32(out, k.outTangent);
        }
    }
    return true;
}

static bool deserializeAnim(const std::vector<uint8_t> &data, size_t &off, FCurveSet &anim)
{
    uint32_t trackCount = 0;
    if (!readU32(data, off, trackCount))
        return false;
    off += 4;
    if (!readF32(data, off, anim.loopDuration))
        return false;
    off += 4;

    if (trackCount > 256)
        return false;

    anim.tracks.clear();
    anim.tracks.reserve(trackCount);
    for (uint32_t t = 0; t < trackCount; ++t) {
        FCurveTrack tr;
        if (!readString(data, off, tr.name))
            return false;
        uint32_t ch = 0;
        if (!readU32(data, off, ch))
            return false;
        off += 4;
        tr.channel = static_cast<int>(ch);
        if (!readF32(data, off, tr.duration))
            return false;
        off += 4;

        uint32_t nKeys = 0;
        if (!readU32(data, off, nKeys))
            return false;
        off += 4;
        if (nKeys > kMaxFcurveKeys)
            return false;

        tr.keys.resize(nKeys);
        for (uint32_t i = 0; i < nKeys; ++i) {
            FCurveKeyframe k;
            if (!readF32(data, off, k.time) || !readF32(data, off + 4, k.value)
                || !readF32(data, off + 8, k.inTangent) || !readF32(data, off + 12, k.outTangent))
                return false;
            off += 16;
            tr.keys[i] = k;
        }
        anim.tracks.push_back(std::move(tr));
    }
    return true;
}

RcoError writeCacheV1(const std::string &rcoPath, const PrfFile &prf, const GmoMesh &mesh, const FCurveSet &anim)
{
    const auto info = rcoSourceInfo(rcoPath);
    const auto root = homeCacheDir() / prf.md5();
    std::error_code ec;
    std::filesystem::create_directories(root, ec);

    std::vector<uint8_t> buf;
    buf.insert(buf.end(), kCacheMagicV1, kCacheMagicV1 + 4);
    writeU32(buf, kCacheVersionV1);
    writeU64(buf, info.mtime);
    writeU64(buf, info.size);

    writeU32(buf, static_cast<uint32_t>(mesh.gridCols));
    writeU32(buf, static_cast<uint32_t>(mesh.gridRows));
    writeU32(buf, static_cast<uint32_t>(mesh.positions.size()));
    for (const auto &p : mesh.positions) {
        writeF32(buf, p.x);
        writeF32(buf, p.y);
        writeF32(buf, p.z);
    }
    for (const auto &uv : mesh.uvs) {
        writeF32(buf, uv.u);
        writeF32(buf, uv.v);
    }

    std::vector<uint8_t> animBlob;
    if (!serializeAnim(anim, animBlob))
        return RcoError::CacheWriteFailed;
    writeU32(buf, static_cast<uint32_t>(animBlob.size()));
    buf.insert(buf.end(), animBlob.begin(), animBlob.end());

    writeU32(buf, static_cast<uint32_t>(prf.textureWidth));
    writeU32(buf, static_cast<uint32_t>(prf.textureHeight));
    writeU32(buf, static_cast<uint32_t>(prf.texture.size()));
    buf.insert(buf.end(), prf.texture.begin(), prf.texture.end());

    std::ofstream out(root / "xmb_cache_v1.bin", std::ios::binary);
    if (!out)
        return RcoError::CacheWriteFailed;
    out.write(reinterpret_cast<const char *>(buf.data()), std::streamsize(buf.size()));
    return out ? RcoError::Ok : RcoError::CacheWriteFailed;
}

RcoResult<bool> loadFromCacheV1(const std::string &rcoPath,
                                const PrfFile &prf,
                                GmoMesh &mesh,
                                FCurveSet &anim,
                                std::vector<uint8_t> &texture,
                                int &texW,
                                int &texH)
{
    RcoResult<bool> result;
    result.value = false;

    const auto info = rcoSourceInfo(rcoPath);
    const auto path = homeCacheDir() / prf.md5() / "xmb_cache_v1.bin";
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        result.error = RcoError::CacheReadFailed;
        return result;
    }

    in.seekg(0, std::ios::end);
    const auto sz = in.tellg();
    in.seekg(0);
    if (sz < 24) {
        result.error = RcoError::CacheReadFailed;
        return result;
    }

    std::vector<uint8_t> data(static_cast<size_t>(sz));
    in.read(reinterpret_cast<char *>(data.data()), sz);
    if (!in) {
        result.error = RcoError::CacheReadFailed;
        return result;
    }

    if (!canRead(data, 0, 4) || std::memcmp(data.data(), kCacheMagicV1, 4) != 0) {
        result.error = RcoError::CacheReadFailed;
        return result;
    }

    size_t off = 4;
    uint32_t version = 0;
    uint64_t mtime = 0, size = 0;
    if (!readU32(data, off, version) || !readU64(data, off + 4, mtime) || !readU64(data, off + 12, size)) {
        result.error = RcoError::BufferOverrun;
        return result;
    }
    off += 20;

    if (version != kCacheVersionV1 || mtime != info.mtime || size != info.size) {
        result.error = RcoError::CacheStale;
        return result;
    }

    uint32_t cols = 0, rows = 0, count = 0;
    if (!readU32(data, off, cols) || !readU32(data, off + 4, rows) || !readU32(data, off + 8, count)) {
        result.error = RcoError::BufferOverrun;
        return result;
    }
    off += 12;

    if (count == 0 || count > 100000) {
        result.error = RcoError::BufferOverrun;
        return result;
    }

    const size_t posBytes = size_t(count) * 12;
    const size_t uvBytes = size_t(count) * 8;
    if (!canRead(data, off, posBytes + uvBytes)) {
        result.error = RcoError::BufferOverrun;
        return result;
    }

    mesh.positions.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        float x = 0, y = 0, z = 0;
        if (!readF32(data, off + i * 12, x) || !readF32(data, off + i * 12 + 4, y)
            || !readF32(data, off + i * 12 + 8, z))
        {
            result.error = RcoError::BufferOverrun;
            return result;
        }
        mesh.positions[i] = {x, y, z};
    }
    off += posBytes;

    mesh.uvs.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        float u = 0, v = 0;
        if (!readF32(data, off + i * 8, u) || !readF32(data, off + i * 8 + 4, v)) {
            result.error = RcoError::BufferOverrun;
            return result;
        }
        mesh.uvs[i] = {u, v};
    }
    off += uvBytes;

    mesh.gridCols = static_cast<int>(cols);
    mesh.gridRows = static_cast<int>(rows);

    uint32_t animLen = 0;
    if (!readU32(data, off, animLen)) {
        result.error = RcoError::BufferOverrun;
        return result;
    }
    off += 4;
    if (!canRead(data, off, animLen)) {
        result.error = RcoError::BufferOverrun;
        return result;
    }
    std::vector<uint8_t> animData(data.begin() + off, data.begin() + off + animLen);
    off += animLen;
    size_t animOff = 0;
    if (!deserializeAnim(animData, animOff, anim)) {
        result.error = RcoError::BufferOverrun;
        return result;
    }

    uint32_t tw = 0, th = 0, tlen = 0;
    if (!readU32(data, off, tw) || !readU32(data, off + 4, th) || !readU32(data, off + 8, tlen)) {
        result.error = RcoError::BufferOverrun;
        return result;
    }
    off += 12;
    if (!canRead(data, off, tlen)) {
        result.error = RcoError::BufferOverrun;
        return result;
    }
    texture.assign(data.begin() + off, data.begin() + off + tlen);
    texW = static_cast<int>(tw);
    texH = static_cast<int>(th);

    result.value = true;
    return result;
}

RcoError writeCacheV2(const std::string &rcoPath, const PrfFile &prf, const GmoMesh &mesh, const FCurveSet &anim)
{
    const auto info = rcoSourceInfo(rcoPath);
    const auto root = homeCacheDir() / prf.md5();
    std::error_code ec;
    std::filesystem::create_directories(root, ec);

    std::vector<uint8_t> rcoBytes;
    if (!readFileBytes(rcoPath, rcoBytes))
        return RcoError::CacheWriteFailed;
    const uint32_t sourceCrc = crc32(rcoBytes);

    std::vector<uint8_t> buf;
    buf.insert(buf.end(), kCacheMagicV2, kCacheMagicV2 + 4);
    writeU32(buf, kCacheVersionV2);
    writeU32(buf, kXmbParserVersion);
    writeU32(buf, sourceCrc);
    writeU64(buf, info.mtime);
    writeU64(buf, info.size);

    writeU32(buf, static_cast<uint32_t>(mesh.gridCols));
    writeU32(buf, static_cast<uint32_t>(mesh.gridRows));
    writeU32(buf, static_cast<uint32_t>(mesh.positions.size()));
    writeU32(buf, static_cast<uint32_t>(mesh.indices.size()));
    writeU32(buf, static_cast<uint32_t>(mesh.gridSource));
    writeU32(buf, static_cast<uint32_t>(mesh.indexTopology));
    writeU32(buf, mesh.windingCw ? 1u : 0u);
    writeU32(buf, static_cast<uint32_t>(mesh.faceCount));
    for (const auto &p : mesh.positions) {
        writeF32(buf, p.x);
        writeF32(buf, p.y);
        writeF32(buf, p.z);
    }
    for (const auto &uv : mesh.uvs) {
        writeF32(buf, uv.u);
        writeF32(buf, uv.v);
    }
    for (uint32_t idx : mesh.indices)
        writeU32(buf, idx);

    std::vector<uint8_t> animBlob;
    if (!serializeAnim(anim, animBlob))
        return RcoError::CacheWriteFailed;
    writeU32(buf, static_cast<uint32_t>(animBlob.size()));
    buf.insert(buf.end(), animBlob.begin(), animBlob.end());

    writeU32(buf, static_cast<uint32_t>(prf.textureWidth));
    writeU32(buf, static_cast<uint32_t>(prf.textureHeight));
    writeU32(buf, static_cast<uint32_t>(prf.texture.size()));
    buf.insert(buf.end(), prf.texture.begin(), prf.texture.end());

    return atomicWriteFile(root / "xmb_cache_v2.bin", buf);
}

RcoResult<bool> loadFromCacheV2(const std::string &rcoPath,
                                const PrfFile &prf,
                                GmoMesh &mesh,
                                FCurveSet &anim,
                                std::vector<uint8_t> &texture,
                                int &texW,
                                int &texH)
{
    RcoResult<bool> result;
    result.value = false;

    const auto v2path = homeCacheDir() / prf.md5() / "xmb_cache_v2.bin";
    std::ifstream in(v2path, std::ios::binary);
    if (in) {
        in.seekg(0, std::ios::end);
        const auto sz = in.tellg();
        in.seekg(0);
        const auto fileSize = sz > 0 ? static_cast<size_t>(sz) : size_t(0);
        std::vector<uint8_t> data(fileSize);
        if (fileSize > 0) {
            in.read(reinterpret_cast<char *>(data.data()), static_cast<std::streamsize>(fileSize));
            if (in && canRead(data, 0, 4) && std::memcmp(data.data(), kCacheMagicV2, 4) == 0) {
                const auto info = rcoSourceInfo(rcoPath);
                std::vector<uint8_t> rcoBytes;
                uint32_t sourceCrc = 0;
                if (readFileBytes(rcoPath, rcoBytes))
                    sourceCrc = crc32(rcoBytes);

                size_t off = 4;
                uint32_t version = 0, parserVer = 0, crcStored = 0;
                uint64_t mtime = 0, size = 0;
                if (readU32(data, off, version) && readU32(data, off + 4, parserVer)
                    && readU32(data, off + 8, crcStored) && readU64(data, off + 12, mtime)
                    && readU64(data, off + 20, size)) {
                    off += 28;
                    if (version == kCacheVersionV2 && parserVer == kXmbParserVersion && crcStored == sourceCrc
                        && mtime == info.mtime && size == info.size) {
                        uint32_t cols = 0, rows = 0, count = 0, idxCount = 0;
                        uint32_t gridSource = 0, indexTopo = 0, winding = 0, faceCount = 0;
                        if (readU32(data, off, cols) && readU32(data, off + 4, rows)
                            && readU32(data, off + 8, count) && readU32(data, off + 12, idxCount)) {
                            off += 16;
                            if (off + 16 <= data.size()
                                && readU32(data, off, gridSource) && readU32(data, off + 4, indexTopo)
                                && readU32(data, off + 8, winding) && readU32(data, off + 12, faceCount)) {
                                off += 16;
                            } else {
                                gridSource = 0;
                                indexTopo = 0;
                                winding = 0;
                                faceCount = 0;
                            }
                            const size_t posBytes = size_t(count) * 12;
                            const size_t uvBytes = size_t(count) * 8;
                            const size_t idxBytes = size_t(idxCount) * 4;
                            if (canRead(data, off, posBytes + uvBytes + idxBytes)) {
                                mesh.positions.resize(count);
                                for (uint32_t i = 0; i < count; ++i) {
                                    float x = 0, y = 0, z = 0;
                                    readF32(data, off + i * 12, x);
                                    readF32(data, off + i * 12 + 4, y);
                                    readF32(data, off + i * 12 + 8, z);
                                    mesh.positions[i] = {x, y, z};
                                }
                                off += posBytes;
                                mesh.uvs.resize(count);
                                for (uint32_t i = 0; i < count; ++i) {
                                    float u = 0, v = 0;
                                    readF32(data, off + i * 8, u);
                                    readF32(data, off + i * 8 + 4, v);
                                    mesh.uvs[i] = {u, v};
                                }
                                off += uvBytes;
                                mesh.indices.resize(idxCount);
                                for (uint32_t i = 0; i < idxCount; ++i) {
                                    uint32_t idx = 0;
                                    readU32(data, off + i * 4, idx);
                                    mesh.indices[i] = idx;
                                }
                                off += idxBytes;
                                mesh.gridCols = int(cols);
                                mesh.gridRows = int(rows);
                                mesh.controlPointCount = int(count);
                                mesh.gridSource = static_cast<GridDimensionSource>(gridSource);
                                mesh.indexTopology = static_cast<IndexTopology>(indexTopo);
                                mesh.windingCw = winding != 0;
                                mesh.faceCount = int(faceCount);

                                uint32_t animLen = 0;
                                if (readU32(data, off, animLen)) {
                                    off += 4;
                                    if (canRead(data, off, animLen)) {
                                        std::vector<uint8_t> animData(data.begin() + off,
                                                                      data.begin() + off + animLen);
                                        off += animLen;
                                        size_t animOff = 0;
                                        if (deserializeAnim(animData, animOff, anim)) {
                                            uint32_t tw = 0, th = 0, tlen = 0;
                                            if (readU32(data, off, tw) && readU32(data, off + 4, th)
                                                && readU32(data, off + 8, tlen) && canRead(data, off + 12, tlen)) {
                                                off += 12;
                                                texture.assign(data.begin() + off, data.begin() + off + tlen);
                                                texW = int(tw);
                                                texH = int(th);
                                                result.value = true;
                                                return result;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return result;
}

} // namespace xmb
