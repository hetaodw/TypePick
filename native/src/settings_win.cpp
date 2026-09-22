// SPDX-License-Identifier: AGPL-3.0-only
#include <typepick/windows.h>
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
HWND main_window, key_edit, enabled_check, timeout_edit, status_label;
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
    {"min_confidence", 0.7}, {"min_margin", 0.1}};
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
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  try {
    if (msg == WM_CREATE) {
      main_window = hwnd;
      ui_font = CreateFontW(-17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        0, 0, CLEARTYPE_QUALITY, 0, L"Microsoft YaHei UI");
      Control(L"STATIC", L"TypePick  ·  拼音输入 + AI 候选推荐", 0, 24, 20, 550, 30);
      Control(L"STATIC", L"1. 点击保存并启动\n2. 按 Win + 空格，切换到 TypePick\n3. 输入拼音，空格或数字选词；看到 AI 推荐时按 Tab 采用", 0, 24, 62, 550, 80);
      enabled_check = Control(L"BUTTON", L"启用 AI 推荐（首版仅在记事本中生效）", BS_AUTOCHECKBOX | WS_TABSTOP, 24, 152, 550, 30);
      Control(L"STATIC", L"启用后，当前拼音、候选词及本次连续输入的最近 100 字\n会发送给 Typesafe/Jev。普通拼音输入在本地运行。", 0, 24, 190, 550, 54);
      Control(L"STATIC", L"Jev API Key（留空表示保留已保存密钥）", 0, 24, 260, 550, 25);
      key_edit = Control(L"EDIT", L"", ES_PASSWORD | ES_AUTOHSCROLL | WS_TABSTOP, 24, 289, 380, 30);
      SendMessageW(key_edit, EM_SETLIMITTEXT, 2500, 0);
      Control(L"BUTTON", L"导入 .env", WS_TABSTOP, 420, 289, 145, 31, 101);
      status_label = Control(L"STATIC", L"", 0, 24, 330, 550, 25); Status();
      Control(L"STATIC", L"推荐等待上限（毫秒）", 0, 24, 372, 205, 25);
      timeout_edit = Control(L"EDIT", L"1500", ES_NUMBER | WS_TABSTOP, 240, 368, 110, 30);
      Control(L"STATIC", L"超时继续正常打字", 0, 370, 372, 200, 25);
      Control(L"BUTTON", L"保存并启动", WS_TABSTOP | BS_DEFPUSHBUTTON, 24, 421, 170, 38, 102);
      Control(L"BUTTON", L"打开记事本", WS_TABSTOP, 209, 421, 170, 38, 103);
      Control(L"BUTTON", L"打开用户目录", WS_TABSTOP, 394, 421, 170, 38, 104);
      Control(L"BUTTON", L"移除本机密钥", WS_TABSTOP, 24, 477, 170, 30, 105);
      Control(L"STATIC", L"Windows x64 MVP · 基于 Rime / 小狼毫\nTypePick 源码：github.com/hetaodw/TypePick · AGPL-3.0", 0, 24, 528, 550, 50);
      try {
        std::ifstream in(UserDir() / L"typepick.json");
        if (in) {
          auto c = typepick::ParseConfig(nlohmann::json::parse(in));
          SendMessageW(enabled_check, BM_SETCHECK, c.enabled ? BST_CHECKED : BST_UNCHECKED, 0);
          SetWindowTextW(timeout_edit, std::to_wstring(c.timeout_ms).c_str());
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
            if (!Launch(exe, L"/q", true) || !Launch(exe)) { Message(L"设置已保存，但服务未能启动。请重试或注销后登录。", true); break; }
            Message(L"设置已保存，启动命令已发送。首次启动会准备词库，可能需要稍等。\n\n按 Win + 空格选择 TypePick；若尚未显示，请注销后重新登录。");
          }
          break;
        case 103: ShellExecuteW(hwnd, L"open", L"notepad.exe", nullptr, nullptr, SW_SHOWNORMAL); break;
        case 104: ShellExecuteW(hwnd, L"open", UserDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL); break;
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
        CW_USEDEFAULT, CW_USEDEFAULT, 610, 630, nullptr, nullptr, instance, nullptr);
      if (!window) throw std::runtime_error("window create");
      ShowWindow(window, show); UpdateWindow(window);
      MSG msg;
      while (GetMessageW(&msg, nullptr, 0, 0) > 0)
        if (!IsDialogMessageW(window, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
  } catch (...) { code = 1; }
  LocalFree(argv);
  return code;
}
