#include "third_party/doctest.h"
#include "../VoLumDspStaging.h"

TEST_CASE("DSP staging keeps live path until staged asset is applied")
{
  volum::dsp_staging::LiveStagedPaths paths;
  paths.live = "C:/assets/old.nam";

  volum::dsp_staging::StagePathOnSuccess(paths, "C:/assets/new.nam");
  CHECK(paths.live == "C:/assets/old.nam");
  CHECK(paths.staged == "C:/assets/new.nam");
  CHECK_FALSE(volum::dsp_staging::LivePathMatchesStagedAsset(paths.live, paths.staged, true));

  volum::dsp_staging::CommitStagedPathOnApply(paths);
  CHECK(paths.live == "C:/assets/new.nam");
  CHECK(paths.staged.empty());
  CHECK(volum::dsp_staging::LivePathMatchesStagedAsset(paths.live, paths.staged, false));
}

TEST_CASE("DSP staging failure clears staged path without mutating live path")
{
  volum::dsp_staging::LiveStagedPaths paths;
  paths.live = "C:/assets/keep.nam";

  volum::dsp_staging::StagePathOnSuccess(paths, "C:/assets/bad.nam");
  volum::dsp_staging::ClearStagedOnFailure(paths);

  CHECK(paths.live == "C:/assets/keep.nam");
  CHECK(paths.staged.empty());
}
