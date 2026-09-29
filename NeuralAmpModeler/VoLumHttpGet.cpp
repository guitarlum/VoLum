#include "VoLumHttpGet.h"

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
  #include <winhttp.h>

  #pragma comment(lib, "winhttp.lib")

namespace
{

struct InternetHandle
{
  HINTERNET value = nullptr;
  ~InternetHandle()
  {
    if (value)
      WinHttpCloseHandle(value);
  }
};

std::wstring Utf8ToWide(const char* text)
{
  if (!text || !*text)
    return {};
  const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, nullptr, 0);
  if (count <= 1)
    return {};
  std::wstring result(static_cast<size_t>(count), L'\0');
  if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, result.data(), count) == 0)
    return {};
  result.pop_back();
  return result;
}

} // namespace

bool VolumHttpGetString(const char* url, std::string& out, int timeoutMs)
{
  out.clear();
  const std::wstring wideUrl = Utf8ToWide(url);
  if (wideUrl.empty() || timeoutMs <= 0)
    return false;

  URL_COMPONENTS parts{};
  parts.dwStructSize = sizeof(parts);
  parts.dwSchemeLength = static_cast<DWORD>(-1);
  parts.dwHostNameLength = static_cast<DWORD>(-1);
  parts.dwUrlPathLength = static_cast<DWORD>(-1);
  parts.dwExtraInfoLength = static_cast<DWORD>(-1);
  if (!WinHttpCrackUrl(wideUrl.c_str(), 0, 0, &parts) || parts.nScheme != INTERNET_SCHEME_HTTPS)
    return false;

  const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
  std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
  if (parts.dwExtraInfoLength)
    path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
  if (path.empty())
    path = L"/";

  InternetHandle session{
    WinHttpOpen(L"VoLum update notifier", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                WINHTTP_NO_PROXY_BYPASS, 0)};
  if (!session.value
      || !WinHttpSetTimeouts(session.value, timeoutMs, timeoutMs, timeoutMs, timeoutMs))
    return false;

  InternetHandle connection{WinHttpConnect(session.value, host.c_str(), parts.nPort, 0)};
  if (!connection.value)
    return false;

  InternetHandle request{
    WinHttpOpenRequest(connection.value, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                       WINHTTP_FLAG_SECURE)};
  if (!request.value || !WinHttpSendRequest(request.value, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0,
                                             0, 0)
      || !WinHttpReceiveResponse(request.value, nullptr))
    return false;

  DWORD status = 0;
  DWORD statusSize = sizeof(status);
  if (!WinHttpQueryHeaders(request.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                           WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX)
      || status != 200)
    return false;

  constexpr size_t kMaximumBytes = 1024 * 1024;
  for (;;)
  {
    DWORD available = 0;
    if (!WinHttpQueryDataAvailable(request.value, &available))
      return false;
    if (available == 0)
      return true;
    if (out.size() + available > kMaximumBytes)
      return false;
    const size_t start = out.size();
    out.resize(start + available);
    DWORD read = 0;
    if (!WinHttpReadData(request.value, out.data() + start, available, &read))
      return false;
    out.resize(start + read);
  }
}

#else

bool VolumHttpGetString(const char*, std::string& out, int)
{
  out.clear();
  return false;
}

#endif
