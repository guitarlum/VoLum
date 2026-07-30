#define DOCTEST_CONFIG_IMPLEMENT
#include "third_party/doctest.h"

#include <cstdio>

int main(int argc, char** argv)
{
  // Unbuffered, so the last line printed is the last line that ran. A test binary
  // that dies on a signal never flushes, and CI captures stdout through a pipe -
  // so a crash used to discard up to a block of output and point at a test that
  // had already finished. doctest writes through std::cout, which is synced with
  // stdout by default.
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  doctest::Context context(argc, argv);
  const int res = context.run();
  if (context.shouldExit())
    return res;
  return res;
}
