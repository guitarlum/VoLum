#include "third_party/doctest.h"

#include "../VoLumChunkSafeRead.h"

#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

// Bounds checking on the length-prefixed strings VoLum reads out of a host state
// chunk: the header, the version, and the two persisted paths. All four are read
// before anything about the bytes has been validated, so a corrupt or hostile
// project file reaches them first.

namespace
{
// Reproduces iPlug2's IByteGetter semantics byte for byte, including the signed
// overflow in GetStr, so these tests exercise the real failure rather than a
// sanitized model of it. Kept verbatim from IPlugStructs.h:73-107.
struct UnsafeChunk
{
  std::vector<uint8_t> bytes;
  // Set by GetStr so a test can prove the unchecked reader was never entered.
  mutable int unsafeReads = 0;
  mutable int largestCopyRequest = 0;

  int Size() const { return static_cast<int>(bytes.size()); }

  int GetBytes(void* pDst, int nBytesToCopy, int startPos) const
  {
    const int endPos = startPos + nBytesToCopy;
    if (startPos >= 0 && endPos <= Size())
    {
      std::memcpy(pDst, bytes.data() + startPos, static_cast<size_t>(nBytesToCopy));
      return endPos;
    }
    return -1;
  }

  template <typename T>
  int Get(T* pDst, int startPos) const
  {
    return GetBytes(pDst, static_cast<int>(sizeof(T)), startPos);
  }

  // Stands in for WDL_String: the only thing that matters is how many bytes the
  // reader would have copied.
  int GetStr(std::string& str, int startPos) const
  {
    ++unsafeReads;

    int len = 0;
    const int strStartPos = GetBytes(&len, static_cast<int>(sizeof(len)), startPos);
    if (strStartPos >= 0)
    {
      // The overflow lives here - but wrapped through unsigned, not by letting the
      // signed addition overflow for real. Written the obvious way, this is UB, and
      // clang on arm64 acts on it: from `strStartPos + len <= Size()` it concludes
      // `len <= Size() - strStartPos`, folds the clamp below to `len`, and the copy
      // that was supposed to be capped at 4 bytes becomes an INT_MAX memmove off the
      // end of the vector. That crashed the whole suite on macOS while passing on
      // MSVC. Unsigned wrap gives the same bit pattern with none of the licence.
      const int strEndPos = static_cast<int>(static_cast<unsigned>(strStartPos) + static_cast<unsigned>(len));
      if (strEndPos <= Size())
      {
        if (len > 0)
        {
          if (len > largestCopyRequest)
            largestCopyRequest = len;
          // The real WDL_String::Set(ptr, len) would read exactly `len` bytes here.
          // Copying only what actually exists keeps the test from segfaulting while
          // still recording the size the reader asked for.
          const int available = Size() - strStartPos;
          const int safe = len < available ? len : available;
          const char* first = reinterpret_cast<const char*>(bytes.data() + strStartPos);
          str.assign(first, static_cast<size_t>(safe));
        }
        else
        {
          str.clear();
        }
      }
      return strEndPos;
    }
    return -1;
  }

  void PutInt(int value)
  {
    const auto* first = reinterpret_cast<const uint8_t*>(&value);
    bytes.insert(bytes.end(), first, first + sizeof(value));
  }

  // Matches IByteChunk::PutStr: the length prefix is strlen, with no terminator
  // written and none counted. Adding a NUL here would have had a later reader
  // conclude the real prefix is off by one and go hunting for a bug that is not
  // there.
  void PutStr(const std::string& s)
  {
    PutInt(static_cast<int>(s.size()));
    bytes.insert(bytes.end(), s.begin(), s.end());
  }

  // A length prefix that lies about how much string follows.
  void PutBogusLen(int declaredLen, int actualPayloadBytes = 4)
  {
    PutInt(declaredLen);
    for (int i = 0; i < actualPayloadBytes; ++i)
      bytes.push_back('A');
  }
};
} // namespace

TEST_CASE("The bounds predicate rejects lengths that overflow the position sum")
{
  constexpr int kMax = std::numeric_limits<int>::max();

  // The exact shape of the bug: start + len wraps negative, so the upstream
  // `strEndPos <= srcSize` check passes and a gigabyte-scale copy is issued.
  CHECK_FALSE(volum::ChunkStrLenInBounds(kMax, 8, 64));
  CHECK_FALSE(volum::ChunkStrLenInBounds(kMax - 1, 8, 64));

  // Negative lengths return a position behind the one they were handed, which
  // silently re-decodes the rest of the chunk from the wrong offset.
  CHECK_FALSE(volum::ChunkStrLenInBounds(-1, 8, 64));
  CHECK_FALSE(volum::ChunkStrLenInBounds(std::numeric_limits<int>::min(), 8, 64));

  // Plainly out of range.
  CHECK_FALSE(volum::ChunkStrLenInBounds(57, 8, 64));
}

TEST_CASE("The bounds predicate accepts every length that genuinely fits")
{
  CHECK(volum::ChunkStrLenInBounds(0, 8, 64));
  CHECK(volum::ChunkStrLenInBounds(1, 8, 64));
  CHECK(volum::ChunkStrLenInBounds(56, 8, 64)); // exact fit to the last byte
  CHECK(volum::ChunkStrLenInBounds(0, 64, 64)); // empty string at the very end
}

TEST_CASE("The bounds predicate rejects impossible positions and sizes")
{
  CHECK_FALSE(volum::ChunkStrLenInBounds(0, -1, 64));
  CHECK_FALSE(volum::ChunkStrLenInBounds(0, 8, -1));
  CHECK_FALSE(volum::ChunkStrLenInBounds(0, 65, 64));
}

TEST_CASE("A well-formed chunk still reads back unchanged")
{
  UnsafeChunk chunk;
  chunk.PutStr("###NeuralAmpModeler###");
  chunk.PutStr("1.2.1");

  std::string header;
  int pos = volum::SafeGetStr(chunk, 0, header);
  REQUIRE(pos > 0);
  CHECK(header == "###NeuralAmpModeler###");

  std::string version;
  pos = volum::SafeGetStr(chunk, pos, version);
  REQUIRE(pos > 0);
  CHECK(version == "1.2.1");
  CHECK(pos == chunk.Size());
}

TEST_CASE("An empty string reads back empty rather than failing")
{
  UnsafeChunk chunk;
  chunk.PutInt(0);

  std::string out = "not touched";
  const int pos = volum::SafeGetStr(chunk, 0, out);
  CHECK(pos == 4);
  CHECK(out.empty());
}

TEST_CASE("An INT_MAX length is refused before the unchecked reader is entered")
{
  UnsafeChunk chunk;
  chunk.PutBogusLen(std::numeric_limits<int>::max());

  std::string out;
  const int pos = volum::SafeGetStr(chunk, 0, out);

  // The assertion that fails against the pre-fix code, which called GetStr
  // directly and let it issue an INT_MAX-byte copy from a 8-byte chunk.
  CHECK(pos == -1);
  CHECK(chunk.unsafeReads == 0);
  CHECK(chunk.largestCopyRequest == 0);
}

TEST_CASE("The unchecked reader really does accept an INT_MAX length")
{
  // Guards the premise of the fix. If iPlug2 ever hardens GetStr this fails and
  // whoever sees it can retire SafeGetStr instead of wondering what it defends.
  UnsafeChunk chunk;
  chunk.PutBogusLen(std::numeric_limits<int>::max());

  std::string out;
  const int endPos = chunk.GetStr(out, 0);

  CHECK(chunk.unsafeReads == 1);
  CHECK(endPos < 0); // the wrapped sum, mistaken for an in-range end position
  CHECK(chunk.largestCopyRequest == std::numeric_limits<int>::max());
}

TEST_CASE("A negative length is refused instead of rewinding the read position")
{
  UnsafeChunk chunk;
  chunk.PutStr("padding so a rewind lands somewhere plausible");
  const int secondField = chunk.Size();
  chunk.PutBogusLen(-16);

  std::string out;
  const int pos = volum::SafeGetStr(chunk, secondField, out);

  CHECK(pos == -1);
  CHECK(chunk.unsafeReads == 0);
}

TEST_CASE("A length one byte past the end is refused")
{
  UnsafeChunk chunk;
  chunk.PutBogusLen(5, /*actualPayloadBytes*/ 4);

  std::string out;
  CHECK(volum::SafeGetStr(chunk, 0, out) == -1);
  CHECK(chunk.unsafeReads == 0);
}

TEST_CASE("A truncated length prefix is refused")
{
  UnsafeChunk chunk;
  chunk.bytes = {0x10, 0x00}; // two of the four prefix bytes

  std::string out;
  CHECK(volum::SafeGetStr(chunk, 0, out) == -1);
  CHECK(chunk.unsafeReads == 0);
}

TEST_CASE("An empty chunk and a negative start position are refused")
{
  UnsafeChunk chunk;

  std::string out;
  CHECK(volum::SafeGetStr(chunk, 0, out) == -1);
  CHECK(volum::SafeGetStr(chunk, -1, out) == -1);
  CHECK(chunk.unsafeReads == 0);
}
