// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <typepick/selector.h>
#include <typepick/weasel_bridge.h>

namespace typepick {
std::string Utf8(const std::wstring& value);
std::wstring Wide(const std::string& value);
Decision CallJev(const Snapshot& snapshot, const Config& config);
}  // namespace typepick
