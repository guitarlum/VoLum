#include "third_party/doctest.h"

#include "../VoLumNumericEntry.h"

#include <limits>

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
