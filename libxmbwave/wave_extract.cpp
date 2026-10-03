// SPDX-FileCopyrightText: 2026 Pavel Švec
// SPDX-License-Identifier: GPL-2.0-or-later

#include "wave_extract.hpp"

#include "xmb_fcurve.hpp"
#include "xmb_gmo_parser.hpp"
#include "xmb_texture.hpp"

#include <cstring>

namespace xmb {
namespace {

constexpr uint8_t kGmoMagic[] = {'O', 'M', 'G', '.', '0', '0', '.', '1', 'P', 'S', 'P', 0};

int findGmoInBlob(const std::vector<uint8_t> &blob)
{
    for (size_t i = 0; i + 12 <= blob.size(); ++i) {
        if (std::memcmp(blob.data() + i, kGmoMagic, 12) == 0)
            return static_cast<int>(i);
    }
    return blob.size() >= 12 ? 0 : -1;
}

void applySceneTransform(GmoMesh &mesh, const rco::WaveSceneTransform &scene)
{
    if (scene.scaleWidth == 1.f && scene.scaleHeight == 1.f && scene.scaleDepth == 1.f && scene.posX == 0.f
        && scene.posY == 0.f && scene.posZ == 0.f)
        return;

    for (auto &p : mesh.positions) {
        p.x = scene.posX + p.x * scene.scaleWidth;
        p.y = scene.posY + p.y * scene.scaleHeight;
        p.z = scene.posZ + p.z * scene.scaleDepth;
    }
}

GmoMesh parseGmoFallbackScan(const std::vector<uint8_t> &data)
{
    for (size_t i = 0; i + 12 <= data.size(); ++i) {
        if (std::memcmp(data.data() + i, kGmoMagic, 12) == 0) {
            auto r = parseGmoResult(data, static_cast<int>(i));
            if (r)
                return std::move(r.value);
        }
    }
    return {};
}

} // namespace

ChannelMap channelMapFromFcurves(const FCurveSet &anim)
{
    ChannelMap map;
    bool hasCh2 = false;
    bool hasCh16 = false;
    for (const auto &tr : anim.tracks) {
        if (tr.channel == 2)
            hasCh2 = true;
        if (tr.channel == 16)
            hasCh16 = true;
    }
    if (hasCh2) {
        map.yWave = {2, 1.f, false};
        map.zOffset = {2, 1.f, false};
        map.waveAmplitude = {2, 1.f, true};
    }
    if (hasCh16) {
        map.wavePhase = {16, 1.f, false};
    }
    if (!hasCh2 && !hasCh16)
        return ChannelMap::defaultMap();
    return map;
}

WaveExtractResult extractWaveFromRcoData(const std::vector<uint8_t> &data, const std::string &pathHint)
{
    WaveExtractResult out;
    out.prf.path = pathHint;
    out.prf.data = data;
    if (data.size() >= 8)
        std::memcpy(&out.prf.version, data.data() + 4, 4);

    auto docRes = rco::readRcoDocument(data);
    std::vector<uint8_t> modelBlob;
    out.report.extractionMethod = "fallback_scan";

    if (docRes) {
        const auto &doc = docRes.value;
        out.report.extractionMethod = "rco_tree";
        out.report.sceneFound = doc.scene.foundModelObject;
        out.report.sceneTransform = doc.scene.modelObject;
        out.report.modelLabel = doc.scene.modelLabel;
        out.report.load.parseLog = "rco_tree";

        if (doc.scene.modelEntry) {
            if (auto blobRes = rco::readNodeResource(doc, *doc.scene.modelEntry))
                modelBlob = std::move(blobRes.value);
        }

        if (doc.scene.imageEntry) {
            out.report.imageLabel = doc.scene.imageEntry->label;
            if (auto imgRes = rco::readNodeResource(doc, *doc.scene.imageEntry)) {
                if (auto tex = decodeGimFromBytes(imgRes.value); !tex.pixels.empty()) {
                    out.prf.texture = std::move(tex.pixels);
                    out.prf.textureWidth = tex.width;
                    out.prf.textureHeight = tex.height;
                    out.report.load.textureMethod = "tree_gim";
                    out.report.load.hasTexture = true;
                }
            }
        }
    }

    if (!modelBlob.empty()) {
        const int gmoOff = findGmoInBlob(modelBlob);
        if (gmoOff >= 0) {
            GmoParseInfo info;
            if (auto meshRes = parseGmoResult(modelBlob, gmoOff, GmoParseOptions{}, &info)) {
                out.mesh = std::move(meshRes.value);
                out.report.load.hasMesh = true;
                out.report.load.controlPointCount = info.controlPointCount;
                out.report.load.parseLog += "; " + info.log;
            }
        }
        if (auto animRes = parseFcurvesResult(modelBlob); !animRes.value.tracks.empty()) {
            out.anim = std::move(animRes.value);
            out.report.load.hasAnim = true;
            out.report.load.parseLog += "; " + fcurveChannelSummary(out.anim);
        }
    }

    if (!out.report.load.hasMesh) {
        out.mesh = parseGmoFallbackScan(data);
        if (!out.mesh.positions.empty()) {
            out.report.load.hasMesh = true;
            out.report.extractionMethod = "fallback_gmo_scan";
            out.report.load.controlPointCount = int(out.mesh.positions.size());
            out.report.load.parseLog += "; fallback_gmo_scan";
        }
    }

    if (!out.report.load.hasAnim) {
        const auto &animSrc = modelBlob.empty() ? data : modelBlob;
        if (auto animRes = parseFcurvesResult(animSrc); !animRes.value.tracks.empty()) {
            out.anim = std::move(animRes.value);
            out.report.load.hasAnim = true;
        } else {
            out.report.load.parseLog += "; fcurve_none_static";
        }
    }

    if (out.report.sceneFound)
        applySceneTransform(out.mesh, out.report.sceneTransform);

    if (out.prf.texture.empty()) {
        TextureExtractOptions opts;
        opts.allowInvert = false;
        bool forceInv = false, skipInv = false;
        if (textureInvertOverrideForHash(out.prf.md5(), forceInv, skipInv)) {
            opts.forceInvert = forceInv;
            opts.skipInvert = skipInv;
        }
        const auto tex = extractTextureFromPrf(data, out.prf.md5(), opts);
        if (!tex.pixels.empty()) {
            out.prf.texture = tex.pixels;
            out.prf.textureWidth = tex.width;
            out.prf.textureHeight = tex.height;
            out.report.load.textureMethod = tex.method;
            out.report.load.textureInverted = tex.inverted;
            out.report.load.hasTexture = true;
        }
    }

    if (out.prf.texture.empty()) {
        ensurePlaceholderTexture(out.prf.texture, out.prf.textureWidth, out.prf.textureHeight);
        out.report.load.textureMethod = "placeholder";
    } else {
        out.report.load.hasTexture = true;
    }

    out.channelMap = channelMapFromFcurves(out.anim);
    out.report.load.error = out.report.load.hasMesh ? RcoError::Ok : RcoError::GmoNotFound;
    out.mesh.controlPointCount = int(out.mesh.positions.size());
    return out;
}

} // namespace xmb
