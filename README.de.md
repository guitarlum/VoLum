**Sprachen:** [English](README.md) | Deutsch

# VoLum — NAM Player

![VoLum standalone UI](docs/volum-ui.png)

Ein Gitarren-Amp-Sammlungsspieler auf Basis von [Neural Amp Modeler](https://github.com/sdatkinson/NeuralAmpModelerPlugin). Enthält 14 Amp-Profile mit einer eigenen Oberfläche zum schnellen Durchstöbern und Umschalten — Standalone-App und VST3-Plugin.

## Funktionen



- **14 mitgelieferte Amps** mit 4 Speaker-Modi und mehreren Gain-Stufen pro Amp (insgesamt ~224 Profile)
- **Dark-Theme-Oberfläche** mit Amp-Browser in der Seitenleiste, Speaker-Tasten, Channel-Stepper und gruppierten Reglern
- **Delay-Effekt** — Tape-, Digital- und Ping-Pong-Modi mit Time-, Feedback- und Mix-Reglern
- **Reverb-Effekt** — Hall (8-Zeilen-FDN) und Plate (Dattorro Allpass-Loop) mit Decay-, Tone- und Mix-Reglern
- **POST-Pedalboard-Ansicht** — Klick auf den POST-Streifen zeigt Delay- und Reverb-Karten mit prozeduraler Fraktal-Kunst und Live-Preset-Infos
- **Einstellungen pro Amp** — Regler, Schalter, Speaker-Modus und Kanal werden pro Amp gespeichert und beim nächsten Start wiederhergestellt
- **Schnelles Amp-Wechseln** — Modelle laden im Hintergrund; ein bereits geladener Amp ist beim Zurückwechseln sofort da
- **Tastenkürzel** — Hoch/Runter: Amp wechseln; Links/Rechts: Kanal wechseln; Klick auf einen Regler für Feineinstellung per Tastatur
- **Standalone + VST3** — gleiche Oberfläche und Funktionen in beiden Varianten

## Download

[![Build status](https://github.com/guitarlum/VoLum/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/guitarlum/VoLum/actions/workflows/ci.yml)

VoLum gibt es unter **[Releases](https://github.com/guitarlum/VoLum/releases)** oder als **[Actions → CI](https://github.com/guitarlum/VoLum/actions/workflows/ci.yml)**-Artefakt.

**Windows**

- **`VoLum-vX.Y.Z-windows-setup.exe`** — **empfohlen:** ein Installer für Standalone, VST3 und mitgelieferte Rigs.
- **`VoLum-vX.Y.Z-windows-portable.zip`** — **optional:** überall entpackbar; für portable oder skriptgestützte Setups.

**macOS**

- **`VoLum-vX.Y.Z-macos-installer.dmg`** (enthält **`VoLum Installer.pkg`**) — **empfohlen**, wenn dieses Paket angeboten wird: gewöhnlicher Installationsassistent; **Standalone-App**, **VST3-Plugin** und/oder **Amp-Rigs** wählbar; Rigs landen ohne manuelles Kopieren an der richtigen Stelle (für systemweite Pfade ggf. **Administratorpasswort**).
- **`VoLum-vX.Y.Z-macos-standalone.dmg`** — **nur Standalone:** `VoLum.app` nach **Programme** ziehen — wenn du **kein** DAW-Plugin brauchst.
- **`VoLum-vX.Y.Z-macos-vst3.zip`** — **optional / für Fortgeschrittene:** VST3-Bundle plus `VoLumRigs` zum **manuellen** Ablegen unter `~/Library/Audio/Plug-Ins/VST3/`; wenn du weißt, wie Plug-in-Ordner funktionierst oder du Installationen automatisierst.

Nicht jedes Release enthält alle Dateitypen — in der Release-Seite die passenden Assets wählen.

## Installation

### Windows (empfohlen)

`VoLum-vX.Y.Z-windows-setup.exe` ausführen.

Dabei wird installiert:

- `VoLum.exe` unter `C:\Program Files\VoLum`
- `VoLum.vst3` unter `C:\Program Files\Common Files\VST3`
- `VoLumRigs` im VoLum-Installationsordner

Das VST3-Plugin findet die mitgelieferten Rigs automatisch über den Registry-Eintrag des Installers — kein manuelles Kopieren nötig.

### Windows (portable)

`VoLum-vX.Y.Z-windows-portable.zip` aus einem Release verwenden, oder das Windows-Artefakt von **CI** (Actions).

1. Das portable Paket entpacken.
2. Du solltest diese Struktur sehen:

```
VoLum_x64.exe                    (Standalone-App)
VoLum.vst3/                      (VST3-Plugin-Bundle)
  Contents/x86_64-win/VoLum.vst3
VoLumRigs/                       (Amp-Profile — erforderlich!)
  Ampete One/
  Marshall JMP 2203 1976/
  ...
```

3. **Standalone** — `VoLum_x64.exe` starten. Die App findet `VoLumRigs` daneben.
4. **VST3 in einer DAW** — den Ordner `VoLum.vst3` **und** den Ordner `VoLumRigs` in deinen VST3-Suchpfad kopieren, **nebeneinander**:

```
C:\Program Files\Common Files\VST3\
  VoLum.vst3/                    <-- der ganze Ordner, nicht nur die innere Datei
  VoLumRigs/                     <-- Amp-Profile, direkt daneben
```

### macOS — Installer `.pkg` (empfohlen)

**Installer-DMG** öffnen und **`VoLum Installer.pkg`** doppelklicken. Dem Assistenten folgen; **Standalone-App**, **VST3-Plugin** und **Amp-Rigs** sind getrennt wählbar. Das ist der einfachste Weg ohne manuelles Kopieren in versteckte Ordner.

Zielpfade (systemweit; macOS kann ein **Administratorpasswort** verlangen):

- **`VoLum.vst3`** → `/Library/Audio/Plug-Ins/VST3/`
- **Mitgelieferte Amps** → `/Library/Application Support/VoLum/VoLumRigs/` (übliches **Application-Support**-Layout — VoLum findet die Rigs dort automatisch)

Unsignierte Builds: ggf. **Rechtsklick → Öffnen** bei der `.pkg` oder **Systemeinstellungen → Datenschutz & Sicherheit → Trotzdem öffnen**.

### macOS — nur Standalone-App

Nur nötig, wenn du **kein** DAW-Plugin brauchst.

1. `VoLum-vX.Y.Z-macos-standalone.dmg` herunterladen.
2. Öffnen und `VoLum.app` nach **Programme** ziehen.
3. `VoLum.app` starten.

Die App enthält die Rigs im Bundle. Bei unsignierten Builds ggf. **Rechtsklick → Öffnen**, wenn macOS blockiert.

### macOS — VST3-Zip (fortgeschritten / manuell)

**`VoLum-vX.Y.Z-macos-vst3.zip`**, wenn du **keinen** Installer-`.pkg` nutzt — z. B. weil im Release kein Installer-DMG dabei ist, oder du die Dateien selbst legen willst.

VoLum ist ein **VST3**-Plugin. Es erscheint **nicht** in **Logic Pro** (Audio Units, kein VST3). Verwende eine DAW mit VST3, z. B. **REAPER**, **Ableton Live**, **Cubase**, **Studio One**, **Bitwig**.

**Du brauchst zwei Dinge im gleichen Ordner:** `VoLum.vst3` **und** `VoLumRigs`. Nur `VoLum.vst3` reicht nicht für die mitgelieferten Amps.

1. `VoLum-vX.Y.Z-macos-vst3.zip` herunterladen und entpacken. Es sollten **`VoLum.vst3`** und **`VoLumRigs`** dabei sein.
2. Den **Benutzer-Plugin-Ordner** im Finder öffnen:
   - **Gehe zu → Gehe zum Ordner…** (**⇧⌘G**)
   - `~/Library/Audio/Plug-Ins/VST3` einfügen, **Eingabetaste**.  
   **Library** ist im Finder oft ausgeblendet; **Gehe zum Ordner** reicht. (Mit **Wahltaste (⌥)** zeigt **Gehe zu** bei manchen Versionen **Library** — links von **Befehl**, nicht Befehl.)  
   Ordner fehlt: **Ablage → Neuer Ordner** bzw. `Audio` / `Plug-Ins` / `VST3` anlegen — oder die DAW legt den Pfad an.
3. **Beides** — `VoLum.vst3` und `VoLumRigs` — in diesen **VST3**-Ordner ziehen, **nebeneinander**:

```
~/Library/Audio/Plug-Ins/VST3/
  VoLum.vst3/
  VoLumRigs/
```

4. DAW öffnen, **Plug-ins neu einlesen**, VoLum als Effekt auf eine Spur legen.

**Sicherheit:** Bei unsignierten Builds ggf. Quarantäne:

```bash
xattr -cr ~/Library/Audio/Plug-Ins/VST3/VoLum.vst3
```

**REAPER:** **Einstellungen → Plug-ins → VST** → Pfad `~/Library/Audio/Plug-Ins/VST3` → **Re-scan**.

### macOS Preview-Builds

Artefakte vom **CI**-Workflow (**VoLum-mac** / **VoLum-win**):

1. Unter **Actions → CI** den letzten grünen Lauf öffnen und die passenden Artefakte laden.
2. Wenn vorhanden: **`*-macos-installer.dmg`** + **`VoLum Installer.pkg`** bevorzugen; sonst **Standalone** (`*-macos-standalone.dmg`) für die App, **`*-macos-vst3.zip`** für die manuelle Plugin-Installation.
3. Entsprechend die Abschnitte oben (**Installer**, **nur Standalone**, **VST3-Zip**).

## Mitgelieferte Amps


| Amp                     | Kanäle |
| ----------------------- | ------ |
| Ampete One              | 4      |
| Bad Cat mini Cat        | 3      |
| Brunetti XL 2           | 3      |
| Fryette Deliverance 120 | 2      |
| H&K TriAmp Mk2          | 6      |
| Lichtlaerm Prometheus   | 3      |
| Marshall 2204 1982      | 6      |
| Marshall JMP 2203 1976  | 6      |
| Marshall JVM 210H OD1   | 6      |
| Orange OD120 1975       | 5      |
| Orange ORS100 1972      | 2      |
| Sebago Texas Flood      | 2      |
| Soldano SLO100          | 3      |
| THC Sunset              | 5      |


Jeder Amp hat 4 Speaker-Modi (AMP direkt, G12, G65, V30) und mehrere Gain-Stufen-Kanäle.

## Einstellungen

Die Einstellungen pro Amp (Regler, Schalter, Speaker, Kanal) werden automatisch gespeichert:

- **Windows:** `%LOCALAPPDATA%\VoLum\volum-settings.json`
- **macOS:** `~/Library/Application Support/VoLum/volum-settings.json`

Die Einstellungen bleiben für Standalone und VST3 über Sitzungen hinweg erhalten.

## Tastatur

- Kein Regler gewählt: `Hoch/Runter` wechselt den Amp, `Links/Rechts` den Kanal
- Regler anklicken, um ihn für die Tastatursteuerung zu wählen
- Gewählter Regler: `Hoch/Runter` Wert ändern, `Links/Rechts` nächster Regler
- `Umschalt` halten für feinere Schritte (`0,1`)
- `Enter` für direkte Zahleingabe
- `Entf` oder `Rücktaste` setzt den gewählten Regler auf den Standardwert
- `Esc` beendet den Regler-Tastaturmodus; Pfeiltasten steuern wieder Amp/Kanal

## Aus dem Quellcode bauen

Siehe den [Entwickler-Leitfaden](NeuralAmpModeler/README.md) (englisch).

## Credits

- [Neural Amp Modeler](https://github.com/sdatkinson/neural-amp-modeler) von Steven Atkinson
- [NAM Plugin](https://github.com/sdatkinson/NeuralAmpModelerPlugin) — Upstream-Basis
- [iPlug2](https://iplug2.github.io) — Plugin-Framework
- Amp-Profile von Lum