// SPDX-License-Identifier: AGPL-3.0-only
#include <typepick/windows.h>
#include <typepick/popup_layout.h>
#include <typepick/diagnostics.h>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <cwctype>
#include <iomanip>
#include <sstream>
#include <locale>

namespace typepick {
namespace {
std::optional<RECT> CandidateWindow() {
  struct Search { DWORD process; std::optional<RECT> rect; } search{};
  GetWindowThreadProcessId(GetForegroundWindow(), &search.process);
  EnumWindows([](HWND window, LPARAM data) -> BOOL {
    auto& s = *reinterpret_cast<Search*>(data);
    DWORD pid = 0; GetWindowThreadProcessId(window, &pid);
    if (pid != s.process || !IsWindowVisible(window)) return TRUE;
    wchar_t cls[128] = {};
    GetClassNameW(window, cls, 128);
    if (wcscmp(cls, L"TypePick.Candidate.0.1") != 0) return TRUE;
    RECT rect = {};
    if (GetWindowRect(window, &rect) && rect.right > rect.left && rect.bottom > rect.top) s.rect = rect;
    return !s.rect;
  }, reinterpret_cast<LPARAM>(&search));
  return search.rect;
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
  uint64_t displayed_revision = 0;

  Impl(RimeApi* a, const std::filesystem::path& path) : api(a) {
    try {
      std::ifstream input(path / "typepick.json");
      if (input) config = ParseConfig(nlohmann::json::parse(input));
    } catch (...) { config.enabled = false; }
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
    if (window) { KillTimer(window, 1); DestroyWindow(window); }
    selector.reset();
    if (font) DeleteObject(font);
  }
  bool Allowed(RimeSessionId sid) const {
    if (!config.enabled || !sid) return false;
    char app[512] = {};
    if (!api->get_property(sid, "client_app", app, sizeof(app))) return false;
    std::string name(app);
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    // No wildcard and no unknown host. Cloud use in other apps is deliberately unsupported in this MVP.
    return std::find(config.allowed_apps.begin(), config.allowed_apps.end(), name) != config.allowed_apps.end();
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
    if (window) ShowWindow(window, SW_HIDE);
  }
  void Reset(const char* reason = "session_reset") {
    if (!context.empty()) log->Write({{"event", "context_reset"}, {"reason", reason}, {"context_bytes", context.size()}});
    selector->Invalidate();
    Hide();
    context.clear();
    session = 0;
    swallowed_tab = false;
  }
  void Tick() {
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
    SetWindowTextW(window, label.c_str());
    MONITORINFO info = {sizeof(info)};
    GetMonitorInfoW(MonitorFromRect(&caret, MONITOR_DEFAULTTONEAREST), &info);
    const auto rect = RecommendationRect(caret, info.rcWork, CandidateWindow());
    SetWindowPos(window, HWND_TOPMOST, rect.left, rect.top, rect.right - rect.left,
                 rect.bottom - rect.top, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(window, nullptr, TRUE);
    log->Write({{"event", "displayed"}, {"revision", result->revision}, {"shown", IsWindowVisible(window) != FALSE}});
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
  if (impl_->caret.top && std::abs(rect.top - impl_->caret.top) > 5) impl_->Reset("caret_line_changed");
  impl_->caret = rect;
}
bool WeaselBridge::BeforeKey(RimeSessionId sid, int key, int mask) {
  const bool release = (mask & WeaselKeyReleaseMask) != 0;
  if (release) {
    if (key == 0xff09 && impl_->swallowed_tab) { impl_->swallowed_tab = false; return true; }
    return false;
  }
  if (impl_->session != sid) impl_->Reset("session_changed");
  impl_->session = sid;
  const bool no_modifiers = (mask & 0xff) == 0;
  if (key == 0xff09 && no_modifiers && impl_->displayed) {
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
  if (!no_modifiers || (!letter && !preedit_edit && key != ' ' && key != '\'' && !(key >= '1' && key <= '9'))) {
    if (!impl_->context.empty()) impl_->log->Write({{"event", "context_reset"},
        {"reason", no_modifiers ? "document_edit_or_navigation" : "modifier_or_shortcut"},
        {"context_bytes", impl_->context.size()}});
    impl_->context.clear();
  }
  return false;
}
void WeaselBridge::AfterKey(RimeSessionId sid, int key, int mask) {
  if ((mask & WeaselKeyReleaseMask) || (mask & 0xff)) return;
  if (!((key >= 'a' && key <= 'z') || key == '\'')) return;
  if (!impl_->Allowed(sid)) {
    impl_->log->Write({{"event", "skipped"}, {"reason", impl_->config.enabled ? "app_not_allowed" : "disabled"}});
    return;
  }
  impl_->session = sid;
  auto snapshot = impl_->Capture(sid);
  impl_->selector->Submit(std::move(snapshot));
}
void WeaselBridge::OnCommit(RimeSessionId sid, const char* text) {
  impl_->selector->Invalidate(); impl_->Hide();
  if (!text || !impl_->Allowed(sid)) { impl_->Reset(); return; }
  if (impl_->session != sid) impl_->context.clear();
  impl_->session = sid;
  auto wide = Wide(impl_->context + text);
  if (wide.size() > 100) {
    size_t start = wide.size() - 100;
    if (wide[start] >= 0xdc00 && wide[start] <= 0xdfff) ++start;
    wide.erase(0, start);
  }
  impl_->context = Utf8(wide);
  impl_->log->Write({{"event", "context_committed"}, {"context_bytes", impl_->context.size()}});
}
}  // namespace typepick
