#pragma once

#include <string>

namespace volum::dsp_staging
{

// Models how live file paths are published relative to staged DSP assets.
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

// While a staged asset is pending apply, the live path must not jump ahead of the object.
inline bool LivePathMatchesStagedAsset(const std::string& livePath, const std::string& stagedPath,
                                       const bool hasStagedAsset)
{
  if (!hasStagedAsset)
    return true;
  return livePath == stagedPath;
}

} // namespace volum::dsp_staging
