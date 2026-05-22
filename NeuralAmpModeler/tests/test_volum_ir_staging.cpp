#include "third_party/doctest.h"
#include "../VoLumIrStaging.h"

TEST_CASE("IR staging keeps live path until staged IR is applied")
{
  volum::ir_staging::LiveStagedPaths paths;
  paths.live = "C:/ir/old.wav";

  volum::ir_staging::StagePathOnSuccess(paths, "C:/ir/new.wav");
  CHECK(paths.live == "C:/ir/old.wav");
  CHECK(paths.staged == "C:/ir/new.wav");
  CHECK_FALSE(volum::ir_staging::LivePathMatchesStagedIr(paths.live, paths.staged, true));

  volum::ir_staging::CommitStagedPathOnApply(paths);
  CHECK(paths.live == "C:/ir/new.wav");
  CHECK(paths.staged.empty());
  CHECK(volum::ir_staging::LivePathMatchesStagedIr(paths.live, paths.staged, false));
}

TEST_CASE("IR staging failure clears staged path without mutating live path")
{
  volum::ir_staging::LiveStagedPaths paths;
  paths.live = "C:/ir/keep.wav";

  volum::ir_staging::StagePathOnSuccess(paths, "C:/ir/bad.wav");
  volum::ir_staging::ClearStagedOnFailure(paths);

  CHECK(paths.live == "C:/ir/keep.wav");
  CHECK(paths.staged.empty());
}
