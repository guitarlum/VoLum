#include "third_party/doctest.h"

#include "VoLumPresetStep.h"

#include <initializer_list>

// The preset bar's < / > arrows. The interesting state is "nothing selected"
// (mIdx == -1), which the bar is left in by SetList, by a factory reset, and by
// deleting the active preset -- so it is the state a user most plausibly presses
// an arrow from.

TEST_CASE("Stepping forward from no selection recalls the first preset")
{
  // Regression: the old arithmetic substituted 0 for "no selection" and then
  // added the direction, so `>` recalled index 1 and the first preset in the bank
  // was unreachable going forward.
  CHECK(volum::StepPresetIndex(-1, +1, 3) == 0);
  CHECK(volum::StepPresetIndex(-1, +1, 2) == 0);
  CHECK(volum::StepPresetIndex(-1, +1, 1) == 0);
}

TEST_CASE("Stepping backward from no selection recalls the last preset")
{
  CHECK(volum::StepPresetIndex(-1, -1, 3) == 2);
  CHECK(volum::StepPresetIndex(-1, -1, 2) == 1);
  CHECK(volum::StepPresetIndex(-1, -1, 1) == 0);
}

TEST_CASE("Stepping wraps in both directions once a preset is selected")
{
  CHECK(volum::StepPresetIndex(0, +1, 3) == 1);
  CHECK(volum::StepPresetIndex(1, +1, 3) == 2);
  CHECK(volum::StepPresetIndex(2, +1, 3) == 0);

  CHECK(volum::StepPresetIndex(2, -1, 3) == 1);
  CHECK(volum::StepPresetIndex(1, -1, 3) == 0);
  CHECK(volum::StepPresetIndex(0, -1, 3) == 2);
}

TEST_CASE("A single-preset bank always lands on that preset")
{
  CHECK(volum::StepPresetIndex(0, +1, 1) == 0);
  CHECK(volum::StepPresetIndex(0, -1, 1) == 0);
}

TEST_CASE("An empty bank yields no selection to recall")
{
  // The caller must not recall anything: index -1 means "do nothing", which is
  // what keeps an empty bank's arrows inert instead of recalling preset 0 of a
  // bank that has none.
  CHECK(volum::StepPresetIndex(-1, +1, 0) == -1);
  CHECK(volum::StepPresetIndex(-1, -1, 0) == -1);
  CHECK(volum::StepPresetIndex(0, +1, 0) == -1);
}

TEST_CASE("Stepping stays in range for every start, direction and bank size")
{
  for (int count = 1; count <= 8; ++count)
  {
    for (int start = -1; start < count; ++start)
    {
      for (const int dir : {-1, +1})
      {
        CAPTURE(count);
        CAPTURE(start);
        CAPTURE(dir);
        const int idx = volum::StepPresetIndex(start, dir, count);
        REQUIRE(idx >= 0);
        REQUIRE(idx < count);
      }
    }
  }
}
