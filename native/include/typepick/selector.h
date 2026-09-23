// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>
#include <nlohmann/json.hpp>

namespace typepick {
using Clock = std::chrono::steady_clock;
struct Snapshot {
  uint64_t session = 0;
  std::string input, context;
  int page = 0, cursor = 0, selection_start = 0, selection_end = 0;
  std::vector<std::string> candidates;
  bool operator==(const Snapshot& b) const;
};
struct Config {
  bool enabled = false;
  bool show_all_confidences = false;
  bool surrounding_context = true, personal_learning = true, phrase_completion = true;
  std::string mode = "jev", model = "jev-1.13.0";
  std::vector<std::string> allowed_apps = {"notepad.exe"};
  int debounce_ms = 150, timeout_ms = 600;
  double min_confidence = 0.7, min_margin = 0.1;
};
struct Decision {
  std::optional<size_t> index;
  double confidence = 0;
  std::string status = "idle";
  double margin = 0;
};
struct Recommendation {
  uint64_t revision = 0;
  Snapshot snapshot;
  Decision decision;
};
Config ParseConfig(const nlohmann::json& value);
bool ValidSnapshot(const Snapshot& snapshot);
nlohmann::json MakeRequest(const Snapshot& snapshot, const Config& config);
Decision ParseDecision(const nlohmann::json& value, size_t count, const Config& config);
using Transport = std::function<Decision(const Snapshot&, const Config&)>;
using Diagnostic = std::function<void(const nlohmann::json&)>;

// Only the worker calls Transport. The IME thread never waits for HTTP.
class Selector {
 public:
  Selector(Config config, Transport transport, Diagnostic diagnostic = {});
  ~Selector();
  Selector(const Selector&) = delete;
  Selector& operator=(const Selector&) = delete;
  uint64_t Submit(Snapshot snapshot);
  void Invalidate();
  std::optional<Recommendation> Poll() const;
 private:
  void Work();
  void Emit(const nlohmann::json& event) noexcept;
  Config config_;
  Transport transport_;
  Diagnostic diagnostic_;
  mutable std::mutex mutex_;
  std::condition_variable cv_;
  bool stop_ = false, pending_ = false;
  uint64_t revision_ = 0;
  Snapshot snapshot_;
  Clock::time_point due_;
  std::optional<Recommendation> ready_;
  std::thread worker_;  // Start only after every field the worker reads is initialized.
};
}  // namespace typepick
