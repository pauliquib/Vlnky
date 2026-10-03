// SPDX-FileCopyrightText: 2026 Pavel Švec
// SPDX-License-Identifier: GPL-2.0-or-later

#include "xmb_channel_map.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>

namespace xmb {
namespace {

std::filesystem::path channelMapConfigPath()
{
    const char *home = std::getenv("HOME");
    return std::filesystem::path(home ? home : "/tmp") / ".config" / "vlnky" / "channel_map.json";
}

ChannelBinding parseBinding(const std::string &json, const char *key, ChannelBinding fallback)
{
    const std::string needle = std::string("\"") + key + "\"";
    const auto pos = json.find(needle);
    if (pos == std::string::npos)
        return fallback;

    ChannelBinding b = fallback;
    const auto chPos = json.find("\"channel\"", pos);
    if (chPos != std::string::npos && chPos < pos + 200) {
        const auto colon = json.find(':', chPos);
        if (colon != std::string::npos)
            b.channel = std::atoi(json.c_str() + colon + 1);
    }
    const auto scPos = json.find("\"scale\"", pos);
    if (scPos != std::string::npos && scPos < pos + 200) {
        const auto colon = json.find(':', scPos);
        if (colon != std::string::npos)
            b.scale = float(std::atof(json.c_str() + colon + 1));
    }
    if (json.find("\"abs\"", pos) != std::string::npos && json.find("\"abs\"", pos) < pos + 200)
        b.useAbs = true;
    return b;
}

ChannelMap parseMapBlock(const std::string &block)
{
    ChannelMap m = ChannelMap::defaultMap();
    m.yOffset = parseBinding(block, "y_offset", m.yOffset);
    m.yWave = parseBinding(block, "y_wave", m.yWave);
    m.zOffset = parseBinding(block, "z_offset", m.zOffset);
    m.wavePhase = parseBinding(block, "wave_phase", m.wavePhase);
    m.waveAmplitude = parseBinding(block, "wave_amplitude", m.waveAmplitude);
    return m;
}

} // namespace

ChannelMap ChannelMap::defaultMap()
{
    ChannelMap m;
    m.yOffset = {0, 2.f, false};
    m.yWave = {2, 2.f, false};
    m.zOffset = {2, 2.f, false};
    m.wavePhase = {16, 0.01f, false};
    m.waveAmplitude = {2, 0.4f, true};
    return m;
}

ChannelMap ChannelMap::loadForRco(const std::string & /*rcoPath*/, const std::string &rcoMd5)
{
    ChannelMap result = defaultMap();
    const auto path = channelMapConfigPath();
    std::ifstream in(path);
    if (!in)
        return result;

    std::string json((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    if (!rcoMd5.empty()) {
        const std::string hashKey = "\"" + rcoMd5 + "\"";
        const auto hpos = json.find(hashKey);
        if (hpos != std::string::npos) {
            const auto brace = json.find('{', hpos);
            const auto end = json.find('}', brace);
            if (brace != std::string::npos && end != std::string::npos)
                return parseMapBlock(json.substr(brace, end - brace + 1));
        }
    }

    const auto defPos = json.find("\"default\"");
    if (defPos != std::string::npos) {
        const auto brace = json.find('{', defPos);
        const auto end = json.find('}', brace);
        if (brace != std::string::npos && end != std::string::npos)
            return parseMapBlock(json.substr(brace, end - brace + 1));
    }

    return result;
}

std::string fcurveChannelSummary(const FCurveSet &anim)
{
    std::ostringstream oss;
    for (const auto &tr : anim.tracks) {
        float vmin = std::numeric_limits<float>::max();
        float vmax = std::numeric_limits<float>::lowest();
        for (const auto &k : tr.keys) {
            vmin = std::min(vmin, k.value);
            vmax = std::max(vmax, k.value);
        }
        oss << "ch" << tr.channel << "[" << vmin << ".." << vmax << "] ";
    }
    return oss.str();
}

} // namespace xmb
