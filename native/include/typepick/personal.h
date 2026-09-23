// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace typepick {
// Keep a valid UTF-8 suffix, including when a byte boundary cuts a character.
inline std::string TextSuffix(std::string text, size_t bytes = 96) {
  if (text.size() <= bytes) return text;
  size_t start = text.size() - bytes;
  while (start < text.size() && (static_cast<unsigned char>(text[start]) & 0xc0) == 0x80) ++start;
  return text.substr(start);
}
inline std::string NormalizePinyin(std::string value) {
  value.erase(std::remove_if(value.begin(), value.end(), [](char c) { return c == ' ' || c == '\''; }), value.end());
  return value;
}
inline bool EndsWith(const std::string& text, const std::string& suffix) {
  return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}
struct PersonalEntry { std::string input, context, text; int count = 0; };
class PersonalStore {
 public:
  std::vector<PersonalEntry> words;
  std::vector<std::string> phrases;
  std::vector<PersonalEntry> terms;
  void LoadTerms(const std::filesystem::path& path) {
    terms.clear();
    try {
      if (!std::filesystem::exists(path) || std::filesystem::file_size(path) > 256 * 1024) return;
      std::ifstream in(path);
      auto json = nlohmann::json::parse(in);
      if (!json.is_array()) return;
      for (const auto& v : json) {
        auto input = NormalizePinyin(v.at("input").get<std::string>());
        auto text = v.at("text").get<std::string>();
        if (!input.empty() && input.size() <= 128 && input.find_first_not_of("abcdefghijklmnopqrstuvwxyz") == std::string::npos &&
            !text.empty() && text.size() <= 256 && text.find_first_of("\r\n\t") == std::string::npos)
          terms.push_back({input, {}, text, 1});
        if (terms.size() == 500) break;
      }
    } catch (...) { terms.clear(); }
  }
  std::string Term(const std::string& input) const {
    auto key = NormalizePinyin(input);
    for (const auto& term : terms) if (term.input == key) return term.text;
    return {};
  }
  static std::vector<std::string> DefaultPhrases() {
    return {"收到，谢谢", "好的，我稍后回复你", "请问方便什么时候沟通？", "辛苦了，感谢支持", "会议时间另行通知"};
  }
  void Load(const std::filesystem::path& path) {
    words.clear();
    try {
      if (!std::filesystem::exists(path) || std::filesystem::file_size(path) > 512 * 1024) return;
      std::ifstream in(path);
      auto json = nlohmann::json::parse(in);
      for (const auto& v : json.at("words")) {
        PersonalEntry e{v.at("input"), v.at("context"), v.at("text"), v.at("count")};
        if (e.input.empty() || e.input.size() > 128 || e.context.size() > 96 ||
            e.text.empty() || e.text.size() > 256 || e.count < 1 || e.count > 10000) continue;
        words.push_back(std::move(e));
        if (words.size() == 500) break;
      }
    } catch (...) { words.clear(); }
  }
  void LoadPhrases(const std::filesystem::path& path) {
    phrases = DefaultPhrases();
    try {
      if (!std::filesystem::exists(path) || std::filesystem::file_size(path) > 256 * 1024) return;
      std::ifstream in(path);
      auto json = nlohmann::json::parse(in);
      if (!json.is_array()) return;
      phrases.clear();
      for (const auto& v : json) {
        if (!v.is_string()) continue;
        auto text = v.get<std::string>();
        if (!text.empty() && text.size() <= 256 && text.find_first_of("\r\n\t") == std::string::npos)
          phrases.push_back(std::move(text));
        if (phrases.size() == 500) break;
      }
    } catch (...) { /* Keep defaults if parsing failed. */ }
  }
  nlohmann::json Json() const {
    auto result = nlohmann::json{{"version", 1}, {"words", nlohmann::json::array()}};
    for (const auto& e : words) result["words"].push_back({{"input", e.input}, {"context", e.context}, {"text", e.text}, {"count", e.count}});
    return result;
  }
  void Learn(const std::string& input, const std::string& context, const std::string& text) {
    const auto key = NormalizePinyin(input);
    if (key.empty() || key.size() > 128 || text.empty() || text.size() > 256) return;
    const auto suffix = TextSuffix(context);
    PersonalEntry entry{key, suffix, text, 1};
    auto found = std::find_if(words.begin(), words.end(), [&](const auto& e) { return e.input == key && e.context == suffix && e.text == text; });
    if (found != words.end()) { entry.count = (std::min)(10000, found->count + 1); words.erase(found); }
    words.push_back(std::move(entry));
    if (words.size() > 500) words.erase(words.begin());
  }
  std::optional<size_t> Choose(const std::string& input, const std::string& context, const std::vector<std::string>& candidates) const {
    std::optional<size_t> best;
    int best_score = 0;
    const auto key = NormalizePinyin(input);
    for (const auto& e : words) {
      if (e.input != key) continue;
      auto candidate = std::find(candidates.begin(), candidates.end(), e.text);
      if (candidate == candidates.end()) continue;
      const bool same = !e.context.empty() && EndsWith(context, e.context);
      if (!same && e.count < 2) continue;
      int score = (same ? 10001 : 0) + e.count;
      if (score >= best_score) { best_score = score; best = static_cast<size_t>(candidate - candidates.begin()); }
    }
    return best;
  }
  std::string Complete(const std::string& context, bool use_history) const {
    if (context.empty()) return {};
    std::string result;
    size_t longest = 0;
    for (const auto& phrase : phrases) {
      // At least two Chinese characters or six ASCII bytes before suggesting.
      for (size_t n = 6; n < phrase.size() && n <= 96; ++n) {
        if ((static_cast<unsigned char>(phrase[n]) & 0xc0) == 0x80) continue;
        if (n > longest && EndsWith(context, phrase.substr(0, n))) { longest = n; result = phrase.substr(n); }
      }
    }
    if (result.empty() && use_history) {
      int count = 1;
      for (const auto& e : words) {
        if (e.context.size() >= 6 && e.count > count && EndsWith(context, e.context)) {
          result = e.text; count = e.count;
        }
      }
    }
    return result;
  }
};
}  // namespace typepick
