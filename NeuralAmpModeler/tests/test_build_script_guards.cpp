#include "third_party/doctest.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

// Source-guard pins over the Windows test-runner script. Nothing here executes
// PowerShell; these lock in two facts about $LASTEXITCODE that were each got wrong
// once, and that no C++ unit test can observe.
namespace
{
std::string ReadRepoFile(const char* relPath)
{
  const auto root = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
  std::ifstream in(root / relPath, std::ios::binary);
  REQUIRE(in.good());
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}
} // namespace

TEST_CASE("Invoke-Check clears the caller's exit code before running a child script")
{
  // A child .ps1 that succeeds without running a native command never writes
  // $LASTEXITCODE - check-test-source-parity.ps1 prints its OK line and falls off the
  // end - so the value Invoke-Check reads afterwards is whatever the *caller's* shell
  // left behind. Launching the suite from a session whose previous command failed
  // therefore aborted it with that stale code at the first check, before anything was
  // built, and the message blamed the check rather than the shell.
  const std::string src = ReadRepoFile("NeuralAmpModeler/scripts/run-tests-win.ps1");

  const auto fn = src.find("function Invoke-Check");
  REQUIRE(fn != std::string::npos);
  const auto clear = src.find("$global:LASTEXITCODE = 0", fn);
  const auto call = src.find("& $path", fn);
  const auto read = src.find("if ($LASTEXITCODE)", fn);
  REQUIRE(clear != std::string::npos);
  REQUIRE(call != std::string::npos);
  REQUIRE(read != std::string::npos);
  CHECK(clear < call); // cleared before the child runs, not after
  CHECK(call < read);
}

TEST_CASE("The iPlug2 patch step is not routed through Invoke-Check")
{
  // It looks like the one call site that skips the guard, and it has been "fixed" as
  // such once. It must not be: apply-iplug2-patches.ps1 does all its work through
  // System.Diagnostics.Process and reports failure with Write-Error under
  // ErrorActionPreference=Stop. So it never writes $LASTEXITCODE for Invoke-Check to
  // read, while its terminating error already propagates out of the bare call and
  // stops the run. Wrapping it converts a working guard into a spurious failure.
  const std::string src = ReadRepoFile("NeuralAmpModeler/scripts/run-tests-win.ps1");

  const auto patchCall = src.find("apply-iplug2-patches.ps1");
  REQUIRE(patchCall != std::string::npos);

  const auto lineStart = src.rfind('\n', patchCall);
  REQUIRE(lineStart != std::string::npos);
  const std::string line = src.substr(lineStart + 1, patchCall - lineStart - 1);
  CHECK(line.find("Invoke-Check") == std::string::npos);
  CHECK(line.find("& (Join-Path") != std::string::npos);

  // And the reason stays next to it, since the shape is the whole trap.
  CHECK(src.find("Deliberately not Invoke-Check") != std::string::npos);
}

TEST_CASE("Every plugin target that compiles NeuralAmpModeler.cpp also compiles the HTTP get")
{
  // Update-check calls VolumHttpGetString from NeuralAmpModeler.cpp. APP/VST3
  // had the translation unit; AU (and AAX on Windows) did not, so macOS
  // `makedist-mac.sh full all` failed linking AU x86_64 with a missing symbol.
  const std::string pbx = ReadRepoFile("NeuralAmpModeler/projects/NeuralAmpModeler-macOS.xcodeproj/project.pbxproj");
  int macPhases = 0;
  for (size_t pos = 0;;)
  {
    const auto begin = pbx.find("isa = PBXSourcesBuildPhase", pos);
    if (begin == std::string::npos)
      break;
    auto end = pbx.find("isa = PBXSourcesBuildPhase", begin + 1);
    if (end == std::string::npos)
      end = pbx.find("/* End PBXSourcesBuildPhase", begin);
    REQUIRE(end != std::string::npos);
    const std::string block = pbx.substr(begin, end - begin);
    if (block.find("NeuralAmpModeler.cpp in Sources") != std::string::npos)
    {
      ++macPhases;
      CHECK(block.find("VoLumHttpGet.mm in Sources") != std::string::npos);
    }
    pos = begin + 1;
  }
  CHECK(macPhases >= 3);

  const char* windowsProjects[] = {"NeuralAmpModeler/projects/NeuralAmpModeler-app.vcxproj",
                                   "NeuralAmpModeler/projects/NeuralAmpModeler-vst3.vcxproj",
                                   "NeuralAmpModeler/projects/NeuralAmpModeler-aax.vcxproj"};
  for (const char* rel : windowsProjects)
  {
    const std::string proj = ReadRepoFile(rel);
    if (proj.find("NeuralAmpModeler.cpp") == std::string::npos)
      continue;
    CHECK(proj.find("VoLumHttpGet.cpp") != std::string::npos);
  }
}

TEST_CASE("Agent artifact-link check skips gitignored paths")
{
  // Windows CI died on `training/a2-final/` in the A2 skill: the folder is
  // gitignored and gone, but the historical note is still the right pointer.
  // A missing *tracked* path must still fail.
  const std::string src = ReadRepoFile("NeuralAmpModeler/scripts/check-agent-artifact-links.ps1");
  CHECK(src.find("git check-ignore -q -- $normalized") != std::string::npos);
  CHECK(src.find("PSNativeCommandUseErrorActionPreference") != std::string::npos);
}
