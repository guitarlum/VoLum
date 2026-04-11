#pragma once

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <Windows.h>
#elif defined(__APPLE__)
  #include <dlfcn.h>
  #include <mach-o/dyld.h>
#endif

namespace volum
{
#ifdef _WIN32
// Installer (Inno) sets this so VST3 — installed under Common Files\VST3\ — can find bundled rigs.
inline std::filesystem::path RegistryRigsRootFromInstaller()
{
  namespace fs = std::filesystem;
  HKEY hKey = nullptr;
  constexpr DWORD kAccess = KEY_READ | KEY_WOW64_64KEY;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\VoLum\\NeuralAmpModeler", 0, kAccess, &hKey) != ERROR_SUCCESS)
    return {};

  auto querySz = [&](const wchar_t* valueName) -> fs::path {
    wchar_t buf[4096]{};
    DWORD bufBytes = sizeof(buf) - sizeof(wchar_t);
    DWORD type = 0;
    const LSTATUS q = RegQueryValueExW(hKey, valueName, nullptr, &type, reinterpret_cast<BYTE*>(buf), &bufBytes);
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
  };

  fs::path root = querySz(L"VoLumRigsRoot");
  if (!root.empty())
  {
    RegCloseKey(hKey);
    return root;
  }

  // Legacy installs: AmpeteRigsPath was ...\rigs\Ampete One — use parent as rigs root.
  fs::path legacyAmpete = querySz(L"AmpeteRigsPath");
  RegCloseKey(hKey);
  if (!legacyAmpete.empty())
    return legacyAmpete.parent_path();
  return {};
}
#endif

// Bundled rigs root: registry (installer VST3), then walk up from this module (VST3 DLL or standalone
// .exe — not the host process), then CWD ./VoLumRigs or ./rigs (dev uses repo rigs/).
inline std::filesystem::path FindRigsRootDirectory()
{
  namespace fs = std::filesystem;
  std::vector<fs::path> candidates;

#ifdef _WIN32
  {
    const fs::path regRoot = RegistryRigsRootFromInstaller();
    if (!regRoot.empty())
      candidates.push_back(regRoot);
  }

  HMODULE hMod = nullptr;
  if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         reinterpret_cast<LPCWSTR>(&FindRigsRootDirectory), &hMod)
      && hMod != nullptr)
  {
    wchar_t module[MAX_PATH];
    const DWORD n = GetModuleFileNameW(hMod, module, MAX_PATH);
    if (n > 0 && n < MAX_PATH)
    {
      fs::path d = fs::path(module).parent_path();
      for (int depth = 0; depth < 12; ++depth)
      {
        candidates.push_back(d / "VoLumRigs");
        candidates.push_back(d / "rigs");
        d = d.parent_path();
      }
    }
  }
#elif defined(__APPLE__)
  {
    fs::path modulePath;
    Dl_info info{};
    if (dladdr(reinterpret_cast<const void*>(&FindRigsRootDirectory), &info) != 0 && info.dli_fname)
    {
      modulePath = fs::weakly_canonical(fs::path(info.dli_fname));
    }
    else
    {
      char buf[4096];
      uint32_t bufSize = sizeof(buf);
      if (_NSGetExecutablePath(buf, &bufSize) == 0)
        modulePath = fs::weakly_canonical(fs::path(buf));
    }

    if (!modulePath.empty())
    {
      fs::path d = modulePath.parent_path();
      // Inside .app bundle: .../VoLum.app/Contents/MacOS/VoLum
      candidates.push_back(d.parent_path() / "Resources" / "VoLumRigs");
      candidates.push_back(d.parent_path() / "Resources" / "rigs");
      if (const char* home = std::getenv("HOME"); home && *home)
      {
        const fs::path appSupport = fs::path(home) / "Library" / "Application Support" / "VoLum";
        candidates.push_back(appSupport / "VoLumRigs");
        candidates.push_back(appSupport / "rigs");
      }
      candidates.push_back(fs::path("/Library/Application Support/VoLum/VoLumRigs"));
      candidates.push_back(fs::path("/Library/Application Support/VoLum/rigs"));
      for (int depth = 0; depth < 12; ++depth)
      {
        candidates.push_back(d / "VoLumRigs");
        candidates.push_back(d / "rigs");
        d = d.parent_path();
      }
    }
  }
#endif

  candidates.push_back(fs::path("VoLumRigs"));
  candidates.push_back(fs::path("rigs"));

  std::error_code ec;
  for (const auto& c : candidates)
  {
    const auto norm = c.lexically_normal();
    if (fs::is_directory(norm, ec))
      return fs::weakly_canonical(norm, ec);
  }
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

// User-writable JSON for VoLum per-amp UI state. Do not use rigs root alone — it may be under
// Program Files (installer) and rejects writes for non-admin users.
inline std::filesystem::path VolumUserSettingsFilePath()
{
  namespace fs = std::filesystem;
#ifdef _WIN32
  const char* la = std::getenv("LOCALAPPDATA");
  if (!la || !*la)
    return {};
  return fs::path(la) / "VoLum" / "volum-settings.json";
#elif defined(__APPLE__)
  const char* home = std::getenv("HOME");
  if (!home || !*home)
    return {};
  return fs::path(home) / "Library" / "Application Support" / "VoLum" / "volum-settings.json";
#else
  (void)0;
  return {};
#endif
}

} // namespace volum
