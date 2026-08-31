#pragma once

// A `.volumpack` is a zip archive, and this is the whole zip implementation.
//
// STORE only - method 0, no compression. That is a deliberate constraint, not a
// shortcut: the payload is already-compressed `.nam` and `.wav` files plus a few KB
// of JSON, so deflate would buy almost nothing, and it would cost a compression
// dependency in a plugin that ships as a single binary on three hosts. Method 0
// needs a CRC32 and a handful of little-endian records, which fits in one header.
//
// Written to be read by anything: a Pack opens in Explorer, Finder, and every
// unzip tool, so a user whose VoLum will not start can still get their captures
// back out by hand. That is worth more than a few percent of file size.
//
// Deliberately knows nothing about VoLum content: it moves named byte blobs. The
// manifest and the library semantics are in VoLumPack.h.
//
// Tests: tests/test_volum_pack.cpp.

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace volum::pack
{

struct ArchiveEntry
{
  std::string name; // forward-slash relative path inside the archive
  std::string data;
};

// ---------------------------------------------------------------------------
// CRC32 (zip's polynomial, reflected)
// ---------------------------------------------------------------------------

inline uint32_t Crc32(const std::string& data, uint32_t seed = 0)
{
  static uint32_t table[256];
  static bool built = false;
  if (!built)
  {
    for (uint32_t i = 0; i < 256; ++i)
    {
      uint32_t c = i;
      for (int k = 0; k < 8; ++k)
        c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
      table[i] = c;
    }
    built = true;
  }
  uint32_t crc = seed ^ 0xFFFFFFFFu;
  for (unsigned char b : data)
    crc = table[(crc ^ b) & 0xFFu] ^ (crc >> 8);
  return crc ^ 0xFFFFFFFFu;
}

namespace detail
{
inline void PutU16(std::string& out, uint16_t v)
{
  out.push_back(static_cast<char>(v & 0xFF));
  out.push_back(static_cast<char>((v >> 8) & 0xFF));
}

inline void PutU32(std::string& out, uint32_t v)
{
  for (int i = 0; i < 4; ++i)
    out.push_back(static_cast<char>((v >> (8 * i)) & 0xFF));
}

inline uint16_t GetU16(const std::string& in, size_t at)
{
  if (at + 2 > in.size())
    return 0;
  return static_cast<uint16_t>(static_cast<unsigned char>(in[at]) | (static_cast<unsigned char>(in[at + 1]) << 8));
}

inline uint32_t GetU32(const std::string& in, size_t at)
{
  if (at + 4 > in.size())
    return 0;
  uint32_t v = 0;
  for (int i = 0; i < 4; ++i)
    v |= static_cast<uint32_t>(static_cast<unsigned char>(in[at + i])) << (8 * i);
  return v;
}

inline constexpr uint32_t kLocalSig = 0x04034b50u;
inline constexpr uint32_t kCentralSig = 0x02014b50u;
inline constexpr uint32_t kEocdSig = 0x06054b50u;

// A name that escapes the archive root, or that a filesystem would refuse. Checked
// on both write and read: an archive is a file from the internet, and "../" in an
// entry name is the oldest way to make an unzip write outside its target.
inline bool SafeEntryName(const std::string& name)
{
  if (name.empty() || name.size() > 512)
    return false;
  if (name.front() == '/' || name.front() == '\\')
    return false;
  if (name.find('\\') != std::string::npos)
    return false; // archives use '/', a backslash is a name here
  if (name.find(':') != std::string::npos)
    return false; // drive letters, NTFS streams
  size_t start = 0;
  while (start <= name.size())
  {
    const size_t slash = name.find('/', start);
    const std::string part = name.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
    if (part == ".." || part == "." || part.empty())
      return false;
    if (slash == std::string::npos)
      break;
    start = slash + 1;
  }
  return true;
}
} // namespace detail

// ---------------------------------------------------------------------------
// Write
// ---------------------------------------------------------------------------

// Serialize entries into a STORE-method zip. Returns "" when an entry name is not
// safe to write (see SafeEntryName), because a Pack we would refuse to read is not
// a Pack worth writing.
inline std::string BuildArchive(const std::vector<ArchiveEntry>& entries)
{
  using namespace detail;
  std::string out;
  struct Central
  {
    std::string name;
    uint32_t crc;
    uint32_t size;
    uint32_t offset;
  };
  std::vector<Central> central;
  central.reserve(entries.size());

  for (const auto& e : entries)
  {
    if (!SafeEntryName(e.name))
      return {};
    const uint32_t crc = Crc32(e.data);
    const uint32_t size = static_cast<uint32_t>(e.data.size());
    const uint32_t offset = static_cast<uint32_t>(out.size());

    PutU32(out, kLocalSig);
    PutU16(out, 20); // version needed
    PutU16(out, 0); // flags
    PutU16(out, 0); // method 0 = STORE
    PutU16(out, 0); // mod time - fixed, so the same library exports byte-identical
    PutU16(out, 0); // mod date
    PutU32(out, crc);
    PutU32(out, size); // compressed
    PutU32(out, size); // uncompressed
    PutU16(out, static_cast<uint16_t>(e.name.size()));
    PutU16(out, 0); // extra len
    out += e.name;
    out += e.data;

    central.push_back({e.name, crc, size, offset});
  }

  const uint32_t cdOffset = static_cast<uint32_t>(out.size());
  for (const auto& c : central)
  {
    PutU32(out, kCentralSig);
    PutU16(out, 20); // version made by
    PutU16(out, 20); // version needed
    PutU16(out, 0); // flags
    PutU16(out, 0); // method
    PutU16(out, 0); // time
    PutU16(out, 0); // date
    PutU32(out, c.crc);
    PutU32(out, c.size);
    PutU32(out, c.size);
    PutU16(out, static_cast<uint16_t>(c.name.size()));
    PutU16(out, 0); // extra
    PutU16(out, 0); // comment
    PutU16(out, 0); // disk
    PutU16(out, 0); // internal attrs
    PutU32(out, 0); // external attrs
    PutU32(out, c.offset);
    out += c.name;
  }
  const uint32_t cdSize = static_cast<uint32_t>(out.size()) - cdOffset;

  PutU32(out, kEocdSig);
  PutU16(out, 0); // this disk
  PutU16(out, 0); // disk with cd
  PutU16(out, static_cast<uint16_t>(central.size()));
  PutU16(out, static_cast<uint16_t>(central.size()));
  PutU32(out, cdSize);
  PutU32(out, cdOffset);
  PutU16(out, 0); // comment len
  return out;
}

// ---------------------------------------------------------------------------
// Read
// ---------------------------------------------------------------------------

struct ReadResult
{
  bool ok = false;
  std::string error;
  std::vector<ArchiveEntry> entries;

  explicit operator bool() const { return ok; }
  const std::string* Find(const std::string& name) const
  {
    for (const auto& e : entries)
      if (e.name == name)
        return &e.data;
    return nullptr;
  }
};

// Parse a STORE-method zip. Every failure is reported rather than thrown, and a
// failure means no entries at all: a half-read Pack must never look like a small
// Pack, or a truncated download would silently import as a partial library.
inline ReadResult ParseArchive(const std::string& blob)
{
  using namespace detail;
  ReadResult res;
  auto fail = [](std::string why) {
    ReadResult bad;
    bad.error = std::move(why);
    return bad;
  };
  if (blob.size() < 22)
    return fail("not a Pack file (too small)");

  // The end-of-central-directory record is last, but may be followed by a comment,
  // so scan backwards for its signature.
  size_t eocd = std::string::npos;
  const size_t lowest = blob.size() >= 22 + 65535 ? blob.size() - (22 + 65535) : 0;
  for (size_t i = blob.size() - 22 + 1; i-- > lowest;)
  {
    if (GetU32(blob, i) == kEocdSig)
    {
      eocd = i;
      break;
    }
  }
  if (eocd == std::string::npos)
    return fail("not a Pack file (no archive directory)");

  const uint16_t count = GetU16(blob, eocd + 10);
  const uint32_t cdSize = GetU32(blob, eocd + 12);
  const uint32_t cdOffset = GetU32(blob, eocd + 16);
  if (static_cast<size_t>(cdOffset) + cdSize > blob.size())
    return fail("Pack file is truncated");

  size_t at = cdOffset;
  for (uint16_t i = 0; i < count; ++i)
  {
    if (at + 46 > blob.size() || GetU32(blob, at) != kCentralSig)
      return fail("Pack directory is damaged");
    const uint16_t method = GetU16(blob, at + 10);
    const uint32_t crc = GetU32(blob, at + 16);
    const uint32_t csize = GetU32(blob, at + 20);
    const uint32_t usize = GetU32(blob, at + 24);
    const uint16_t nameLen = GetU16(blob, at + 28);
    const uint16_t extraLen = GetU16(blob, at + 30);
    const uint16_t commentLen = GetU16(blob, at + 32);
    const uint32_t localOffset = GetU32(blob, at + 42);
    if (at + 46 + nameLen > blob.size())
      return fail("Pack directory is damaged");
    const std::string name = blob.substr(at + 46, nameLen);
    at += 46 + nameLen + extraLen + commentLen;

    if (name.empty() || name.back() == '/')
      continue; // directory entry: nothing to extract
    if (method != 0)
      return fail("Pack uses an unsupported compression method");
    if (!SafeEntryName(name))
      return fail("Pack contains an unsafe file path");
    if (csize != usize)
      return fail("Pack directory is damaged");

    // The local header repeats the name; the data follows it.
    if (static_cast<size_t>(localOffset) + 30 > blob.size() || GetU32(blob, localOffset) != kLocalSig)
      return fail("Pack file is damaged");
    const uint16_t localNameLen = GetU16(blob, localOffset + 26);
    const uint16_t localExtraLen = GetU16(blob, localOffset + 28);
    const size_t dataAt = static_cast<size_t>(localOffset) + 30 + localNameLen + localExtraLen;
    if (dataAt + usize > blob.size())
      return fail("Pack file is truncated");
    std::string data = blob.substr(dataAt, usize);
    if (Crc32(data) != crc)
      return fail("Pack file is corrupt (checksum mismatch in \"" + name + "\")");
    res.entries.push_back({name, std::move(data)});
  }

  res.ok = true;
  return res;
}

// ---------------------------------------------------------------------------
// Filesystem convenience
// ---------------------------------------------------------------------------

inline bool ReadWholeFile(const std::filesystem::path& path, std::string& out)
{
  std::ifstream in(path, std::ios::binary);
  if (!in.good())
    return false;
  out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  return !in.bad();
}

inline bool WriteWholeFile(const std::filesystem::path& path, const std::string& data)
{
  std::error_code ec;
  const auto parent = path.parent_path();
  if (!parent.empty())
  {
    std::filesystem::create_directories(parent, ec);
    if (ec)
      return false;
  }
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out.good())
    return false;
  out.write(data.data(), static_cast<std::streamsize>(data.size()));
  out.close();
  return out.good();
}

inline bool WriteArchiveToFile(const std::filesystem::path& path, const std::vector<ArchiveEntry>& entries)
{
  const std::string blob = BuildArchive(entries);
  if (blob.empty())
    return false;
  return WriteWholeFile(path, blob);
}

inline ReadResult ReadArchiveFromFile(const std::filesystem::path& path)
{
  std::string blob;
  if (!ReadWholeFile(path, blob))
  {
    ReadResult res;
    res.error = "could not read the Pack file";
    return res;
  }
  return ParseArchive(blob);
}

} // namespace volum::pack
