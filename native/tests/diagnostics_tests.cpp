// SPDX-License-Identifier: AGPL-3.0-only
#include <typepick/diagnostics.h>
#include <stdexcept>
#include <sstream>
int main() {
  const auto dir = std::filesystem::temp_directory_path() /
      ("typepick-diagnostics-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  const auto path = dir / "ai.jsonl";
  typepick::DiagnosticLog log(path);
  log.Write({{"event", "request_finished"}, {"status", "uncertain"}, {"confidence", .47},
             {"context", "private input"}, {"api_key", "secret key"}, {"candidate_count", "private text"}});
  {
    std::ifstream f(path); nlohmann::json entry; f >> entry;
    if (entry.contains("context") || entry.contains("api_key") || entry.contains("candidate_count") ||
        entry["status"] != "uncertain" || !entry.contains("time_unix_ms"))
      throw std::runtime_error("diagnostics privacy or metadata failure");
  }
  { std::ofstream f(path, std::ios::app); f << std::string(1024 * 1024, 'x'); }
  log.Write({{"event", "startup"}});
  if (!std::filesystem::exists(path.string()+".1") || std::filesystem::file_size(path) > 1024)
    throw std::runtime_error("diagnostics rotation failed");
  // A non-directory parent must not let logging failures break input handling.
  typepick::DiagnosticLog blocked(path / "impossible.jsonl");
  blocked.Write({{"event", "startup"}});
  std::filesystem::remove(path);
  std::filesystem::remove(path.string()+".1");
  std::filesystem::remove(dir);
}
