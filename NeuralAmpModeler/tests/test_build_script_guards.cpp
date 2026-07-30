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
