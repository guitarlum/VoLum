# Contributing to VoLum

> **VoLum** ([guitarlum/VoLum](https://github.com/guitarlum/VoLum)): some older guidance below still comes from the upstream NAM plugin shell. For current VoLum behavior (curated rigs, PRE/POST effects, per-amp JSON persistence, and packaging), use the root [README.md](README.md) and [NeuralAmpModeler/README.md](NeuralAmpModeler/README.md). VoLum ships Windows/macOS **standalone + VST3** builds and uses `NeuralAmpModeler/scripts/run-tests-win.ps1` for Release **x64** Windows tests.

Thanks for your interest in the project! Here are a few quick tips to make sure that your PR will go smoothly:

## "Communication is the best policy"
This is a fun, scrappy project. 
Things might change--quickly--including these guidelines.
If you're not sure about something or have a suggestion, reach out!

## Have an idea?
If you have an idea that you'd like to see in VoLum, start by [raising an Issue](https://github.com/guitarlum/VoLum/issues/new) and describe what you'd like to see.
This way, we can be sure that it's something that will fit in nicely with the plan before you start working.

## Working on Issues
If you'd like to work on an [existing Issue](https://github.com/guitarlum/VoLum/issues), then speak up in the issue's discussion thread.
Please share the scope you plan to take so parallel work stays easy to coordinate.

## Testing
VoLum includes a **doctest** suite (`NeuralAmpModeler-Tests`) that runs on Windows and macOS CI. Run `NeuralAmpModeler/scripts/run-tests-win.ps1` on Windows, or `NeuralAmpModeler/scripts/run-tests-mac.sh` on macOS, before landing C++/DSP/UI changes.

Use the test map in `NeuralAmpModeler/README.md` when deciding what to update. In short: DSP changes need focused doctests, parameter/state changes need `test_eparam_order.cpp`, `test_keyboard_steps.cpp`, `test_volum_chunk_version.cpp`, or `test_volum_chunk_codec.cpp`, main amp `.nam` changes need `test_nam_rigs.cpp`, and PRE capture `.nam` changes under `rigs/PrePedals/` need discovery, load, and package coverage.

The **historical upstream** NAM plugin did not ship unit tests in-tree. VoLum keeps its own focused doctest suite and release packaging checks in this repository.

CI also builds and verifies Windows/macOS packages, runs VST3 validation through pluginval, uses the Steinberg validator when available, and smoke-tests the Windows installer. Manual standalone/DAW checks are still useful for audio feel and visual review, but they should not be the only regression coverage for a shipped behavior.

## Code style
I don't care too much about the specifics of style, but it helps keep things orderly and helps make sure that the changes in a PR are real changes and not just e.g. an IDE replacing tabs with spaces.
Going on the main criterion of ease of adoption, the C++ code (`.cpp` and `.h` files) are formatted according to the LLVM code style that `clang-format` enforces. 
To easily apply the format to your code, run

```bash
bash format.bash
```

and commit the changes.
