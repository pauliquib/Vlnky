// SPDX-FileCopyrightText: 2026 Pavel Švec
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace xmb {

uint32_t crc32(const uint8_t *data, size_t len);
uint32_t crc32(const std::vector<uint8_t> &data);

} // namespace xmb
