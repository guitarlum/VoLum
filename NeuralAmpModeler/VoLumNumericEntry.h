#pragma once

// Parsing for numbers the user types into VoLum's exact-value box.
//
// iPlug2's own path is IParam::StringToValue, whose numeric fallback is atof():
// empty text, whitespace, "abc" and "-" all become 0, which is then constrained and
// applied as a real parameter edit. So mistyping into the exact-value box did not
// cancel - it moved the knob to zero or to its minimum. "1,5" was worse: atof
// stopped at the comma and applied 1.
//
// Rejecting bad input needs the raw string, so VoLumExactEntryControl takes the
// non-parameter completion path and parses here instead. Kept free of iPlug2 so the
// rules are testable directly (see tests/test_volum_numeric_entry.cpp).

#include <cmath>
#include <cstdlib>
#include <string>

namespace volum
{

inline bool IsAsciiSpace(char c)
{
  return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

// True when c can appear in a trailing unit ("dB", "%", "ms", "Hz", "dB/oct").
inline bool IsUnitChar(char c)
{
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '%' || c == '/';
}

// Parses a typed value. Returns false - leaving out untouched - for anything that is
// not a finite number, so the caller can keep the parameter where it was rather than
// applying a fallback the user never asked for.
//
// Accepts a trailing unit, because the value shown next to the box carries one and
// people retype what they see. Accepts a comma as the decimal separator when there
// is no dot: it is what a German keyboard produces, and there is no grouping
// ambiguity in a range of a few digits.
inline bool ParseNumericEntry(const std::string& text, double& out)
{
  std::size_t begin = 0;
  std::size_t end = text.size();
  while (begin < end && IsAsciiSpace(text[begin]))
    ++begin;
  while (end > begin && IsAsciiSpace(text[end - 1]))
    --end;

  if (begin == end)
    return false;

  std::string body = text.substr(begin, end - begin);

  if (body.find('.') == std::string::npos)
  {
    const std::size_t comma = body.find(',');
    if (comma != std::string::npos && body.find(',', comma + 1) == std::string::npos)
      body[comma] = '.';
  }

  const char* const start = body.c_str();
  char* stop = nullptr;
  const double value = std::strtod(start, &stop);

  if (stop == start)
    return false; // nothing numeric at all: "", "abc", "-", "+"

  // strtod backs up over an exponent it cannot complete, so "1e" parses as 1 and
  // leaves "e" behind - which the unit rule below would then wave through, turning a
  // half-typed number into a different, plausible one. No VoLum unit begins with e,
  // so a leftover e can only be that.
  if (*stop == 'e' || *stop == 'E')
    return false;

  // Everything after the number must be whitespace or a unit. This is what rejects
  // "1,5" once the second comma rule does not apply, "1.2.3", and "5-3".
  for (const char* p = stop; *p; ++p)
  {
    if (!IsAsciiSpace(*p) && !IsUnitChar(*p))
      return false;
  }

  // strtod also accepts "nan" and "inf", and overflows "1e999" to HUGE_VAL.
  if (!std::isfinite(value))
    return false;

  out = value;
  return true;
}

} // namespace volum
