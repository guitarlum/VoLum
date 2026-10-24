#include "third_party/doctest.h"

#ifdef __linux__

  #include "../../iPlug2/IGraphics/Platforms/IGraphicsLinuxDialogs.cpp"
  #include "../../iPlug2/IGraphics/Platforms/IGraphicsLinuxClipboard.cpp"

  #include <X11/Xlib.h>

  #include <chrono>
  #include <csignal>
  #include <cstdio>
  #include <cstdlib>
  #include <filesystem>
  #include <fstream>
  #include <string>
  #include <sys/stat.h>
  #include <sys/wait.h>
  #include <unistd.h>
  #include <vector>

using iplug::IPlugLinuxAddRunLoop;
using iplug::IPlugLinuxPollRunLoop;
using iplug::IPlugLinuxRemoveRunLoop;
using iplug::igraphics::linux_clipboard::GetText;
using iplug::igraphics::linux_clipboard::kWaitMs;
using iplug::igraphics::linux_clipboard::SetText;
using iplug::igraphics::linux_dialogs::Kind;
using iplug::igraphics::linux_dialogs::Launch;
using iplug::igraphics::linux_dialogs::Request;
using iplug::igraphics::linux_dialogs::Result;
using iplug::igraphics::linux_dialogs::SetPathOverride;
using iplug::igraphics::linux_dialogs::Start;

namespace
{

const char* kToolScript = R"sh(#!/bin/sh
printf 'BEGIN %s\n' "$(basename "$0")" >> "$DIALOG_LOG"
for a in "$@"; do
  printf 'ARG %s\n' "$a" >> "$DIALOG_LOG"
done
printf 'END\n' >> "$DIALOG_LOG"
if [ -n "$FAKE_SLEEP" ]; then
  sleep "$FAKE_SLEEP"
fi
if [ "$FAKE_EXIT" = "1" ]; then
  exit 1
fi
if [ -n "$FAKE_OUT" ]; then
  printf '%s\n' "$FAKE_OUT"
fi
exit 0
)sh";

const char* kGdbusScript = R"sh(#!/bin/sh
printf 'BEGIN gdbus\n' >> "$DIALOG_LOG"
for a in "$@"; do
  printf 'ARG %s\n' "$a" >> "$DIALOG_LOG"
done
printf 'END\n' >> "$DIALOG_LOG"
monitor=0
for a in "$@"; do
  if [ "$a" = "monitor" ]; then
    monitor=1
  fi
done
if [ "$monitor" = "1" ]; then
  if [ -n "$PORTAL_MONITOR_LINE" ]; then
    printf '%s\n' "$PORTAL_MONITOR_LINE"
  fi
  if [ "$PORTAL_MONITOR_EXIT" = "1" ]; then
    exit 1
  fi
  exec sleep 30
fi
if [ "$PORTAL_CALL_EXIT" = "1" ]; then
  exit 1
fi
if [ -n "$PORTAL_CALL_LINE" ]; then
  printf '%s\n' "$PORTAL_CALL_LINE"
fi
exit 0
)sh";

void WriteExe(const std::string& dir, const char* name, const char* body)
{
  const std::string path = dir + "/" + name;
  std::ofstream out(path, std::ios::binary);
  out << body;
  out.close();
  chmod(path.c_str(), 0755);
}

void SetOrClear(const char* name, const char* value)
{
  if (value)
    setenv(name, value, 1);
  else
    unsetenv(name);
}

struct DialogEnv
{
  std::string dir;
  std::string log;
  IPlugLinuxPollRunLoop* loop = nullptr;

  DialogEnv()
  {
    dir = (std::filesystem::temp_directory_path()
           / ("volum-dlg-" + std::to_string(static_cast<long long>(getpid())) + "-"
              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())))
            .string();
    std::filesystem::create_directories(dir);
    log = dir + "/log";
    setenv("DIALOG_LOG", log.c_str(), 1);
    SetPathOverride(dir.c_str());
    loop = new IPlugLinuxPollRunLoop;
    IPlugLinuxAddRunLoop(loop);
  }

  ~DialogEnv()
  {
    iplug::igraphics::linux_dialogs::CancelOwner(this);
    if (loop)
    {
      // Reap timers are 100 ms. Drain them before the loop is popped, or the
      // next test sees a leftover binding on the process-wide registry.
      for (int i = 0; i < 8; ++i)
        loop->RunOnce(30);
      IPlugLinuxRemoveRunLoop(loop);
      delete loop;
    }
    SetPathOverride(nullptr);
    unsetenv("DIALOG_LOG");
    unsetenv("FAKE_SLEEP");
    unsetenv("FAKE_EXIT");
    unsetenv("FAKE_OUT");
    unsetenv("PORTAL_CALL_LINE");
    unsetenv("PORTAL_CALL_EXIT");
    unsetenv("PORTAL_MONITOR_LINE");
    unsetenv("PORTAL_MONITOR_EXIT");
    std::error_code ignore;
    std::filesystem::remove_all(dir, ignore);
  }

  void Tool(const char* name) { WriteExe(dir, name, kToolScript); }
  void Gdbus() { WriteExe(dir, "gdbus", kGdbusScript); }

  void Pump(const bool& done, int budgetMs = 3000)
  {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(budgetMs);
    while (!done && std::chrono::steady_clock::now() < deadline)
      loop->RunOnce(30);
  }

  std::string Log() const
  {
    std::ifstream in(log);
    return std::string(std::istreambuf_iterator<char>(in), {});
  }
};

const char* kPortalOk = "(objectpath '/org/freedesktop/portal/desktop/request/t/x',)";
const char* kPortalFile =
  "/org/freedesktop/portal/desktop/request/t/x: org.freedesktop.portal.Request.Response (uint32 0, {'uris': "
  "<['file:///tmp/a%20b%27%20%E2%9C%93.nam']>})";
const char* kPortalCancel =
  "/org/freedesktop/portal/desktop/request/t/x: org.freedesktop.portal.Request.Response (uint32 1, {'uris': <[]>})";

Request OpenRequest()
{
  Request request;
  request.kind = Kind::OpenFile;
  request.extensions = "nam";
  request.path = "/tmp";
  return request;
}

} // namespace

TEST_CASE("Linux file dialog prefers the portal and delivers the path asynchronously")
{
  DialogEnv env;
  env.Gdbus();
  env.Tool("zenity");
  SetOrClear("PORTAL_CALL_LINE", kPortalOk);
  SetOrClear("PORTAL_MONITOR_LINE", kPortalFile);

  bool called = false;
  Result got;
  const auto started = std::chrono::steady_clock::now();
  Start(
    OpenRequest(),
    [&](const Result& result) {
      called = true;
      got = result;
    },
    &env);
  const auto elapsed =
    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
  CHECK(elapsed < 200);
  CHECK_FALSE(called);
  env.Pump(called);

  REQUIRE(called);
  CHECK(got.accepted);
  CHECK(got.tool == "portal");
  REQUIRE(got.paths.size() == 1);
  CHECK(got.paths[0] == std::string("/tmp/a b' \xE2\x9C\x93.nam"));
  const std::string log = env.Log();
  CHECK(log.find("BEGIN gdbus") != std::string::npos);
  CHECK(log.find("BEGIN zenity") == std::string::npos);
}

TEST_CASE("Linux file dialog treats a portal cancel as final")
{
  DialogEnv env;
  env.Gdbus();
  env.Tool("zenity");
  SetOrClear("PORTAL_CALL_LINE", kPortalOk);
  SetOrClear("PORTAL_MONITOR_LINE", kPortalCancel);

  bool called = false;
  Result got;
  Start(
    OpenRequest(),
    [&](const Result& result) {
      called = true;
      got = result;
    },
    &env);
  CHECK_FALSE(called);
  env.Pump(called);
  REQUIRE(called);
  CHECK_FALSE(got.accepted);
  CHECK(got.tool == "portal");
  CHECK(env.Log().find("BEGIN zenity") == std::string::npos);
}

TEST_CASE("Linux file dialog falls through the chain when a tool is missing or the portal is down")
{
  DialogEnv down;
  down.Gdbus();
  down.Tool("zenity");
  down.Tool("kdialog");
  SetOrClear("PORTAL_CALL_EXIT", "1");
  SetOrClear("FAKE_OUT", "/tmp/from zenity.nam");

  bool called = false;
  Result got;
  Start(
    OpenRequest(),
    [&](const Result& result) {
      called = true;
      got = result;
    },
    &down);
  down.Pump(called);
  REQUIRE(called);
  CHECK(got.tool == "zenity");
  REQUIRE(got.paths.size() == 1);
  CHECK(got.paths[0] == "/tmp/from zenity.nam");
  const std::string log = down.Log();
  const auto gdbus = log.find("BEGIN gdbus");
  const auto zenity = log.find("BEGIN zenity");
  REQUIRE(gdbus != std::string::npos);
  REQUIRE(zenity != std::string::npos);
  CHECK(gdbus < zenity);
  CHECK(log.find("BEGIN kdialog") == std::string::npos);

  DialogEnv missing;
  missing.Tool("kdialog");
  SetOrClear("FAKE_OUT", "/tmp/from kdialog.nam");
  called = false;
  Start(
    OpenRequest(),
    [&](const Result& result) {
      called = true;
      got = result;
    },
    &missing);
  missing.Pump(called);
  REQUIRE(called);
  CHECK(got.tool == "kdialog");
  CHECK(got.paths[0] == "/tmp/from kdialog.nam");
  CHECK(missing.Log().find("BEGIN gdbus") == std::string::npos);
  CHECK(missing.Log().find("BEGIN zenity") == std::string::npos);
}

TEST_CASE("Linux file dialog does not open kdialog after the user cancels zenity")
{
  DialogEnv env;
  env.Tool("zenity");
  env.Tool("kdialog");
  SetOrClear("FAKE_EXIT", "1");

  bool called = false;
  Result got;
  Start(
    OpenRequest(),
    [&](const Result& result) {
      called = true;
      got = result;
    },
    &env);
  env.Pump(called);
  REQUIRE(called);
  CHECK_FALSE(got.accepted);
  CHECK(got.tool == "zenity");
  CHECK(env.Log().find("BEGIN kdialog") == std::string::npos);
}

TEST_CASE("Linux file dialog reports nothing when every tool is missing, still asynchronously")
{
  DialogEnv env;
  bool called = false;
  Result got;
  Start(
    OpenRequest(),
    [&](const Result& result) {
      called = true;
      got = result;
    },
    &env);
  CHECK_FALSE(called);
  env.Pump(called);
  REQUIRE(called);
  CHECK_FALSE(got.accepted);
  CHECK(got.paths.empty());
  CHECK(got.tool.empty());
}

TEST_CASE("Linux file dialog passes a path with spaces and shell metacharacters as one argument")
{
  DialogEnv env;
  env.Tool("zenity");
  const std::string name = "a b;$(reboot)'\".txt";
  SetOrClear("FAKE_OUT", "/tmp/out.nam");
  Request request;
  request.kind = Kind::SaveFile;
  request.path = "/tmp/My Folder";
  request.fileName = name;

  bool called = false;
  Start(request, [&](const Result&) { called = true; }, &env);
  env.Pump(called);
  REQUIRE(called);
  const std::string log = env.Log();
  CHECK(log.find("ARG --filename=/tmp/My Folder/" + name) != std::string::npos);
  CHECK(log.find("ARG --save") != std::string::npos);
}

TEST_CASE("Linux file dialog returns several paths and a directory with a trailing slash")
{
  DialogEnv files;
  files.Tool("zenity");
  SetOrClear("FAKE_OUT", "/tmp/one.nam\n/tmp/two files.wav");
  Request request;
  request.kind = Kind::OpenFiles;
  request.extensions = "nam wav";
  bool called = false;
  Result got;
  Start(
    request,
    [&](const Result& result) {
      called = true;
      got = result;
    },
    &files);
  files.Pump(called);
  REQUIRE(called);
  REQUIRE(got.paths.size() == 2);
  CHECK(got.paths[0] == "/tmp/one.nam");
  CHECK(got.paths[1] == "/tmp/two files.wav");
  CHECK(files.Log().find("ARG --multiple") != std::string::npos);

  DialogEnv dir;
  dir.Tool("zenity");
  SetOrClear("FAKE_OUT", "/tmp/My Folder");
  request = Request();
  request.kind = Kind::Directory;
  called = false;
  Start(
    request,
    [&](const Result& result) {
      called = true;
      got = result;
    },
    &dir);
  dir.Pump(called);
  REQUIRE(called);
  REQUIRE(got.paths.size() == 1);
  CHECK(got.paths[0] == "/tmp/My Folder/");
  CHECK(dir.Log().find("ARG --directory") != std::string::npos);
}

TEST_CASE("Linux message box returns before zenity exits")
{
  DialogEnv env;
  env.Tool("zenity");
  SetOrClear("FAKE_SLEEP", "1");
  SetOrClear("FAKE_EXIT", nullptr);
  SetOrClear("FAKE_OUT", nullptr);
  Request request;
  request.kind = Kind::MessageBox;
  request.messageType = 0;
  request.message = "Saved.";
  request.title = "VoLum";

  bool called = false;
  Result got;
  const auto started = std::chrono::steady_clock::now();
  Start(
    request,
    [&](const Result& result) {
      called = true;
      got = result;
    },
    &env);
  const auto elapsed =
    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
  CHECK(elapsed < 200);
  CHECK_FALSE(called);
  env.Pump(called, 4000);
  REQUIRE(called);
  CHECK(got.messageResult == 1);
  CHECK(got.tool == "zenity");
  CHECK(env.Log().find("ARG --info") != std::string::npos);
}

TEST_CASE("Linux OpenURL spawns xdg-open without a shell")
{
  DialogEnv env;
  env.Tool("xdg-open");
  const std::string url = "https://example.com/a b?q=1";
  CHECK(Launch(url));
  bool seen = false;
  for (int i = 0; i < 50 && !seen; ++i)
  {
    env.loop->RunOnce(20);
    seen = env.Log().find("ARG " + url) != std::string::npos;
  }
  CHECK(seen);
}

TEST_CASE("Linux clipboard wait is capped at 100 ms")
{
  CHECK(kWaitMs == 100);
  #ifdef VOLUM_TEST_SANITIZERS
  return;
  #endif

  char displayName[16] = {};
  pid_t xvfb = -1;
  for (int n = 61; n < 64 && xvfb < 0; ++n)
  {
    std::snprintf(displayName, sizeof(displayName), ":%d", n);
    const std::string socket = std::string("/tmp/.X11-unix/X") + std::to_string(n);
    if (std::filesystem::exists(socket))
      continue;
    xvfb = fork();
    if (xvfb == 0)
    {
      execlp(
        "Xvfb", "Xvfb", displayName, "-screen", "0", "640x480x24", "-nolisten", "tcp", static_cast<char*>(nullptr));
      _exit(127);
    }
    if (xvfb < 0)
      break;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!std::filesystem::exists(socket) && std::chrono::steady_clock::now() < deadline)
      usleep(20000);
    if (!std::filesystem::exists(socket))
    {
      kill(xvfb, SIGTERM);
      waitpid(xvfb, nullptr, 0);
      xvfb = -1;
    }
  }
  if (xvfb < 0)
  {
    MESSAGE("Xvfb did not start; the 100 ms wait was not timed");
    return;
  }

  XSetErrorHandler([](Display*, XErrorEvent*) { return 0; });
  Display* owner = XOpenDisplay(displayName);
  Display* reader = XOpenDisplay(displayName);
  REQUIRE(owner);
  REQUIRE(reader);
  const int screen = DefaultScreen(owner);
  const Window ownerWindow = XCreateSimpleWindow(owner, RootWindow(owner, screen), 0, 0, 8, 8, 0, 0, 0);
  const Window readerWindow =
    XCreateSimpleWindow(reader, RootWindow(reader, DefaultScreen(reader)), 0, 0, 8, 8, 0, 0, 0);
  const Atom clipboard = XInternAtom(owner, "CLIPBOARD", False);
  XSetSelectionOwner(owner, clipboard, ownerWindow, CurrentTime);
  XFlush(owner);
  REQUIRE(XGetSelectionOwner(owner, clipboard) == ownerWindow);

  std::string text;
  const auto started = std::chrono::steady_clock::now();
  const bool got = GetText(reinterpret_cast<_XDisplay*>(reader), readerWindow, text);
  const auto elapsed =
    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
  CHECK_FALSE(got);
  CHECK(elapsed >= 50);
  CHECK(elapsed < 250);

  CHECK(SetText(reinterpret_cast<_XDisplay*>(reader), readerWindow, "hello café"));
  std::string back;
  const auto cached = std::chrono::steady_clock::now();
  CHECK(GetText(reinterpret_cast<_XDisplay*>(reader), readerWindow, back));
  const auto cachedMs =
    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - cached).count();
  CHECK(back == "hello café");
  CHECK(cachedMs < 50);

  XCloseDisplay(reader);
  XCloseDisplay(owner);
  kill(xvfb, SIGTERM);
  waitpid(xvfb, nullptr, 0);
}

#endif
