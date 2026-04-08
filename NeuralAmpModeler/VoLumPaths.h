#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <vector>

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <Windows.h>
#endif

namespace volum
{
#ifdef _WIN32
// Set by the Windows installer (Inno Setup) so VST3 (under Common Files\VST3\...) can find bundled rigs.
inline std::filesystem::path RegistryAmpeteRigsDirectory()
{
  namespace fs = std::filesystem;
  HKEY hKey = nullptr;
  constexpr DWORD kAccess = KEY_READ | KEY_WOW64_64KEY;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\VoLum\\NeuralAmpModeler", 0, kAccess, &hKey) != ERROR_SUCCESS)
    return {};

  wchar_t buf[4096]{};
  DWORD bufBytes = sizeof(buf) - sizeof(wchar_t);
  DWORD type = 0;
  const LSTATUS q = RegQueryValueExW(hKey, L"AmpeteRigsPath", nullptr, &type, reinterpret_cast<BYTE*>(buf), &bufBytes);
  RegCloseKey(hKey);
  if (q != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ))
    return {};

  const wchar_t* pathStr = buf;
  wchar_t expanded[4096];
  if (type == REG_EXPAND_SZ)
  {
    const DWORD n = ExpandEnvironmentStringsW(buf, expanded, 4096);
    if (n == 0 || n > 4096)
      return {};
    pathStr = expanded;
  }
  std::error_code ec;
  fs::path p(pathStr);
  if (!fs::is_directory(p, ec))
    return {};
  return fs::weakly_canonical(p, ec);
}
#endif

// Locate rigs/Ampete One: installer registry (Windows VST3), relative to module / repo tree, or CWD.
inline std::filesystem::path FindAmpeteRigsDirectory()
{
  namespace fs = std::filesystem;
  std::vector<fs::path> candidates;

#ifdef _WIN32
  {
    const fs::path regDir = RegistryAmpeteRigsDirectory();
    if (!regDir.empty())
      candidates.push_back(regDir);
  }

  wchar_t module[MAX_PATH];
  const DWORD n = GetModuleFileNameW(nullptr, module, MAX_PATH);
  if (n > 0 && n < MAX_PATH)
  {
    fs::path exe(module);
    fs::path d = exe.parent_path();
    for (int depth = 0; depth < 12; ++depth)
    {
      candidates.push_back(d / "rigs" / "Ampete One");
      d = d.parent_path();
    }
  }
#endif

  // Working directory (e.g. run from repo root)
  candidates.push_back(fs::path("rigs") / "Ampete One");

  std::error_code ec;
  for (const auto& c : candidates)
  {
    const auto norm = c.lexically_normal();
    if (fs::is_directory(norm, ec))
    {
      return fs::weakly_canonical(norm, ec);
    }
  }
  return {};
}
// Return the parent rigs/ root directory (parent of "Ampete One").
inline std::filesystem::path FindRigsRootDirectory()
{
  auto ampeteDir = FindAmpeteRigsDirectory();
  if (!ampeteDir.empty())
    return ampeteDir.parent_path();
  return {};
}

struct ChannelFile
{
  std::string filename;
  std::string label;
};

// Scan an amp folder for .nam files matching a speaker prefix (e.g. "V30"),
// returning sorted list of {filename, channelLabel} pairs.
inline std::vector<ChannelFile> DiscoverChannels(
  const std::filesystem::path& rigsRoot,
  const char* ampFolder,
  const char* speakerPrefix)
{
  namespace fs = std::filesystem;
  std::vector<ChannelFile> result;

  fs::path ampDir = rigsRoot / ampFolder;
  std::error_code ec;
  if (!fs::is_directory(ampDir, ec))
    return result;

  std::string prefix = std::string(speakerPrefix) + "-";

  for (const auto& entry : fs::directory_iterator(ampDir, ec))
  {
    if (!entry.is_regular_file(ec))
      continue;
    std::string name = entry.path().filename().string();
    if (name.size() > 4
        && name.compare(name.size() - 4, 4, ".nam") == 0
        && name.compare(0, prefix.size(), prefix) == 0)
    {
      auto lastDash = name.rfind('-');
      if (lastDash != std::string::npos && lastDash + 1 < name.size() - 4)
      {
        std::string label = name.substr(lastDash + 1, name.size() - 4 - lastDash - 1);
        for (auto& c : label) c = (char)std::toupper((unsigned char)c);
        result.push_back({name, label});
      }
    }
  }

  std::sort(result.begin(), result.end(), [](const ChannelFile& a, const ChannelFile& b) {
    return a.label < b.label;
  });

  return result;
}

} // namespace volum
