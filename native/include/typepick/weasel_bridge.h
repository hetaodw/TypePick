// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <filesystem>
#include <memory>
#include <string>
#include <windows.h>
#include <rime_api.h>

namespace typepick {
// Weasel IPC uses compact 16-bit modifiers, not the native Rime 32-bit mask.
inline constexpr int WeaselKeyReleaseMask = 1 << 14;
// Owned by WeaselServer. These methods run on its message thread.
class WeaselBridge {
 public:
  WeaselBridge(RimeApi* api, const std::filesystem::path& user_data);
  ~WeaselBridge();
  bool BeforeKey(RimeSessionId session, int keycode, int mask);
  void AfterKey(RimeSessionId session, int keycode, int mask);
  void OnCommit(RimeSessionId session, const char* text);
  // 0 queries capabilities; updates: 1 unsafe/unknown, 2 safe scope, 3 safe + context.
  unsigned UpdateContext(RimeSessionId session, unsigned kind, const std::wstring& text);
  std::string TakeCommit(RimeSessionId session);
  void Reset(const char* reason = "session_reset");
  // Candidate navigation changes a recommendation, not already committed text.
  void InvalidateCandidates();
  void Position(const RECT& rect);
 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
}  // namespace typepick
