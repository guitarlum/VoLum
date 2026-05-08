# VoLum Effect Staging Notes

This folder documents the effect-staging surface only. The branch deliberately keeps the modes that tested well and removes the WIP modes that need separate future passes.

## Live Staging Surface

- PRE Compressor: 1176-style FET compressor with a hidden -5 dB output calibration at displayed `Output = 0 dB`.
- Delay: `Digital`, `Analog`, and `Reverse`.
- Reverb: `Hall`, `Plate`, and `Oktaverb`.

## Deferred

- Tape delay is removed from staging.
- TremVerb is removed from staging.
# VoLum Effects Design Guides

This folder contains internal design documents for VoLum's built-in compressor,
delay, and reverb effects. They describe **target sonic behavior**, **DSP block
diagrams**, and **parameter ranges**, with citations to publicly available
references.

These documents are for VoLum's developers and curious users browsing the
source. They are not user-facing manuals. The user-facing manual lives at
`[../../user-guide.en.md](../../user-guide.en.md)` and
`[../../user-guide.de.md](../../user-guide.de.md)`.

## Trademark and affiliation disclaimer

VoLum is **not affiliated with, endorsed by, or sponsored by** any of the
following companies or their products. References to specific gear (such as
"1176-style", "Memory-Man-inspired", "Echoplex-flavored", "Specular Tempus",
"Strymon Flint", "Valhalla Plate", "EMT 140", "Eventide Blackhole",
"Roland Space Echo RE-201") are made for the sole purpose of describing the
**target sonic character** and **public DSP techniques** behind each effect.
All trademarks and product names are the property of their respective owners,
including but not limited to:

- Universal Audio, Inc. ("1176", "UAD")
- Electro-Harmonix / New Sensor Corp. ("Deluxe Memory Man")
- Roland Corporation ("Space Echo", "RE-201")
- Maestro / Gibson Brands, Inc. ("Echoplex", "EP-3")
- Strymon / Damage Control Engineering, LLC ("Flint", "Volante", "Brigadier",
"Timeline", "BigSky")
- GFI System ("Specular Tempus")
- Valhalla DSP, LLC ("ValhallaPlate", "ValhallaShimmer")
- Eventide, Inc. ("Blackhole", "H3000")
- EMT GmbH ("EMT 140", "EMT 240")
- Lexicon ("480L")

VoLum implements its own DSP using **public, textbook techniques** (FDN
reverbs, allpass diffusers, BBD-style fractional-delay modeling, FET-style
detector-and-VCA topology, Schroeder/Moorer/Dattorro-derived structures, etc.)
and is informed by **publicly available reference material** (academic papers,
service manuals, blog posts, manufacturer manuals). VoLum does **not**
incorporate or reverse-engineer source code from any other product.

## What is in scope here

Each `*.md` file in this folder corresponds to one VoLum effect mode and
contains:

- A short prose description of the **target sound** and which classic gear
inspired it.
- A list of **public reference sources** (URLs / titles / authors) used to
inform the design. Source content is paraphrased, not reproduced.
- A **signal-flow diagram** in ASCII showing the actual VoLum DSP topology.
- A **DSP block list** describing each stage's role.
- **Parameter ranges, default targets, and curves** that the implementation
should hit.
- **Validation/listening tests** to confirm the implementation matches the
target.

## What is NOT in this folder

- No verbatim text from manuals or papers.
- No reproduced schematic images.
- No code from other plugins.
- No copyrighted reference recordings.

Anything IP-sensitive (raw schematic snippets, manual excerpts, reference
recordings collected during research) lives in
`docs/design/effects/_working_notes/`, which is gitignored.

## Index

### Compressor

- `[compressor-1176.md](compressor-1176.md)` - 1176-style FET compressor

### Delay (mode order: Digital, Analog, Tape, Reverse)

- `[delay-digital.md](delay-digital.md)` - clean digital line + global
ping-pong toggle
- `[delay-analog-memoryman.md](delay-analog-memoryman.md)` - BBD-style analog
delay with optical chorus and compander
- `[delay-tape.md](delay-tape.md)` - tape echo with wow/flutter/saturation +
3-way sub-toggle
- `[delay-reverse.md](delay-reverse.md)` - reverse delay with shaped fade

### Reverb (mode order: Hall, Plate, Oktaverb, TremVerb)

- `[reverb-hall.md](reverb-hall.md)` - 8-line FDN hall + 3-way sub-toggle
- `[reverb-plate.md](reverb-plate.md)` - Dattorro-derived plate + 3-way
sub-toggle
- `[reverb-oktaverb.md](reverb-oktaverb.md)` - shimmer reverb (octave-up +
fifth) + 3-way sub-toggle
- `[reverb-tremverb.md](reverb-tremverb.md)` - short plate/room base +
photocell tremolo on wet