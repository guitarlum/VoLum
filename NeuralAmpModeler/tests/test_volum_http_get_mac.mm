#include "third_party/doctest.h"

#include "VoLumHttpGetMac.h"

#include <atomic>
#include <chrono>
#include <string>

namespace
{

enum class StubMode
{
  Ok,
  NotFound,
  Oversized,
  Hang
};

std::atomic<StubMode> gStubMode{StubMode::Ok};
std::atomic<int> gStubRequests{0};
constexpr NSUInteger kStubBodyBytes = 64 * 1024;

} // namespace

@interface VolumHttpStubProtocol : NSURLProtocol
@end

@implementation VolumHttpStubProtocol

+ (BOOL)canInitWithRequest:(NSURLRequest*)request
{
  return [request.URL.host isEqualToString:@"volum.test"];
}

+ (NSURLRequest*)canonicalRequestForRequest:(NSURLRequest*)request
{
  return request;
}

- (void)startLoading
{
  ++gStubRequests;
  const StubMode mode = gStubMode.load();
  if (mode == StubMode::Hang)
    return;

  const NSUInteger length = mode == StubMode::Oversized ? 1024 * 1024 + 1 : kStubBodyBytes;
  NSMutableData* body = [NSMutableData dataWithLength:length];
  memset(body.mutableBytes, 'v', body.length);
  NSHTTPURLResponse* response = [[NSHTTPURLResponse alloc] initWithURL:self.request.URL
                                                            statusCode:mode == StubMode::NotFound ? 404 : 200
                                                           HTTPVersion:@"HTTP/1.1"
                                                          headerFields:@{}];
  [self.client URLProtocol:self didReceiveResponse:response cacheStoragePolicy:NSURLCacheStorageNotAllowed];
  [self.client URLProtocol:self didLoadData:body];
  [self.client URLProtocolDidFinishLoading:self];
}

- (void)stopLoading
{
}

@end

namespace
{

bool StubGet(StubMode mode, std::string& out, int timeoutMs = 3000, const char* url = "https://volum.test/appcast.json")
{
  gStubMode = mode;
  NSURLSessionConfiguration* config = [NSURLSessionConfiguration ephemeralSessionConfiguration];
  config.protocolClasses = @[ [VolumHttpStubProtocol class] ];
  return VolumHttpGetStringWithConfiguration(config, url, out, timeoutMs);
}

} // namespace

TEST_CASE("Mac HTTP get keeps the response after NSURLSession releases it")
{
  // 1.3.0 kept the completion handler's NSData unretained and read it after the
  // handler returned; the update check crashed REAPER on its worker thread.
  // Against this stub that read faulted within a few dozen requests.
  for (int i = 0; i < 50; ++i)
  {
    std::string body;
    REQUIRE(StubGet(StubMode::Ok, body));
    REQUIRE(body.size() == kStubBodyBytes);
    REQUIRE(body.find_first_not_of('v') == std::string::npos);
  }
}

TEST_CASE("Mac HTTP get refuses an error status, an oversized body and plain http")
{
  std::string body = "stale";
  CHECK_FALSE(StubGet(StubMode::NotFound, body));
  CHECK(body.empty());

  body = "stale";
  CHECK_FALSE(StubGet(StubMode::Oversized, body));
  CHECK(body.empty());

  const int requestsBefore = gStubRequests.load();
  CHECK_FALSE(StubGet(StubMode::Ok, body, 3000, "http://volum.test/appcast.json"));
  CHECK(gStubRequests.load() == requestsBefore);
}

TEST_CASE("Mac HTTP get gives up at its timeout and a late reply is harmless")
{
  std::string body = "stale";
  const auto start = std::chrono::steady_clock::now();
  CHECK_FALSE(StubGet(StubMode::Hang, body, 300));
  const auto elapsed = std::chrono::steady_clock::now() - start;
  CHECK(body.empty());
  CHECK(elapsed < std::chrono::seconds(3));

  // The cancelled task completes after the call returned; the next request
  // must still work.
  std::string next;
  CHECK(StubGet(StubMode::Ok, next));
  CHECK(next.size() == kStubBodyBytes);
}
