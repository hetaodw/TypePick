// SPDX-License-Identifier: AGPL-3.0-only
#include <typepick/windows.h>
#include <typepick/personal.h>
#include <wincred.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shellapi.h>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;
namespace {
constexpr wchar_t CredentialTarget[] = L"TypePick/Jev";
HWND main_window, key_edit, enabled_check, confidence_check, timeout_edit, status_label;
HWND context_check, learning_check, completion_check, edge_check, chrome_check, word_check, wechat_check;
HWND vocabulary_window, terms_edit, phrases_edit;
HFONT ui_font;
fs::path UserDir() {
  PWSTR p = nullptr;
  if (FAILED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &p)))
    throw std::runtime_error("user directory");
  fs::path result = fs::path(p) / L"TypePick";
  CoTaskMemFree(p);
  fs::create_directories(result);
  return result;
}
fs::path InstallDir() {
  wchar_t p[32768] = {};
  GetModuleFileNameW(nullptr, p, 32768);
  return fs::path(p).parent_path();
}
bool HasKey() {
  PCREDENTIALW c = nullptr;
  if (!CredReadW(CredentialTarget, CRED_TYPE_GENERIC, 0, &c)) return false;
  bool exists = c->CredentialBlobSize > 0;
  CredFree(c);
  return exists;
}
bool StoreKey(const std::wstring& key, const wchar_t* target = CredentialTarget) {
  if (key.empty() || key.size() * sizeof(wchar_t) > CRED_MAX_CREDENTIAL_BLOB_SIZE ||
      key.find_first_of(L"\r\n") != std::wstring::npos) return false;
  CREDENTIALW c = {};
  c.Type = CRED_TYPE_GENERIC;
  c.TargetName = const_cast<wchar_t*>(target);
  c.UserName = const_cast<wchar_t*>(L"TypePick");
  c.CredentialBlobSize = static_cast<DWORD>(key.size() * sizeof(wchar_t));
  c.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<wchar_t*>(key.data()));
  c.Persist = CRED_PERSIST_LOCAL_MACHINE;
  return CredWriteW(&c, 0) != FALSE;
}
std::string Trim(std::string s) {
  auto begin = s.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos) return {};
  return s.substr(begin, s.find_last_not_of(" \t\r\n") - begin + 1);
}
std::wstring ReadEnv(const fs::path& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) throw std::runtime_error("env read");
  std::string line;
  while (std::getline(stream, line)) {
    if (line.size() > 16384) throw std::runtime_error("env line too long");
    if (line.compare(0, 3, "\xef\xbb\xbf") == 0) line.erase(0, 3);
    auto eq = line.find('=');
    if (eq == std::string::npos) continue;
    auto name = Trim(line.substr(0, eq));
    if (name != "key" && name != "TYPESAFE_API_KEY") continue;
    auto value = Trim(line.substr(eq + 1));
    if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"') ||
        (value.front() == '\'' && value.back() == '\'')))
      value = value.substr(1, value.size() - 2);
    return typepick::Wide(value);
  }
  throw std::runtime_error("env key missing");
}
void Message(const wchar_t* text, bool error = false) {
  MessageBoxW(main_window, text, L"TypePick", MB_OK | (error ? MB_ICONERROR : MB_ICONINFORMATION));
}
void Status() {
  SetWindowTextW(status_label, HasKey() ? L"密钥状态：已保存在本机 Windows 凭据管理器" : L"密钥状态：尚未设置，普通拼音输入仍可使用");
}
bool Launch(const fs::path& exe, const wchar_t* arguments = nullptr, bool wait = false) {
  std::wstring command = L"\"" + exe.wstring() + L"\"";
  if (arguments) command += L" " + std::wstring(arguments);
  STARTUPINFOW si = {sizeof(si)};
  si.dwFlags = STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_HIDE;
  PROCESS_INFORMATION pi = {};
  if (!CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr,
                      exe.parent_path().c_str(), &si, &pi)) return false;
  bool ok = true;
  if (wait) {
    if (WaitForSingleObject(pi.hProcess, 5000) != WAIT_OBJECT_0) ok = false;
    else { DWORD code = 1; GetExitCodeProcess(pi.hProcess, &code); ok = code == 0; }
  }
  CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
  return ok;
}
bool StopServer() {
  DWORD pid = 0;
  GetWindowThreadProcessId(FindWindowW(L"TypePickIPCWindow_1.0", nullptr), &pid);
  HANDLE process = pid ? OpenProcess(SYNCHRONIZE, FALSE, pid) : nullptr;
  bool ok = Launch(InstallDir() / L"TypePickServer.exe", L"/q", true);
  if (process) { ok = WaitForSingleObject(process, 10000) == WAIT_OBJECT_0 && ok; CloseHandle(process); }
  return ok;
}
bool Checked(HWND control) { return SendMessageW(control, BM_GETCHECK, 0, 0) == BST_CHECKED; }
void WriteJson(const fs::path& path, const nlohmann::json& json) {
  auto tmp = path; tmp += L".tmp";
  { std::ofstream out(tmp); out << json.dump(2); out.close(); if (!out) throw std::runtime_error("write"); }
  if (!MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) throw std::runtime_error("replace");
}
bool Save() {
  wchar_t key[2600] = {}, timeout[16] = {};
  GetWindowTextW(key_edit, key, 2600);
  GetWindowTextW(timeout_edit, timeout, 16);
  wchar_t* end = nullptr;
  const auto ms = wcstol(timeout, &end, 10);
  if (!end || *end || ms < 100 || ms > 3000) {
    SecureZeroMemory(key, sizeof(key));
    Message(L"等待时间请输入 100–3000 毫秒。", true); return false;
  }
  if (*key && !StoreKey(key)) {
    SecureZeroMemory(key, sizeof(key));
    Message(L"密钥保存失败，请检查密钥格式及 Windows 凭据服务。", true); return false;
  }
  SecureZeroMemory(key, sizeof(key));
  SetWindowTextW(key_edit, L"");
  Status();
  const bool enabled = SendMessageW(enabled_check, BM_GETCHECK, 0, 0) == BST_CHECKED;
  if (enabled && !HasKey()) { Message(L"请先输入密钥，或导入包含 key=... 的 .env 文件。", true); return false; }
  nlohmann::json config = {{"enabled", enabled}, {"mode", "jev"}, {"model", "jev-1.13.0"},
    {"allowed_apps", {"notepad.exe"}}, {"debounce_ms", 150}, {"timeout_ms", ms},
    {"min_confidence", 0.7}, {"min_margin", 0.1},
    {"show_all_confidences", SendMessageW(confidence_check, BM_GETCHECK, 0, 0) == BST_CHECKED}};
  if (Checked(edge_check)) config["allowed_apps"].push_back("msedge.exe");
  if (Checked(chrome_check)) config["allowed_apps"].push_back("chrome.exe");
  if (Checked(word_check)) config["allowed_apps"].push_back("winword.exe");
  if (Checked(wechat_check)) { config["allowed_apps"].push_back("weixin.exe"); config["allowed_apps"].push_back("wechat.exe"); }
  config["surrounding_context"] = Checked(context_check);
  config["personal_learning"] = Checked(learning_check);
  config["phrase_completion"] = Checked(completion_check);
  auto dir = UserDir();
  auto tmp = dir / (L"typepick." + std::to_wstring(GetCurrentProcessId()) + L".tmp");
  { std::ofstream out(tmp); out << config.dump(2); out.close(); if (!out) throw std::runtime_error("config write"); }
  if (!MoveFileExW(tmp.c_str(), (dir / L"typepick.json").c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    throw std::runtime_error("config replace");
  return true;
}
HWND Control(const wchar_t* cls, const wchar_t* text, DWORD style, int x, int y, int w, int h, int id = 0) {
  HWND c = CreateWindowExW(cls == std::wstring(L"EDIT") ? WS_EX_CLIENTEDGE : 0, cls, text,
    WS_CHILD | WS_VISIBLE | style, x, y, w, h, main_window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
  SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
  return c;
}
void ImportEnv() {
  wchar_t path[32768] = {};
  OPENFILENAMEW ofn = {sizeof(ofn)};
  ofn.hwndOwner = main_window; ofn.lpstrFile = path; ofn.nMaxFile = 32768;
  ofn.lpstrFilter = L"环境配置文件\0.env;*.env\0所有文件\0*.*\0";
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
  if (!GetOpenFileNameW(&ofn)) return;
  auto key = ReadEnv(path);
  const bool saved = StoreKey(key);
  SecureZeroMemory(key.data(), key.size() * sizeof(wchar_t));
  if (!saved) throw std::runtime_error("credential save");
  Status(); Message(L"密钥已导入本机。勾选 AI 推荐后点击“保存并启动”。");
}
std::vector<std::string> Lines(HWND edit) {
  std::wstring value(GetWindowTextLengthW(edit) + 1, L'\0');
  GetWindowTextW(edit, value.data(), static_cast<int>(value.size())); value.pop_back();
  std::istringstream stream(typepick::Utf8(value)); std::string line;
  std::vector<std::string> lines;
  while (std::getline(stream, line)) { line = Trim(line); if (!line.empty()) lines.push_back(line); }
  if (lines.size() > 500) throw std::runtime_error("too many entries");
  return lines;
}
LRESULT CALLBACK VocabularyProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  try {
    if (msg == WM_CREATE) {
      auto add = [&](const wchar_t* cls, const wchar_t* text, DWORD style, int x, int y, int w, int h, int id = 0) {
        HWND control = CreateWindowExW(wcscmp(cls, L"EDIT") == 0 ? WS_EX_CLIENTEDGE : 0, cls, text,
          WS_CHILD | WS_VISIBLE | style, x, y, w, h, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE); return control;
      };
      add(L"STATIC", L"自定义词：每行“全拼 空格 词语”，例如：xiangmu TypePick项目", 0, 20, 18, 600, 30);
      terms_edit = add(L"EDIT", L"", ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | WS_TABSTOP, 20, 52, 580, 150);
      add(L"STATIC", L"短句：每行一句。输入开头至少两个汉字后，提示剩余部分。", 0, 20, 222, 600, 30);
      phrases_edit = add(L"EDIT", L"", ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | WS_TABSTOP, 20, 258, 580, 160);
      SendMessageW(terms_edit, EM_SETLIMITTEXT, 60000, 0); SendMessageW(phrases_edit, EM_SETLIMITTEXT, 60000, 0);
      add(L"BUTTON", L"保存词库", WS_TABSTOP | BS_DEFPUSHBUTTON, 20, 442, 170, 36, 201);
      add(L"STATIC", L"仅保存在本机。保存后回设置页点击“保存并启动”生效。", 0, 20, 492, 590, 30);
      typepick::PersonalStore store;
      store.LoadTerms(UserDir() / "terms.json"); store.LoadPhrases(UserDir() / "phrases.json");
      std::wstring terms, phrases;
      for (const auto& e : store.terms) terms += typepick::Wide(e.input + " " + e.text) + L"\r\n";
      for (const auto& text : store.phrases) phrases += typepick::Wide(text) + L"\r\n";
      SetWindowTextW(terms_edit, terms.c_str()); SetWindowTextW(phrases_edit, phrases.c_str());
      return 0;
    }
    if (msg == WM_COMMAND && LOWORD(wp) == 201) {
      auto terms = nlohmann::json::array();
      for (const auto& line : Lines(terms_edit)) {
        auto split = line.find_first_of(" \t");
        if (split == std::string::npos) throw std::runtime_error("term format");
        auto input = line.substr(0, split), text = Trim(line.substr(split));
        if (input.empty() || input.size() > 128 || input.find_first_not_of("abcdefghijklmnopqrstuvwxyz") != std::string::npos || text.empty() || text.size() > 256)
          throw std::runtime_error("term format");
        terms.push_back({{"input", input}, {"text", text}});
      }
      auto phrases = Lines(phrases_edit);
      for (const auto& text : phrases) if (text.size() > 256) throw std::runtime_error("phrase too long");
      WriteJson(UserDir() / "terms.json", terms); WriteJson(UserDir() / "phrases.json", phrases);
      MessageBoxW(hwnd, L"已保存。请回设置页点击“保存并启动”加载词库。", L"TypePick", MB_OK);
      return 0;
    }
    if (msg == WM_DESTROY) { vocabulary_window = nullptr; EnableWindow(main_window, TRUE); SetForegroundWindow(main_window); return 0; }
  } catch (...) { MessageBoxW(hwnd, L"保存失败。自定义词使用“小写全拼 空格 词语”；每项不超过约 80 汉字，每类最多 500 项。", L"TypePick", MB_OK | MB_ICONERROR); }
  return DefWindowProcW(hwnd, msg, wp, lp);
}
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  try {
    if (msg == WM_CREATE) {
      main_window = hwnd;
      ui_font = CreateFontW(-17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        0, 0, CLEARTYPE_QUALITY, 0, L"Microsoft YaHei UI");
      Control(L"STATIC", L"TypePick  ·  拼音输入 + 智能候选", 0, 24, 16, 550, 30);
      Control(L"STATIC", L"Win + 空格切换到 TypePick；空格或数字选词，Tab 采用推荐。\n候选顺序保持稳定，AI / 个人词库 / 短句补全分别标注。", 0, 24, 52, 550, 52);
      enabled_check = Control(L"BUTTON", L"启用智能推荐", BS_AUTOCHECKBOX | WS_TABSTOP, 24, 110, 550, 28);
      Control(L"STATIC", L"AI 会发送当前拼音、候选词及最近 100 字给 Typesafe/Jev。\n个人学习记录保存在本机；敏感输入框暂停所有推荐。", 0, 24, 145, 550, 50);
      Control(L"STATIC", L"Jev API Key（留空保留现有密钥）", 0, 24, 204, 550, 25);
      key_edit = Control(L"EDIT", L"", ES_PASSWORD | ES_AUTOHSCROLL | WS_TABSTOP, 24, 233, 380, 30);
      SendMessageW(key_edit, EM_SETLIMITTEXT, 2500, 0);
      Control(L"BUTTON", L"导入 .env", WS_TABSTOP, 420, 233, 145, 31, 101);
      status_label = Control(L"STATIC", L"", 0, 24, 274, 550, 25); Status();
      Control(L"STATIC", L"推荐等待上限（毫秒）", 0, 24, 312, 205, 25);
      timeout_edit = Control(L"EDIT", L"1500", ES_NUMBER | WS_TABSTOP, 240, 308, 110, 30);
      Control(L"STATIC", L"超时继续正常打字", 0, 370, 312, 200, 25);
      Control(L"BUTTON", L"保存并启动", WS_TABSTOP | BS_DEFPUSHBUTTON, 24, 356, 170, 38, 102);
      Control(L"BUTTON", L"打开记事本", WS_TABSTOP, 209, 356, 170, 38, 103);
      Control(L"BUTTON", L"打开用户目录", WS_TABSTOP, 394, 356, 170, 38, 104);
      Control(L"BUTTON", L"移除本机密钥", WS_TABSTOP, 24, 411, 170, 30, 105);
      confidence_check = Control(L"BUTTON", L"开发模式：低置信度也显示", BS_AUTOCHECKBOX | WS_TABSTOP, 210, 411, 360, 30);
      SendMessageW(confidence_check, BM_SETCHECK, BST_CHECKED, 0);
      Control(L"STATIC", L"推荐应用：记事本 + 以下勾选项（扩展应用为实验支持）", 0, 24, 460, 550, 25);
      edge_check = Control(L"BUTTON", L"Edge", BS_AUTOCHECKBOX | WS_TABSTOP, 24, 491, 120, 28);
      chrome_check = Control(L"BUTTON", L"Chrome", BS_AUTOCHECKBOX | WS_TABSTOP, 152, 491, 130, 28);
      word_check = Control(L"BUTTON", L"Word", BS_AUTOCHECKBOX | WS_TABSTOP, 302, 491, 120, 28);
      wechat_check = Control(L"BUTTON", L"微信", BS_AUTOCHECKBOX | WS_TABSTOP, 443, 491, 120, 28);
      context_check = Control(L"BUTTON", L"使用光标前最近 100 字（支持已有文字及编辑后重新读取）", BS_AUTOCHECKBOX | WS_TABSTOP, 24, 533, 550, 28);
      learning_check = Control(L"BUTTON", L"本机学习选词习惯（保存选词及其前文，可清除）", BS_AUTOCHECKBOX | WS_TABSTOP, 24, 566, 550, 28);
      completion_check = Control(L"BUTTON", L"启用本地短句补全", BS_AUTOCHECKBOX | WS_TABSTOP, 24, 599, 550, 28);
      for (auto check : {context_check, learning_check, completion_check}) SendMessageW(check, BM_SETCHECK, BST_CHECKED, 0);
      Control(L"BUTTON", L"编辑词库和短句", WS_TABSTOP, 24, 646, 230, 36, 106);
      Control(L"BUTTON", L"清除学习记录", WS_TABSTOP, 280, 646, 230, 36, 107);
      Control(L"STATIC", L"Windows x64 · Rime / 小狼毫 · AGPL-3.0\n源码：github.com/hetaodw/TypePick", 0, 24, 703, 550, 50);
      try {
        std::ifstream in(UserDir() / L"typepick.json");
        if (in) {
          auto raw = nlohmann::json::parse(in);
          auto c = typepick::ParseConfig(raw);
          // Existing MVP settings migrate to the requested development behavior.
          SendMessageW(confidence_check, BM_SETCHECK,
              raw.value("show_all_confidences", true) ? BST_CHECKED : BST_UNCHECKED, 0);
          SendMessageW(enabled_check, BM_SETCHECK, c.enabled ? BST_CHECKED : BST_UNCHECKED, 0);
          SetWindowTextW(timeout_edit, std::to_wstring(c.timeout_ms).c_str());
          SendMessageW(context_check, BM_SETCHECK, c.surrounding_context ? BST_CHECKED : BST_UNCHECKED, 0);
          SendMessageW(learning_check, BM_SETCHECK, c.personal_learning ? BST_CHECKED : BST_UNCHECKED, 0);
          SendMessageW(completion_check, BM_SETCHECK, c.phrase_completion ? BST_CHECKED : BST_UNCHECKED, 0);
          const std::pair<HWND, const char*> apps[] = {{edge_check, "msedge.exe"}, {chrome_check, "chrome.exe"}, {word_check, "winword.exe"}, {wechat_check, "weixin.exe"}, {wechat_check, "wechat.exe"}};
          for (const auto& app : apps) if (std::find(c.allowed_apps.begin(), c.allowed_apps.end(), app.second) != c.allowed_apps.end()) SendMessageW(app.first, BM_SETCHECK, BST_CHECKED, 0);
        }
      } catch (...) { Message(L"现有配置无法读取。请在此窗口重新保存设置。", true); }
      return 0;
    }
    if (msg == WM_COMMAND) {
      switch (LOWORD(wp)) {
        case 101: ImportEnv(); break;
        case 102:
          if (Save()) {
            auto exe = InstallDir() / L"TypePickServer.exe";
            if (!fs::exists(exe)) { Message(L"请先运行 TypePick 安装包，再打开安装后的设置程序。", true); break; }
            if (!StopServer() || !Launch(exe)) { Message(L"设置已保存，但服务未能启动。请重试或注销后登录。", true); break; }
            Message(L"设置已保存，启动命令已发送。首次启动会准备词库，可能需要稍等。\n\n按 Win + 空格选择 TypePick；若尚未显示，请注销后重新登录。");
          }
          break;
        case 103: ShellExecuteW(hwnd, L"open", L"notepad.exe", nullptr, nullptr, SW_SHOWNORMAL); break;
        case 104: ShellExecuteW(hwnd, L"open", UserDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL); break;
        case 106: {
          if (vocabulary_window) { SetForegroundWindow(vocabulary_window); break; }
          WNDCLASSW wc = {}; wc.lpfnWndProc = VocabularyProc; wc.hInstance = GetModuleHandleW(nullptr);
          wc.lpszClassName = L"TypePick.Vocabulary"; wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
          wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1); RegisterClassW(&wc);
          vocabulary_window = CreateWindowExW(0, wc.lpszClassName, L"TypePick 词库和短句", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
            CW_USEDEFAULT, CW_USEDEFAULT, 640, 570, hwnd, nullptr, wc.hInstance, nullptr);
          if (vocabulary_window) { EnableWindow(hwnd, FALSE); ShowWindow(vocabulary_window, SW_SHOWNORMAL); }
          break;
        }
        case 107:
          if (MessageBoxW(hwnd, L"清除本机自动学习的选词及前文？自定义词和短句会保留。", L"TypePick", MB_YESNO | MB_ICONQUESTION) == IDYES) {
            if (!StopServer()) throw std::runtime_error("stop server");
            WriteJson(UserDir() / "personal.json", {{"version", 1}, {"words", nlohmann::json::array()}});
            if (!Launch(InstallDir() / L"TypePickServer.exe")) throw std::runtime_error("start server");
            Message(L"学习记录已清除。");
          }
          break;
        case 105:
          if (MessageBoxW(hwnd, L"移除本机保存的 Jev 密钥？", L"TypePick", MB_YESNO | MB_ICONQUESTION) == IDYES) {
            if (!CredDeleteW(CredentialTarget, CRED_TYPE_GENERIC, 0) && GetLastError() != ERROR_NOT_FOUND)
              throw std::runtime_error("credential delete");
            Status();
          }
          break;
      }
      return 0;
    }
    if (msg == WM_DESTROY) { DeleteObject(ui_font); PostQuitMessage(0); return 0; }
  } catch (...) { Message(L"操作失败。请检查文件是否可读、用户目录是否可写，以及系统凭据服务是否正常。", true); }
  return DefWindowProcW(hwnd, msg, wp, lp);
}
}
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
  int argc = 0;
  auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  int code = 0;
  try {
    if (argc == 3 && std::wstring(argv[1]) == L"--import-env") {
      auto key = ReadEnv(argv[2]);
      code = StoreKey(key) && HasKey() ? 0 : 2;
      SecureZeroMemory(key.data(), key.size() * sizeof(wchar_t));
    } else if (argc == 2 && std::wstring(argv[1]) == L"--self-test") {
      auto target = L"TypePick/Test/" + std::to_wstring(GetCurrentProcessId());
      bool ok = StoreKey(L"synthetic-test-only", target.c_str());
      PCREDENTIALW c = nullptr;
      ok = ok && CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &c);
      if (c) { ok = ok && c->CredentialBlobSize == 38; CredFree(c); }
      const bool removed = CredDeleteW(target.c_str(), CRED_TYPE_GENERIC, 0) != FALSE;
      code = ok && removed ? 0 : 3;
    } else {
      WNDCLASSW wc = {};
      wc.lpfnWndProc = WindowProc; wc.hInstance = instance;
      wc.lpszClassName = L"TypePick.Settings.0.1";
      wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
      wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
      wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
      RegisterClassW(&wc);
      HWND window = CreateWindowExW(0, wc.lpszClassName, L"TypePick 设置", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 610, 800, nullptr, nullptr, instance, nullptr);
      if (!window) throw std::runtime_error("window create");
      ShowWindow(window, show); UpdateWindow(window);
      MSG msg;
      while (GetMessageW(&msg, nullptr, 0, 0) > 0)
        if (!(vocabulary_window && IsDialogMessageW(vocabulary_window, &msg)) && !IsDialogMessageW(window, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
  } catch (...) { code = 1; }
  LocalFree(argv);
  return code;
}
