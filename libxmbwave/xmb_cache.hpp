// SPDX-FileCopyrightText: 2026 Pavel Švec
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "rco_error.hpp"
#include "xmbwave.hpp"

#include <cstdint>
#include <string>

namespace xmb {

struct RcoSourceInfo {
    uint64_t mtime = 0;
    uint64_t size = 0;
};

RcoSourceInfo rcoSourceInfo(const std::string &path);

RcoResult<bool> loadFromCacheV1(const std::string &rcoPath,
                                const PrfFile &prf,
                                GmoMesh &mesh,
                                FCurveSet &anim,
                                std::vector<uint8_t> &texture,
                                int &texW,
                                int &texH);

RcoError writeCacheV1(const std::string &rcoPath,
                      const PrfFile &prf,
                      const GmoMesh &mesh,
                      const FCurveSet &anim);

RcoError writeCacheV2(const std::string &rcoPath,
                      const PrfFile &prf,
                      const GmoMesh &mesh,
                      const FCurveSet &anim);

RcoResult<bool> loadFromCacheV2(const std::string &rcoPath,
                                const PrfFile &prf,
                                GmoMesh &mesh,
                                FCurveSet &anim,
                                std::vector<uint8_t> &texture,
                                int &texW,
                                int &texH);

bool loadMeshFromLegacyCache(const std::string &cacheDir, GmoMesh &mesh);

std::string cacheRootForHash(const std::string &hash);

} // namespace xmb
