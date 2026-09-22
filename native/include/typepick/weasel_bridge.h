// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <filesystem>
#include <memory>
#include <windows.h>
#include <rime_api.h>

namespace typepick {
// Owned by WeaselServer. These methods run on its message thread.
class WeaselBridge {
 public:
  WeaselBridge(RimeApi* api, const std::filesystem::path& user_data);
  ~WeaselBridge();
  bool BeforeKey(RimeSessionId session, int keycode, int mask);
  void AfterKey(RimeSessionId session, int keycode, int mask);
  void OnCommit(RimeSessionId session, const char* text);
  void Reset();
  void Position(const RECT& rect);
 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
}  // namespace typepick
