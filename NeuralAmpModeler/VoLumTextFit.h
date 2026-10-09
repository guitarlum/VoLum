#pragma once

// FitTextToWidth's trim loop without IGraphics, plus a one-entry memo of its
// result. The measure callback returns the width MeasureText reports for a
// string, so tests can count measures. VoLumColorHelpers.h wraps both around
// IGraphics::MeasureText.

#include <string>
#include <vector>

namespace volum::textfit
{

// Trims `s` (appending an ellipsis) until measure() fits within maxW. Drops
// whole UTF-8 sequences, never a continuation byte on its own.
template <typename Measure>
std::string Fit(const char* s, float maxW, Measure&& measure)
{
  if (!s || !*s)
    return "";
  std::string str = s;
  if (measure(str.c_str()) <= maxW)
    return str;
  while (str.size() > 1)
  {
    str.pop_back();
    while (!str.empty() && (static_cast<unsigned char>(str.back()) & 0xC0) == 0x80)
      str.pop_back();
    const std::string cand = str + "\xE2\x80\xA6";
    if (measure(cand.c_str()) <= maxW)
      return cand;
  }
  return str + "\xE2\x80\xA6";
}

// Greedy word wrap for short UI copy. Every returned line fits maxW when each
// individual word fits; confirmation copy uses capped library names, so that
// invariant holds without splitting UTF-8 words.
template <typename Measure>
std::vector<std::string> WrapWords(const std::string& text, float maxW, Measure&& measure)
{
  std::vector<std::string> lines;
  std::string line;
  size_t pos = 0;
  while (pos < text.size())
  {
    while (pos < text.size() && text[pos] == ' ')
      ++pos;
    if (pos >= text.size())
      break;
    const size_t end = text.find(' ', pos);
    const std::string word = text.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
    const std::string candidate = line.empty() ? word : line + " " + word;
    if (!line.empty() && measure(candidate.c_str()) > maxW)
    {
      lines.push_back(line);
      line = word;
    }
    else
      line = candidate;
    pos = (end == std::string::npos) ? text.size() : end + 1;
  }
  if (!line.empty())
    lines.push_back(line);
  if (lines.empty())
    lines.emplace_back();
  return lines;
}

// WrapWords limited to what a fixed-height message area can show. Text that does
// not fit is cut at the last visible line, which ends in an ellipsis, so a long
// confirmation never just stops mid-sentence at the clip edge.
template <typename Measure>
std::vector<std::string> WrapWordsClamped(const std::string& text, float maxW, size_t maxLines, Measure&& measure)
{
  auto lines = WrapWords(text, maxW, measure);
  if (maxLines == 0 || lines.size() <= maxLines)
    return lines;
  std::string tail;
  for (size_t i = maxLines - 1; i < lines.size(); ++i)
    tail += (tail.empty() ? "" : " ") + lines[i];
  lines.resize(maxLines);
  lines.back() = Fit(tail.c_str(), maxW, measure);
  return lines;
}

// Everything the measured width depends on besides the string: font face and
// size, alignment, rotation, the width to fit and the backing pixel scale
// (NanoVG rounds glyph metrics at the device scale).
struct FitStyle
{
  const char* font = "";
  float size = 0.f;
  int align = 0;
  int valign = 0;
  float angle = 0.f;
  float maxW = 0.f;
  float scale = 1.f;
};

// Remembers the last fitted string for one text slot (one rail row line).
// Re-measures only when the text or any FitStyle field differs from the last
// call; Reset() forces the next call to re-measure.
class Memo
{
public:
  template <typename Measure>
  const std::string& Get(const char* s, const FitStyle& style, Measure&& measure)
  {
    const char* text = s ? s : "";
    const char* font = style.font ? style.font : "";
    if (!mValid || mText != text || mFont != font || mSize != style.size || mAlign != style.align
        || mVAlign != style.valign || mAngle != style.angle || mMaxW != style.maxW || mScale != style.scale)
    {
      mText.assign(text);
      mFont.assign(font);
      mSize = style.size;
      mAlign = style.align;
      mVAlign = style.valign;
      mAngle = style.angle;
      mMaxW = style.maxW;
      mScale = style.scale;
      mFitted = Fit(text, style.maxW, measure);
      mValid = true;
    }
    return mFitted;
  }

  void Reset() { mValid = false; }

private:
  bool mValid = false;
  std::string mText;
  std::string mFont;
  float mSize = 0.f;
  int mAlign = 0;
  int mVAlign = 0;
  float mAngle = 0.f;
  float mMaxW = 0.f;
  float mScale = 0.f;
  std::string mFitted;
};

} // namespace volum::textfit
