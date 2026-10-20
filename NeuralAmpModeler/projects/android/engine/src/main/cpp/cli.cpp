#include "engine.h"

#include <exception>
#include <iostream>

int main(int argc, char** argv)
{
  if (argc != 4)
  {
    std::cerr << "usage: volum_engine_render <input.wav> <rig.nam> <output.wav>\n";
    return 2;
  }
  try
  {
    const auto result = volum::android::engine::RenderFixedChain(argv[1], argv[2], argv[3]);
    std::cout << volum::android::engine::ResultJson(result) << '\n';
    return 0;
  }
  catch (const std::exception& error)
  {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
