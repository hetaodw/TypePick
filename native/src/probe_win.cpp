// SPDX-License-Identifier: AGPL-3.0-only
// Developer probe against real librime. Not a replacement for a Windows IME test.
#include <typepick/windows.h>
#include <iostream>
#include <fstream>
#include <map>

int wmain(int argc, wchar_t** argv) {
  using namespace typepick;
  try {
    std::map<std::wstring, std::wstring> args;
    for (int i = 1; i < argc; ++i) {
      const std::wstring name = argv[i];
      if (name == L"--demo" || name == L"--live" || name == L"--bridge-smoke") args[name] = L"1";
      else if (i + 1 < argc) args[name] = argv[++i];
      else throw std::runtime_error("missing argument");
    }
    if (!args.count(L"--rime") || !args.count(L"--data") || !args.count(L"--user"))
      throw std::runtime_error("usage: TypePickProbe --rime rime.dll --data data --user scratch [--demo|--live|--bridge-smoke] [--input yanjiu] [--context text]");
    const auto dll = std::filesystem::absolute(args[L"--rime"]);
    HMODULE module = LoadLibraryExW(dll.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!module) throw std::runtime_error("could not load rime.dll");
    auto get_api = reinterpret_cast<RimeApi* (*)()>(GetProcAddress(module, "rime_get_api"));
    if (!get_api) throw std::runtime_error("missing rime_get_api");
    auto* api = get_api();
    const auto data = Utf8(std::filesystem::absolute(args[L"--data"]).wstring());
    const auto user_path = std::filesystem::absolute(args[L"--user"]);
    std::filesystem::create_directories(user_path);
    const auto user = Utf8(user_path.wstring());
    RIME_STRUCT(RimeTraits, traits);
    traits.shared_data_dir = data.c_str(); traits.user_data_dir = user.c_str();
    traits.app_name = "rime.typepick"; traits.distribution_name = "TypePick Probe";
    traits.distribution_code_name = "TypePick"; traits.distribution_version = "0.1.0";
    traits.min_log_level = 2; traits.log_dir = "";
    api->setup(&traits); api->initialize(&traits);
    if (api->start_maintenance(True)) api->join_maintenance_thread();
    const auto sid = api->create_session();
    if (!sid || !api->select_schema(sid, "typepick_demo")) throw std::runtime_error("Rime schema unavailable");
    api->set_property(sid, "client_app", "notepad.exe");
    api->set_option(sid, "ascii_mode", False);
    const auto input = Utf8(args.count(L"--input") ? args[L"--input"] : L"yanjiu");
    const auto context = Utf8(args.count(L"--context") ? args[L"--context"] : L"这个问题需要进一步");
    nlohmann::json result = {{"engine", "librime"}, {"input", input}, {"context", context}};
    if (args.count(L"--bridge-smoke")) {
      std::ofstream f(user_path / "typepick.json");
      f << R"({"enabled":true,"mode":"demo","debounce_ms":0})"; f.close();
      {
        WeaselBridge bridge(api, user_path);
        bridge.OnCommit(sid, context.c_str());
        for (char ch : input) {
          bridge.BeforeKey(sid, ch, 0);
          api->process_key(sid, ch, 0);
          bridge.AfterKey(sid, ch, 0);
        }
        RECT caret = {100, 100, 100, 120}; bridge.Position(caret);
        const auto deadline = Clock::now() + std::chrono::seconds(2);
        bool shown = false;
        while (Clock::now() < deadline) {
          MSG msg;
          while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
          HWND popup = FindWindowW(L"TypePick.Recommendation.0.1", nullptr);
          if (popup && IsWindowVisible(popup)) { shown = true; break; }
          Sleep(5);
        }
        if (!shown) throw std::runtime_error("bridge failed to recommend");
        if (args.count(L"--reject")) {
          const auto why = args[L"--reject"];
          if (why == L"focus") bridge.Reset();
          else if (why == L"input") api->process_key(sid, 'a', 0);
          else if (why == L"app") api->set_property(sid, "client_app", "chrome.exe");
          else throw std::runtime_error("unknown rejection scenario");
          if (bridge.BeforeKey(sid, 0xff09, 0)) throw std::runtime_error("stale recommendation accepted");
          RIME_STRUCT(RimeCommit, rejected_commit);
          if (api->get_commit(sid, &rejected_commit)) {
            api->free_commit(&rejected_commit);
            throw std::runtime_error("rejected Tab still committed");
          }
          result["rejected"] = Utf8(why);
        } else {
          if (!bridge.BeforeKey(sid, 0xff09, 0)) throw std::runtime_error("bridge failed to accept Tab");
          if (!bridge.BeforeKey(sid, 0xff09, 0x8000)) throw std::runtime_error("Tab release leaked");
        }
        result["mode"] = "demo_bridge";
      }
    } else {
      if (!api->simulate_key_sequence(sid, input.c_str())) throw std::runtime_error("Rime rejected input");
      RIME_STRUCT(RimeContext, ctx);
      if (!api->get_context(sid, &ctx)) throw std::runtime_error("Rime returned no context");
      Snapshot snap; snap.session = sid; snap.input = input; snap.context = context;
      for (int i = 0; i < ctx.menu.num_candidates && i < 10; ++i) snap.candidates.emplace_back(ctx.menu.candidates[i].text);
      api->free_context(&ctx);
      result["candidates"] = snap.candidates;
      if (args.count(L"--demo") || args.count(L"--live")) {
        Config c; c.enabled = true; c.debounce_ms = 0;
        c.mode = args.count(L"--live") ? "jev" : "demo";
        Selector selector(c, CallJev); selector.Submit(snap);
        const auto deadline = Clock::now() + std::chrono::seconds(5);
        while (Clock::now() < deadline && !selector.Poll()) Sleep(5);
        const auto recommendation = selector.Poll();
        if (!recommendation) throw std::runtime_error("recommendation did not finish");
        result["mode"] = c.mode; result["status"] = recommendation->decision.status;
        if (recommendation->decision.index) api->select_candidate_on_current_page(sid, *recommendation->decision.index);
      }
    }
    RIME_STRUCT(RimeCommit, commit);
    if (api->get_commit(sid, &commit)) { result["commit"] = commit.text; api->free_commit(&commit); }
    api->destroy_session(sid); api->finalize(); FreeLibrary(module);
    std::cout << result.dump(2) << '\n';
    if (args.count(L"--bridge-smoke") && !args.count(L"--reject") && !result.contains("commit")) return 2;
    return 0;
  } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
