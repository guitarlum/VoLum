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
#elif defined(__linux__)
  #include <dlfcn.h>
  #include <unistd.h>
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

// Windows order: walk up from the module's folder, then the installer's registry root. The
// tree beside the binary comes first so a portable build never runs on another install's
// rigs; the registry root serves the VST3 installed under Common Files.
inline std::vector<std::filesystem::path> WindowsRigsCandidates(const std::filesystem::path& moduleDir,
                                                                const std::filesystem::path& registryRoot)
{
  namespace fs = std::filesystem;
  std::vector<fs::path> candidates;
  if (!moduleDir.empty())
  {
    fs::path d = moduleDir;
    for (int depth = 0; depth < 12; ++depth)
    {
      candidates.push_back(d / "VoLumRigs");
      candidates.push_back(d / "rigs");
      d = d.parent_path();
    }
  }
  if (!registryRoot.empty())
    candidates.push_back(registryRoot);
  return candidates;
}

#if defined(__linux__)
// XDG Base Directory: an empty or relative XDG_DATA_HOME is not a path. Fall back to
// $HOME/.local/share, then VoLum, the same folder the Windows files use under LOCALAPPDATA.
inline bool LinuxAbsoluteXdg(const char* value)
{
  return value && value[0] == '/';
}

inline std::filesystem::path LinuxVoLumDataDir()
{
  namespace fs = std::filesystem;
  if (const char* xdg = std::getenv("XDG_DATA_HOME"); LinuxAbsoluteXdg(xdg))
    return fs::path(xdg) / "VoLum";
  const char* home = std::getenv("HOME");
  if (!home || !*home)
    return {};
  return fs::path(home) / ".local" / "share" / "VoLum";
}

// App: dladdr and /proc/self/exe name the same file, and the exe path is the one readlink
// resolved. Plugin: the VoLum symbol lives in the .so, which is not the host executable.
inline std::filesystem::path LinuxChooseModuleFile(const std::filesystem::path& fromDladdr,
                                                   const std::filesystem::path& fromExe)
{
  namespace fs = std::filesystem;
  std::error_code ec;
  const bool dlOk = !fromDladdr.empty() && fs::is_regular_file(fromDladdr, ec);
  ec.clear();
  const bool exeOk = !fromExe.empty() && fs::is_regular_file(fromExe, ec);
  if (dlOk && exeOk)
  {
    ec.clear();
    if (fs::equivalent(fromDladdr, fromExe, ec) && !ec)
      return fromExe;
  }
  if (dlOk)
    return fromDladdr;
  if (exeOk)
    return fromExe;
  return !fromExe.empty() ? fromExe : fromDladdr;
}

inline std::filesystem::path LinuxModuleFile()
{
  namespace fs = std::filesystem;
  fs::path fromDl;
  Dl_info info{};
  if (dladdr(reinterpret_cast<const void*>(&LinuxModuleFile), &info) != 0 && info.dli_fname && info.dli_fname[0])
    fromDl = fs::path(info.dli_fname);

  fs::path fromExe;
  char buf[4096];
  const ssize_t n = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
  if (n > 0 && static_cast<std::size_t>(n) < sizeof(buf) - 1)
  {
    buf[n] = '\0';
    fromExe = fs::path(buf);
  }
  return LinuxChooseModuleFile(fromDl, fromExe);
}

// Locked order: beside the binary, <bundle>.vst3/Contents/Resources/VoLumRigs (the .so sits in
// Contents/x86_64-linux, so Resources is the parent of that directory), the user data dir,
// then /usr/share/VoLum/VoLumRigs. moduleFile is the .so or the executable, not its folder.
// volumDataDir is $XDG_DATA_HOME/VoLum, or the $HOME/.local/share/VoLum fallback.
inline std::vector<std::filesystem::path> LinuxRigsCandidates(
  const std::filesystem::path& moduleFile, const std::filesystem::path& volumDataDir,
  const std::filesystem::path& systemRigs = "/usr/share/VoLum/VoLumRigs")
{
  namespace fs = std::filesystem;
  std::vector<fs::path> candidates;
  if (!moduleFile.empty())
  {
    const fs::path dir = moduleFile.parent_path();
    candidates.push_back(dir / "VoLumRigs");
    candidates.push_back(dir.parent_path() / "Resources" / "VoLumRigs");
  }
  if (!volumDataDir.empty())
    candidates.push_back(volumDataDir / "VoLumRigs");
  if (!systemRigs.empty())
    candidates.push_back(systemRigs);
  return candidates;
}
#endif

// Bundled rigs candidates: the module (VST3 DLL / .so or standalone executable — not the host
// process) and, on Windows, the installer's registry root. On Linux the order is beside the
// binary, the bundle Resources directory, the XDG VoLum dir, then /usr/share. Then CWD
// ./VoLumRigs or ./rigs (dev uses repo rigs/).
inline std::vector<std::filesystem::path> RigsRootCandidates()
{
  namespace fs = std::filesystem;
  std::vector<fs::path> candidates;

#ifdef _WIN32
  fs::path moduleDir;
  HMODULE hMod = nullptr;
  if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         reinterpret_cast<LPCWSTR>(&RigsRootCandidates), &hMod)
      && hMod != nullptr)
  {
    wchar_t module[MAX_PATH];
    const DWORD n = GetModuleFileNameW(hMod, module, MAX_PATH);
    if (n > 0 && n < MAX_PATH)
      moduleDir = fs::path(module).parent_path();
  }
  candidates = WindowsRigsCandidates(moduleDir, RegistryRigsRootFromInstaller());
#elif defined(__APPLE__)
  {
    fs::path modulePath;
    Dl_info info{};
    if (dladdr(reinterpret_cast<const void*>(&RigsRootCandidates), &info) != 0 && info.dli_fname)
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
#elif defined(__linux__)
  candidates = LinuxRigsCandidates(LinuxModuleFile(), LinuxVoLumDataDir());
#endif

  candidates.push_back(fs::path("VoLumRigs"));
  candidates.push_back(fs::path("rigs"));
  return candidates;
}

// A tree that holds VoLum's own content, not just a folder with the right name. A
// leftover or older install without the pedal captures must not shadow a complete one.
inline bool IsCompleteRigsRoot(const std::filesystem::path& dir)
{
  namespace fs = std::filesystem;
  std::error_code ec;
  if (fs::is_regular_file(dir / "factory-presets.json", ec))
    return true;
  const fs::path pre = dir / "PrePedals";
  if (!fs::is_directory(pre, ec))
    return false;
  for (const auto& entry : fs::directory_iterator(pre, ec))
  {
    if (entry.is_regular_file(ec) && entry.path().extension() == ".nam")
      return true;
  }
  return false;
}

// First complete candidate; else the first existing directory, so a bare tree still
// serves its amps.
inline std::filesystem::path PickRigsRoot(const std::vector<std::filesystem::path>& candidates)
{
  namespace fs = std::filesystem;
  std::error_code ec;
  fs::path firstDir;
  for (const auto& c : candidates)
  {
    const auto norm = c.lexically_normal();
    if (!fs::is_directory(norm, ec))
      continue;
    if (IsCompleteRigsRoot(norm))
      return fs::weakly_canonical(norm, ec);
    if (firstDir.empty())
      firstDir = norm;
  }
  return firstDir.empty() ? fs::path{} : fs::weakly_canonical(firstDir, ec);
}

inline std::filesystem::path FindRigsRootDirectory()
{
  return PickRigsRoot(RigsRootCandidates());
}

// First `dir / leaf` that is a regular file, in candidate order; empty if none.
inline std::filesystem::path FirstRigsFile(const std::vector<std::filesystem::path>& dirs,
                                           const std::filesystem::path& leaf)
{
  namespace fs = std::filesystem;
  std::error_code ec;
  for (const auto& d : dirs)
  {
    const auto file = (d / leaf).lexically_normal();
    if (fs::is_regular_file(file, ec))
      return fs::weakly_canonical(file, ec);
  }
  return {};
}

// Factory presets index PRE captures in the rigs root, so its own file wins. A rigs
// root without one (a 1.2.x install ships none) falls through to the tree beside the
// binary before the registry, so a newer portable or dev build still finds its bank.
inline std::filesystem::path FindFactoryPresetsFile()
{
  auto dirs = RigsRootCandidates();
  if (const auto root = FindRigsRootDirectory(); !root.empty())
    dirs.insert(dirs.begin(), root);
  return FirstRigsFile(dirs, "factory-presets.json");
}

struct ChannelFile
{
  std::string filename;
  std::string label;
};

// filesystem::path::string() is the ANSI code page on Windows and UTF-8 on
// macOS. Channel / pedal discovery must use UTF-8 so a non-ASCII leaf matches
// on both, the same way content::PathToUtf8 does for the library.
inline std::string PathLeafUtf8(const std::filesystem::path& path)
{
#if defined(__cpp_char8_t)
  const std::u8string utf8 = path.filename().u8string();
  return std::string(reinterpret_cast<const char*>(utf8.data()), utf8.size());
#else
  return path.filename().u8string();
#endif
}

inline std::string PathStemUtf8(const std::filesystem::path& path)
{
#if defined(__cpp_char8_t)
  const std::u8string utf8 = path.stem().u8string();
  return std::string(reinterpret_cast<const char*>(utf8.data()), utf8.size());
#else
  return path.stem().u8string();
#endif
}

// Scan an amp folder for .nam files matching a speaker prefix (e.g. "V30"),
// returning sorted list of {filename, channelLabel} pairs.
inline std::vector<ChannelFile> DiscoverChannels(const std::filesystem::path& rigsRoot, const char* ampFolder,
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
    std::string name = PathLeafUtf8(entry.path());
    if (name.size() > 4 && name.compare(name.size() - 4, 4, ".nam") == 0 && name.compare(0, prefix.size(), prefix) == 0)
    {
      auto lastDash = name.rfind('-');
      if (lastDash != std::string::npos && lastDash + 1 < name.size() - 4)
      {
        std::string label = name.substr(lastDash + 1, name.size() - 4 - lastDash - 1);
        for (auto& c : label)
          c = (char)std::toupper((unsigned char)c);
        result.push_back({name, label});
      }
    }
  }

  std::sort(result.begin(), result.end(), [](const ChannelFile& a, const ChannelFile& b) { return a.label < b.label; });

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
#elif defined(__linux__)
  const fs::path dir = LinuxVoLumDataDir();
  if (dir.empty())
    return {};
  return dir / "volum-settings.json";
#else
  (void)0;
  return {};
#endif
}

// Machine-wide update-check throttle and reminder state. Plugins may write this
// sidecar without touching the standalone-owned volum-settings.json.
inline std::filesystem::path VolumUpdateStateFilePath()
{
  namespace fs = std::filesystem;
#ifdef _WIN32
  const char* la = std::getenv("LOCALAPPDATA");
  if (!la || !*la)
    return {};
  return fs::path(la) / "VoLum" / "volum-update-state.json";
#elif defined(__APPLE__)
  const char* home = std::getenv("HOME");
  if (!home || !*home)
    return {};
  return fs::path(home) / "Library" / "Application Support" / "VoLum" / "volum-update-state.json";
#elif defined(__linux__)
  const fs::path dir = LinuxVoLumDataDir();
  if (dir.empty())
    return {};
  return dir / "volum-update-state.json";
#else
  (void)0;
  return {};
#endif
}

// VoLum-owned content store directory (1.2.0 BYO + presets). Sibling of
// volum-settings.json, but unlike that file the content store is read AND
// written by ALL formats (standalone + VST3 + AU) so a plugin instance can
// resolve a preset/project's references to custom amps / IRs / pedals by their
// stable opaque ids. VoLum copies every imported .nam/.wav in here so it never
// depends on the user's original file staying put.
inline std::filesystem::path VolumContentDir()
{
  namespace fs = std::filesystem;
#ifdef _WIN32
  const char* la = std::getenv("LOCALAPPDATA");
  if (!la || !*la)
    return {};
  return fs::path(la) / "VoLum" / "content";
#elif defined(__APPLE__)
  const char* home = std::getenv("HOME");
  if (!home || !*home)
    return {};
  return fs::path(home) / "Library" / "Application Support" / "VoLum" / "content";
#elif defined(__linux__)
  const fs::path dir = LinuxVoLumDataDir();
  if (dir.empty())
    return {};
  return dir / "content";
#else
  (void)0;
  return {};
#endif
}

// Current-build-only settings. Keep new schema extensions out of volum-settings.json so older
// installed VoLum builds can still read the shared settings file safely.
inline std::filesystem::path VolumDualAmpSettingsFilePath()
{
  namespace fs = std::filesystem;
#ifdef _WIN32
  const char* la = std::getenv("LOCALAPPDATA");
  if (!la || !*la)
    return {};
  return fs::path(la) / "VoLum" / "volum-dual-amp-settings.json";
#elif defined(__APPLE__)
  const char* home = std::getenv("HOME");
  if (!home || !*home)
    return {};
  return fs::path(home) / "Library" / "Application Support" / "VoLum" / "volum-dual-amp-settings.json";
#elif defined(__linux__)
  const fs::path dir = LinuxVoLumDataDir();
  if (dir.empty())
    return {};
  return dir / "volum-dual-amp-settings.json";
#else
  (void)0;
  return {};
#endif
}

// Rolling diagnostic log, sibling of volum-settings.json. See VoLumDiagLog.h.
inline std::filesystem::path VolumDiagLogFilePath()
{
  namespace fs = std::filesystem;
#ifdef _WIN32
  const char* la = std::getenv("LOCALAPPDATA");
  if (!la || !*la)
    return {};
  return fs::path(la) / "VoLum" / "volum.log";
#elif defined(__APPLE__)
  const char* home = std::getenv("HOME");
  if (!home || !*home)
    return {};
  return fs::path(home) / "Library" / "Application Support" / "VoLum" / "volum.log";
#elif defined(__linux__)
  const fs::path dir = LinuxVoLumDataDir();
  if (dir.empty())
    return {};
  return dir / "volum.log";
#else
  (void)0;
  return {};
#endif
}

} // namespace volum
