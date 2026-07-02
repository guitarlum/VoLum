**Sprachen:** [English](README.md) | Deutsch

# VoLum

<p align="center">
  <img src="docs/user-guide-main.png" alt="VoLum Standalone-Oberfläche" width="820">
</p>

VoLum ist eine Open-Source-Gitarren-Amp-Sammlung für Bühne, Studio und Übungsplatz. VoLum nutzt den [Neural Amp Modeler](https://github.com/sdatkinson/NeuralAmpModelerCore)-Kern, ist aber ein eigenes fokussiertes Produkt: 15 kuratierte Amps, PRE-Pedale (inklusive Transpose/Octaver-Pitch-Pedal), Dual Amp, POST Delay/Reverb/Tremolo, eigene Amp-/IR-/Pedal-Importe, Presets pro Amp, Tuner, Metronom und eine schnelle dunkle Oberfläche als Standalone-App oder VST3.

[VoLum herunterladen](https://github.com/guitarlum/VoLum/releases) oder direkt ins [Benutzerhandbuch](docs/user-guide.de.md).

## Was VoLum Besonders Macht

- **Eine kuratierte Amp-Sammlung statt leerem Blatt:** 15 Profi-Amps — Vintage, modern und Boutique — jeder mit mehreren Gain-Kanälen und vier Speaker-Modi, alle als vollwertige NAM-Architecture-2-(A2)-Profile. Einen auswählen und spielen.
- **Ein komplettes Rig in einem Fenster:** Pitch, Kompressor und zwei NAM-Drive-Pedal-Slots vorne, Amp und Cab in der Mitte, Delay/Reverb/Tremolo dahinter — die ganze `PRE | AMP | POST`-Kette auf einen Blick.
- **Ein Pitch-Pedal für Gitarre gebaut:** latenzarmes Transpose (mono oder polyphon) für Drop-Tunings und Capo-Shifts sowie ein polyphoner Octaver, der seine Stimmung sogar auf tief gestimmten Bass-Saiten hält.
- **Dual Amp:** zwei Amps gleichzeitig fahren, jede Spur pannen und die Support-Polarität drehen — für breite, geschichtete Sounds aus einer einzigen Spur.
- **POST-Effekte mit echtem Charakter:** Digital-, Analog- und Reverse-Delay sowie Optical-, Bias- und Harmonic-Tremolo — beide tempo-synchronisierbar — plus eine Reverb-Suite, die über Hall und Plate hinausgeht: **Oktaverb** schichtet Halo-, Shimmer- und Bloom-Pitch-Wash-Stimmen für üppige Oktav-Shimmer-Räume.
- **Bring Your Own:** eigene Amp-Captures, Impulsantworten und Pedal-Captures in eine verwaltete Bibliothek importieren, die auch nach dem Verschieben der Originaldateien weiter funktioniert.
- **Merkt sich deine Sounds:** jeder Amp behält eigene Regler, Kanal, Cab, PRE/POST und Dual-Amp-Setup, und Presets pro Amp rufen ein ganzes Rig mit einem Klick ab.
- **Für Bühne und Studio gemacht:** lautloser Tuner und Metronom, volle Tastatursteuerung und eine schnelle dunkle Oberfläche — kostenlos und Open Source, als Standalone oder VST3.

## Download

[![Build status](https://github.com/guitarlum/VoLum/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/guitarlum/VoLum/actions/workflows/ci.yml)

Nutze **[Releases](https://github.com/guitarlum/VoLum/releases)** für stabile Pakete. **[Actions -> CI](https://github.com/guitarlum/VoLum/actions/workflows/ci.yml)** ist nur für Preview-Builds aus dem aktuellen Entwicklungsstand gedacht.

| Plattform | Empfohlenes Paket | Wann du es nimmst |
| --- | --- | --- |
| Windows | `VoLum-vX.Y.Z-windows-setup.exe` | Einfachste Installation: Standalone-App, VST3 und mitgelieferte Rigs. |
| Windows | `VoLum-vX.Y.Z-windows-portable.zip` | Portable oder automatisierte Installation. `VoLum.vst3` und `VoLumRigs` zusammenhalten. |
| macOS | `VoLum-vX.Y.Z-macos-installer.dmg` | Einfachste Installation, wenn vorhanden. Enthält `VoLum Installer.pkg`. |
| macOS | `VoLum-vX.Y.Z-macos-standalone.dmg` | Nur Standalone-App. |
| macOS | `VoLum-vX.Y.Z-macos-vst3.zip` | Manuelle VST3-Installation. VoLum ist VST3 und erscheint deshalb nicht in Logic Pro. |

Nicht jedes Release enthält alle Pakettypen. Öffne die Release-Seite und wähle das Paket für dein System.

## Wichtiger Sicherheitshinweis

Die Signierung der VoLum-Releases ist noch im Aufbau. Siehe die [Code-Signing-Policy](CODE_SIGNING.md).

- **Windows:** SmartScreen kann melden, dass die App von einem unbekannten Herausgeber stammt. Wenn du der Build-Quelle vertraust, wähle **Weitere Informationen -> Trotzdem ausführen**.
- **macOS:** Gatekeeper kann unsignierte oder nicht notarisierte Builds blockieren. Nutze **Rechtsklick -> Öffnen** bei App oder Installer, oder **Systemeinstellungen -> Datenschutz & Sicherheit -> Trotzdem öffnen**.
- **macOS VST3-Zip:** wenn die DAW das Plugin nach dem Rescan weiter versteckt, entferne die Quarantäne:

```bash
xattr -cr ~/Library/Audio/Plug-Ins/VST3/VoLum.vst3
```

Preview-Builds aus CI sind Entwicklungsartefakte und sollten wie unsignierte Test-Builds behandelt werden.

## Schnellinstallation

### Windows Installer

`VoLum-vX.Y.Z-windows-setup.exe` ausführen. Der Installer legt ab:

- `VoLum.exe` unter `C:\Program Files\VoLum`
- `VoLum.vst3` unter `C:\Program Files\Common Files\VST3`
- `VoLumRigs` im VoLum-Installationsordner

Das VST3 findet die mitgelieferten Rigs automatisch.

### Windows Portable

`VoLum-vX.Y.Z-windows-portable.zip` entpacken. Für Standalone `VoLum_x64.exe` starten. Für VST3 beide Ordner in deinen VST3-Suchpfad kopieren:

```text
C:\Program Files\Common Files\VST3\
  VoLum.vst3\
  VoLumRigs\
```

### macOS Installer

`VoLum-vX.Y.Z-macos-installer.dmg` öffnen und `VoLum Installer.pkg` starten. Der Installer kann Standalone-App, VST3 und mitgelieferte Rigs ablegen.

### macOS Standalone

`VoLum-vX.Y.Z-macos-standalone.dmg` öffnen, `VoLum.app` nach **Programme** ziehen und starten. Die App enthält die mitgelieferten Rigs.

### macOS VST3-Zip

`VoLum-vX.Y.Z-macos-vst3.zip` entpacken und sowohl `VoLum.vst3` als auch `VoLumRigs` in deinen VST3-Ordner legen:

```text
~/Library/Audio/Plug-Ins/VST3/
  VoLum.vst3/
  VoLumRigs/
```

Danach Plugins in der DAW neu scannen. Nutze einen VST3-fähigen Host wie REAPER, Ableton Live, Cubase, Studio One oder Bitwig.

### Linux

VoLum bietet derzeit keinen nativen Linux-Build an. Einige Nutzer haben berichtet, dass das Windows-VST3 unter Linux mit [yabridge](https://github.com/robbert-vdh/yabridge) gut läuft; dieser Weg wird von VoLum aber nicht offiziell getestet.

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

Jeder Amp enthält die Speaker-Modi `AMP`, `G12`, `G65` und `V30`.

## Mehr Erfahren

- [Benutzerhandbuch](docs/user-guide.de.md): Oberfläche, Dual Amp, PRE-Pedale, POST-Effekte, eigene Inhalte, Presets, Tuner, Metronom, Tastatursteuerung und Einstellungen.
- [Entwickler-Leitfaden](NeuralAmpModeler/README.md): Build-, Test-, Packaging- und Architekturhinweise.
- [Fehler melden oder Feature vorschlagen](https://github.com/guitarlum/VoLum/issues/new/choose): nutze die Vorlage **Bug report** für Abstürze oder Fehlverhalten und **Feature request** für Ideen.
- Einstellungen liegen lokal unter `%LOCALAPPDATA%\VoLum\volum-settings.json` auf Windows und `~/Library/Application Support/VoLum/volum-settings.json` auf macOS.

## Credits

- [Neural Amp Modeler](https://github.com/sdatkinson/neural-amp-modeler) von Steven Atkinson
- [NeuralAmpModelerPlugin](https://github.com/sdatkinson/NeuralAmpModelerPlugin), die ursprüngliche Plugin-Shell, aus der VoLum entstanden ist
- [iPlug2](https://iplug2.github.io), das Plugin-Framework
- Amp-Profile von Lum