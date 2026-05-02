**Thanks for making a Pull Request!**
Please fill out this template so that you can be sure that your PR does everything it needs to be accepted.

## Description
What does your PR do?
Include [Closing words](https://docs.github.com/en/issues/tracking-your-work-with-issues/using-issues/linking-a-pull-request-to-an-issue) to link this PR to the Issue(s) that it relates to.

## PR Checklist
- [ ] Did you format your code using `bash format.bash`?
- [ ] Did you add/update focused doctests for the behavior changed?
- [ ] Did you run `NeuralAmpModeler/scripts/run-tests-win.ps1` or `NeuralAmpModeler/scripts/run-tests-mac.sh`?
- [ ] If this changes main amp `.nam` files under `rigs/`, did you update `test_nam_rigs.cpp` coverage as needed?
- [ ] If this changes PRE captures under `rigs/PrePedals/`, did you update discovery/load/package coverage?
- [ ] If this changes packaging, installers, or VST3 layout, did you update the verify/validation scripts?
- [ ] Does your PR add, remove, or rename any plugin parameters? If yes...
  - [ ] Have you ensured that the plug-in unserializes correctly?
  - [ ] Have you updated `test_eparam_order.cpp`, `test_keyboard_steps.cpp`, `test_volum_chunk_version.cpp`, or `test_volum_chunk_codec.cpp`?
  - [ ] Have you ensured that _older_ versions of the plug-in load correctly? (See `NeuralAmpModeler/Unserialization.cpp`.)
- [ ] Does your PR add or remove any graphical assets? If yes, are they defined in [config.h](https://github.com/olilarkin/NeuralAmpModelerPlugin/blob/main/NeuralAmpModeler/config.h) and added in the two required locations in [main.rc](https://github.com/olilarkin/NeuralAmpModelerPlugin/blob/main/NeuralAmpModeler/resources/main.rc)?
  
