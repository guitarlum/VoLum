#pragma once

// Bounds-checked string reads out of a plug-in state chunk.
//
// iPlug2's IByteGetter::GetStr is not safe against hostile or corrupt input:
//
//     int strEndPos = strStartPos + len;      // signed overflow
//     if (strEndPos <= srcSize)               // wrapped value passes
//       str.Set((char*) (pSrc + strStartPos), len);
//
// `len` comes straight from the chunk. A declared length near INT_MAX overflows
// the sum, the wrapped-negative result satisfies the upper-bound check, and
// WDL_String::Set then copies gigabytes from past the end of the chunk. A
// negative length skips the copy but returns a position *behind* the one it was
// given, so the rest of the state is decoded from the wrong offset.
//
// VoLum reads four such strings from every chunk (header, version, NAM path, IR
// path), all before it has any reason to trust the bytes. SafeGetStr validates
// the declared length using arithmetic that cannot overflow and only then hands
// the read to the chunk. Fixing it here rather than in the fork keeps the
// guarantee attached to the code that depends on it and keeps it testable
// without iPlug2 (see tests/test_volum_chunk_safe_read.cpp).

#include <string>

namespace volum
{

// True when a string of declaredLen bytes starting at strStartPos fits inside a
// chunk of chunkSize bytes.
//
// Written as subtraction rather than `strStartPos + declaredLen <= chunkSize`
// precisely because the addition is what overflows. Both operands are known
// non-negative before the comparison, so `chunkSize - strStartPos` cannot
// underflow either.
inline bool ChunkStrLenInBounds(int declaredLen, int strStartPos, int chunkSize)
{
  if (chunkSize < 0 || strStartPos < 0 || strStartPos > chunkSize)
    return false;
  if (declaredLen < 0)
    return false;
  return declaredLen <= chunkSize - strStartPos;
}

// Reads a length-prefixed string, returning the new position or -1 on any
// malformed prefix. Templated on the chunk and string types so tests can supply
// doubles; production passes iplug::IByteChunk and WDL_String.
template <typename Chunk, typename Str>
int SafeGetStr(const Chunk& chunk, int startPos, Str& str)
{
  if (startPos < 0)
    return -1;

  int declaredLen = 0;
  const int strStartPos = chunk.Get(&declaredLen, startPos);
  if (strStartPos < 0)
    return -1;

  if (!ChunkStrLenInBounds(declaredLen, strStartPos, chunk.Size()))
    return -1;

  // Now provably in range, so the unchecked reader cannot overflow.
  return chunk.GetStr(str, startPos);
}

} // namespace volum
