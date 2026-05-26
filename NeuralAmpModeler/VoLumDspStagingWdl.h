#pragma once

#include "VoLumDspStaging.h"

// WDL_String is provided by iPlug headers before this include in NeuralAmpModeler.h.

namespace volum::dsp_staging
{

struct WdlStagedPathPair
{
  WDL_String live;
  WDL_String staged;
};

inline void StagePathOnSuccess(WdlStagedPathPair& paths, const WDL_String& candidatePath)
{
  paths.staged = candidatePath;
}

inline void StagePathOnSuccess(WdlStagedPathPair& paths, const char* candidatePath)
{
  paths.staged.Set(candidatePath);
}

inline void CommitStagedPathOnApply(WdlStagedPathPair& paths)
{
  if (paths.staged.GetLength())
    paths.live = paths.staged;
  paths.staged.Set("");
}

inline void ClearStagedPath(WdlStagedPathPair& paths)
{
  paths.staged.Set("");
}

inline void ClearLiveAndStagedPath(WdlStagedPathPair& paths)
{
  paths.live.Set("");
  paths.staged.Set("");
}

} // namespace volum::dsp_staging
