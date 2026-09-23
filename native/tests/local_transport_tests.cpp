// SPDX-License-Identifier: AGPL-3.0-only
#include <typepick/windows.h>
#include <iostream>

int main(int argc, char** argv) {
  if (argc != 4) return 2;
  auto config = typepick::ParseConfig({{"enabled", true}, {"mode", "laya"},
      {"local_port", std::stoi(argv[1])}, {"timeout_ms", 300}, {"show_all_confidences", true}});
  typepick::Snapshot snapshot;
  snapshot.session = 1; snapshot.input = "yanjiu";
  snapshot.context = "这家商店主要卖"; snapshot.candidates = {"研究", "烟酒"};
  const auto started = typepick::Clock::now();
  const auto result = typepick::CallJev(snapshot, config);
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(typepick::Clock::now() - started).count();
  std::cout << result.status << " " << elapsed << "ms\n";
  if (result.status != argv[2] || elapsed > 2500) return 1;
  const int expected = std::stoi(argv[3]);
  if (expected < 0 ? result.index.has_value() : result.index != static_cast<size_t>(expected)) return 1;
  if (expected >= 0 && result.confidence != 0.45) return 1;
  return 0;
}
