#include "VoLumHttpGet.h"

#import <Foundation/Foundation.h>

bool VolumHttpGetString(const char* url, std::string& out, int timeoutMs)
{
  out.clear();
  if (!url || !*url || timeoutMs <= 0)
    return false;

  @autoreleasepool
  {
    NSString* text = [NSString stringWithUTF8String:url];
    NSURL* nsUrl = text ? [NSURL URLWithString:text] : nil;
    if (!nsUrl || ![[nsUrl scheme] isEqualToString:@"https"])
      return false;

    NSURLSessionConfiguration* config = [NSURLSessionConfiguration ephemeralSessionConfiguration];
    config.HTTPCookieStorage = nil;
    config.URLCredentialStorage = nil;
    config.URLCache = nil;
    config.requestCachePolicy = NSURLRequestReloadIgnoringLocalCacheData;
    config.timeoutIntervalForRequest = timeoutMs / 1000.0;
    config.timeoutIntervalForResource = timeoutMs / 1000.0;

    dispatch_semaphore_t done = dispatch_semaphore_create(0);
    __block NSData* received = nil;
    __block NSInteger statusCode = 0;
    __block NSError* requestError = nil;
    NSURLSession* session = [NSURLSession sessionWithConfiguration:config];
    NSURLSessionDataTask* task =
      [session dataTaskWithURL:nsUrl
            completionHandler:^(NSData* data, NSURLResponse* response, NSError* error) {
              received = data;
              requestError = error;
              if ([response isKindOfClass:[NSHTTPURLResponse class]])
                statusCode = static_cast<NSHTTPURLResponse*>(response).statusCode;
              dispatch_semaphore_signal(done);
            }];
    [task resume];

    const long waitResult =
      dispatch_semaphore_wait(done, dispatch_time(DISPATCH_TIME_NOW, static_cast<int64_t>(timeoutMs) * NSEC_PER_MSEC));
    if (waitResult != 0)
      [task cancel];
    [session finishTasksAndInvalidate];

    constexpr NSUInteger kMaximumBytes = 1024 * 1024;
    if (waitResult != 0 || requestError || statusCode != 200 || !received || received.length > kMaximumBytes)
      return false;
    out.assign(static_cast<const char*>(received.bytes), received.length);
    return true;
  }
}
