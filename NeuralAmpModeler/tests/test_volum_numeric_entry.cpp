#include "third_party/doctest.h"

#include "../VoLumNumericEntry.h"
#include "../../iPlug2/IGraphics/Controls/ITextEntryKeyFilter.h"

#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>

// Regression coverage for the exact-value box. iPlug2 converted the typed text with
// IParam::StringToValue, whose numeric fallback is atof(), so "", " ", "abc" and "-"
// all became 0 and were applied as a genuine parameter edit - a typo moved the knob
// to zero or to its minimum instead of cancelling. "1,5" applied 1, because atof
// stops at the comma.
//
// The rule being pinned: unparseable text must leave the caller's value untouched,
// and the function must say so, so the caller can decline to edit the parameter.

using volum::ParseNumericEntry;

namespace
{
// Returns the parsed value, or the sentinel when the text was rejected. The
// sentinel stands in for "the parameter was left alone".
constexpr double kUntouched = -12345.678;

double Parse(const char* text)
{
  double out = kUntouched;
  const bool ok = ParseNumericEntry(text, out);
  if (!ok)
    CHECK(out == kUntouched); // rejection must not write through
  return out;
}
} // namespace

TEST_CASE("Text that is not a number leaves the value untouched")
{
  // Every one of these produced 0 through atof, and 0 is a legitimate value for
  // most VoLum knobs, so the edit was indistinguishable from a deliberate one.
  CHECK(Parse("") == kUntouched);
  CHECK(Parse("   ") == kUntouched);
  CHECK(Parse("abc") == kUntouched);
  CHECK(Parse("-") == kUntouched);
  CHECK(Parse("+") == kUntouched);
  CHECK(Parse(".") == kUntouched);
  CHECK(Parse("--3") == kUntouched);
  CHECK(Parse("dB") == kUntouched);
}

TEST_CASE("Ordinary numbers parse, including negatives and exponents")
{
  CHECK(Parse("0") == doctest::Approx(0.0));
  CHECK(Parse("7") == doctest::Approx(7.0));
  CHECK(Parse("-6.5") == doctest::Approx(-6.5));
  CHECK(Parse("  12.25  ") == doctest::Approx(12.25));
  CHECK(Parse("+3") == doctest::Approx(3.0));
  CHECK(Parse(".5") == doctest::Approx(0.5));
  CHECK(Parse("1e3") == doctest::Approx(1000.0));
}

TEST_CASE("A trailing unit is accepted, because the readout shows one")
{
  CHECK(Parse("-6.5 dB") == doctest::Approx(-6.5));
  CHECK(Parse("50%") == doctest::Approx(50.0));
  CHECK(Parse("440 Hz") == doctest::Approx(440.0));
  CHECK(Parse("12dB/oct") == doctest::Approx(12.0));
}

TEST_CASE("A comma is read as the decimal separator, not stopped at")
{
  // atof applied 1 here, silently, which is the worst outcome: a plausible value
  // the user did not type.
  CHECK(Parse("1,5") == doctest::Approx(1.5));
  CHECK(Parse("-0,25") == doctest::Approx(-0.25));

  // Ambiguous or malformed grouping is refused rather than guessed at.
  CHECK(Parse("1,234,567") == kUntouched);
  CHECK(Parse("1.5,5") == kUntouched);
}

TEST_CASE("Trailing junk that is not a unit is refused")
{
  CHECK(Parse("1.2.3") == kUntouched);
  CHECK(Parse("5-3") == kUntouched);
  CHECK(Parse("3 4") == kUntouched);
  CHECK(Parse("2 + 2") == kUntouched);

  // A half-typed exponent is the same trap as "1,5": strtod backs up over the part it
  // cannot complete and reports 1, and the unit rule then accepts the leftover e as a
  // unit. Committing a plausible number the user did not type is the outcome this
  // whole parser exists to prevent.
  CHECK(Parse("1e") == kUntouched);
  CHECK(Parse("2.5E") == kUntouched);
  CHECK(Parse("1e+") == kUntouched);
  CHECK(Parse("1e3") == doctest::Approx(1000.0)); // a complete one still parses
}

TEST_CASE("Values that are not finite are refused")
{
  // strtod accepts these spellings and overflows the last one to HUGE_VAL. Handing
  // any of them to a parameter puts a NaN or an infinity into the DSP.
  CHECK(Parse("nan") == kUntouched);
  CHECK(Parse("NaN") == kUntouched);
  CHECK(Parse("inf") == kUntouched);
  CHECK(Parse("-inf") == kUntouched);
  CHECK(Parse("infinity") == kUntouched);
  CHECK(Parse("1e999") == kUntouched);
  CHECK(Parse("-1e999") == kUntouched);
}

TEST_CASE("An out-of-range but finite number is passed through for the caller to clamp")
{
  // Clamping belongs to the parameter, which knows its own range; rejecting here
  // would make typing 200 into a 0..100 control do nothing at all, when the useful
  // behaviour is to land on 100.
  CHECK(Parse("1000000") == doctest::Approx(1000000.0));
  CHECK(Parse("-1000000") == doctest::Approx(-1000000.0));
}

TEST_CASE("A list-valued parameter refuses text that is not one of its own values")
{
  // The enum/bool branch of the exact-entry box handed the text to
  // IParam::StringToValue, which returns 0 for anything it does not recognise - so a
  // typo moved the parameter to its minimum, reintroducing for list params exactly
  // the defect the numeric path above removes. No list param reaches that control
  // today, which is why this is a source pin rather than a behavioural test: the
  // point is that whoever wires the first one does not inherit the trap.
  const auto path = std::filesystem::path(__FILE__).parent_path().parent_path() / "VoLumExactEntry.h";
  std::ifstream in(path, std::ios::binary);
  REQUIRE(in.good());
  std::ostringstream ss;
  ss << in.rdbuf();
  const std::string src = ss.str();

  CHECK(src.find("pParam->MapDisplayText(str ? str : \"\", &mapped)") != std::string::npos);
  CHECK(src.find("pParam->StringToValue(") == std::string::npos);
}

TEST_CASE("F-77 a comma typed into a numeric entry is a decimal separator")
{
  // The key filter in iPlug2's ITextEntryControl let only '.' through, so the comma of
  // "7,5" was dropped and 75 clamped to the top of the range.
  CHECK(iplug::igraphics::NormalizeDecimalSeparatorKey(',') == '.');
  CHECK(iplug::igraphics::NormalizeDecimalSeparatorKey('.') == '.');
  CHECK(iplug::igraphics::NormalizeDecimalSeparatorKey('7') == '7');
  CHECK(iplug::igraphics::NormalizeDecimalSeparatorKey('-') == '-');
  CHECK(iplug::igraphics::NormalizeDecimalSeparatorKey(0) == 0);

  // Pasted text bypasses the key filter; the parse on our side takes the comma too.
  double out = 0.0;
  REQUIRE(volum::ParseNumericEntry("7,5", out));
  CHECK(out == doctest::Approx(7.5));
}

TEST_CASE("F-77 the numeric key filter applies the comma rule to double parameters")
{
  const auto path = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "iPlug2" / "IGraphics"
                    / "Controls" / "ITextEntryControl.cpp";
  std::ifstream in(path, std::ios::binary);
  REQUIRE(in.good());
  std::ostringstream ss;
  ss << in.rdbuf();
  const std::string src = ss.str();

  const auto doubles = src.find("case IParam::kTypeDouble:");
  REQUIRE(doubles != std::string::npos);
  const auto normalise = src.find("stbKey = NormalizeDecimalSeparatorKey(stbKey);", doubles);
  const auto accept = src.find("stbKey == '.'", doubles);
  REQUIRE(normalise != std::string::npos);
  REQUIRE(accept != std::string::npos);
  CHECK(normalise < accept); // converted before the filter decides
}

TEST_CASE("F-77 Esc in the number box closes the exact-entry panel with it")
{
  // Esc inside the box ends iPlug2's text entry without telling the panel, which stayed
  // up and ate the next click. The panel notices on its next draw or click.
  const auto path = std::filesystem::path(__FILE__).parent_path().parent_path() / "VoLumExactEntry.h";
  std::ifstream in(path, std::ios::binary);
  REQUIRE(in.good());
  std::ostringstream ss;
  ss << in.rdbuf();
  const std::string src = ss.str();

  const auto draw = src.find("void Draw(IGraphics& g) override");
  REQUIRE(draw != std::string::npos);
  const auto drawSync = src.find("SyncTextEntryState();", draw);
  const auto drawHide = src.find("if (mHide)", draw);
  REQUIRE(drawSync != std::string::npos);
  CHECK(drawSync < drawHide);

  const auto down = src.find("void OnMouseDown(", draw);
  REQUIRE(down != std::string::npos);
  const auto downSync = src.find("SyncTextEntryState();", down);
  const auto downHide = src.find("if (mHide)", down);
  REQUIRE(downSync != std::string::npos);
  CHECK(downSync < downHide);
}

TEST_CASE("F-77 the first click after Esc reaches the control underneath, with no draw in between")
{
  // Not editing yet (panel just shown) or the box is this panel's own: it takes clicks.
  CHECK(volum::ExactEntryTakesClicks(false, false));
  CHECK(volum::ExactEntryTakesClicks(true, true));
  // Esc ended the box: the panel thought it was editing but iPlug2's text entry is no
  // longer ours. It must stop hit-testing at once, so the click falls through.
  CHECK_FALSE(volum::ExactEntryTakesClicks(true, false));

  const auto path = std::filesystem::path(__FILE__).parent_path().parent_path() / "VoLumExactEntry.h";
  std::ifstream in(path, std::ios::binary);
  REQUIRE(in.good());
  std::ostringstream ss;
  ss << in.rdbuf();
  const std::string src = ss.str();

  const auto hit = src.find("bool IsHit(float x, float y) const override");
  REQUIRE(hit != std::string::npos);
  const auto rule = src.find("volum::ExactEntryTakesClicks(mEditing, inEntry == this)", hit);
  const auto base = src.find("IControl::IsHit(x, y)", hit);
  REQUIRE(rule != std::string::npos);
  REQUIRE(base != std::string::npos);
  CHECK(rule < base);
}