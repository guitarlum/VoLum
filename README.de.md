**Sprachen:** [English](README.md) | Deutsch

# VoLum

**Open-Source-Gitarren-Amps für Bühne, Studio und Übungsplatz.**

![VoLum Dual Amp: die Bilder von THC Sunset und Soldano SLO100 reagieren auf die Gitarre](docs/volum-dual-amp.gif)

VoLum nutzt den [Neural Amp Modeler](https://github.com/sdatkinson/NeuralAmpModelerCore)-Kern, ist aber eine eigene, fokussierte App: 15 kuratierte Amps, PRE-Pedale (Pitch mit Transpose und Octaver, ein Kompressor, zwei NAM-Pedal-Slots), Dual Amp, POST-Chorus, -Delay, -Reverb und -Tremolo, deine eigenen Amps, IRs und Pedale, Presets pro Amp, eine PLAY-Ansicht für MIDI-Fußcontroller, ein Tuner und ein Metronom. Nutze es als Standalone-App, als VST3 oder unter macOS als AU.

[VoLum herunterladen](https://github.com/guitarlum/VoLum/releases) oder das [Benutzerhandbuch](docs/user-guide.de.md) lesen.

<p align="center">
  <img src="docs/user-guide-main.png" alt="VoLum BUILD-Ansicht" width="820">
</p>

## Was VoLum Besonders Macht

- **Erst BUILD, dann PLAY, in einem Fenster:** in BUILD baust du einen Sound, in PLAY spielst du deine Sounds. Jeder mitgelieferte Amp bringt ein oder zwei Werk-Presets mit (21 insgesamt), die du ohne Speichern auf PLAY legen kannst. PLAY startet mit fünf davon auf den Programmen 0 bis 4.
- **MIDI-Aufruf ohne Learn:** 128 Programmnummern, dieselbe Liste wie in PLAY. Sende Program Change, oder den Recall CC (102), wenn dein Host Program Change nicht weitergibt.
- **Dual Amp:** zwei Amps gleichzeitig, jeder mit eigenem Kanal, Cab oder IR, eigenen Reglern und Panorama.
- **POST-Chorus:** vier Stimmen vor Delay und Reverb, die Modulation trifft also den Amp vor den Echos und dem Raum.
- **Eine Pack-Datei:** ein `.volumpack` sichert, zieht um oder teilt deine eigenen Amps, IRs, Pedale und Presets in einem Schritt.
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
- **macOS:** Gatekeeper kann App oder Installer blockieren. Versuche sie einmal zu öffnen, geh dann zu **Systemeinstellungen -> Datenschutz & Sicherheit** und klicke **Trotzdem öffnen**. Auf älteren macOS-Versionen geht auch **Rechtsklick -> Öffnen**.
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

## PLAY

PLAY startet mit fünf Werk-Sounds auf den Programmen 0 bis 4: The bestest Clean, SLO Crunch, Modern Rhythm, Crack the Skye und Ampete Lead. Ersetze sie oder füge eigene hinzu: leg ein beliebiges Werk-Preset oder ein in BUILD gespeichertes User-Preset auf eine Programmnummer. Klicke eine Zeile an, um sie zu spielen, oder ruf sie mit einem MIDI-Fußcontroller auf.

<p align="center">
  <img src="docs/user-guide-play.png" alt="VoLum PLAY-Board" width="820">
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
