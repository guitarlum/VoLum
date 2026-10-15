# L5 real-audio findings (dev e858a96d, 2026-10-09 08:03-09:03)

Rig: UA-2X2, ASIO4ALL v2, OUT1 cabled to IN1. VoLum output always routed to device
channel 2 (out1=2, out2=2). Sandboxed LOCALAPPDATA (%TEMP%\volum-ui-sandbox, volum-stress-*).
Machine was under heavy load from other lanes the whole hour (cl, 3x link, 4x NeuralAmpModeler-Tests, ~690 MB free RAM).

## Bugs
- L5-1 Dual Amp toggle is a hard switch (glitch). out/scan-ds512/isolated.txt, analysis-v2.txt; 6/6 toggles.
- L5-2 Smaller isolated steps on Comp (3/4), POST pedal (2/4) and amp-switch (4/8) toggles (glitch, low confidence). same files.
- L5-3 DirectSound never offers 48 kHz for the UA-2X2 (11025/22050/44100/96000); ASIO@48k -> DS raises an ASIO-worded "Sample Rate" notice (UX). out/switch/applies.csv, out/persist/.
- L5-4 DirectSound stream has phase jumps (dropped/repeated blocks) in steady play: sanity 3.6-4.2 s, scan 40 s and 75 s; tone control clean (glitch, DS only). out/sanity-512, out/scan-ds512/analysis.txt.

## Not reproduced
- F-95: 0/12 refused relaunches (out/f95-asio/f95.txt).
- Driver/buffer/rate: 25 applies, all streams opened, VoLum responsive; persistence 7/7 (out/switch, out/persist).
- stress-standalone-rate-switch: 4/4 cycles (out/stress.console.txt); fixed script 2/2 (out/stress-seedini.console.txt).

## CPU (% of one core, ASIO4ALL 128 @ 48k, out/cpu/cpu.json)
Soldano single BUILD 68.1 / PLAY 90.8; Marshall 2204 single 29.4 / 51.9; Marshall + THC Sunset dual 48.0 / 65.3.
