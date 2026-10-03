// SPDX-FileCopyrightText: 2026 Pavel Švec
// SPDX-License-Identifier: GPL-2.0-or-later

#include "xmb_fcurve.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace xmb {
namespace {

constexpr size_t kMaxFcurveKeys = 4096;
constexpr size_t kFcurveBlobMax = 4096;

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

bool readF32(const std::vector<uint8_t> &d, size_t off, float &out)
{
    if (!canRead(d, off, 4))
        return false;
    std::memcpy(&out, d.data() + off, 4);
    return true;
}

bool keyframeValid(float time, float value, float inT, float outT)
{
    if (!std::isfinite(time) || !std::isfinite(value) || !std::isfinite(inT) || !std::isfinite(outT))
        return false;
    if (std::abs(time) > 1e6f || std::abs(value) > 1e6f || std::abs(inT) > 1e6f || std::abs(outT) > 1e6f)
        return false;
    return true;
}

FCurveTrack parseFcurveBlob(const std::string &name, const std::vector<uint8_t> &blob)
{
    FCurveTrack tr;
    tr.name = name;
    if (blob.size() < 12)
        return tr;

    uint32_t numKeys = 0;
    uint32_t channel = 0;
    uint32_t durationU = 0;
    if (!readU32(blob, 0, numKeys) || !readU32(blob, 4, channel) || !readU32(blob, 8, durationU))
        return tr;

    tr.channel = static_cast<int>(channel);
    tr.duration = durationU > 0 ? static_cast<float>(durationU) : 1501.f;

    if (numKeys > kMaxFcurveKeys)
        numKeys = kMaxFcurveKeys;

    size_t p = 20;
    bool usedFallback = false;

    if (numKeys <= 1) {
        usedFallback = true;
        while (p + 8 <= blob.size() && tr.keys.size() < 64) {
            float a = 0, b = 0;
            if (!readF32(blob, p, a) || !readF32(blob, p + 4, b)) {
                p += 4;
                continue;
            }
            if (!keyframeValid(static_cast<float>(tr.keys.size()), b, 0.f, 0.f)) {
                p += 4;
                continue;
            }
            tr.keys.push_back({static_cast<float>(tr.keys.size()), b, 0.f, 0.f});
            p += 8;
        }
        if (tr.keys.size() > 1) {
            for (size_t i = 0; i < tr.keys.size(); ++i)
                tr.keys[i].time = (static_cast<float>(i) / static_cast<float>(tr.keys.size() - 1)) * tr.duration;
        }
    } else {
        for (uint32_t i = 0; i < numKeys && p + 16 <= blob.size(); ++i) {
            FCurveKeyframe k;
            if (!readF32(blob, p, k.time) || !readF32(blob, p + 4, k.value) || !readF32(blob, p + 8, k.inTangent)
                || !readF32(blob, p + 12, k.outTangent))
                break;
            if (!keyframeValid(k.time, k.value, k.inTangent, k.outTangent)) {
                usedFallback = true;
                break;
            }
            tr.keys.push_back(k);
            p += 16;
        }
    }

    if (usedFallback && tr.keys.size() < 2 && numKeys > 1) {
        tr.keys.clear();
        p = 20;
        while (p + 8 <= blob.size() && tr.keys.size() < 64) {
            float a = 0, b = 0;
            if (!readF32(blob, p, a) || !readF32(blob, p + 4, b)) {
                p += 4;
                continue;
            }
            if (std::abs(a) > 1e6f || std::abs(b) > 1e6f) {
                p += 4;
                continue;
            }
            tr.keys.push_back({static_cast<float>(tr.keys.size()), b, 0.f, 0.f});
            p += 8;
        }
        if (tr.keys.size() > 1) {
            for (size_t i = 0; i < tr.keys.size(); ++i)
                tr.keys[i].time = (static_cast<float>(i) / static_cast<float>(tr.keys.size() - 1)) * tr.duration;
        }
    }

    if (tr.keys.empty())
        tr.keys.push_back({0.f, 0.f, 0.f, 0.f});

    return tr;
}

} // namespace

float computeLoopDuration(const FCurveSet &anim, bool *multipleDurations)
{
    if (multipleDurations)
        *multipleDurations = false;
    if (anim.tracks.empty())
        return 1501.f;

    float maxDur = 0.f;
    float firstDur = anim.tracks[0].duration;
    for (const auto &tr : anim.tracks) {
        maxDur = std::max(maxDur, tr.duration > 0 ? tr.duration : 1501.f);
        if (multipleDurations && std::abs(tr.duration - firstDur) > 1.f)
            *multipleDurations = true;
    }
    return maxDur > 0 ? maxDur : 1501.f;
}

FCurveResult parseFcurvesResult(const std::vector<uint8_t> &data)
{
    FCurveResult result;
    const char *needle = "fcurve-";
    const size_t nlen = 7;
    size_t pos = 0;
    while (pos < data.size()) {
        auto it = std::search(data.begin() + pos, data.end(),
                              reinterpret_cast<const uint8_t *>(needle),
                              reinterpret_cast<const uint8_t *>(needle + nlen));
        if (it == data.end())
            break;
        const size_t start = static_cast<size_t>(it - data.begin());
        size_t nameEnd = start;
        while (nameEnd < data.size() && data[nameEnd] != 0)
            ++nameEnd;
        std::string name(reinterpret_cast<const char *>(data.data() + start), nameEnd - start);
        size_t p = nameEnd + 1;
        while (p % 4)
            ++p;
        if (p >= data.size())
            break;
        const size_t blobLen = std::min(kFcurveBlobMax, data.size() - p);
        std::vector<uint8_t> blob(data.begin() + p, data.begin() + p + blobLen);
        result.value.tracks.push_back(parseFcurveBlob(name, blob));
        pos = nameEnd + 1;
    }

    bool multi = false;
    result.value.loopDuration = computeLoopDuration(result.value, &multi);
    if (multi)
        result.value.loopDuration = computeLoopDuration(result.value, nullptr);
    return result;
}

} // namespace xmb
