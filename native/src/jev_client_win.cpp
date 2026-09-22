// SPDX-License-Identifier: AGPL-3.0-only
#include <typepick/windows.h>
#include <winhttp.h>
#include <wincred.h>
#include <algorithm>
#include <stdexcept>

namespace typepick {
std::string Utf8(const std::wstring& v) {
  if (v.empty()) return {};
  const int n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, v.data(), (int)v.size(), nullptr, 0, nullptr, nullptr);
  if (!n) return {};
  std::string out(n, '\0');
  WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, v.data(), (int)v.size(), out.data(), n, nullptr, nullptr);
  return out;
}
std::wstring Wide(const std::string& v) {
  if (v.empty()) return {};
  const int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, v.data(), (int)v.size(), nullptr, 0);
  if (!n) return {};
  std::wstring out(n, L'\0');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, v.data(), (int)v.size(), out.data(), n);
  return out;
}
namespace {
struct InternetHandle {
  HINTERNET value = nullptr;
  explicit InternetHandle(HINTERNET v) : value(v) {}
  ~InternetHandle() { if (value) WinHttpCloseHandle(value); }
  operator HINTERNET() const { return value; }
};
Decision Failure(const std::string& status) { Decision d; d.status = status; return d; }
bool Remaining(HINTERNET handle, Clock::time_point deadline) {
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count();
  return ms > 0 && WinHttpSetTimeouts(handle, (int)ms, (int)ms, (int)ms, (int)ms);
}
}
Decision CallJev(const Snapshot& s, const Config& c) {
  if (!c.enabled || !ValidSnapshot(s)) return Failure("disabled");
  if (c.mode == "demo") {
    // Test fixture, never represented as a real AI judgement.
    size_t choice = 0;
    const auto target = s.context.find("商店") != std::string::npos ? "烟酒" : "研究";
    for (size_t i = 0; i < s.candidates.size(); ++i) if (s.candidates[i] == target) choice = i;
    return Decision{choice, 1.0, "demo"};
  }
  wchar_t key[4096] = {};
  const DWORD n = GetEnvironmentVariableW(L"TYPESAFE_API_KEY", key, 4096);
  if (n >= 4096) return Failure("invalid_api_key");
  std::wstring key_value(key, n);
  SecureZeroMemory(key, sizeof(key));
  if (key_value.empty()) {
    PCREDENTIALW credential = nullptr;
    if (CredReadW(L"TypePick/Jev", CRED_TYPE_GENERIC, 0, &credential)) {
      if (credential->CredentialBlobSize % sizeof(wchar_t) == 0)
        key_value.assign(reinterpret_cast<wchar_t*>(credential->CredentialBlob),
                         credential->CredentialBlobSize / sizeof(wchar_t));
      CredFree(credential);
    }
  }
  if (key_value.empty()) return Failure("missing_api_key");
  // Credentials are never included in logs or plaintext settings.
  if (key_value.find_first_of(L"\r\n") != std::wstring::npos) return Failure("invalid_api_key");
  const auto deadline = Clock::now() + std::chrono::milliseconds(c.timeout_ms);
  InternetHandle session(WinHttpOpen(L"TypePick/0.1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
  if (!session || !Remaining(session, deadline)) return Failure("network_error");
  InternetHandle connection(WinHttpConnect(session, L"api.typesafe.ai", INTERNET_DEFAULT_HTTPS_PORT, 0));
  if (!connection) return Failure("network_error");
  InternetHandle request(WinHttpOpenRequest(connection, L"POST", L"/v1/systemone", nullptr,
      WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
  if (!request) return Failure("network_error");
  DWORD redirect_policy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
  if (!WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &redirect_policy, sizeof(redirect_policy)))
    return Failure("network_error");
  const auto payload = MakeRequest(s, c).dump();
  const auto headers = L"Content-Type: application/json\r\nAuthorization: Bearer " + key_value + L"\r\n";
  if (!Remaining(request, deadline) ||
      !WinHttpSendRequest(request, headers.c_str(), (DWORD)-1,
          (void*)payload.data(), (DWORD)payload.size(), (DWORD)payload.size(), 0) ||
      !Remaining(request, deadline) || !WinHttpReceiveResponse(request, nullptr))
    return Failure(Clock::now() >= deadline ? "timeout" : "network_error");
  DWORD status = 0, status_size = sizeof(status);
  if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                           nullptr, &status, &status_size, nullptr)) return Failure("network_error");
  if (status != 200) return Failure("http_" + std::to_string(status));
  std::string body;
  char buffer[4096];
  DWORD bytes = 0;
  do {
    if (!Remaining(request, deadline)) return Failure("timeout");
    if (!WinHttpReadData(request, buffer, sizeof(buffer), &bytes)) return Failure("network_error");
    if (body.size() + bytes > 65536) return Failure("response_too_large");
    body.append(buffer, bytes);
  } while (bytes);
  const auto parsed = nlohmann::json::parse(body, nullptr, false);
  if (parsed.is_discarded()) return Failure("invalid_response");
  return ParseDecision(parsed, s.candidates.size(), c);
}
}  // namespace typepick
