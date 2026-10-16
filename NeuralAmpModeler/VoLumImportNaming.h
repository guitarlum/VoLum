#pragma once

// Names and duplicate detection for files added through Manage > Add (custom IRs
// and pedals). Long filenames often differ only at the end (Cab_..._D57_1,
// Cab_..._D57_2), so shortening keeps both ends, a name that is still taken gets
// a number, and only a byte-identical file is turned away.

#include "VoLumCustomModel.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace volum::custom
{
inline constexpr const char* kEllipsisUtf8 = "\xE2\x80\xA6";
inline constexpr std::size_t kEllipsisBytes = 3;

// Longest suffix of a well-formed UTF-8 string that is at most maxBytes long and
// starts on a whole sequence.
inline std::string Utf8Suffix(const std::string& s, std::size_t maxBytes)
{
  std::size_t at = 0;
  while (at < s.size() && s.size() - at > maxBytes)
  {
    const std::size_t len = Utf8ValidSequenceLength(s, at);
    at += len == 0 ? 1 : len;
  }
  return s.substr(at);
}

// `s` unchanged when it fits fitBytes, otherwise its start and end around an
// ellipsis, with at most keepBytes of the original text. Never splits a UTF-8
// sequence; malformed input is cut at the first bad byte first, like ClampName.
inline std::string MiddleEllipsize(const std::string& s, std::size_t fitBytes, std::size_t keepBytes)
{
  const std::string valid = Utf8Prefix(s, s.size());
  if (valid.size() <= fitBytes)
    return valid;
  if (keepBytes < 2)
    return Utf8Prefix(valid, fitBytes);
  std::string tail = Utf8Suffix(valid, keepBytes * 2 / 5);
  // Start the kept end on a word: "…_D57_1" rather than "…30_D57_1".
  const std::size_t sep = tail.find_first_of("_- .");
  if (sep != std::string::npos && sep > 0 && sep + 2 < tail.size())
    tail.erase(0, sep);
  std::string head = Utf8Prefix(valid, keepBytes - tail.size());
  while (head.size() > 1 && std::string("_- .").find(head.back()) != std::string::npos)
    head.pop_back();
  return head + kEllipsisUtf8 + tail;
}

// The stored-name form: the result, ellipsis included, is at most maxBytes long.
inline std::string MiddleEllipsize(const std::string& s, std::size_t maxBytes)
{
  return MiddleEllipsize(s, maxBytes, maxBytes > kEllipsisBytes ? maxBytes - kEllipsisBytes : 0);
}

// Cab-row IR chip label: up to 12 bytes as is, otherwise 11 bytes of the name
// around an ellipsis, so IRs that differ only at the end still read apart.
inline std::string IrChipLabel(const std::string& name)
{
  return MiddleEllipsize(name, 12, 11);
}

// Library name for an imported file whose filename base is `base`: shortened to
// maxBytes, then " 2", " 3"... while isTaken says the name is in use. Empty when
// base is empty or every candidate is taken.
inline std::string UniqueImportName(const std::string& base, std::size_t maxBytes,
                                    const std::function<bool(const std::string&)>& isTaken)
{
  const std::string first = MiddleEllipsize(base, maxBytes);
  if (first.empty() || !isTaken(first))
    return first;
  for (int n = 2; n < 10000; ++n)
  {
    const std::string tail = " " + std::to_string(n);
    if (tail.size() >= maxBytes)
      break;
    const std::string candidate = MiddleEllipsize(base, maxBytes - tail.size()) + tail;
    if (!isTaken(candidate))
      return candidate;
  }
  return {};
}

// True when both files exist and hold the same bytes.
inline bool FilesIdentical(const std::filesystem::path& a, const std::filesystem::path& b)
{
  std::error_code ec;
  if (a.empty() || b.empty())
    return false;
  const auto sizeA = std::filesystem::file_size(a, ec);
  if (ec)
    return false;
  const auto sizeB = std::filesystem::file_size(b, ec);
  if (ec || sizeA != sizeB)
    return false;
  std::ifstream fa(a, std::ios::binary);
  std::ifstream fb(b, std::ios::binary);
  if (!fa || !fb)
    return false;
  char bufA[16384];
  char bufB[16384];
  while (fa && fb)
  {
    fa.read(bufA, sizeof(bufA));
    fb.read(bufB, sizeof(bufB));
    const auto got = fa.gcount();
    if (got != fb.gcount() || std::char_traits<char>::compare(bufA, bufB, (std::size_t)got) != 0)
      return false;
  }
  return fa.eof() && fb.eof();
}

// Index of the first file in `stored` with the same bytes as `src`, or -1.
inline int FindIdenticalFile(const std::filesystem::path& src, const std::vector<std::filesystem::path>& stored)
{
  for (int i = 0; i < (int)stored.size(); ++i)
    if (FilesIdentical(src, stored[(size_t)i]))
      return i;
  return -1;
}

// Status line after an import: files added under a numbered name because theirs
// was taken ({wanted, given}), and files already in the library ({file, existing
// library name}). Empty when there is nothing to say. It is one line under the
// Manage list, so with both kinds it falls back to counts.
inline std::string ImportOutcomeSummary(const std::vector<std::pair<std::string, std::string>>& renamed,
                                        const std::vector<std::pair<std::string, std::string>>& duplicates)
{
  if (!renamed.empty() && !duplicates.empty())
    return std::to_string(renamed.size()) + " added with a number (name taken), " + std::to_string(duplicates.size())
           + " skipped (already in your library).";
  if (renamed.size() == 1)
    return "\"" + renamed.front().first + "\" was taken - added as \"" + renamed.front().second + "\".";
  if (renamed.size() > 1)
    return std::to_string(renamed.size()) + " names were taken - added with a number.";
  if (duplicates.size() == 1)
    return "Already in your library as \"" + duplicates.front().second + "\" - skipped.";
  if (duplicates.size() > 1)
    return std::to_string(duplicates.size()) + " files are already in your library - skipped.";
  return {};
}
} // namespace volum::custom
