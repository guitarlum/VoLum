#pragma once

#include <string>

namespace volum::ir_staging
{

// Models how IR file paths are published relative to staged impulse responses.
struct LiveStagedPaths
{
  std::string live;
  std::string staged;
};

inline void StagePathOnSuccess(LiveStagedPaths& paths, const std::string& candidatePath)
{
  paths.staged = candidatePath;
}

inline void CommitStagedPathOnApply(LiveStagedPaths& paths)
{
  if (!paths.staged.empty())
    paths.live = paths.staged;
  paths.staged.clear();
}

inline void ClearStagedOnFailure(LiveStagedPaths& paths)
{
  paths.staged.clear();
}

// While a staged IR is pending apply, the live path must not jump ahead of the impulse.
inline bool LivePathMatchesStagedIr(const std::string& livePath, const std::string& stagedPath, const bool hasStagedIr)
{
  if (!hasStagedIr)
    return true;
  return livePath == stagedPath;
}

} // namespace volum::ir_staging
