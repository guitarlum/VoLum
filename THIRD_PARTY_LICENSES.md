# Third-Party Licenses

VoLum is distributed under the MIT License. It also includes or builds against the following third-party open-source components.

This file is an attribution index. The authoritative license text for each component lives in the referenced upstream project or bundled source.

## Neural Amp Modeler Plugin

- Upstream: https://github.com/sdatkinson/NeuralAmpModelerPlugin
- License: MIT
- Copyright: Steven Atkinson and contributors
- Use in VoLum: original plugin shell that VoLum started from and substantially extends.

## NeuralAmpModelerCore

- Upstream: https://github.com/sdatkinson/NeuralAmpModelerCore
- License: MIT
- Copyright: Steven Atkinson and contributors
- Local path: `NeuralAmpModelerCore/`
- Use in VoLum: NAM model loading and neural amp DSP runtime.

## AudioDSPTools

- Upstream: https://github.com/guitarlum/AudioDSPTools
- License: MIT
- Local path: `AudioDSPTools/`
- Use in VoLum: DSP helpers and effects including delay, reverb, impulse response loading, filters, and gate utilities.

## iPlug2

- Upstream: https://github.com/iPlug2/iPlug2
- VoLum fork: https://github.com/guitarlum/iPlug2
- License: MIT / WDL-OL compatible permissive licensing
- Local path: `iPlug2/`
- Use in VoLum: cross-platform plugin and standalone application framework.

## Eigen

- Upstream: https://gitlab.com/libeigen/eigen
- License: Mozilla Public License 2.0, with portions under permissive BSD-style licenses
- Local paths: `eigen/` and `AudioDSPTools/Dependencies/eigen/`
- Use in VoLum: linear algebra support used by DSP and dependency code.

## doctest

- Upstream: https://github.com/doctest/doctest
- License: MIT
- Copyright: 2016-2023 Viktor Kirilov
- Local path: `NeuralAmpModeler/tests/third_party/doctest.h`
- Use in VoLum: C++ unit test framework.

## Steinberg VST3 SDK

- Upstream: https://github.com/steinbergmedia/vst3sdk
- License: Steinberg VST3 SDK licensing terms
- Use in VoLum: VST3 plugin target through iPlug2.

