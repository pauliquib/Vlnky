// SPDX-FileCopyrightText: 2026 Pavel Švec
// SPDX-License-Identifier: GPL-2.0-or-later

#include "xmbwave.hpp"

#include "wave_extract.hpp"
#include "xmb_cache.hpp"
#include "xmb_channel_map.hpp"
#include "xmb_gmo_parser.hpp"
#include "xmb_fcurve.hpp"
#include "xmb_texture.hpp"
#include "rco/rco_reader.hpp"

#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace xmb {
namespace {

std::string md5Hex(const std::vector<uint8_t> &data)
{
    uint64_t h = 14695981039346656037ULL;
    for (uint8_t b : data) {
        h ^= b;
        h *= 1099511628211ULL;
    }
    std::ostringstream oss;
    oss << std::hex << h;
    return oss.str();
}

} // namespace

std::string PrfFile::md5() const
{
    return md5Hex(data);
}

std::vector<float> GmoMesh::controlPointsFlat() const
{
    std::vector<float> out;
    out.reserve(positions.size() * 3);
    for (const auto &p : positions)
        out.insert(out.end(), {p.x, p.y, p.z});
    return out;
}

std::vector<uint8_t> GmoMesh::toCacheBin() const
{
    std::vector<uint8_t> buf;
    auto putU32 = [&](uint32_t v) {
        uint8_t b[4];
        std::memcpy(b, &v, 4);
        buf.insert(buf.end(), b, b + 4);
    };
    auto putF32 = [&](float v) {
        uint8_t b[4];
        std::memcpy(b, &v, 4);
        buf.insert(buf.end(), b, b + 4);
    };
    putU32(static_cast<uint32_t>(gridCols));
    putU32(static_cast<uint32_t>(gridRows));
    putU32(static_cast<uint32_t>(positions.size()));
    for (const auto &p : positions) {
        putF32(p.x);
        putF32(p.y);
        putF32(p.z);
    }
    for (const auto &uv : uvs) {
        putF32(uv.u);
        putF32(uv.v);
    }
    return buf;
}

float FCurveTrack::evaluate(float t) const
{
    if (keys.empty())
        return 0.f;
    if (keys.size() == 1)
        return keys[0].value;

    const float dur = duration > 0 ? duration : 1501.f;
    t = std::fmod(t, dur);
    if (t < 0)
        t += dur;

    for (size_t i = 0; i + 1 < keys.size(); ++i) {
        const auto &k0 = keys[i];
        const auto &k1 = keys[i + 1];
        if (k0.time <= t && t <= k1.time) {
            const float span = k1.time - k0.time;
            if (span <= 0.f)
                return k1.value;
            const float u = (t - k0.time) / span;
            const float u2 = u * u;
            const float u3 = u2 * u;
            const float h00 = 2 * u3 - 3 * u2 + 1;
            const float h10 = u3 - 2 * u2 + u;
            const float h01 = -2 * u3 + 3 * u2;
            const float h11 = u3 - u2;
            return h00 * k0.value + h10 * span * k0.outTangent + h01 * k1.value + h11 * span * k1.inTangent;
        }
    }
    return keys.back().value;
}

float FCurveSet::evaluateChannel(int channel, float t) const
{
    for (const auto &tr : tracks) {
        if (tr.channel == channel)
            return tr.evaluate(t);
    }
    return 0.f;
}

std::string FCurveSet::toAnimJson() const
{
    std::ostringstream oss;
    oss << "{\n  \"loop_duration\": " << loopDuration << ",\n  \"tracks\": [\n";
    for (size_t ti = 0; ti < tracks.size(); ++ti) {
        const auto &tr = tracks[ti];
        oss << "    {\"name\": \"" << tr.name << "\", \"channel\": " << tr.channel << ", \"duration\": " << tr.duration
            << ", \"keys\": [";
        for (size_t ki = 0; ki < tr.keys.size(); ++ki) {
            const auto &k = tr.keys[ki];
            oss << "{\"time\":" << k.time << ",\"value\":" << k.value << "}";
            if (ki + 1 < tr.keys.size())
                oss << ',';
        }
        oss << "]}";
        if (ti + 1 < tracks.size())
            oss << ',';
        oss << '\n';
    }
    oss << "  ]\n}\n";
    return oss.str();
}

PrfResult loadRcoResult(const std::string &path)
{
    PrfResult result;
    result.value.path = path;

    if (!std::filesystem::exists(path)) {
        result.error = RcoError::FileNotFound;
        return result;
    }

    std::ifstream in(path, std::ios::binary);
    if (!in) {
        result.error = RcoError::FileReadFailed;
        return result;
    }
    in.seekg(0, std::ios::end);
    const auto sz = in.tellg();
    if (sz < 0) {
        result.error = RcoError::FileReadFailed;
        return result;
    }
    in.seekg(0);
    result.value.data.resize(static_cast<size_t>(sz));
    in.read(reinterpret_cast<char *>(result.value.data.data()), sz);
    if (!in) {
        result.error = RcoError::FileReadFailed;
        return result;
    }

    if (result.value.data.size() < 4 || result.value.data[1] != 'P' || result.value.data[2] != 'R'
        || result.value.data[3] != 'F') {
        result.error = RcoError::InvalidPrfMagic;
        return result;
    }

    if (result.value.data.size() >= 8)
        std::memcpy(&result.value.version, result.value.data.data() + 4, 4);

    return result;
}

void ensurePlaceholderTexture(std::vector<uint8_t> &texture, int &texW, int &texH)
{
    if (!texture.empty() && texW > 0 && texH > 0)
        return;
    texture = proceduralWaveTexture();
    texW = 128;
    texH = 128;
}

RcoLoadReport loadRcoBundle(const std::string &path,
                            PrfFile &prf,
                            GmoMesh &mesh,
                            FCurveSet &anim,
                            ChannelMap *outChannelMap,
                            rco::WaveSceneTransform *outScene,
                            bool *outSceneFound)
{
    RcoLoadReport report;
    const auto t0 = std::chrono::steady_clock::now();

    mesh = {};
    anim = {};

    auto prfRes = loadRcoResult(path);
    if (!prfRes) {
        report.error = prfRes.error;
        return report;
    }
    prf = std::move(prfRes.value);

    std::vector<uint8_t> cachedTex;
    int cachedTexW = 0, cachedTexH = 0;
    auto cacheRes = loadFromCacheV2(path, prf, mesh, anim, cachedTex, cachedTexW, cachedTexH);
    if (cacheRes && cacheRes.value) {
        report.cacheHit = true;
        report.hasMesh = !mesh.positions.empty();
        report.hasAnim = !anim.tracks.empty();
        report.controlPointCount = mesh.controlPointCount;
        report.extractionMethod = "cache_v2";
        if (!cachedTex.empty()) {
            prf.texture = std::move(cachedTex);
            prf.textureWidth = cachedTexW;
            prf.textureHeight = cachedTexH;
            report.hasTexture = true;
        }
    } else {
        auto extracted = extractWaveFromRcoData(prf.data, path);
        mesh = std::move(extracted.mesh);
        anim = std::move(extracted.anim);
        prf = std::move(extracted.prf);
        prf.path = path;
        report = extracted.report.load;
        report.extractionMethod = extracted.report.extractionMethod;
        report.modelLabel = extracted.report.modelLabel;
        report.imageLabel = extracted.report.imageLabel;
        if (outChannelMap) {
            *outChannelMap = extracted.channelMap;
            const auto userMap = ChannelMap::loadForRco(path, prf.md5());
            if (userMap.yWave.channel >= 0)
                *outChannelMap = userMap;
        }
        if (outScene)
            *outScene = extracted.report.sceneTransform;
        if (outSceneFound)
            *outSceneFound = extracted.report.sceneFound;

        if (report.hasMesh) {
            writeCache(prf, mesh, anim);
            writeCacheV2(path, prf, mesh, anim);
        }
    }

    if (!report.hasTexture && prf.texture.empty()) {
        ensurePlaceholderTexture(prf.texture, prf.textureWidth, prf.textureHeight);
        report.textureMethod = "placeholder";
    } else if (!prf.texture.empty()) {
        report.hasTexture = true;
        double sum = 0;
        for (uint8_t b : prf.texture)
            sum += b;
        report.textureAvg = float(sum / double(prf.texture.size()));
    }

    if (report.hasMesh)
        report.error = RcoError::Ok;
    else if (report.error == RcoError::Ok)
        report.error = RcoError::TooFewControlPoints;

    const auto t1 = std::chrono::steady_clock::now();
    report.parseMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    return report;
}

PrfFile loadRco(const std::string &path)
{
    auto r = loadRcoResult(path);
    if (!r)
        throw std::runtime_error(std::string(rcoErrorString(r.error)) + ": " + path);
    return std::move(r.value);
}

GmoMesh parseGmo(const std::vector<uint8_t> &data, int gmoOffset)
{
    auto r = parseGmoResult(data, gmoOffset);
    if (!r)
        throw std::runtime_error(rcoErrorString(r.error));
    return std::move(r.value);
}

FCurveSet parseFcurves(const std::vector<uint8_t> &data)
{
    auto r = parseFcurvesResult(data);
    return std::move(r.value);
}

std::string writeCache(const PrfFile &prf, const GmoMesh &mesh, const FCurveSet &anim)
{
    namespace fs = std::filesystem;
    const auto root = fs::path(std::getenv("HOME") ? std::getenv("HOME") : "/tmp") / ".cache" / "vlnky" / prf.md5();
    fs::create_directories(root);

    std::ofstream rawBin(root / "control_points.bin", std::ios::binary);
    const auto bin = mesh.toCacheBin();
    rawBin.write(reinterpret_cast<const char *>(bin.data()), bin.size());

    if (!prf.texture.empty()) {
        std::ofstream tex(root / "texture.raw", std::ios::binary);
        tex.write(reinterpret_cast<const char *>(prf.texture.data()), prf.texture.size());
    }

    std::ofstream animJson(root / "anim.json");
    animJson << anim.toAnimJson();

    return root.string();
}

} // namespace xmb
