#include "third_party/doctest.h"

#include "../VoLumAmpListScroll.h"

using namespace volum::amplist;

TEST_CASE("Sidebar list is scrollable only when content overflows")
{
  CHECK_FALSE(Scrollable(/*contentH=*/100.f, /*rectH=*/200.f));
  CHECK_FALSE(Scrollable(200.f, 200.f)); // equal within slack -> not scrollable
  CHECK(Scrollable(260.f, 200.f));
}

TEST_CASE("Row right edge reserves the scrollbar gutter only when scrollable")
{
  const float rectR = 300.f;
  const float rectH = 200.f;
  // Not scrollable: rows use the full width.
  CHECK(RowRightX(rectR, /*contentH=*/100.f, rectH) == doctest::Approx(rectR));
  // Scrollable: rows stop before the bar + gutter.
  CHECK(RowRightX(rectR, /*contentH=*/400.f, rectH)
        == doctest::Approx(rectR - kScrollbarW - kScrollGutter));
}

TEST_CASE("Scroll thumb size and position track the offset")
{
  const float rectT = 0.f, rectB = 200.f, rectH = 200.f;
  const float contentH = 400.f; // 2x overflow

  const auto top = ComputeScroll(rectT, rectB, rectH, contentH, /*scrollOffset=*/0.f);
  CHECK(top.trackTop == doctest::Approx(2.f));
  CHECK(top.trackH == doctest::Approx(196.f));
  CHECK(top.maxScroll == doctest::Approx(contentH - rectH));
  // Thumb is proportional (rectH/contentH of the track) and never below the min.
  CHECK(top.thumbH == doctest::Approx(std::max(kMinThumbH, 196.f * (rectH / contentH))));
  CHECK(top.thumbY == doctest::Approx(top.trackTop)); // at offset 0 -> top of track

  // At max offset the thumb bottom sits at the track bottom.
  const auto bot = ComputeScroll(rectT, rectB, rectH, contentH, top.maxScroll);
  CHECK(bot.thumbY + bot.thumbH == doctest::Approx(bot.trackTop + bot.trackH));
}

TEST_CASE("Thumb-drag maps cursor y back to a clamped scroll offset")
{
  const float rectT = 0.f, rectB = 200.f, rectH = 200.f;
  const float contentH = 400.f;
  const auto m = ComputeScroll(rectT, rectB, rectH, contentH, 0.f);

  // y at the track top -> offset 0.
  CHECK(ThumbYToScroll(m.trackTop, m.trackTop, m.trackH, m.thumbH, m.maxScroll) == doctest::Approx(0.f));
  // y far below the track -> clamped to maxScroll.
  CHECK(ThumbYToScroll(10000.f, m.trackTop, m.trackH, m.thumbH, m.maxScroll) == doctest::Approx(m.maxScroll));
  // y above the track -> clamped to 0.
  CHECK(ThumbYToScroll(-10000.f, m.trackTop, m.trackH, m.thumbH, m.maxScroll) == doctest::Approx(0.f));

  // Round-trip: a thumbY produced for a given offset maps back to that offset.
  const float wantOffset = m.maxScroll * 0.5f;
  const auto mid = ComputeScroll(rectT, rectB, rectH, contentH, wantOffset);
  CHECK(ThumbYToScroll(mid.thumbY, m.trackTop, m.trackH, m.thumbH, m.maxScroll) == doctest::Approx(wantOffset));
}
