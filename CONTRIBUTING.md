# Contributing to the Neural Amp Modeler Plugin

> **VoLum fork** ([guitarlum/VoLum](https://github.com/guitarlum/VoLum)): this document still describes **upstream NAM** (tests, AU, issue links). For VoLum-specific behavior (curated rigs, per-amp JSON persistence, Windows-focused build), use the root [README.md](README.md) and [NeuralAmpModeler/README.md](NeuralAmpModeler/README.md). VoLum ships Windows **standalone + VST3** with `scripts/run-tests-win.ps1` for Release **x64** tests.

Thanks for your interest in the project! Here are a few quick tips to make sure that your PR will go smoothly:

## "Communication is the best policy"
This is a fun, scrappy project. 
Things might change--quickly--including these guidelines.
If you're not sure about something or have a suggestion, reach out!

## Have an idea?
If you have an idea that you'd like to see in the plugin, start by [raising an Issue](https://github.com/sdatkinson/NeuralAmpModelerPlugin/issues/new?assignees=&labels=enhancement&template=feature_request.md&title=%5BFEATURE%5D) and describe what you'd like to see. 
This way, we can be sure that it's something that will fit in nicely with the plan before you start working.

## Working on Issues
If you'd like to work on an [existing Issue](https://github.com/sdatkinson/NeuralAmpModelerPlugin/issues), then speak up in the issue's discussion thread.
I would like to ask that you please try to give me a timeline for your work--I'd hate to have you duplicate work if I know that I'm going to e.g. get to it today and beat you to the punch.

## Testing
The **VoLum fork** includes a small **doctest** suite (`NeuralAmpModeler-Tests`, Release x64): run `NeuralAmpModeler/scripts/run-tests-win.ps1` before landing C++/DSP/UI changes.

The **historical upstream** NAM plugin did not ship unit tests in-tree. If you are contributing to upstream NAM and want to help by proposing a framework, please [raise an Issue](https://github.com/sdatkinson/NeuralAmpModelerPlugin/issues/new?assignees=&labels=enhancement&template=feature_request.md&title=%5BFEATURE%5D).

Either way, there are practical checks that help catch regressions:
- [ ] The standalone plugin builds.
- [ ] The plugin runs.
  - [ ] The plugin makes sound.
  - [ ] You can load a new-style (file) model.
  - [ ] You can load an old-style (directory) model.
  - [ ] You can load a supported IR.
  - [ ] The EQ section works.
- [ ] The VST3 plugin builds and can be loaded in [the VST3 SDK VST3PluginTestHost](https://steinbergmedia.github.io/vst3_dev_portal/pages/What+is+the+VST+3+SDK/Plug-in+Test+Host.html)
  - [ ] The plugin passes all unit tests implemented by the VST3PluginTestHost's unit testing tool.
- [ ] The AU plugin builds.

## Code style
I don't care too much about the specifics of style, but it helps keep things orderly and helps make sure that the changes in a PR are real changes and not just e.g. an IDE replacing tabs with spaces.
Going on the main criterion of ease of adoption, the C++ code (`.cpp` and `.h` files) are formatted according to the LLVM code style that `clang-format` enforces. 
To easily apply the format to your code, run

```bash
bash format.bash
```

and commit the changes.
