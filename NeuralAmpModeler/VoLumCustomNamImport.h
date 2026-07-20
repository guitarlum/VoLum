#pragma once

#include "VoLumContentStore.h"

#include <exception>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace volum
{
namespace content
{

// Result of preparing a custom-amp manifest for persistence. The registry is
// deliberately not mutated here: callers commit `amp` only after success.
struct PreparedCustomNamImport
{
  custom::CustomAmp amp;
  std::string error;
  std::vector<std::string> copiedPaths;

  explicit operator bool() const { return error.empty(); }
};

// Copy newly-picked captures into the content store and validate every resolved
// capture before the caller commits the amp manifest. Any failure removes every
// file copied by this transaction, preserving the previous registry/amp.
//
// `validator(path)` returns "" on success or a user-facing reason on failure.
template <typename Validator>
PreparedCustomNamImport PrepareCustomNamImport(ContentStore& store, const custom::CustomAmp& draft,
                                               const std::string& ampId, Validator&& validator)
{
  PreparedCustomNamImport result;
  result.amp = draft;

  auto fail = [&](const custom::CustomNamFile& file, std::string reason) {
    for (const auto& rel : result.copiedPaths)
      store.RemoveStoredFile(rel);
    result.copiedPaths.clear();
    result.error = "\"" + file.file + "\": " + std::move(reason);
    return result;
  };

  // Use a transaction-specific prefix even when editing an existing amp. That
  // prevents copy_file(overwrite_existing) from replacing a working capture
  // before all files have passed validation.
  const std::string transactionPrefix = ampId + "_" + MintRawId("import");

  for (size_t i = 0; i < result.amp.files.size(); ++i)
  {
    auto& file = result.amp.files[i];
    if (!file.sourcePath.empty())
    {
      const std::string rel = store.ImportFileCopy(
        PathFromUtf8(file.sourcePath), "amps", transactionPrefix + "_" + std::to_string(i));
      if (rel.empty())
        return fail(file, "could not copy the capture into the VoLum content library");
      file.storedPath = rel;
      result.copiedPaths.push_back(rel);
    }

    if (file.storedPath.empty())
      return fail(file, "the saved capture path is empty");

    const std::filesystem::path resolved = store.ResolveStored(file.storedPath);
    std::error_code ec;
    if (!std::filesystem::is_regular_file(resolved, ec) || ec)
      return fail(file, "the copied capture is missing or unreadable");

    std::string validationError;
    try
    {
      validationError = validator(resolved);
    }
    catch (const std::exception& e)
    {
      validationError = e.what();
    }
    catch (...)
    {
      validationError = "unknown NAM validation error";
    }
    if (!validationError.empty())
      return fail(file, std::move(validationError));
  }

  for (auto& file : result.amp.files)
    file.sourcePath.clear();
  return result;
}

} // namespace content
} // namespace volum
