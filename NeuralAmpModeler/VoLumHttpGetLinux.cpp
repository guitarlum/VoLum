#ifdef __linux__
  #ifndef _GNU_SOURCE
    #define _GNU_SOURCE
  #endif
#endif
#include "VoLumHttpGet.h"

#ifdef __linux__

  #include <cctype>
  #include <cstddef>
  #include <cstdint>
  #include <dlfcn.h>
  #include <mutex>
  #include <new>

// Linux update check. libcurl is loaded with dlopen so the plugin and the
// standalone have no DT_NEEDED on libcurl; a missing library returns false.
// Option numbers are the libcurl ABI (stable since the option was added). No
// curl headers are included, and nothing here is linked with -lcurl.
//
// VOLUM_TEST_CURL_SONAME (test-only, honored in Release builds because the
// Linux doctest suite is GCC Release): when set and non-empty, dlopen that
// soname instead of libcurl.so.4. A name that does not load returns false
// and does not crash. Production leaves the variable unset.

namespace
{

constexpr int kOptWriteData = 10001;
constexpr int kOptUrl = 10002;
constexpr int kOptUserAgent = 10018;
constexpr int kOptWriteFunction = 20011;
constexpr int kOptFollowLocation = 52;
constexpr int kOptSslVerifyPeer = 64;
constexpr int kOptMaxRedirs = 68;
constexpr int kOptHttpGet = 80;
constexpr int kOptSslVerifyHost = 81;
constexpr int kOptNoSignal = 99;
constexpr int kOptTimeoutMs = 155;
constexpr int kOptConnectTimeoutMs = 156;
constexpr int kOptProtocols = 181;
constexpr int kOptRedirProtocols = 182;
constexpr int kOptProtocolsStr = 10318;
constexpr int kOptRedirProtocolsStr = 10319;
constexpr int kInfoResponseCode = 0x200000 + 2;
constexpr long kCurlProtoHttp = 1L;
constexpr long kCurlProtoHttps = 2L;
constexpr long kCurlGlobalDefault = 3L; // CURL_GLOBAL_SSL | CURL_GLOBAL_WIN32
constexpr std::size_t kMaximumBytes = 1024 * 1024;

using CurlGlobalInitFn = int (*)(long flags);
using CurlEasyInitFn = void* (*)();
using CurlEasySetoptFn = int (*)(void* easy, int option, ...);
using CurlEasyPerformFn = int (*)(void* easy);
using CurlEasyGetinfoFn = int (*)(void* easy, int info, ...);
using CurlEasyCleanupFn = void (*)(void* easy);

struct CurlApi
{
  void* lib = nullptr;
  CurlGlobalInitFn global_init = nullptr;
  CurlEasyInitFn easy_init = nullptr;
  CurlEasySetoptFn easy_setopt = nullptr;
  CurlEasyPerformFn easy_perform = nullptr;
  CurlEasyGetinfoFn easy_getinfo = nullptr;
  CurlEasyCleanupFn easy_cleanup = nullptr;
};

struct CurlSink
{
  std::string body;
  bool overflow = false;
};

struct EasyHandle
{
  const CurlApi* api = nullptr;
  void* handle = nullptr;
  ~EasyHandle()
  {
    if (api && handle && api->easy_cleanup)
      api->easy_cleanup(handle);
  }
};

extern "C" std::size_t VolumCurlWrite(char* ptr, std::size_t size, std::size_t nmemb, void* userdata)
{
  if (!ptr || !userdata)
    return 0;
  if (nmemb != 0 && size > static_cast<std::size_t>(-1) / nmemb)
    return 0;
  const std::size_t nbytes = size * nmemb;
  auto* sink = static_cast<CurlSink*>(userdata);
  if (sink->overflow || sink->body.size() + nbytes > kMaximumBytes)
  {
    sink->overflow = true;
    return 0;
  }
  try
  {
    sink->body.append(ptr, nbytes);
  }
  catch (const std::bad_alloc&)
  {
    sink->overflow = true;
    return 0;
  }
  return nbytes;
}

std::once_flag gCurlInitOnce;
int gCurlInitResult = 1;
std::mutex gCurlLoadMu;
CurlApi gCurlApi{};
bool gCurlReady = false;

bool ResolveCurl(void* lib, CurlApi& api)
{
  dlerror();
  api = {};
  api.lib = lib;
  api.global_init = reinterpret_cast<CurlGlobalInitFn>(dlsym(lib, "curl_global_init"));
  api.easy_init = reinterpret_cast<CurlEasyInitFn>(dlsym(lib, "curl_easy_init"));
  api.easy_setopt = reinterpret_cast<CurlEasySetoptFn>(dlsym(lib, "curl_easy_setopt"));
  api.easy_perform = reinterpret_cast<CurlEasyPerformFn>(dlsym(lib, "curl_easy_perform"));
  api.easy_getinfo = reinterpret_cast<CurlEasyGetinfoFn>(dlsym(lib, "curl_easy_getinfo"));
  api.easy_cleanup = reinterpret_cast<CurlEasyCleanupFn>(dlsym(lib, "curl_easy_cleanup"));
  return api.global_init && api.easy_init && api.easy_setopt && api.easy_perform && api.easy_getinfo
         && api.easy_cleanup;
}

// closeAfter is set when the caller must dlclose a test-only soname after the
// request. The production libcurl.so.4 handle stays open for the process.
bool LoadCurl(CurlApi& api, void** closeAfter)
{
  *closeAfter = nullptr;
  const char* overrideSoname = std::getenv("VOLUM_TEST_CURL_SONAME");
  const bool overridden = overrideSoname && overrideSoname[0];
  if (!overridden)
  {
    std::lock_guard<std::mutex> lock(gCurlLoadMu);
    if (gCurlReady)
    {
      api = gCurlApi;
      return true;
    }
    void* lib = dlopen("libcurl.so.4", RTLD_NOW | RTLD_LOCAL);
    if (!lib || !ResolveCurl(lib, gCurlApi))
    {
      if (lib)
        dlclose(lib);
      gCurlApi = {};
      return false;
    }
    gCurlReady = true;
    api = gCurlApi;
    return true;
  }

  void* lib = dlopen(overrideSoname, RTLD_NOW | RTLD_LOCAL);
  if (!lib || !ResolveCurl(lib, api))
  {
    if (lib)
      dlclose(lib);
    api = {};
    return false;
  }
  *closeAfter = lib;
  return true;
}

bool EnsureCurlInit(const CurlApi& api)
{
  std::call_once(gCurlInitOnce, [&] { gCurlInitResult = api.global_init(kCurlGlobalDefault); });
  return gCurlInitResult == 0;
}

bool SchemeIsHttp(const char* url, bool& https)
{
  auto starts = [](const char* value, const char* prefix) {
    for (; *prefix; ++value, ++prefix)
    {
      if (!*value)
        return false;
      const unsigned char a = static_cast<unsigned char>(*value);
      const unsigned char b = static_cast<unsigned char>(*prefix);
      if (std::tolower(a) != std::tolower(b))
        return false;
    }
    return true;
  };
  if (starts(url, "https://"))
  {
    https = true;
    return true;
  }
  if (starts(url, "http://"))
  {
    https = false;
    return true;
  }
  return false;
}

bool RestrictProtocols(const CurlApi& api, void* easy, bool https)
{
  const char* redir = https ? "https" : "http,https";
  const long redirBits = https ? kCurlProtoHttps : (kCurlProtoHttp | kCurlProtoHttps);
  const int initStr = api.easy_setopt(easy, kOptProtocolsStr, "http,https");
  const int redirStr = api.easy_setopt(easy, kOptRedirProtocolsStr, redir);
  const int initBits = api.easy_setopt(easy, kOptProtocols, kCurlProtoHttp | kCurlProtoHttps);
  const int redirBitsRc = api.easy_setopt(easy, kOptRedirProtocols, redirBits);
  return (initStr == 0 || initBits == 0) && (redirStr == 0 || redirBitsRc == 0);
}

} // namespace

bool VolumHttpGetString(const char* url, std::string& out, int timeoutMs)
{
  out.clear();
  if (!url || !*url || timeoutMs <= 0)
    return false;

  bool https = false;
  if (!SchemeIsHttp(url, https))
    return false;

  CurlApi api{};
  void* extraLib = nullptr;
  if (!LoadCurl(api, &extraLib))
    return false;
  struct CloseExtra
  {
    void* lib = nullptr;
    ~CloseExtra()
    {
      if (lib)
        dlclose(lib);
    }
  } closeExtra{extraLib};

  if (!EnsureCurlInit(api))
    return false;

  EasyHandle easy{&api, api.easy_init()};
  if (!easy.handle)
    return false;

  CurlSink sink;
  const long timeout = static_cast<long>(timeoutMs);
  if (api.easy_setopt(easy.handle, kOptUrl, url) != 0
      || api.easy_setopt(easy.handle, kOptUserAgent, "VoLum update notifier") != 0
      || api.easy_setopt(easy.handle, kOptWriteFunction, VolumCurlWrite) != 0
      || api.easy_setopt(easy.handle, kOptWriteData, &sink) != 0
      || api.easy_setopt(easy.handle, kOptFollowLocation, 1L) != 0
      || api.easy_setopt(easy.handle, kOptMaxRedirs, 10L) != 0
      || api.easy_setopt(easy.handle, kOptSslVerifyPeer, 1L) != 0
      || api.easy_setopt(easy.handle, kOptSslVerifyHost, 2L) != 0
      || api.easy_setopt(easy.handle, kOptTimeoutMs, timeout) != 0
      || api.easy_setopt(easy.handle, kOptConnectTimeoutMs, timeout) != 0
      || api.easy_setopt(easy.handle, kOptNoSignal, 1L) != 0 || api.easy_setopt(easy.handle, kOptHttpGet, 1L) != 0
      || !RestrictProtocols(api, easy.handle, https))
    return false;

  if (api.easy_perform(easy.handle) != 0 || sink.overflow)
    return false;

  long status = 0;
  if (api.easy_getinfo(easy.handle, kInfoResponseCode, &status) != 0 || status != 200)
    return false;
  out = std::move(sink.body);
  return true;
}

#endif
