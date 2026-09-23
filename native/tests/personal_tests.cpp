// SPDX-License-Identifier: AGPL-3.0-only
#include <typepick/personal.h>
#include <iostream>
#include <chrono>
#include <stdexcept>
#define CHECK(x) do { if (!(x)) throw std::runtime_error("failed: " #x); } while (false)
int main() {
  try {
    typepick::PersonalStore store;
    std::vector<std::string> candidates = {"研究", "烟酒"};
    store.Learn("yan'jiu", "便利店主要卖", "烟酒");
    CHECK(store.Choose("yanjiu", "你好，便利店主要卖", candidates) == 1);
    CHECK(!store.Choose("yanjiu", "需要进一步", candidates));
    CHECK(!store.Choose("nihao", "便利店主要卖", candidates));
    CHECK(!store.Choose("yanjiu", "便利店主要卖", {"研究", "眼睛"}));
    store.Learn("yanjiu", "便利店主要卖", "烟酒");
    CHECK(store.Choose("yanjiu", "", candidates) == 1);
    CHECK(store.Complete("便利店主要卖", true) == "烟酒");
    CHECK(store.Complete("便利店主要卖", false).empty());
    store.phrases = store.DefaultPhrases();
    CHECK(store.Complete("他说：收到", false) == "，谢谢");
    CHECK(store.Complete("收", false).empty());
    CHECK(store.Complete("收到，谢谢", false).empty());
    CHECK(typepick::TextSuffix("你好世界", 7) == "世界");
    const auto dir = std::filesystem::temp_directory_path() / ("typepick-personal-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(dir);
    const auto path = dir / "personal.json";
    { std::ofstream out(path); out << store.Json(); }
    typepick::PersonalStore restored; restored.Load(path);
    CHECK(restored.Choose("yanjiu", "便利店主要卖", candidates) == 1);
    { std::ofstream out(path); out << "invalid"; }
    restored.Load(path); CHECK(restored.words.empty());
    for (int i = 0; i < 700; ++i) store.Learn("test", std::to_string(i), "词");
    CHECK(store.words.size() == 500);
    std::filesystem::remove(path); std::filesystem::remove(dir);
    std::cout << "Personal choices, completion, UTF-8 bounds, persistence and corrupt recovery passed\n";
  } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
