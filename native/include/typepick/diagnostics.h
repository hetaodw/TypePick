// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <nlohmann/json.hpp>

namespace typepick {
// Metadata only; never persist snapshots, input text, candidates or credentials.
class DiagnosticLog {
 public:
  explicit DiagnosticLog(std::filesystem::path path) : path_(std::move(path)) {}
  void Write(const nlohmann::json& fields) noexcept {
    try {
      nlohmann::json record;
      for (const char* key : {"event", "reason", "status", "mode"})
        if (fields.contains(key) && fields[key].is_string()) record[key] = fields[key];
      for (const char* key : {"revision", "context_bytes", "input_bytes", "candidate_count",
           "elapsed_ms", "confidence", "margin", "min_confidence", "min_margin",
           "timeout_ms", "debounce_ms", "enabled", "stale", "shown", "error_code"})
        if (fields.contains(key) && (fields[key].is_number() || fields[key].is_boolean())) record[key] = fields[key];
      record["time_unix_ms"] = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch()).count();
      std::lock_guard<std::mutex> lock(mutex_);
      std::filesystem::create_directories(path_.parent_path());
      if (std::filesystem::exists(path_) && std::filesystem::file_size(path_) >= 1024 * 1024) {
        auto previous = path_; previous += ".1";
        std::filesystem::remove(previous);
        std::filesystem::rename(path_, previous);
      }
      std::ofstream out(path_, std::ios::app | std::ios::binary);
      out << record.dump() << '\n';
    } catch (...) { /* Diagnostics must never interrupt typing. */ }
  }
 private:
  std::filesystem::path path_;
  std::mutex mutex_;
};
}
