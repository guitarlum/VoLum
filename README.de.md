**Sprachen:** [English](README.md) | Deutsch

# VoLum

**Open-Source-Gitarren-Amps für Bühne, Studio und Übungsplatz.**

![VoLum PLAY mit Dual Amp: die Bilder von THC Sunset und Soldano SLO100 bewegen sich mit der Gitarre](docs/volum-dual-amp.gif)

VoLum öffnet in **PLAY**: rechts deine Sounds, darunter acht Fußschalter und dazu Amp-Bilder, die sich mit deinem Spiel bewegen. Klicke einen Sound an oder schalte mit einem MIDI-Fußcontroller durch, schalte Pedale ein und aus und spiel. Fünf Werk-Sounds liegen auf den Programmen 0 bis 4 bereit, du kannst also ab der ersten Minute spielen.

Willst du einen Sound ändern, wechsle zu **BUILD**. Ein Hinweis **Tweak your sound** zeigt auf den Schalter, bis du ihn einmal benutzt hast.

[VoLum herunterladen](https://github.com/guitarlum/VoLum/releases) oder das [Benutzerhandbuch](docs/user-guide.de.md) lesen.

## Was VoLum Besonders Macht

- **Zum Spielen gemacht:** PLAY ist das ganze Rig auf einem Bildschirm: deine Sounds, PRE- und POST-Fußschalter, die Meter, der Tuner und das Metronom.
- **MIDI-Aufruf ohne Learn:** 128 Programmnummern, dieselbe Liste wie in PLAY. Sende Program Change, oder den Recall CC (102), wenn dein Host Program Change nicht weitergibt.
- **Dual Amp:** zwei Amps gleichzeitig, jeder mit eigenem Kanal, Cab oder IR, eigenen Reglern und Panorama.
- **15 kuratierte Amps, 21 Werk-Presets:** leg jedes davon ohne Speichern auf PLAY.
- **Eigene Inhalte:** lade eigene NAM-Amps, IRs und Pedale. Ein `.volumpack` sichert, zieht um oder teilt alles in einer Datei.
- **Standalone, VST3 und AU:** läuft auf dem [Neural Amp Modeler](https://github.com/sdatkinson/NeuralAmpModelerCore)-Kern, unter Windows und macOS.
- **Nur Update-Hinweis:** VoLum kann dir sagen, dass es eine neuere Version gibt. Es lädt nie selbst etwas herunter.

## Download

[![Build status](https://github.com/guitarlum/VoLum/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/guitarlum/VoLum/actions/workflows/ci.yml)

Stabile Pakete gibt es unter **[Releases](https://github.com/guitarlum/VoLum/releases)**. **[Actions -> CI](https://github.com/guitarlum/VoLum/actions/workflows/ci.yml)** hat nur Preview-Builds aus dem aktuellen Entwicklungsstand.

| Plattform | Paket | Wann du es nimmst |
| --- | --- | --- |
| Windows | `VoLum-vX.Y.Z-windows-setup.exe` | Einfachste Installation: Standalone-App, VST3 und mitgelieferte Rigs. |
| Windows | `VoLum-vX.Y.Z-windows-portable.zip` | Portable oder automatisierte Installation. `VoLum.vst3` und `VoLumRigs` zusammenhalten. |
| macOS | `VoLum-vX.Y.Z-macos-installer.dmg` | Einfachste Installation: Standalone-App, VST3, AU und mitgelieferte Rigs. |
| macOS | `VoLum-vX.Y.Z-macos-standalone.dmg` | Nur die Standalone-App. |
| macOS | `VoLum-vX.Y.Z-macos-vst3.zip` | Manuelle VST3-Installation. |
| macOS | `VoLum-vX.Y.Z-macos-component.zip` | Manuelle AU-Installation, für Logic Pro und GarageBand. |

Nicht jedes Release enthält jedes Paket. Öffne die Release-Seite und wähle das Paket für dein System.

## Wichtiger Sicherheitshinweis

Die Signierung der VoLum-Releases ist noch im Aufbau. Siehe die [Code-Signing-Policy](CODE_SIGNING.md).

- **Windows:** SmartScreen kann vor einem unbekannten Herausgeber warnen. Wenn du der Quelle vertraust, wähle **Weitere Informationen -> Trotzdem ausführen**.
- **macOS:** Gatekeeper kann App oder Installer blockieren. Versuche sie einmal zu öffnen, geh dann zu **Systemeinstellungen -> Datenschutz & Sicherheit** und klicke **Trotzdem öffnen**.
- **macOS-Plug-in-Zips:** zeigt die DAW das Plug-in nach einem Rescan immer noch nicht, entferne die Quarantäne-Markierung:

```bash
xattr -cr ~/Library/Audio/Plug-Ins/VST3/VoLum.vst3
xattr -cr ~/Library/Audio/Plug-Ins/Components/VoLum.component
```

Behandle Preview-Builds aus CI wie unsignierte Test-Builds.

## Schnellinstallation

### Windows Installer

Starte `VoLum-vX.Y.Z-windows-setup.exe`. Der Installer legt ab:

- `VoLum.exe` unter `C:\Program Files\VoLum`
- `VoLum.vst3` unter `C:\Program Files\Common Files\VST3`
- `VoLumRigs` im VoLum-Installationsordner

Das VST3 findet die mitgelieferten Rigs automatisch.

### Windows Portable

Entpacke `VoLum-vX.Y.Z-windows-portable.zip`. Für die Standalone-App startest du `VoLum_x64.exe`. Für das VST3 kopierst du beide Ordner in deinen VST3-Ordner:

```text
C:\Program Files\Common Files\VST3\
  VoLum.vst3\
  VoLumRigs\
```

### macOS Installer

Öffne `VoLum-vX.Y.Z-macos-installer.dmg` und starte `VoLum Installer.pkg`. Er installiert die Standalone-App, das VST3, das AU und die mitgelieferten Rigs.

### macOS Standalone

Öffne `VoLum-vX.Y.Z-macos-standalone.dmg`, zieh `VoLum.app` nach **Programme** und starte es. Die App enthält die mitgelieferten Rigs.

### macOS VST3-Zip

Entpacke `VoLum-vX.Y.Z-macos-vst3.zip` und leg `VoLum.vst3` und `VoLumRigs` beide in deinen VST3-Ordner:

```text
~/Library/Audio/Plug-Ins/VST3/
  VoLum.vst3/
  VoLumRigs/
```

Scanne die Plug-ins in deiner DAW neu.

### macOS AU-Zip

Entpacke `VoLum-vX.Y.Z-macos-component.zip` und leg `VoLum.component` und `VoLumRigs` beide in deinen Components-Ordner:

```text
~/Library/Audio/Plug-Ins/Components/
  VoLum.component/
  VoLumRigs/
```

Starte deine DAW neu, damit sie das neue AU findet.

### Linux

Es gibt keinen nativen Linux-Build. Einige Nutzer berichten, dass das Windows-VST3 unter Linux mit [yabridge](https://github.com/robbert-vdh/yabridge) gut läuft; VoLum testet diesen Weg aber nicht.

## Mitgelieferte Amps

| Amp | Kanäle |
| --- | --- |
| Ampete One | 4 |
| Bad Cat Mini Cat | 3 |
| Brunetti XL 2 | 3 |
| Diezel Herbert Mk1 | 4 |
| Fryette Deliverance 120 | 2 |
| H&K TriAmp Mk2 | 6 |
| Lichtlaerm Prometheus | 3 |
| Marshall 2204 1982 | 6 |
| Marshall JMP 2203 1976 | 6 |
| Marshall JVM 210H OD1 | 6 |
| Orange OD120 1975 | 5 |
| Orange ORS100 1972 | 2 |
| Sebago Texas Flood | 2 |
| Soldano SLO100 | 3 |
| THC Sunset | 5 |

Jeder Amp hat vier Cab-Optionen: **No Cab** (nur der Amp), **G12**, **G65** und **V30**. Du kannst auch dein eigenes Cab-IR laden.

## Deine Sounds

PLAY startet mit fünf Werk-Sounds auf den Programmen 0 bis 4: The bestest Clean, SLO Crunch, Modern Rhythm, Crack the Skye und Ampete Lead. Ersetze sie oder füge eigene hinzu: leg ein beliebiges Werk-Preset oder ein in BUILD gespeichertes User-Preset auf eine Programmnummer.

## BUILD

In BUILD formst du einen Sound: wähle Amp und Kanal, ein Cab oder dein eigenes IR, stell die Regler ein, füge PRE-Pedale (Pitch mit Transpose und Octaver, ein Kompressor, zwei NAM-Pedal-Slots) und POST-Chorus, -Delay, -Reverb und -Tremolo hinzu und speichere alles als Preset. Mit `P` oder dem Schalter oben rechts wechselst du hin und her; der Wechsel ändert nie, was du hörst.

<p align="center">
  <img src="docs/user-guide-main.png" alt="VoLum BUILD-Ansicht" width="820">
</p>

## Mehr Erfahren

- [Benutzerhandbuch](docs/user-guide.de.md): BUILD und PLAY, Amps und Cabs, Pedale und Effekte, Dual Amp, Presets, MIDI, eigene Inhalte, Packs, Einstellungen und Tastenkürzel.
- [Entwickler-Leitfaden](NeuralAmpModeler/README.md): Build-, Test-, Packaging- und Architekturhinweise.
- [Fehler melden oder Feature vorschlagen](https://github.com/guitarlum/VoLum/issues/new/choose): nimm **Bug report** für Abstürze oder Fehlverhalten und **Feature request** für Ideen.

## Credits

- [Neural Amp Modeler](https://github.com/sdatkinson/neural-amp-modeler) von Steven Atkinson
- [NeuralAmpModelerPlugin](https://github.com/sdatkinson/NeuralAmpModelerPlugin), die ursprüngliche Plug-in-Shell, aus der VoLum entstanden ist
- [iPlug2](https://iplug2.github.io), das Plug-in-Framework
- Amp-Profile von Lum
