#pragma once

#include <string>

bool VolumHttpGetString(const char* url, std::string& out, int timeoutMs);
