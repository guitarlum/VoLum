# VoLum Effect Staging Notes

This folder documents the effect-staging surface only. The branch deliberately keeps the modes that tested well and removes the WIP modes that need separate future passes.

## Live Staging Surface

- PRE Compressor: 1176-style FET compressor with a hidden -5 dB output calibration at displayed `Output = 0 dB`.
- Delay: `Digital`, `Analog`, and `Reverse`.
- Reverb: `Hall`, `Plate`, and `Oktaverb`.

## Deferred

- Tape delay is removed from staging.
- TremVerb is removed from staging.

## Index

- `[compressor-1176.md](compressor-1176.md)` - PRE 1176-style compressor.
- `[delay-digital.md](delay-digital.md)` - clean digital delay with Ping-Pong toggle.
- `[delay-analog.md](delay-analog.md)` - BBD-style analog delay.
- `[delay-reverse.md](delay-reverse.md)` - restored reverse delay core with Bloom.
- `[reverb-hall.md](reverb-hall.md)` - Hall using the selected Cathedral-ish recipe.
- `[reverb-plate.md](reverb-plate.md)` - restored Dattorro-style plate.
- `[reverb-oktaverb.md](reverb-oktaverb.md)` - repaired `OCT / OCT+5TH / OCT+SUB` voices.