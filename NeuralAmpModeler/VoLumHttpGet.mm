#include "VoLumHttpGet.h"
#include "VoLumHttpGetMac.h"

#include <memory>

#if !__has_feature(objc_arc)
  #error "VoLumHttpGet.mm must be compiled with -fobjc-arc"
#endif

namespace
{

constexpr NSUInteger kMaximumBytes = 1024 * 1024;

struct HttpResult
{
  bool received = false;
  NSInteger statusCode = 0;
  std::string body;
};

} // namespace

bool VolumHttpGetStringWithConfiguration(NSURLSessionConfiguration* config, const char* url, std::string& out,
                                         int timeoutMs)
{
  out.clear();
  if (!config || !url || !*url || timeoutMs <= 0)
    return false;

  @autoreleasepool
  {
    NSString* text = [NSString stringWithUTF8String:url];
    NSURL* nsUrl = text ? [NSURL URLWithString:text] : nil;
    if (!nsUrl || ![[nsUrl scheme] isEqualToString:@"https"])
      return false;

    config.HTTPCookieStorage = nil;
    config.URLCredentialStorage = nil;
    config.URLCache = nil;
    config.requestCachePolicy = NSURLRequestReloadIgnoringLocalCacheData;
    config.timeoutIntervalForRequest = timeoutMs / 1000.0;
    config.timeoutIntervalForResource = timeoutMs / 1000.0;

    // The completion handler can outlive this call after a timeout, and the
    // NSData it receives is only valid until it returns: it copies into a
    // result it co-owns and never touches this frame.
    auto result = std::make_shared<HttpResult>();
    dispatch_semaphore_t done = dispatch_semaphore_create(0);
    NSURLSession* session = [NSURLSession sessionWithConfiguration:config];
    NSURLSessionDataTask* task =
      [session dataTaskWithURL:nsUrl
            completionHandler:^(NSData* data, NSURLResponse* response, NSError* error) {
              if ([response isKindOfClass:[NSHTTPURLResponse class]])
                result->statusCode = static_cast<NSHTTPURLResponse*>(response).statusCode;
              if (!error && data && data.length <= kMaximumBytes)
              {
                result->body.assign(static_cast<const char*>(data.bytes), data.length);
                result->received = true;
              }
              dispatch_semaphore_signal(done);
            }];
    [task resume];

    const long waitResult =
      dispatch_semaphore_wait(done, dispatch_time(DISPATCH_TIME_NOW, static_cast<int64_t>(timeoutMs) * NSEC_PER_MSEC));
    if (waitResult != 0)
      [task cancel];
    [session finishTasksAndInvalidate];

    if (waitResult != 0 || !result->received || result->statusCode != 200)
      return false;
    out = std::move(result->body);
    return true;
  }
}

bool VolumHttpGetString(const char* url, std::string& out, int timeoutMs)
{
  return VolumHttpGetStringWithConfiguration([NSURLSessionConfiguration ephemeralSessionConfiguration], url, out,
                                             timeoutMs);
}
