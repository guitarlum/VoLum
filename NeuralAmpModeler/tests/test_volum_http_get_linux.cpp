#ifdef __linux__
  #ifndef _GNU_SOURCE
    #define _GNU_SOURCE
  #endif
#endif
#include "third_party/doctest.h"

#ifdef __linux__

  #include "VoLumHttpGet.h"
  #include "VoLumTestTempDir.h"

  #include <arpa/inet.h>
  #include <chrono>
  #include <cstdint>
  #include <cstdio>
  #include <cstdlib>
  #include <cstring>
  #include <fcntl.h>
  #include <fstream>
  #include <iterator>
  #include <netinet/in.h>
  #include <signal.h>
  #include <spawn.h>
  #include <sstream>
  #include <string>
  #include <sys/socket.h>
  #include <sys/wait.h>
  #include <unistd.h>
  #include <vector>

extern char** environ;

namespace
{

class EnvGuard
{
public:
  explicit EnvGuard(const char* name)
  : mName(name)
  {
    if (const char* value = std::getenv(name))
    {
      mHad = true;
      mValue = value;
    }
  }

  ~EnvGuard()
  {
    if (mHad)
      ::setenv(mName, mValue.c_str(), 1);
    else
      ::unsetenv(mName);
  }

  void Set(const std::string& value) { ::setenv(mName, value.c_str(), 1); }

private:
  const char* mName;
  bool mHad = false;
  std::string mValue;
};

struct LocalNet
{
  EnvGuard noProxy{"NO_PROXY"};
  EnvGuard noProxyLower{"no_proxy"};
  LocalNet()
  {
    noProxy.Set("127.0.0.1,localhost");
    noProxyLower.Set("127.0.0.1,localhost");
  }
};

class Child
{
public:
  pid_t pid = -1;

  ~Child() { Stop(); }

  void Stop()
  {
    if (pid <= 0)
      return;
    ::kill(pid, SIGTERM);
    for (int i = 0; i < 50; ++i)
    {
      const pid_t waited = ::waitpid(pid, nullptr, WNOHANG);
      if (waited == pid || (waited < 0 && errno != EINTR))
      {
        pid = -1;
        return;
      }
      ::usleep(20000);
    }
    ::kill(pid, SIGKILL);
    while (::waitpid(pid, nullptr, 0) < 0 && errno == EINTR) {}
    pid = -1;
  }
};

bool OnPath(const char* name)
{
  const char* path = std::getenv("PATH");
  if (!path)
    return false;
  std::stringstream parts(path);
  std::string dir;
  while (std::getline(parts, dir, ':'))
  {
    if (!dir.empty() && ::access((dir + "/" + name).c_str(), X_OK) == 0)
      return true;
  }
  return false;
}

void WriteText(const std::filesystem::path& path, const std::string& text)
{
  std::ofstream out(path.string(), std::ios::binary);
  out << text;
}

std::string ReadText(const std::filesystem::path& path)
{
  std::ifstream in(path.string(), std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in), {});
}

pid_t Spawn(const std::vector<std::string>& args, const std::filesystem::path& log)
{
  std::vector<char*> argv;
  argv.reserve(args.size() + 1);
  for (const auto& arg : args)
    argv.push_back(const_cast<char*>(arg.c_str()));
  argv.push_back(nullptr);

  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);
  const int logFd = ::open(log.string().c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (logFd >= 0)
  {
    posix_spawn_file_actions_adddup2(&actions, logFd, STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&actions, logFd, STDERR_FILENO);
    posix_spawn_file_actions_addclose(&actions, logFd);
  }
  pid_t pid = -1;
  const int rc = ::posix_spawnp(&pid, argv[0], &actions, nullptr, argv.data(), environ);
  if (logFd >= 0)
    ::close(logFd);
  posix_spawn_file_actions_destroy(&actions);
  if (rc != 0)
    return -1;
  return pid;
}

int ReserveLoopbackPort()
{
  const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0)
    return -1;
  int reuse = 1;
  ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;
  if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
  {
    ::close(fd);
    return -1;
  }
  socklen_t len = sizeof(addr);
  if (::getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &len) != 0)
  {
    ::close(fd);
    return -1;
  }
  const int port = ntohs(addr.sin_port);
  ::close(fd);
  return port;
}

bool ProbeHttp(int port, const std::string& resource, int expectStatus)
{
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  const std::string needle10 = "HTTP/1.0 " + std::to_string(expectStatus);
  const std::string needle11 = "HTTP/1.1 " + std::to_string(expectStatus);
  while (std::chrono::steady_clock::now() < deadline)
  {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
      return false;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
    {
      ::close(fd);
      ::usleep(50000);
      continue;
    }
    const std::string req = "GET " + resource + " HTTP/1.0\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
    if (::send(fd, req.data(), req.size(), 0) < 0)
    {
      ::close(fd);
      ::usleep(50000);
      continue;
    }
    std::string resp;
    char buf[512];
    for (;;)
    {
      const ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
      if (n <= 0)
        break;
      resp.append(buf, static_cast<std::size_t>(n));
      if (resp.size() > 8192)
        break;
    }
    ::close(fd);
    if (resp.find(needle10) != std::string::npos || resp.find(needle11) != std::string::npos)
      return true;
    ::usleep(50000);
  }
  return false;
}

bool WaitForConnect(int port)
{
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (std::chrono::steady_clock::now() < deadline)
  {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
      return false;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    const int rc = ::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    ::close(fd);
    if (rc == 0)
      return true;
    ::usleep(50000);
  }
  return false;
}

bool Start(Child& child, int& port, const std::filesystem::path& log, const std::vector<std::string>& argsBeforePort,
           const std::vector<std::string>& argsAfterPort, bool httpProbe)
{
  for (int attempt = 0; attempt < 5; ++attempt)
  {
    port = ReserveLoopbackPort();
    if (port <= 0)
      continue;
    std::vector<std::string> args = argsBeforePort;
    args.push_back(std::to_string(port));
    args.insert(args.end(), argsAfterPort.begin(), argsAfterPort.end());
    child.Stop();
    child.pid = Spawn(args, log);
    if (child.pid <= 0)
      continue;
    const bool up = httpProbe ? ProbeHttp(port, "/hello.txt", 200) : WaitForConnect(port);
    if (up)
      return true;
    child.Stop();
  }
  return false;
}

} // namespace

TEST_CASE("Linux HTTP get rejects a non-http URL")
{
  std::string body = "stale";
  CHECK_FALSE(VolumHttpGetString(nullptr, body, 1000));
  CHECK(body.empty());
  body = "stale";
  CHECK_FALSE(VolumHttpGetString("", body, 1000));
  CHECK(body.empty());
  body = "stale";
  CHECK_FALSE(VolumHttpGetString("file:///etc/passwd", body, 1000));
  CHECK(body.empty());
  body = "stale";
  CHECK_FALSE(VolumHttpGetString("ftp://127.0.0.1/x", body, 1000));
  CHECK(body.empty());
}

TEST_CASE("Linux HTTP get reads a local python3 http.server body")
{
  if (!OnPath("python3"))
  {
    MESSAGE("python3 is missing; skipping Linux HTTP get");
    return;
  }

  namespace fs = std::filesystem;
  LocalNet net;
  const fs::path dir = volum_test::ProcessTempRoot() / "volum_linux_http_server";
  fs::remove_all(dir);
  fs::create_directories(dir);
  const std::string hello = "volum-linux-ok";
  WriteText(dir / "hello.txt", hello);
  WriteText(dir / "exact.bin", std::string(1024 * 1024, 'e'));
  WriteText(dir / "over.bin", std::string(1024 * 1024 + 1, 'v'));

  Child server;
  int port = 0;
  const fs::path log = dir / "server.log";
  const bool started = Start(
    server, port, log, {"python3", "-m", "http.server", "--bind", "127.0.0.1", "--directory", dir.string()}, {}, true);
  INFO(ReadText(log));
  REQUIRE(started);

  std::string body = "stale";
  const std::string url = "http://127.0.0.1:" + std::to_string(port);
  CHECK(VolumHttpGetString((url + "/hello.txt").c_str(), body, 3000));
  CHECK(body == hello);

  body = "stale";
  CHECK(VolumHttpGetString((url + "/exact.bin").c_str(), body, 3000));
  CHECK(body.size() == 1024 * 1024);
  CHECK(body.find_first_not_of('e') == std::string::npos);

  body = "stale";
  CHECK_FALSE(VolumHttpGetString((url + "/missing.txt").c_str(), body, 3000));
  CHECK(body.empty());

  body = "stale";
  CHECK_FALSE(VolumHttpGetString((url + "/over.bin").c_str(), body, 3000));
  CHECK(body.empty());
  fs::remove_all(dir);
}

TEST_CASE("Linux HTTP get returns false when VOLUM_TEST_CURL_SONAME cannot be loaded")
{
  if (!OnPath("python3"))
  {
    MESSAGE("python3 is missing; skipping Linux HTTP get");
    return;
  }

  namespace fs = std::filesystem;
  LocalNet net;
  const fs::path dir = volum_test::ProcessTempRoot() / "volum_linux_http_missing_curl";
  fs::remove_all(dir);
  fs::create_directories(dir);
  WriteText(dir / "hello.txt", "still-there");

  Child server;
  int port = 0;
  const fs::path log = dir / "server.log";
  const bool started = Start(
    server, port, log, {"python3", "-m", "http.server", "--bind", "127.0.0.1", "--directory", dir.string()}, {}, true);
  INFO(ReadText(log));
  REQUIRE(started);

  const std::string url = "http://127.0.0.1:" + std::to_string(port) + "/hello.txt";
  std::string body;
  REQUIRE(VolumHttpGetString(url.c_str(), body, 3000));
  REQUIRE(body == "still-there");

  EnvGuard soname("VOLUM_TEST_CURL_SONAME");
  soname.Set("libdoesnotexist.so");
  body = "stale";
  CHECK_FALSE(VolumHttpGetString(url.c_str(), body, 3000));
  CHECK(body.empty());

  soname.Set("libcurl.so.4");
  body.clear();
  CHECK(VolumHttpGetString(url.c_str(), body, 3000));
  CHECK(body == "still-there");
  fs::remove_all(dir);
}

TEST_CASE("Linux HTTP get sends the update-notifier user agent and follows a redirect")
{
  if (!OnPath("python3"))
  {
    MESSAGE("python3 is missing; skipping Linux HTTP get");
    return;
  }

  namespace fs = std::filesystem;
  LocalNet net;
  const fs::path dir = volum_test::ProcessTempRoot() / "volum_linux_http_redirect";
  fs::remove_all(dir);
  fs::create_directories(dir);
  const fs::path script = dir / "serve.py";
  const fs::path uaFile = dir / "ua.txt";
  const fs::path hello = dir / "hello.txt";
  WriteText(hello, "unused");
  WriteText(script,
            "from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer\n"
            "import sys\n"
            "ua_path = sys.argv[2]\n"
            "class H(BaseHTTPRequestHandler):\n"
            "    def do_GET(self):\n"
            "        if self.path.startswith('/redir'):\n"
            "            self.send_response(302)\n"
            "            self.send_header('Location', '/landed')\n"
            "            self.end_headers()\n"
            "            return\n"
            "        ua = self.headers.get('User-Agent', '')\n"
            "        with open(ua_path, 'w', encoding='utf-8') as handle:\n"
            "            handle.write(ua)\n"
            "        body = b'landed'\n"
            "        self.send_response(200)\n"
            "        self.send_header('Content-Length', str(len(body)))\n"
            "        self.end_headers()\n"
            "        self.wfile.write(body)\n"
            "    def log_message(self, fmt, *args):\n"
            "        pass\n"
            "ThreadingHTTPServer(('127.0.0.1', int(sys.argv[1])), H).serve_forever()\n");

  Child server;
  int port = 0;
  const fs::path log = dir / "server.log";
  // Port is inserted between the script path and the user-agent path.
  const bool started = Start(server, port, log, {"python3", script.string()}, {uaFile.string()}, false);
  INFO(ReadText(log));
  REQUIRE(started);

  std::string body = "stale";
  const std::string url = "http://127.0.0.1:" + std::to_string(port) + "/redir";
  CHECK(VolumHttpGetString(url.c_str(), body, 3000));
  CHECK(body == "landed");
  CHECK(ReadText(uaFile) == "VoLum update notifier");
  fs::remove_all(dir);
}

TEST_CASE("Linux HTTP get gives up at its timeout")
{
  if (!OnPath("python3"))
  {
    MESSAGE("python3 is missing; skipping Linux HTTP get");
    return;
  }

  namespace fs = std::filesystem;
  LocalNet net;
  const fs::path dir = volum_test::ProcessTempRoot() / "volum_linux_http_timeout";
  fs::remove_all(dir);
  fs::create_directories(dir);
  const fs::path script = dir / "hang.py";
  WriteText(script,
            "import socket, threading, time, sys\n"
            "port = int(sys.argv[1])\n"
            "server = socket.socket()\n"
            "server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)\n"
            "server.bind(('127.0.0.1', port))\n"
            "server.listen(16)\n"
            "def hold(conn):\n"
            "    time.sleep(30)\n"
            "    conn.close()\n"
            "while True:\n"
            "    conn, _ = server.accept()\n"
            "    threading.Thread(target=hold, args=(conn,), daemon=True).start()\n");

  Child server;
  int port = 0;
  const fs::path log = dir / "server.log";
  const bool started = Start(server, port, log, {"python3", script.string()}, {}, false);
  INFO(ReadText(log));
  REQUIRE(started);

  const std::string url = "http://127.0.0.1:" + std::to_string(port) + "/hang";
  std::string body = "stale";
  const auto instantStart = std::chrono::steady_clock::now();
  CHECK_FALSE(VolumHttpGetString(url.c_str(), body, 0));
  const auto instantElapsed = std::chrono::steady_clock::now() - instantStart;
  CHECK(body.empty());
  CHECK(instantElapsed < std::chrono::milliseconds(500));

  body = "stale";
  const auto start = std::chrono::steady_clock::now();
  CHECK_FALSE(VolumHttpGetString(url.c_str(), body, 400));
  const auto elapsed = std::chrono::steady_clock::now() - start;
  CHECK(body.empty());
  CHECK(elapsed < std::chrono::seconds(3));
  CHECK(elapsed >= std::chrono::milliseconds(100));
  fs::remove_all(dir);
}

TEST_CASE("Linux HTTP get rejects a self-signed certificate")
{
  if (!OnPath("python3") || !OnPath("openssl"))
  {
    MESSAGE("python3 or openssl is missing; skipping Linux HTTP TLS check");
    return;
  }

  namespace fs = std::filesystem;
  LocalNet net;
  const fs::path dir = volum_test::ProcessTempRoot() / "volum_linux_http_tls";
  fs::remove_all(dir);
  fs::create_directories(dir);
  WriteText(dir / "hello.txt", "secret");
  const fs::path cert = dir / "cert.pem";
  const fs::path key = dir / "key.pem";
  const fs::path opensslLog = dir / "openssl.log";
  Child openssl;
  openssl.pid = Spawn({"openssl", "req", "-x509", "-newkey", "rsa:2048", "-keyout", key.string(), "-out", cert.string(),
                       "-days", "1", "-nodes", "-subj", "/CN=127.0.0.1", "-addext", "subjectAltName=IP:127.0.0.1"},
                      opensslLog);
  REQUIRE(openssl.pid > 0);
  int status = 0;
  while (::waitpid(openssl.pid, &status, 0) < 0 && errno == EINTR) {}
  openssl.pid = -1;
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
  {
    MESSAGE("openssl could not mint a certificate; skipping Linux HTTP TLS check\n" << ReadText(opensslLog));
    return;
  }

  const fs::path script = dir / "https.py";
  WriteText(script,
            "import http.server, ssl, sys\n"
            "port, cert, key, directory = int(sys.argv[1]), sys.argv[2], sys.argv[3], sys.argv[4]\n"
            "import os\n"
            "os.chdir(directory)\n"
            "httpd = http.server.ThreadingHTTPServer(('127.0.0.1', port), http.server.SimpleHTTPRequestHandler)\n"
            "ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)\n"
            "ctx.minimum_version = ssl.TLSVersion.TLSv1_2\n"
            "ctx.load_cert_chain(cert, key)\n"
            "httpd.socket = ctx.wrap_socket(httpd.socket, server_side=True)\n"
            "httpd.serve_forever()\n");

  Child server;
  int port = 0;
  const fs::path log = dir / "server.log";
  const bool started =
    Start(server, port, log, {"python3", script.string()}, {cert.string(), key.string(), dir.string()}, false);
  INFO(ReadText(log));
  REQUIRE(started);

  std::string body = "stale";
  const std::string url = "https://127.0.0.1:" + std::to_string(port) + "/hello.txt";
  CHECK_FALSE(VolumHttpGetString(url.c_str(), body, 3000));
  CHECK(body.empty());
  fs::remove_all(dir);
}

TEST_CASE("Linux test binary does not DT_NEEDED libcurl")
{
  if (!OnPath("readelf"))
  {
    MESSAGE("readelf is missing; skipping DT_NEEDED check");
    return;
  }

  FILE* pipe = ::popen("readelf -d /proc/self/exe", "r");
  REQUIRE(pipe);
  std::string text;
  char buf[512];
  while (std::fgets(buf, sizeof(buf), pipe))
    text += buf;
  const int rc = ::pclose(pipe);
  REQUIRE(rc == 0);
  CHECK(text.find("NEEDED") != std::string::npos);
  CHECK(text.find("libcurl") == std::string::npos);
}

#endif
