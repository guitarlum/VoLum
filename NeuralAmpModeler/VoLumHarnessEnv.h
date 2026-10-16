#pragma once

// Test harness only: environment variables that stand in for native file dialogs,
// which nothing can drive on a locked workstation (e2e-standalone-win.ps1).
// Unset or empty, the dialog runs.
//
//   VOLUM_PACK_SAVE_PATH / VOLUM_PACK_OPEN_PATH  Pack export / import (-Scenario pack)
//   VOLUM_IMPORT_OPEN_PATHS                      Manage > Add for IRs and pedals,
//                                                '|'-separated (-Scenario import)

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#if defined(_WIN32)
  #include <filesystem>
  #include <windows.h>
#endif

namespace volum
{
inline std::string HarnessEnv(const char* name)
{
#if defined(_WIN32)
  const std::wstring wideName(name, name + std::strlen(name));
  wchar_t buf[8192] = {};
  const DWORD len = GetEnvironmentVariableW(wideName.c_str(), buf, 8192);
  if (len == 0 || len >= 8192)
    return {};
  const auto u8 = std::filesystem::path(buf).u8string();
  return std::string(u8.begin(), u8.end());
#else
  const char* v = std::getenv(name);
  return v ? std::string(v) : std::string();
#endif
}

inline std::vector<std::string> SplitHarnessPaths(const std::string& joined)
{
  std::vector<std::string> out;
  std::size_t start = 0;
  while (start <= joined.size())
  {
    const std::size_t bar = joined.find('|', start);
    const std::size_t end = bar == std::string::npos ? joined.size() : bar;
    if (end > start)
      out.push_back(joined.substr(start, end - start));
    if (bar == std::string::npos)
      break;
    start = bar + 1;
  }
  return out;
}
} // namespace volum
