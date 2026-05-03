#pragma once

#if __has_include(<nlohmann/json.hpp>)
#include <nlohmann/json.hpp>
#elif __has_include(<json.hpp>)
#include <json.hpp>
#else
#error "nlohmann json header not found (expected iPlug Dependencies/Extras layout)"
#endif

namespace volum
{

inline bool RenameJsonKeyIfPresent(nlohmann::json& j, const char* from, const char* to)
{
  auto existing = j.find(from);
  if (existing == j.end())
    return false;

  j[to] = *existing;
  j.erase(existing);
  return true;
}

} // namespace volum
