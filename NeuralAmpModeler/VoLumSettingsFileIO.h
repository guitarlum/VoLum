#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <thread>

#if __has_include(<nlohmann/json.hpp>)
  #include <nlohmann/json.hpp>
#elif __has_include(<json.hpp>)
  #include <json.hpp>
#else
  #error "nlohmann json header not found (expected iPlug Dependencies/Extras layout)"
#endif

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
#endif

namespace volum
{

inline std::filesystem::path MakeAtomicJsonTempPath(const std::filesystem::path& target)
{
  const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
  const auto threadHash = std::hash<std::thread::id>{}(std::this_thread::get_id());

  std::ostringstream suffix;
  suffix << ".tmp." << ticks << "." << threadHash;

  std::filesystem::path tmp = target;
  tmp += suffix.str();
  return tmp;
}

inline bool ReplaceFileAtomically(const std::filesystem::path& tmp, const std::filesystem::path& target,
                                  std::error_code& ec)
{
  ec.clear();
#ifdef _WIN32
  for (int attempt = 0; attempt < 20; ++attempt)
  {
    if (MoveFileExW(tmp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
      ec.clear();
      return true;
    }

    const DWORD lastError = GetLastError();
    ec = std::error_code(static_cast<int>(lastError), std::system_category());
    if (lastError != ERROR_ACCESS_DENIED && lastError != ERROR_SHARING_VIOLATION)
      return false;

    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  return false;
#else
  std::filesystem::rename(tmp, target, ec);
  return !ec;
#endif
}

inline bool WriteJsonAtomically(const std::filesystem::path& target, const nlohmann::json& json, std::error_code& ec)
{
  ec.clear();
  if (target.empty())
  {
    ec = std::make_error_code(std::errc::invalid_argument);
    return false;
  }

  const auto parent = target.parent_path();
  if (!parent.empty())
  {
    std::filesystem::create_directories(parent, ec);
    if (ec)
      return false;
  }

  // dump() throws nlohmann::type_error on invalid UTF-8 anywhere in the document,
  // and every user-entered name reaches this function. The individual name cuts are
  // code-point aware (see Utf8Prefix), but this is the single choke point through
  // which all of them pass, so serialize before touching the filesystem and report
  // a failure rather than letting an exception escape into UI or host code.
  std::string serialized;
  try
  {
    serialized = json.dump(2);
  }
  catch (const std::exception&)
  {
    ec = std::make_error_code(std::errc::invalid_argument);
    return false;
  }

  const auto tmp = MakeAtomicJsonTempPath(target);
  {
    std::ofstream out(tmp, std::ios::out | std::ios::trunc);
    if (!out)
    {
      ec = std::make_error_code(std::errc::io_error);
      return false;
    }
    out << serialized;
    out.close();
    if (!out.good())
    {
      ec = std::make_error_code(std::errc::io_error);
      std::error_code removeEc;
      std::filesystem::remove(tmp, removeEc);
      return false;
    }
  }

  if (!ReplaceFileAtomically(tmp, target, ec))
  {
    std::error_code removeEc;
    std::filesystem::remove(tmp, removeEc);
    return false;
  }
  return true;
}

} // namespace volum
