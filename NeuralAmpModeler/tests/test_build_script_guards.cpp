#include "third_party/doctest.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

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

TEST_CASE("Tests, app and VST3 share the Release|x64 optimization props")
{
  // The tests were built /O2 without /GL and linked without /LTCG while the app
  // and VST3 shipped with both, so the goldens and the realtime budget checked a
  // differently compiled binary. One sheet keeps the flags in lockstep.
  const std::string props = ReadRepoFile("NeuralAmpModeler/config/VoLum-release-opt.props");
  CHECK(props.find("<WholeProgramOptimization>true</WholeProgramOptimization>") != std::string::npos);
  CHECK(props.find("<LinkTimeCodeGeneration>UseFastLinkTimeCodeGeneration</LinkTimeCodeGeneration>")
        != std::string::npos);

  const char* projects[] = {"NeuralAmpModeler/projects/NeuralAmpModeler-Tests.vcxproj",
                            "NeuralAmpModeler/projects/NeuralAmpModeler-app.vcxproj",
                            "NeuralAmpModeler/projects/NeuralAmpModeler-vst3.vcxproj"};
  for (const char* rel : projects)
  {
    INFO(std::string(rel));
    const std::string proj = ReadRepoFile(rel);
    const auto import = proj.find("config\\VoLum-release-opt.props\"");
    REQUIRE(import != std::string::npos);
    // After Microsoft.Cpp.props, inside the Release|x64 property-sheet group.
    CHECK(proj.find("Microsoft.Cpp.props\"") < import);
    const auto group = proj.rfind("<ImportGroup", import);
    REQUIRE(group != std::string::npos);
    CHECK(proj.find("</ImportGroup>", group) > import);
    const std::string groupTag = proj.substr(group, proj.find('>', group) - group);
    CHECK(groupTag.find("'Release|x64'") != std::string::npos);
    // A project-level override would silently undo the shared sheet.
    CHECK(proj.find("<WholeProgramOptimization>false") == std::string::npos);
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

TEST_CASE("Every packager ships factory-presets.json beside the amp folders")
{
  // The packagers copy rigs/ by *.nam, so a data file at the rigs root is dropped
  // unless named. Without it a release falls back to one default preset per amp and
  // PLAY switches on a :v2 read Invalid; tests run from the repo and never notice.
  const auto count = [](const std::string& src, const std::string& needle) {
    size_t n = 0;
    for (size_t at = src.find(needle); at != std::string::npos; at = src.find(needle, at + 1))
      ++n;
    return n;
  };
  CHECK(count(ReadRepoFile("NeuralAmpModeler/installer/VoLum.iss"), "rigs\\factory-presets.json") == 1);
  CHECK(count(ReadRepoFile("NeuralAmpModeler/scripts/makezip-win.py"), "\"factory-presets.json\"") == 1);
  CHECK(count(ReadRepoFile("NeuralAmpModeler/scripts/package-portable.ps1"), "\\factory-presets.json\"") == 1);
  // App bundle, VST3 install, VST3 zip, AU zip.
  CHECK(count(ReadRepoFile("NeuralAmpModeler/scripts/makedist-mac.sh"), "cp \"$RIGS_SRC/factory-presets.json\"") == 4);
  CHECK(count(ReadRepoFile("NeuralAmpModeler/scripts/makeinstaller-mac.sh"), "cp \"$RIGS_SRC/factory-presets.json\"")
        == 1);

  CHECK(count(ReadRepoFile("NeuralAmpModeler/scripts/verify-packaging-win.ps1"), "factory-presets.json") >= 1);
  CHECK(count(ReadRepoFile("NeuralAmpModeler/scripts/verify-installer-win.ps1"), "factory-presets.json") >= 1);
  CHECK(count(ReadRepoFile("NeuralAmpModeler/scripts/verify-packaging-mac.sh"), "factory-presets.json") >= 3);
}

TEST_CASE("macOS CI keeps the pluginval editor run off the macOS 26 GPU")
{
  // macos-latest is macOS 26. Its paravirtual GPU aborts pluginval while the
  // editor is open, and that editor run is the Mac GUI coverage. The jobs stay
  // on macos-15, and the script must not answer the crash by skipping the GUI.
  const std::string workflow = ReadRepoFile(".github/workflows/ci.yml");
  const auto job = workflow.find("name: macOS (tests + full distribution + plugin validation)");
  REQUIRE(job != std::string::npos);
  const auto next = workflow.find("\n  macos-installer-upgrade:", job);
  REQUIRE(next != std::string::npos);
  const auto body = workflow.substr(job, next - job);
  CHECK(body.find("runs-on: macos-15") != std::string::npos);
  CHECK(body.find("runs-on: macos-latest") == std::string::npos);
  CHECK(body.find("validate-vst3-mac.sh") != std::string::npos);
  CHECK(body.find("checkout-submodules.sh") != std::string::npos);

  const std::string script = ReadRepoFile("NeuralAmpModeler/scripts/validate-vst3-mac.sh");
  CHECK(script.find("--skip-gui-tests") == std::string::npos);
  CHECK(script.find("--validate-in-process") != std::string::npos);

  const std::string retry = ReadRepoFile("NeuralAmpModeler/scripts/checkout-submodules.sh");
  CHECK(retry.find("git submodule update --init --recursive") != std::string::npos);
  CHECK(retry.find("for attempt in") != std::string::npos);
}

TEST_CASE("Metal NanoVG flush never hands the paravirtual GPU an empty command buffer")
{
  // macOS VMs (CI, and users running VoLum in one) abort in
  // AppleParavirtCommandBuffer endCurrentChunk when a command buffer or encoder
  // ends with no work. The flush used to enqueue its buffer before knowing it had
  // a target and return without committing it when no drawable came back.
  const std::string mtl = ReadRepoFile("iPlug2/Dependencies/IGraphics/MetalNanoVG/src/nanovg_mtl.m");
  const auto begin = mtl.find("- (void)renderFlush {");
  REQUIRE(begin != std::string::npos);
  const auto end = mtl.find("- (int)renderGetTextureSizeForImage", begin);
  REQUIRE(end != std::string::npos);
  const std::string flush = mtl.substr(begin, end - begin);

  CHECK(flush.find("[commandBuffer enqueue]") == std::string::npos);
  CHECK(flush.find("_buffers->ncalls == 0 && !_clearBufferOnFlush") != std::string::npos);
  const auto drawable = flush.find("_metalLayer.nextDrawable");
  const auto create = flush.find("[_commandQueue commandBuffer]");
  REQUIRE(drawable != std::string::npos);
  REQUIRE(create != std::string::npos);
  CHECK(drawable < create);
  // Every early return before the commit gives the frame slot back.
  const auto commit = flush.find("[commandBuffer commit]");
  REQUIRE(commit != std::string::npos);
  size_t returns = 0, cancels = 0;
  for (auto p = flush.find("return;"); p != std::string::npos && p < commit; p = flush.find("return;", p + 1))
    ++returns;
  for (auto p = flush.find("[self renderCancel];"); p != std::string::npos && p < commit;
       p = flush.find("[self renderCancel];", p + 1))
    ++cancels;
  CHECK(returns > 0);
  CHECK(returns == cancels);
  // The per-frame sync was an empty blit on unified memory; readback syncs only
  // where a discrete GPU needs it.
  CHECK(flush.find("synchronizeResource") == std::string::npos);
  const auto read = mtl.find("void mnvgReadPixels(");
  REQUIRE(read != std::string::npos);
  const std::string readBody = mtl.substr(read, mtl.find("enum MNVGTarget mnvgTarget()", read) - read);
  CHECK(readBody.find("!mtl.metalLayer.device.hasUnifiedMemory") != std::string::npos);
  CHECK(readBody.find("synchronizeResource") != std::string::npos);

  const auto cancel = mtl.find("- (void)renderCancel {");
  REQUIRE(cancel != std::string::npos);
  CHECK(mtl.substr(cancel, 200).find("_clearBufferOnFlush = NO;") != std::string::npos);
}

TEST_CASE("Windows pluginval waits for the verdict instead of trusting the step exit")
{
  // pluginval.exe is a GUI-subsystem binary: run with & PowerShell does not
  // wait, and CI reported success with the log cut off at the Editor test.
  const std::string script = ReadRepoFile("NeuralAmpModeler/scripts/validate-vst3-win.ps1");
  CHECK(script.find("& $pluginval.FullName") == std::string::npos);
  CHECK(script.find("Start-Process -FilePath $pluginval.FullName") != std::string::npos);
  CHECK(script.find("-Wait -PassThru") != std::string::npos);
  CHECK(script.find("Completed tests in pluginval / Editor Automation") != std::string::npos);
  CHECK(script.find("^SUCCESS") != std::string::npos);
  CHECK(script.find("--validate-in-process") != std::string::npos);
  CHECK(script.find("--skip-gui-tests") == std::string::npos);

  // The runner has no OpenGL 2 driver; the GL2 editor needs Mesa first.
  const std::string workflow = ReadRepoFile(".github/workflows/ci.yml");
  const auto job = workflow.find("name: Windows (tests + installer + portable zip + plugin validation)");
  REQUIRE(job != std::string::npos);
  const auto mesa = workflow.find("ssciwr/setup-mesa-dist-win", job);
  const auto validate = workflow.find("validate-vst3-win.ps1", job);
  REQUIRE(validate != std::string::npos);
  CHECK(mesa < validate);
}

TEST_CASE("clang-format CI checks a dispatched branch from its merge base with dev")
{
  // A workflow_dispatch has no github.event.before. Falling back to HEAD^ checked
  // only the last commit, so a feature branch went green with unformatted files
  // and dev's push run failed after the merge.
  const std::string workflow = ReadRepoFile(".github/workflows/ci.yml");
  const auto step = workflow.find("name: Check clang-format output");
  REQUIRE(step != std::string::npos);
  const auto end = workflow.find("shell: bash", step);
  REQUIRE(end != std::string::npos);
  const std::string body = workflow.substr(step, end - step);
  const auto fallback = body.find("0000000000000000000000000000000000000000");
  const auto mergeBase = body.find("git merge-base origin/dev HEAD");
  const auto headParent = body.find("BASE=\"HEAD^\"");
  REQUIRE(fallback != std::string::npos);
  CHECK(mergeBase != std::string::npos);
  CHECK(mergeBase > fallback);
  CHECK(headParent > mergeBase);

  const auto job = workflow.find("name: Formatting");
  REQUIRE(job != std::string::npos);
  CHECK(workflow.substr(job, step - job).find("fetch-depth: 0") != std::string::npos);
}

TEST_CASE("Linux plug-in hides static libstdc++ and the gate rejects an exported copy")
{
  // Facet ids in libstdc++.a are STB_GNU_UNIQUE. Exporting them makes a host
  // locale bind the plug-in's num_put to the host codecvt, which crashes Carla.
  // The stdio forks in the shared headers were a workaround; the link line is
  // the constraint, and the headers stay on the shared ostream path.
  const std::string cmake = ReadRepoFile("NeuralAmpModeler/projects/linux/CMakeLists.txt");
  const auto vst3 = cmake.find("add_library(VoLum-vst3 SHARED");
  const auto app = cmake.find("add_executable(VoLum-app");
  REQUIRE(vst3 != std::string::npos);
  REQUIRE(app != std::string::npos);
  const std::string vst3Body = cmake.substr(vst3, app - vst3);
  const std::string appBody = cmake.substr(app);
  CHECK(vst3Body.find("-Wl,--exclude-libs,ALL") != std::string::npos);
  CHECK(vst3Body.find("volum-vst3.version") != std::string::npos);
  CHECK(appBody.find("-Wl,--exclude-libs,ALL") != std::string::npos);
  CHECK(appBody.find("volum-app.version") != std::string::npos);
  CHECK(cmake.find("if(NOT VOLUM_SANITIZE)") != std::string::npos);

  const std::string version = ReadRepoFile("NeuralAmpModeler/projects/linux/volum-vst3.version");
  CHECK(version.find("GetPluginFactory") != std::string::npos);
  CHECK(version.find("ModuleEntry") != std::string::npos);
  CHECK(version.find("ModuleExit") != std::string::npos);
  CHECK(version.find("_ZN5iplug23IPlugLinuxCheckUIThreadEPKc") == std::string::npos);
  CHECK(version.find("local:") != std::string::npos);
  CHECK(cmake.find("VOLUM_UI_THREAD_PROBE") != std::string::npos);

  const std::string probe = ReadRepoFile("NeuralAmpModeler/projects/linux/volum-vst3-probe.version");
  CHECK(probe.find("_ZN5iplug23IPlugLinuxCheckUIThreadEPKc") != std::string::npos);
  CHECK(probe.find("GetPluginFactory") != std::string::npos);

  const std::string gate = ReadRepoFile("NeuralAmpModeler/scripts/validate-vst3-linux.sh");
  CHECK(gate.find("check-stdlib-exports.sh") != std::string::npos);
  CHECK(gate.find("ui-thread-probe.sh") != std::string::npos);

  const std::string diag = ReadRepoFile("NeuralAmpModeler/VoLumDiagLog.h");
  const std::string io = ReadRepoFile("NeuralAmpModeler/VoLumSettingsFileIO.h");
  CHECK(diag.find("snprintf") == std::string::npos);
  CHECK(io.find("snprintf") == std::string::npos);
}

TEST_CASE("release workflow builds the Mac assets on the editor-test runner")
{
  // The build steps are gated on matrix.os. Pinning the runner to macos-15
  // without the same gate skips the Mac build and still reports success.
  const std::string workflow = ReadRepoFile(".github/workflows/release-build.yml");
  CHECK(workflow.find("matrix.os == 'macos-15'") != std::string::npos);
  CHECK(workflow.find("matrix.os == 'macos-latest'") == std::string::npos);
  CHECK(workflow.find("rebuild_assets") != std::string::npos);
  CHECK(workflow.find("[{\"os\":\"macos-15\",\"build_dir\":\"build-mac\"}]") != std::string::npos);
}

namespace
{
std::string TrimCopy(std::string s)
{
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
    s.erase(s.begin());
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
    s.pop_back();
  return s;
}

bool IsStepStart(const std::string& line)
{
  std::size_t i = 0;
  while (i < line.size() && line[i] == ' ')
    ++i;
  return line.compare(i, 7, "- name:") == 0;
}

// A release step publishes when its script creates a release, pushes a ref,
// or writes the update feed. Comments do not count.
bool LinePublishes(const std::string& raw)
{
  const std::string trimmed = TrimCopy(raw);
  if (trimmed.empty() || trimmed[0] == '#')
    return false;
  std::string line = raw;
  const auto comment = line.find(" #");
  if (comment != std::string::npos)
    line.resize(comment);
  const auto has = [&](const char* needle) { return line.find(needle) != std::string::npos; };
  if (has("gh release") || has("git push") || has("git tag") || has("gh api"))
    return true;
  if (has("appcast") || has("update-feed") || has("update_feed"))
    return true;
  if (!has("curl"))
    return false;
  return has("-X POST") || has("-X PUT") || has("-X PATCH") || has("-X DELETE") || has("--data") || has(" -d ") || has("-d @");
}

bool StepIsPublishGated(const std::string& body)
{
  return body.find("inputs.dry_run != true") != std::string::npos || body.find("publish == 'true'") != std::string::npos
         || body.find("publish == true") != std::string::npos;
}
} // namespace

int CountGatedPublishSteps(const std::string& workflow)
{
  std::vector<std::string> lines;
  std::string cur;
  for (char ch : workflow)
  {
    if (ch == '\n')
    {
      if (!cur.empty() && cur.back() == '\r')
        cur.pop_back();
      lines.push_back(std::move(cur));
      cur.clear();
    }
    else
      cur.push_back(ch);
  }
  if (!cur.empty())
    lines.push_back(std::move(cur));

  int publishSteps = 0;
  std::string stepName;
  std::string stepBody;
  const auto finish = [&]() {
    if (stepName.empty())
      return;
    bool publishes = false;
    std::size_t pos = 0;
    while (pos < stepBody.size())
    {
      const auto nl = stepBody.find('\n', pos);
      const auto line = stepBody.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
      if (LinePublishes(line))
        publishes = true;
      if (nl == std::string::npos)
        break;
      pos = nl + 1;
    }
    if (publishes)
    {
      ++publishSteps;
      INFO("step " << stepName);
      CHECK(StepIsPublishGated(stepBody));
    }
    stepName.clear();
    stepBody.clear();
  };
  for (const auto& line : lines)
  {
    if (IsStepStart(line))
    {
      finish();
      const auto nameAt = line.find("- name:");
      stepName = TrimCopy(line.substr(nameAt + 7));
      stepBody = line + "\n";
    }
    else if (!stepName.empty())
      stepBody += line + "\n";
  }
  finish();
  return publishSteps;
}

TEST_CASE("release publish steps stay closed unless the dry run is off")
{
  // A dry run used to POST a real create-release call and hope the token was
  // refused. The gate is the step condition. GitHub Actions rejects inputs in
  // permissions, so the dry run is its own Build caller with contents: read
  // and the shipping Build caller is contents: write. Both run release-build.yml.
  const std::string native = ReadRepoFile(".github/workflows/release-native.yml");
  const std::string build = ReadRepoFile(".github/workflows/release-build.yml");
  CHECK(native.find("linux_dry_run") == std::string::npos);
  CHECK(native.find("-X POST") == std::string::npos);
  CHECK(build.find("-X POST") == std::string::npos);
  CHECK(native.find("github.ref != 'refs/heads/main'") == std::string::npos);
  CHECK(native.find("contents: read") != std::string::npos);
  const auto dryBuild = native.find("if: inputs.dry_run == true\n    permissions:\n      contents: read");
  CHECK(dryBuild != std::string::npos);

  const int nativeSteps = CountGatedPublishSteps(native);
  const int buildSteps = CountGatedPublishSteps(build);
  CHECK(nativeSteps >= 3);
  CHECK(buildSteps >= 3);
}
