// SPDX-License-Identifier: AGPL-3.0-only
#include <typepick/windows.h>
#include <typepick/popup_layout.h>
#include <typepick/diagnostics.h>
#include <typepick/personal.h>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <cwctype>
#include <iomanip>
#include <sstream>
#include <locale>

namespace typepick {
namespace {
HWND CandidateWindow() {
  struct Search { DWORD process; HWND window = nullptr; } search{};
  GetWindowThreadProcessId(GetForegroundWindow(), &search.process);
  EnumWindows([](HWND window, LPARAM data) -> BOOL {
    auto& s = *reinterpret_cast<Search*>(data);
    DWORD pid = 0; GetWindowThreadProcessId(window, &pid);
    if (pid != s.process || !IsWindowVisible(window)) return TRUE;
    wchar_t cls[128] = {};
    GetClassNameW(window, cls, 128);
    if (wcscmp(cls, L"TypePick.Candidate.0.1") != 0) return TRUE;
    RECT rect = {};
    if (GetWindowRect(window, &rect) && rect.right > rect.left && rect.bottom > rect.top) s.window = window;
    return !s.window;
  }, reinterpret_cast<LPARAM>(&search));
  return search.window;
}
}
struct WeaselBridge::Impl {
  RimeApi* api;
  Config config;
  std::shared_ptr<DiagnosticLog> log;
  std::unique_ptr<Selector> selector;
  HWND window = nullptr;
  HFONT font = nullptr;
  RECT caret = {};
  RimeSessionId session = 0;
  std::string context;
  std::wstring label;
  std::optional<Recommendation> displayed;
  bool swallowed_tab = false;
  bool swallowed_escape = false;
  uint64_t displayed_revision = 0;
  PersonalStore personal;
  std::filesystem::path personal_path;
  bool dirty = false, scope_received = false, scope_safe = false, has_surrounding = false;
  bool local_pending = false, completion_pending = false;
  Clock::time_point learned_at;
  Snapshot before_key;
  std::optional<Snapshot> custom_snapshot;
  std::string completion, completion_context, pending_commit;
  HWND integrated_window = nullptr;

  Impl(RimeApi* a, const std::filesystem::path& path) : api(a) {
    try {
      std::ifstream input(path / "typepick.json");
      if (input) config = ParseConfig(nlohmann::json::parse(input));
    } catch (...) { config.enabled = false; }
    personal_path = path / "personal.json";
    personal.Load(personal_path);
    personal.LoadPhrases(path / "phrases.json");
    personal.LoadTerms(path / "terms.json");
    log = std::make_shared<DiagnosticLog>(path / "logs" / "ai-diagnostics.jsonl");
    log->Write({{"event", "startup"}, {"enabled", config.enabled}, {"mode", config.mode},
        {"timeout_ms", config.timeout_ms}, {"debounce_ms", config.debounce_ms},
        {"min_confidence", config.min_confidence}, {"min_margin", config.min_margin},
        {"show_all_confidences", config.show_all_confidences}});
    selector = std::make_unique<Selector>(config, CallJev,
        [logger = log](const nlohmann::json& event) { logger->Write(event); });
    if (!config.enabled) return;
    static const wchar_t* cls = L"TypePick.Recommendation.0.1";
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = cls;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&wc);
    window = CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        cls, L"TypePick", WS_POPUP | WS_BORDER, 0, 0, 420, 40,
        nullptr, nullptr, wc.hInstance, this);
    font = CreateFontW(-18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                      0, 0, CLEARTYPE_QUALITY, 0, L"Microsoft YaHei UI");
    if (window) SetTimer(window, 1, 25, nullptr);
    else log->Write({{"event", "popup_error"}, {"error_code", GetLastError()}});
  }
  ~Impl() {
    SavePersonal();
    if (window) { KillTimer(window, 1); DestroyWindow(window); }
    selector.reset();
    if (font) DeleteObject(font);
  }
  std::string App(RimeSessionId sid) const {
    char app[512] = {};
    if (!sid || !api->get_property(sid, "client_app", app, sizeof(app))) return {};
    std::string name(app);
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return name;
  }
  bool HostAllowed(RimeSessionId sid) const {
    if (!config.enabled || !sid) return false;
    const auto name = App(sid);
    return std::find(config.allowed_apps.begin(), config.allowed_apps.end(), name) != config.allowed_apps.end();
  }
  bool Allowed(RimeSessionId sid) const {
    if (!HostAllowed(sid)) return false;
    // Legacy Notepad clients retain continuous-input context. Other hosts must
    // negotiate a safe TSF scope; a negative signal blocks every host.
    if (sid == session && scope_received) return scope_safe;
    return App(sid) == "notepad.exe";
  }
  void SavePersonal() noexcept {
    if (!dirty) return;
    try {
      auto tmp = personal_path; tmp += L".tmp";
      std::ofstream out(tmp); out << personal.Json().dump(2); out.close();
      if (out && MoveFileExW(tmp.c_str(), personal_path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) dirty = false;
    } catch (...) {}
  }
  Snapshot Capture(RimeSessionId sid) const {
    Snapshot s;
    if (!Allowed(sid)) return s;
    RIME_STRUCT(RimeContext, rc);
    if (!api->get_context(sid, &rc)) return s;
    s.session = sid;
    const char* raw = api->get_input(sid);
    if (raw) s.input = raw;
    s.context = context;
    s.page = rc.menu.page_no;
    s.cursor = rc.composition.cursor_pos;
    s.selection_start = rc.composition.sel_start;
    s.selection_end = rc.composition.sel_end;
    for (int i = 0; i < rc.menu.num_candidates && i < 10; ++i)
      if (rc.menu.candidates[i].text) s.candidates.emplace_back(rc.menu.candidates[i].text);
    api->free_context(&rc);
    return s;
  }
  void Hide() {
    displayed.reset();
    displayed_revision = 0;
    local_pending = false; completion_pending = false;
    completion.clear(); completion_context.clear();
    custom_snapshot.reset();
    // Post only: the TSF host may currently be waiting for this IPC response.
    if (integrated_window) PostMessageW(integrated_window, WM_APP + 0x381, GetTickCount(), 0);
    integrated_window = nullptr;
    if (window) ShowWindow(window, SW_HIDE);
  }
  void Reset(const char* reason = "session_reset") {
    if (!context.empty()) log->Write({{"event", "context_reset"}, {"reason", reason}, {"context_bytes", context.size()}});
    selector->Invalidate();
    Hide();
    context.clear();
    session = 0;
    swallowed_tab = false;
    swallowed_escape = false;
    scope_received = scope_safe = has_surrounding = false;
    before_key = {}; pending_commit.clear();
  }
  void ShowLabel(const Snapshot* snapshot = nullptr) {
    HWND candidate = CandidateWindow();
    if (candidate && snapshot) {
      // Bounded COPYDATA is delivered only from the timer, never inside a key
      // IPC callback. Receiver verifies the complete candidate list.
      std::wstring payload = label + L"\n";
      for (const auto& word : snapshot->candidates) payload += Wide(word) + L"\n";
      COPYDATASTRUCT data = {0x5459504b, static_cast<DWORD>((payload.size() + 1) * sizeof(wchar_t)), payload.data()};
      DWORD_PTR accepted = 0;
      if (SendMessageTimeoutW(candidate, WM_COPYDATA, reinterpret_cast<WPARAM>(window),
          reinterpret_cast<LPARAM>(&data), SMTO_ABORTIFHUNG | SMTO_BLOCK, 20, &accepted) && accepted) {
        integrated_window = candidate;
        ShowWindow(window, SW_HIDE);
        log->Write({{"event", "displayed"}, {"shown", true}});
        return;
      }
    }
    SetWindowTextW(window, label.c_str());
    MONITORINFO info = {sizeof(info)};
    GetMonitorInfoW(MonitorFromRect(&caret, MONITOR_DEFAULTTONEAREST), &info);
    std::optional<RECT> candidate_rect;
    RECT r = {}; if (candidate && GetWindowRect(candidate, &r)) candidate_rect = r;
    const auto rect = RecommendationRect(caret, info.rcWork, candidate_rect);
    SetWindowPos(window, HWND_TOPMOST, rect.left, rect.top, rect.right - rect.left,
                 rect.bottom - rect.top, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(window, nullptr, TRUE);
    log->Write({{"event", "displayed"}, {"shown", IsWindowVisible(window) != FALSE}});
  }
  void Tick() {
    if (dirty && Clock::now() - learned_at > std::chrono::seconds(2)) SavePersonal();
    if (local_pending && displayed) {
      local_pending = false;
      if (Capture(session) == displayed->snapshot) ShowLabel(&displayed->snapshot);
      else Hide();
      return;
    }
    if (completion_pending) {
      completion_pending = false;
      if (Allowed(session) && context == completion_context &&
          (!custom_snapshot || Capture(session) == *custom_snapshot)) ShowLabel(custom_snapshot ? &*custom_snapshot : nullptr); else Hide();
      return;
    }
    if (displayed && displayed->decision.status == "personal") return;
    const auto result = selector->Poll();
    if (!result || !result->decision.index || result->snapshot.session != session ||
        result->revision == displayed_revision) return;
    // Validate again before display; schema/page/candidate changes also invalidate a result.
    if (!(Capture(session) == result->snapshot)) {
      log->Write({{"event", "display_skipped"}, {"reason", "snapshot_changed"}, {"revision", result->revision}});
      selector->Invalidate(); Hide(); return;
    }
    displayed = result;
    displayed_revision = result->revision;
    std::wostringstream score;
    score.imbue(std::locale::classic());
    score << std::fixed << std::setprecision(1) << result->decision.confidence * 100.0;
    label = (config.mode == "demo" ? L"TypePick 演示 · " : L"TypePick AI · ") +
        score.str() + L"% · " + Wide(result->snapshot.candidates[*result->decision.index]) + L"    [Tab 采用]";
    ShowLabel(&result->snapshot);
  }
  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* self = reinterpret_cast<Impl*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
      self = static_cast<Impl*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
      SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (self) {
      if (msg == WM_TIMER) { self->Tick(); return 0; }
      if (msg == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
      if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT r; GetClientRect(hwnd, &r);
        HBRUSH brush = CreateSolidBrush(RGB(240, 247, 255));
        FillRect(dc, &r, brush); DeleteObject(brush);
        auto old = SelectObject(dc, self->font);
        SetBkMode(dc, TRANSPARENT); SetTextColor(dc, RGB(25, 62, 110));
        r.left += 12; r.right -= 10;
        DrawTextW(dc, self->label.c_str(), -1, &r, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        SelectObject(dc, old);
        EndPaint(hwnd, &ps);
        return 0;
      }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
  }
};
WeaselBridge::WeaselBridge(RimeApi* api, const std::filesystem::path& dir)
    : impl_(std::make_unique<Impl>(api, dir)) {}
WeaselBridge::~WeaselBridge() = default;
void WeaselBridge::Reset(const char* reason) { impl_->Reset(reason); }
void WeaselBridge::InvalidateCandidates() {
  impl_->selector->Invalidate(); impl_->Hide(); impl_->swallowed_tab = false;
}
void WeaselBridge::Position(const RECT& rect) {
  // Moving the caret outside the current line means the cached context may be stale.
  if (!impl_->has_surrounding && impl_->caret.top && std::abs(rect.top - impl_->caret.top) > 5) impl_->Reset("caret_line_changed");
  impl_->caret = rect;
}
bool WeaselBridge::BeforeKey(RimeSessionId sid, int key, int mask) {
  const bool release = (mask & WeaselKeyReleaseMask) != 0;
  if (release) {
    if (key == 0xff09 && impl_->swallowed_tab) { impl_->swallowed_tab = false; return true; }
    if (key == 0xff1b && impl_->swallowed_escape) { impl_->swallowed_escape = false; return true; }
    return false;
  }
  if (impl_->session != sid) impl_->Reset("session_changed");
  impl_->session = sid;
  const bool no_modifiers = (mask & 0xff) == 0;
  impl_->before_key = impl_->Capture(sid);
  if (key == 0xff1b && no_modifiers && !impl_->completion_pending &&
      !impl_->completion.empty() && !impl_->custom_snapshot && impl_->Allowed(sid)) {
    impl_->selector->Invalidate(); impl_->Hide(); impl_->swallowed_escape = true;
    return true;
  }
  if (key == 0xff09 && no_modifiers && !impl_->completion_pending && !impl_->completion.empty() && impl_->Allowed(sid)) {
    const char* raw = impl_->api->get_input(sid);
    if (impl_->context == impl_->completion_context &&
        (impl_->custom_snapshot ? impl_->Capture(sid) == *impl_->custom_snapshot : (!raw || !*raw))) {
      if (impl_->custom_snapshot) impl_->api->clear_composition(sid);
      impl_->pending_commit = impl_->completion;
      impl_->selector->Invalidate(); impl_->Hide();
      impl_->swallowed_tab = true;
      return true;
    }
  }
  if (key == 0xff09 && no_modifiers && !impl_->local_pending && impl_->displayed) {
    const auto recommendation = *impl_->displayed;
    const auto current = impl_->Capture(sid);
    impl_->selector->Invalidate();
    impl_->Hide();
    if (current == recommendation.snapshot && recommendation.decision.index) {
      const bool selected = impl_->api->select_candidate_on_current_page(sid, *recommendation.decision.index);
      impl_->swallowed_tab = selected;
      return selected;
    }
  }
  impl_->selector->Invalidate();
  impl_->Hide();
  // Editing active Pinyin does not change the already committed document text.
  const bool letter = key >= 'a' && key <= 'z';
  const char* input = impl_->api->get_input(sid);
  const bool composing = input && *input;
  const bool preedit_edit = composing && (key == 0xff08 || key == 0xffff ||
      key == 0xff1b || key == '-' || key == '=' || (key >= 0xff50 && key <= 0xff57));
  const bool printable = key >= 0x20 && key <= 0x7e;
  if (!no_modifiers || (!letter && !preedit_edit && !printable && key != 0xff0d)) {
    if (!impl_->context.empty()) impl_->log->Write({{"event", "context_reset"},
        {"reason", no_modifiers ? "document_edit_or_navigation" : "modifier_or_shortcut"},
        {"context_bytes", impl_->context.size()}});
    impl_->context.clear();
  }
  return false;
}
void WeaselBridge::AfterKey(RimeSessionId sid, int key, int mask) {
  if ((mask & WeaselKeyReleaseMask) || (mask & 0xff)) return;
  // Resubmit after deleting or paging active Pinyin as well as typing letters.
  if (!((key >= 'a' && key <= 'z') || key == '\'' || key == 0xff08 || key == 0xffff || key == '-' || key == '=')) return;
  if (!impl_->Allowed(sid)) {
    impl_->log->Write({{"event", "skipped"}, {"reason", impl_->config.enabled ? "app_not_allowed" : "disabled"}});
    return;
  }
  impl_->session = sid;
  auto snapshot = impl_->Capture(sid);
  const auto term = impl_->personal.Term(snapshot.input);
  if (!term.empty()) {
    impl_->completion = term; impl_->completion_context = impl_->context;
    impl_->custom_snapshot = snapshot;
    impl_->label = L"自定义词 · " + Wide(term) + L"    [Tab 采用]";
    impl_->completion_pending = true;
    return;
  }
  if (impl_->config.personal_learning && !snapshot.input.empty()) {
    if (auto index = impl_->personal.Choose(snapshot.input, snapshot.context, snapshot.candidates)) {
      Recommendation local;
      local.snapshot = snapshot; local.decision.index = index; local.decision.status = "personal";
      impl_->displayed = std::move(local);
      impl_->label = L"个人词库 · " + Wide(snapshot.candidates[*index]) + L"    [Tab 采用]";
      impl_->local_pending = true;
      return;
    }
  }
  impl_->selector->Submit(std::move(snapshot));
}
void WeaselBridge::OnCommit(RimeSessionId sid, const char* text) {
  impl_->selector->Invalidate(); impl_->Hide();
  if (!text || !impl_->Allowed(sid)) {
    // A commit in a denied field must not restore the legacy Notepad fallback.
    impl_->context.clear(); impl_->before_key = {}; return;
  }
  if (impl_->session != sid) impl_->context.clear();
  impl_->session = sid;
  const auto& prior = impl_->before_key;
  if (impl_->config.personal_learning && prior.session == sid &&
      std::find(prior.candidates.begin(), prior.candidates.end(), text) != prior.candidates.end()) {
    impl_->personal.Learn(prior.input, prior.context, text);
    impl_->dirty = true; impl_->learned_at = Clock::now();
  }
  auto wide = Wide(impl_->context + text);
  if (wide.size() > 100) {
    size_t start = wide.size() - 100;
    if (wide[start] >= 0xdc00 && wide[start] <= 0xdfff) ++start;
    wide.erase(0, start);
  }
  impl_->context = Utf8(wide);
  impl_->log->Write({{"event", "context_committed"}, {"context_bytes", impl_->context.size()}});
  const char* raw = impl_->api->get_input(sid);
  if (impl_->config.phrase_completion && (!raw || !*raw)) {
    impl_->completion = impl_->personal.Complete(impl_->context, impl_->config.personal_learning);
    if (!impl_->completion.empty()) {
      impl_->completion_context = impl_->context;
      impl_->label = L"短句补全 · " + Wide(impl_->completion) + L"    [Tab 采用 · Esc 关闭]";
      impl_->completion_pending = true;
    }
  }
}
unsigned WeaselBridge::UpdateContext(RimeSessionId sid, unsigned kind, const std::wstring& text) {
  if (!impl_->HostAllowed(sid)) return 0;
  if (kind == 0) return (impl_->config.surrounding_context ? 3u : 1u) | (impl_->App(sid) == "notepad.exe" ? 4u : 0u);
  if (impl_->session != sid) impl_->Reset("context_session_changed");
  impl_->session = sid;
  impl_->scope_received = true;
  impl_->scope_safe = kind == 2 || kind == 3;
  impl_->has_surrounding = kind == 3 && impl_->config.surrounding_context;
  if (!impl_->scope_safe) {
    impl_->selector->Invalidate(); impl_->Hide(); impl_->context.clear();
    return 0;
  }
  if (impl_->has_surrounding) {
    auto bounded = text.substr(text.size() > 100 ? text.size() - 100 : 0);
    if (!bounded.empty() && bounded[0] >= 0xdc00 && bounded[0] <= 0xdfff) bounded.erase(0, 1);
    auto context = Utf8(bounded);
    if (impl_->context != context) {
      impl_->selector->Invalidate(); impl_->Hide();
      impl_->context = std::move(context);
    }
  }
  return 1;
}
std::string WeaselBridge::TakeCommit(RimeSessionId sid) {
  if (sid != impl_->session) return {};
  std::string text; text.swap(impl_->pending_commit); return text;
}
}  // namespace typepick
