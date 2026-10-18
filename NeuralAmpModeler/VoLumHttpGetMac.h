#pragma once

#import <Foundation/Foundation.h>

#include <string>

// VolumHttpGetString with the session configuration supplied by the caller, so
// tests can serve the request from an NSURLProtocol instead of the network.
bool VolumHttpGetStringWithConfiguration(NSURLSessionConfiguration* config, const char* url, std::string& out,
                                         int timeoutMs);
