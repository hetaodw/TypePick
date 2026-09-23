// SPDX-License-Identifier: AGPL-3.0-only
#include <typepick/selector.h>
#include <atomic>
#include <future>
#include <iostream>
#include <stdexcept>
using namespace typepick;
using namespace std::chrono_literals;
#define CHECK(x) do { if (!(x)) throw std::runtime_error("failed: " #x); } while (false)
Snapshot Sample(std::string input = "yanjiu") {
  Snapshot s;
  s.session = 1; s.input = input; s.context = "这个问题需要"; s.candidates = {"研究", "烟酒"};
  return s;
}
std::optional<Recommendation> Wait(Selector& selector, int timeout = 1500) {
  const auto until = Clock::now() + std::chrono::milliseconds(timeout);
  while (Clock::now() < until) {
    if (auto r = selector.Poll()) return r;
    std::this_thread::sleep_for(2ms);
  }
  return {};
}
nlohmann::json Answer(std::string choice = "c0", double confidence = .95) {
  return {{"answers", {{"candidate", {{"type", "choice"}, {"choice", choice},
    {"confidence", confidence}, {"probabilities", {{"c0", .95}, {"c1", .03}, {"abstain", .02}}}}}}}};
}
int main() {
  try {
    Config c; c.enabled = true; c.debounce_ms = 0;
    CHECK(ValidSnapshot(Sample()));
    auto duplicate = Sample(); duplicate.candidates[1] = duplicate.candidates[0];
    CHECK(!ValidSnapshot(duplicate));
    CHECK(!ValidSnapshot(Snapshot{}));
    CHECK(!ParseConfig(nlohmann::json::object()).enabled);
    bool rejected = false;
    try { ParseConfig({{"allowed_apps", {"unknown.exe"}}}); } catch (...) { rejected = true; }
    CHECK(rejected);
    CHECK(ParseConfig({{"allowed_apps", {"Chrome.exe", "winword.exe", "weixin.exe"}}}).allowed_apps[0] == "chrome.exe");
    auto request = MakeRequest(Sample(), c);
    CHECK(request["questions"]["candidate"]["criteria"]["c1"] == "烟酒");
    CHECK(request["questions"]["candidate"]["criteria"].contains("abstain"));
    CHECK(ParseDecision(Answer(), 2, c).index == 0);
    CHECK(!ParseDecision(Answer("c99"), 2, c).index);
    CHECK(!ParseDecision(Answer("c0", .1), 2, c).index);
    CHECK(!ParseDecision(Answer("c1"), 2, c).index); // Choice contradicts its probabilities.
    CHECK(!ParseDecision(Answer("abstain"), 2, c).index);
    auto development = ParseConfig({{"enabled", true}, {"show_all_confidences", true}});
    CHECK(ParseDecision(Answer("c0", 0.0), 2, development).index == 0);
    CHECK(ParseDecision(Answer("c0", .47), 2, development).confidence == .47);
    auto low_margin = Answer("c0", .47);
    low_margin["answers"]["candidate"]["probabilities"] = {{"c0", .47}, {"c1", .46}, {"abstain", .07}};
    CHECK(ParseDecision(low_margin, 2, development).index == 0);
    CHECK(!ParseDecision(low_margin, 2, c).index);
    CHECK(!ParseDecision(Answer("abstain"), 2, development).index);
    CHECK(!ParseDecision(Answer("c99"), 2, development).index);
    auto broken = Answer(); broken["answers"]["candidate"]["probabilities"]["c0"] = -1;
    CHECK(!ParseDecision(broken, 2, c).index);
    broken = Answer(); broken["answers"]["candidate"]["probabilities"].erase("abstain");
    CHECK(!ParseDecision(broken, 2, c).index);
    CHECK(!ParseDecision({{"unrelated", true}}, 2, c).index);
    std::atomic<int> calls = 0;
    {
      auto empty = Sample(); empty.context = " \n";
      Selector s(c, [&](const Snapshot&, const Config&) { ++calls; return Decision{0, 1, "test"}; });
      s.Submit(empty); std::this_thread::sleep_for(30ms);
      CHECK(calls == 0); CHECK(!s.Poll());
    }
    {
      Config disabled;
      Selector s(disabled, [&](const Snapshot&, const Config&) { ++calls; return Decision{0, 1, "test"}; });
      s.Submit(Sample()); std::this_thread::sleep_for(30ms);
      CHECK(calls == 0); CHECK(!s.Poll());
    }
    {
      Config debounce = c; debounce.debounce_ms = 60;
      Selector s(debounce, [&](const Snapshot& in, const Config&) {
        ++calls; CHECK(in.input == "yanjiu"); return Decision{0, 1, "test"};
      });
      s.Submit(Sample("ya")); s.Submit(Sample("yan")); s.Submit(Sample());
      const auto result = Wait(s);
      CHECK(result && result->decision.index == 0); CHECK(calls == 1);
    }
    {
      std::promise<void> entered, release;
      auto unblock = release.get_future().share();
      std::atomic<int> started = 0;
      Selector s(c, [&](const Snapshot&, const Config&) {
        if (++started == 1) { entered.set_value(); unblock.wait(); }
        return Decision{0, 1, "test"};
      });
      s.Submit(Sample("old"));
      const bool did_enter = entered.get_future().wait_for(1s) == std::future_status::ready;
      if (!did_enter) { release.set_value(); CHECK(did_enter); }
      auto current = Sample("new"); current.session = 2;
      s.Submit(current); release.set_value();
      const auto result = Wait(s);
      CHECK(result && result->snapshot.input == "new" && result->snapshot.session == 2);
      s.Invalidate(); CHECK(!s.Poll());
    }
    {
      Config timeout = c; timeout.timeout_ms = 100;
      Selector s(timeout, [](const Snapshot&, const Config&) {
        std::this_thread::sleep_for(120ms); return Decision{0, 1, "test"};
      });
      const auto t = Clock::now(); s.Submit(Sample());
      CHECK(Clock::now() - t < 50ms);
      const auto result = Wait(s);
      CHECK(result && !result->decision.index && result->decision.status == "timeout");
    }
    {
      Selector s(c, [](const Snapshot&, const Config&) { return Decision{123, 1, "test"}; });
      s.Submit(Sample()); const auto result = Wait(s); CHECK(result && !result->decision.index);
    }
    {
      Selector s(c, [](const Snapshot&, const Config&) -> Decision { throw std::runtime_error("provider down"); });
      s.Submit(Sample()); const auto result = Wait(s); CHECK(result && result->decision.status == "provider_error");
    }
    std::cout << "PASS: validation, consent, candidate contract, debounce, stale/session isolation, "
                 "invalidate, nonblocking submit, timeout, out-of-range, provider failure\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n'; return 1;
  }
}
