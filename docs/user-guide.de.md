# VoLum Benutzerhandbuch

**Sprachen:** [English](user-guide.en.md) | Deutsch

VoLum ist eine Gitarren-Amp-Sammlung für Bühne, Studio und Übungstisch. In **BUILD** baust du einen Sound, in **PLAY** spielst du deine Sounds. Dieses Handbuch beschreibt VoLum 1.3. Downloads und Installation stehen in der [README](../README.de.md).

## Inhalt

- [Schnellstart](#schnellstart)
- [Die BUILD-Ansicht](#die-build-ansicht)
- [Amps Und Cabs](#amps-und-cabs)
- [PRE-Pedale](#pre-pedale)
- [POST-Effekte](#post-effekte)
- [Dual Amp](#dual-amp)
- [Presets](#presets)
- [PLAY](#play)
- [MIDI-Fußcontroller](#midi-fußcontroller)
- [Tuner Und Metronom](#tuner-und-metronom)
- [Eigene Amps, IRs Und Pedale](#eigene-amps-irs-und-pedale)
- [Sichern Und Teilen Mit Packs](#sichern-und-teilen-mit-packs)
- [Einstellungen](#einstellungen)
- [VoLum In Einer DAW](#volum-in-einer-daw)
- [Tastenkürzel](#tastenkürzel)
- [Fehlerbehebung](#fehlerbehebung)
- [Wo VoLum Deine Dateien Ablegt](#wo-volum-deine-dateien-ablegt)
- [Fehler Melden Oder Feature Vorschlagen](#fehler-melden-oder-feature-vorschlagen)

## Schnellstart

1. **Gitarre anschließen.** Öffne in der Standalone-App die Einstellungen (das Zahnrad oben rechts), bleib auf **SIGNAL** und klicke **Audio & MIDI devices...**. Wähle dein Audio-Interface, einen Mono-Eingang für die Gitarre und deine Ausgänge. In einer DAW legst du VoLum auf eine Spur, die deinen Gitarreneingang aufnimmt.
2. **Amp wählen** in der linken Liste, dann Kanal und Cab.
3. **Pedale und Effekte hinzufügen.** Klicke **PRE** für Pedale vor dem Amp, **POST** für Chorus, Delay, Reverb und Tremolo.
4. **Sound speichern.** Drücke **Strg+S**, gib einen Namen ein und drücke Enter.
5. **Spielen.** Drücke **P**, um zu PLAY zu wechseln. Fünf Sounds liegen schon auf den Programmen 0 bis 4. Klicke einen an oder schalte mit den Pfeiltasten oder einem MIDI-Fußcontroller durch.

## Die BUILD-Ansicht

![VoLum BUILD-Ansicht](user-guide-main.png)

In BUILD baust du einen Sound.

1. **Amp-Liste:** die 15 mitgelieferten Amps, darunter deine eigenen unter **CUSTOM**.
2. **Amp-Panel:** der Amp, den du bearbeitest. Mit Dual Amp teilt es sich in **MAIN** und **SUPPORT**.
3. **Kanal- und Cab-Reihe:** der Kanalwähler und die Cab-Tasten.
4. **Reglerzeile:** die Regler des gewählten Amps, Pedals oder Effekts.
5. **PRE | AMP | POST-Leiste:** öffnet immer einen Bereich. Die Tasten `1`, `2` und `3` tun dasselbe.
6. **Toolbar (oben rechts):** PLAY/BUILD-Umschalter, Tuner, Metronom und das Zahnrad für die Einstellungen.

Der Preset-Name steht in der Mitte der Kopfzeile, mit `<` und `>` zum Durchschalten der Presets.

Der PLAY/BUILD-Umschalter zeigt immer, wohin ein Klick führt: einen Stomp-Ring in BUILD, Fader in PLAY. Fahr mit der Maus darüber, um das Ziel zu lesen. `P` tut dasselbe per Tastatur.

**Regler:** zieh an einem Regler oder nimm das Mausrad. Doppelklick setzt ihn zurück. Klick einen Regler an, um ihn auszuwählen, und bediene ihn dann mit den Pfeiltasten (siehe [Tastenkürzel](#tastenkürzel)). Pedal- und Effektregler bleiben auch bei ausgeschaltetem Pedal bedienbar, so kannst du es vor dem Einschalten einstellen.

### Jeder Amp Merkt Sich Sein Rig

VoLum merkt sich Cab, Kanal, Regler, PRE-Pedale, POST-Effekte und Dual Amp für jeden Amp getrennt. Wechselst du zu einem Amp zurück, ist alles so, wie du es verlassen hast.

Willst du beim Ausprobieren verschiedener Amps dieselben Pedale oder Effekte behalten, klicke das **Schloss** in der PRE- oder POST-Kopfzeile:

- Solange es zu ist, bleibt PRE (oder POST) beim Amp-Wechsel, wie es ist.
- Weichen die gesperrten Pedale von dem ab, was dieser Amp gespeichert hat, erscheint ein **Store**-Pfeil. Ein Klick speichert sie nur für diesen Amp.
- Ein zweiter Klick aufs Schloss entsperrt. Der Amp kehrt zu seinen eigenen gespeicherten Pedalen zurück. Ungespeicherte Änderungen im gesperrten Bereich gehen verloren.
- Der Wechsel zu PLAY behält, was du hörst: VoLum speichert den gesperrten Bereich für den aktuellen Amp und öffnet das Schloss.

## Amps Und Cabs

Wähle einen Amp in der linken Liste oder drücke `Up` / `Down`, wenn kein Regler ausgewählt ist. Zur Orientierung:

- **Clean, Blues und Boutique:** Sebago Texas Flood (eine Pedal-Plattform im Stil des Dumble Steel String Singer), THC Sunset, Bad Cat Mini Cat.
- **Vintage und Classic-Rock-Crunch:** Orange ORS100, Orange OD120, Marshall JMP 2203, Marshall 2204.
- **Modern und High Gain:** Soldano SLO100, Diezel Herbert, Marshall JVM, H&K TriAmp, Fryette Deliverance, Lichtlaerm Prometheus, Brunetti XL 2.
- **Allrounder:** Der Ampete One vereint eine amerikanische und eine britische Stimme in einem Amp.

**Kanäle:** jeder Amp hat zwei bis sechs Kanäle, einen pro aufgenommener Gain-Einstellung. Schalte sie mit den Kanalpfeilen durch, oder im AMP-Bereich mit `Left` / `Right`.

**Cabs:** wähle **No Cab** (nur der Amp), **G12**, **G65**, **V30** oder **Custom IR** für deine eigene Lautsprecher-Impulsantwort (siehe [Eigene IRs](#eigene-irs)). `S` schaltet zum nächsten Cab, `Shift+S` zurück.

**Regler:** **INPUT**, **GATE** (Schwelle des Noise Gates), **BASS**, **MID**, **TREBLE** und **OUTPUT**. Die EQ-Regler sind zusätzliche Klangregler; sie müssen nicht den Reglerstellungen bei der Aufnahme entsprechen. **OUTPUT** ist bei `0.0 dB` neutral; ganz zugedreht zeigt er `-∞ dB` und schaltet den Amp stumm.

Jedes mitgelieferte Amp-, Cab- und PRE-Pedal-Capture ist ein NAM-Architecture-2-(A2)-Profil. Kommt dein Rechner nicht mit, probiere **LITE** in den [Einstellungen](#signal-tab).

## PRE-Pedale

![VoLum PRE-Bereich](user-guide-pre.png)

PRE-Pedale liegen vor dem Amp, in dieser Reihenfolge: **PITCH**, **COMP**, **NAM 1**, **NAM 2**.

1. Klicke **PRE** (oder drücke `1`).
2. Klicke eine Pedalkarte an, um sie auszuwählen.
3. Schalte sie mit der LED auf der Karte ein, oder mit `Space` (`B` im Plug-in).
4. Stell sie in der Reglerzeile ein.

### Pitch

![VoLum Pitch-Pedal im Transpose-Modus](user-guide-pitch-transpose.png)

Wähle den Modus mit **TRANSPOSE** / **OCTAVER**. Ein neues Rig startet auf **OCTAVER**.

- **TRANSPOSE** verschiebt dein ganzes Signal, für Drop-Tunings und Kapodaster-Effekte. **SEMI** stellt das Intervall von −12 bis +7 Halbtönen ein (Start bei −2). **MIX** mischt deinen trockenen Ton dazu, **LEVEL** stellt den Ausgang ein. Wähle die Engine mit **INSTANT** / **POLY**:
  - **INSTANT** (Standard) ist für Einzeltöne und Leads: geringste Latenz (etwa 8,6 ms) und der knackigste Anschlag.
  - **POLY** ist für Akkorde und Riffs: jeder Ton des Akkords wird verschoben, bei etwa 14 ms Latenz.
- **OCTAVER** fügt Oktaven hinzu und folgt Akkorden. **OCT DN** und **OCT UP** stellen die Lautstärke der unteren und oberen Oktave ein, **DRY** deinen Originalton und **LEVEL** den Ausgang. **VINTAGE** klingt rauer und dunkler, **MODERN** bleibt sauber. Er startet mit OCT DN auf 80 %, OCT UP aus, DRY auf 100 % und MODERN und erkennt Einzeltöne bis zum 24. Bund der hohen E-Saite.

![VoLum Pitch-Pedal im Octaver-Modus](user-guide-pitch-octaver.png)

Beide Modi halten die Stimmung auch bei langem Sustain und arbeiten bis hinunter zum tiefen Fis einer 8-Saiter. In einer DAW meldet VoLum die Pitch-Verzögerung an den Host, damit die Spur im Timing bleibt.

### Kompressor

**COMP** gleicht dein Anschlagen aus. **INPUT** stellt ein, wie stark er komprimiert, **ATTACK** und **RELEASE**, wie schnell er zupackt und loslässt, und **OUTPUT** den Pegel. **OUTPUT** ganz zu (`-∞ dB`) schaltet ihn stumm.

### NAM-Pedale

![VoLum Menü für PRE-Pedal-Captures](user-guide-pre-pedal.png)

**NAM 1** und **NAM 2** nehmen je ein aufgenommenes Drive-, Boost- oder Fuzz-Pedal auf. Beide starten leer. Ein Klick auf eine leere Karte öffnet das Capture-Menü; bei einer Karte mit Pedal wählt der erste Klick sie aus und der zweite öffnet das Menü. Die Captures sind gruppiert (Klon, TS / Boost, Distortion, Fuzz) und von wenig nach viel Gain sortiert. Deine eigenen Pedale stehen unten unter **CUSTOM**.

Gute Startpunkte:

- Clean- oder Low-Gain-Amps: Nuke, Bender, Myth, Mash.
- Amps an der Grenze zur Verzerrung: Revival Drive.
- Mid- und High-Gain-Amps: Klon, TS, TS+, Fatbee.

Die Regler sind **GAIN**, **BASS**, **MID**, **MID Hz**, **TREBLE** und **LEVEL**. **LEVEL** ganz zu (`-∞ dB`) schaltet das Pedal stumm.

## POST-Effekte

![VoLum POST-Bereich](user-guide-post.png)

POST-Effekte liegen hinter dem Amp, in dieser Reihenfolge: **CHORUS**, **DELAY**, **REVERB**, **TREM**.

1. Klicke **POST** (oder drücke `3`).
2. Klicke eine Effektkarte an, um sie auszuwählen.
3. Schalte sie mit der LED auf der Karte ein, oder mit `Space` (`B` im Plug-in).
4. Wähle oben in der Reglerzeile einen Modus und stell die Regler ein.

Jeder Effekt merkt sich seine Regler pro Modus. Du kannst also einen anderen Modus ausprobieren und zurückwechseln, ohne etwas zu verlieren. Ein Moduswechsel nimmt nie alte Echos oder Hallfahnen in den neuen Modus mit.

### Chorus

![VoLum Chorus-Karte](user-guide-chorus.png)

Chorus liegt vor Delay und Reverb, er färbt also den Amp und nicht die Echos. Wähle eine Stimme:

- **CLASSIC:** ein Stereo-Sweep im Stil des Juno-60.
- **WARPED:** Band-Wow-und-Flutter. Bei **MIX** 100 % wird daraus ein Vibrato.
- **CLEAR:** ein breiter Chorus im Dimension-Stil, der auch in Mono in Stimmung bleibt.
- **ENSEMBLE** (Standard): ein 80er-Rack-Chorus mit drei Stimmen über links, Mitte und rechts.

Die Regler sind **RATE**, **DEPTH**, **TONE** (macht nur den Chorus dunkler, nicht deinen trockenen Ton), **WIDTH** (0 % ist monotauglich) und **MIX**. Jede Stimme startet mit **MIX** 50 %. Bei **MIX** 0 % ist der Chorus ganz aus dem Signal, du kannst ihn also eingeschaltet lassen und einblenden. Chorus hat kein Tempo-Sync.

### Delay

Wähle **DIGITAL**, **ANALOG** oder **REVERSE**. Die Regler sind **TIME**, **FEEDBACK**, **MIX**, **TONE** und je Modus ein Charakterregler: **GRIT** (Digital), **WEAR** (Analog) oder **BLOOM** (Reverse).

- **PING-PONG** (Digital und Analog) lässt die Echos rechts, links, rechts springen. Sie liegen etwa 3 bis 7 dB leiser als das normale Delay, weil jedes Echo nur auf einer Seite spielt. Es funktioniert mit Dual Amp, egal wie die Amps im Panorama stehen.
- **TEMPO SYNC** rastet die Echos auf das Tempo ein. **TIME** wird zu einem **DIVISION**-Wähler: 1/2, 1/4, 1/4., 1/4T, 1/8, 1/8., 1/8T, 1/16.

### Reverb

Wähle **HALL**, **PLATE** oder **OKTAVERB**. Die Regler sind **MIX**, **DECAY**, **TONE** und **PRE-DLY** (die Pause, bevor der Hall einsetzt, standardmäßig 10 ms). **OKTAVERB** fügt tonhöhenverschobenen Shimmer hinzu, mit drei Stimmen, **HALO**, **SHIMMER** und **BLOOM**, und einem **INTENSITY**-Regler.

### Tremolo

![VoLum Tremolo-Karte](user-guide-tremolo.png)

Tremolo liegt ganz hinten und pulsiert daher den ganzen Sound samt Hall. Wähle eine Stimme:

- **OPTICAL** (Standard): ein hackiges Pulsieren wie mit Fotozelle.
- **BIAS:** ein weicher, gleichmäßiger Sinus-Puls.
- **HARMONIC:** teilt den Sound in Bässe und Höhen, die abwechselnd pulsieren. Der zusätzliche Regler **X-OVER** stellt die Trennfrequenz ein.

Die Regler sind **RATE**, **DEPTH**, **SHAPE** (von weichem Sinus bis hartem Rechteck) und **MIX**. **TEMPO SYNC** macht aus **RATE** einen **DIVISION**-Wähler, wie beim Delay.

### Tempo

Delay und Tremolo teilen sich ein Tempo. In einer DAW folgen sie dem Songtempo. In der Standalone-App folgen sie dem BPM-Wert des Metronoms, auch wenn der Klick aus ist.

## Dual Amp

![VoLum Dual Amp](user-guide-dual-amp.png)

Dual Amp spielt zwei Amps gleichzeitig: **MAIN** und **SUPPORT**.

1. Öffne den AMP-Bereich (`2`).
2. Klicke die **Dual-Amp**-Taste mit dem geteilten Panel ("Switch to Dual Amp"). `Space` (`B` im Plug-in) tut dasselbe.
3. Klicke die SUPPORT-Seite an (**Choose support amp**) und wähle den zweiten Amp aus der Liste. **(none)** ganz oben leert die Spur wieder.
4. Klicke MAIN oder SUPPORT (oder drücke `Tab`), um zu wählen, welchen Amp Cab-Reihe und Reglerzeile bearbeiten.

Jede Spur hat ihren eigenen Kanal, ihr Cab oder eigenes IR, ihre Regler und einen kleinen **PAN**-Regler in der Titelleiste. Beim ersten Einschalten geht MAIN ganz nach links und SUPPORT ganz nach rechts.

Die SUPPORT-Spur hat eine **Ø**-Taste (Polarität), standardmäßig an. Manche Amp-Paare klingen damit voller, andere ohne. Klingt das Paar dünn oder hohl, schalte **Ø** um.

Der SUPPORT-Regler **OUTPUT** ganz zu (`-∞ dB`) schaltet diese Spur stumm. VoLum gleicht das Timing beider Amps an, bevor es sie mischt.

## Presets

![VoLum Preset-Menü](user-guide-presets.png)

Ein Preset speichert das ganze Rig eines Amps: Cab, Kanal, Regler, PRE-Pedale, POST-Effekte und Dual Amp. Jeder Amp hat seine eigene Liste.

### Werk-Presets

Jeder mitgelieferte Amp bringt ein oder zwei Werk-Presets mit, 21 insgesamt. Sie sind schreibgeschützt und nutzen nur Inhalte, die mit VoLum kommen. Du kannst jedes davon auf PLAY legen, ohne es vorher zu speichern.

| Amp | Werk-Presets |
| --- | --- |
| Ampete One | Ampete Rhythm, Ampete Lead |
| Bad Cat Mini Cat | BadCat Crunch |
| Brunetti XL 2 | American Lead |
| Diezel Herbert Mk1 | Thicc Rhythm, Sanitarium |
| Fryette Deliv. 120 | Dry Rhythm |
| H&K TriAmp Mk2 | HiFi Heavy, HiFi Crunch |
| Lichtlaerm Prom. | Modern Rhythm, Modern Lead |
| Marshall 2204 | JCM800 Crunch |
| Marshall JMP 2203 | Crack the Skye |
| Marshall JVM | Modern British |
| Orange OD120 | Stoner |
| Orange ORS100 | An old Soul |
| Sebago Texas Fl. | The bestest Clean |
| Soldano SLO100 | SLO Lead, SLO Crunch |
| THC Sunset | Sunset Crunch, Sunset Clean |

### Preset Speichern

Drücke **Strg+S** in einem beliebigen BUILD-Bereich. Ein Namensfeld öffnet sich:

- Auf einem Werk-Preset, auf Default oder einem unbenannten Rig schlägt es **New Preset** vor. **Save** legt ein neues User-Preset an. Werk-Presets werden nie überschrieben.
- Auf einem deiner User-Presets schlägt es dessen Namen vor. Die Taste heißt **Update** und überschreibt es. Tippst du einen anderen Namen, wird daraus **Save**, das ein neues Preset anlegt.
- Ein User-Preset darf wie ein Werk-Preset heißen; es steht dann unter **USER**. Ist der Name ein zweites Mal vergeben, hängt VoLum eine Zahl an, zum Beispiel "SLO Lead 2".

`Enter` speichert. **Cancel**, `Esc` oder ein Klick neben das Feld schließt es ohne Speichern. Namensfelder funktionieren wie jedes Textfeld: `Strg+Backspace` löscht ein Wort, `Strg+Left` / `Strg+Right` springen wortweise (mit `Shift` zum Markieren), Doppelklick markiert ein Wort, und `Strg+Z` / `Strg+Y` machen rückgängig und wiederholen.

### Das Preset-Menü

Klicke den Preset-Namen in der Kopfzeile, um das Menü zu öffnen:

- **Default (factory settings)** setzt den Amp auf seine Auslieferungswerte zurück.
- **FACTORY** und **USER** listen die Presets. Ein Klick auf die Überschrift klappt sie auf oder zu.
- **Overwrite "Name"** speichert das aktuelle Rig nach einer Rückfrage in das User-Preset, auf dem du gerade bist.
- **Save current as new...** öffnet das Namensfeld.
- **Manage presets...** öffnet ein Panel mit allen User-Presets dieses Amps. **+ Save current as new** legt eines an. Jede Zeile hat Symbole zum Überschreiben, Umbenennen und Löschen. Doppelklick auf eine Zeile lädt sie.

Die Pfeile `<` / `>` schalten durch die Werk-Presets des Amps, dann durch seine User-Presets. **(unsaved)** hinter dem Namen heißt, dass du seit dem Laden etwas geändert hast. Es verschwindet, sobald das Rig wieder übereinstimmt.

Speichern in BUILD ändert nie, welcher Sound auf einem PLAY-Schalter liegt. Löschst du ein Preset, das auf PLAY liegt, nennt die Rückfrage diese Schalter (zum Beispiel "On PLAY 03 and 12: they will read Invalid."). Sie behalten ihre Programmnummern und zeigen **Invalid slot**, bis du ihnen einen anderen Sound gibst.

## PLAY

PLAY ist deine Bühnenansicht: deine Sounds als Liste, acht Stomp-Schalter und das Amp-Bild. Wechsle mit `P` oder dem PLAY/BUILD-Umschalter. Der Wechsel ändert nie, was du hörst.

Ein **Sound** ist ein Amp mit einem seiner Presets. Jeder Sound liegt auf einer Programmnummer von 0 bis 127, denselben Nummern, die ein MIDI-Fußcontroller sendet.

### Die Fünf Start-Sounds

![VoLum PLAY mit den fünf Start-Sounds](user-guide-play-start.png)

Beim ersten Öffnen deiner Bibliothek mit VoLum 1.3 bekommt PLAY fünf Werk-Sounds:

| Programm | Sound | Amp |
| --- | --- | --- |
| 0 | The bestest Clean | Sebago Texas Fl. |
| 1 | SLO Crunch | Soldano SLO100 |
| 2 | Modern Rhythm | Lichtlaerm Prom. |
| 3 | Crack the Skye | Marshall JMP 2203 |
| 4 | Ampete Lead | Ampete One |

Viele Fußcontroller zählen ab 1, dort sind das die Presets 1 bis 5. Das passiert nur einmal. Ein Board, das du geändert oder geleert hast, wird nie wieder befüllt.

### Sounds Spielen

![VoLum PLAY-Board](user-guide-play.png)

- **Sound aufrufen:** klicke seine Zeile, drücke `Up` / `Down` (oder `Left` / `Right`) oder sende MIDI. Die Pfeiltasten überspringen leere und kaputte Einträge und springen am Ende wieder zum Anfang.
- **LIVE** markiert den zuletzt aufgerufenen Sound. **(unsaved)** heißt, dass du ihn seitdem geändert hast.
- **Stomps:** die acht Schalter schalten Pitch, Comp, NAM 1, NAM 2, Chorus, Delay, Reverb und Tremolo ein und aus. Klicke sie oder drücke `1` bis `8`. Sonst ändern sie nichts. Ein leerer NAM-Slot tut nichts. Rechtsklick auf einen Stomp öffnet dieses Pedal in BUILD.
- **Anpassen und speichern:** ändere einen Sound in BUILD oder mit den Stomps und drücke **Strg+S**. Hält der LIVE-Schalter noch den Sound, von dem du ausgegangen bist, nimmt deine gespeicherte Version seinen Platz ein. So wird ein geänderter Werk-Sound zu deiner eigenen Kopie auf demselben Schalter. Strg+S wählt nie eine neue Programmnummer.

### Sounds Hinzufügen Ersetzen Und Anordnen

![VoLum PLAY-Auswahl Add Sound](user-guide-play-picker.png)

Die **+**-Taste am Ende der Liste richtet sich danach, was du gerade spielst:

- **+ Add this sound** legt, was du hörst, auf die nächste freie Programmnummer. Ein User-Sound oder ein unverändertes Werk-Preset kommt direkt drauf. Hat das Rig noch keinen Namen oder hast du ein Werk-Preset geändert, öffnet sich zuerst das Namensfeld; **Save** legt den User-Sound an und fügt ihn in einem Schritt hinzu. Der Werk-Sound behält seine eigene Zeile.
- **+ Add Sound** erscheint, wenn das, was du hörst, schon auf der Liste steht. Es öffnet die Auswahl: wähle eine **PROGRAM**-Nummer (standardmäßig die nächste freie; eine belegte Nummer zeigt, was sie ersetzt), dann ein Werk- oder User-Preset.

Zum Ersetzen eines Sounds doppelklickst du seine Zeile oder nimmst ihre Zuweisungstaste. Zum Leeren einer Zeile klickst du ihr kleines `×`. Zieh eine Zeile auf eine andere, um beide zu tauschen, zwischen zwei Zeilen, um die Sounds weiterzuschieben, oder auf die gestrichelte **Add**-Fläche, um sie ans Ende zu setzen. Sind alle Programmnummern belegt, öffnet **+** die Auswahl auf Programm 0.

Leerst du alle Zeilen, zeigt PLAY **No Sounds assigned** mit einer **+**-Taste in der Mitte. Der Amp, den du gespielt hast, klingt weiter.

### Bewegte Art

Das Amp-Bild leuchtet und bewegt sich, während du spielst, und kehrt zum Standbild zurück, wenn du aufhörst. Je härter du spielst, desto mehr bewegt es sich. Jeder mitgelieferte Amp hat seine eigene Bewegung; eigene Amps drehen sich langsam und atmen. Für ein ruhiges Bild und etwas weniger CPU schalte **Animate art in PLAY** in den [Einstellungen](#signal-tab) aus.

## MIDI-Fußcontroller

Ein Fußcontroller ruft deine PLAY-Sounds auf:

- **Program Change N** ruft den Sound auf Programm N auf.
- **CC 102 mit Wert N** tut dasselbe. Nimm das, wenn deine DAW Program Change nicht an Plug-ins weitergibt.

Programmnummern gehen von 0 bis 127. Eine Nummer ohne Sound oder mit gelöschtem Sound wird ignoriert, der aktuelle Sound spielt weiter.

### Controller Anschließen

- **Standalone:** wähle den MIDI-Eingang des Controllers unter **Einstellungen > SIGNAL > Audio & MIDI devices...**.
- **DAW:** leite das MIDI des Controllers auf die VoLum-Spur. Die Standalone-App und das AU empfangen Program Change immer. Manche VST3-Hosts geben es nicht weiter; sende dann stattdessen den Recall CC.

### Der MIDI-Tab

![VoLum Einstellungen, MIDI-Tab](user-guide-settings-midi.png)

**What this VoLum listens to:**

- **All channels** (Standard; in MIDI heißt das Omni) passt für einen Gitarristen mit einem Board.
- **One channel** mit einem Kanal von `CH 1` bis `CH 16` lässt zwei VoLums ein MIDI-Kabel teilen.
- **Recall CC** stellt die CC-Nummer ein, die Sounds aufruft: `0` bis `119`, Standard `102`.

**What each program number plays** zeigt deine Sounds wie ein Fußcontroller: 16 Bänke mit je 8 Schaltern. Bank 1 hält die Programme 0 bis 7, Bank 2 die 8 bis 15, und so weiter bis Bank 16 (120 bis 127). Es ist dieselbe Liste wie in PLAY; eine Änderung hier erscheint dort und umgekehrt.

- Wechsle die Bank mit den Pfeilen neben **BANK**, dem Mausrad, `PageUp` / `PageDown` oder den 16 Punkten (ein leuchtender Punkt heißt, die Bank hat Sounds).
- Klicke einen Schalter, um seinen Sound zu wählen. Fahr über einen Schalter und klicke `×`, um ihn zu leeren.
- Zieh einen Schalter auf einen anderen, um beide zu tauschen, oder auf einen leeren, um ihn zu verschieben. Beim Ziehen wechselt das Verweilen über einem Pfeil oder Punkt die Bank.
- Der Schalter des LIVE-Sounds leuchtet **LIVE**. Ein Schalter, dessen Sound gelöscht wurde, ist rot.

Die PLAY-Liste teilen sich alle VoLums auf deinem Rechner. Die Empfangseinstellung und der Recall CC gehören zu jeder Plug-in-Instanz; eine neue Instanz startet auf All channels und CC 102.

VoLum nutzt keine MIDI-Noten, kein Pitch Bend, kein Bank Select (CC 0/32), kein MIDI Learn und keine MIDI-Ausgabe. CC 120 bis 127 können nicht der Recall CC sein, weil Hosts sie reservieren.

Unter macOS macht der MIDI-Eingang das AU zu einem "aumf" (Music Effect) statt einem "aufx" (Effect). Verhält sich eine AU-Instanz aus einer älteren VoLum-Version nach dem Update seltsam, entferne sie und füge sie neu ein.

## Tuner Und Metronom

![VoLum Tuner](user-guide-tuner.png)

Öffne den Tuner mit seiner Toolbar-Taste oder `T`. Solange er offen ist, ist deine Gitarre stumm, damit du leise stimmen kannst; das Metronom klickt weiter. Schließe ihn mit `Esc` oder einem Klick daneben.

![VoLum Metronom](user-guide-metronome.png)

Öffne das Metronom mit seiner Toolbar-Taste oder `M`. Schalte es ein, stell das Tempo mit `+` / `-` ein oder tippe einen Wert (30 bis 300 BPM, Standard 120), stell die Lautstärke ein und wähle `1/4`, `2/4`, `3/4`, `4/4` oder `6/8`.

## Eigene Amps, IRs Und Pedale

VoLum lädt deine eigenen NAM-Captures und Impulsantworten. Importe werden in VoLums eigene Bibliothek kopiert und funktionieren daher weiter, wenn du die Originale verschiebst oder löschst. Die Standalone-App und alle Plug-ins teilen sich diese Bibliothek.

### Eigene Amps

![VoLum Builder für eigene Amps](user-guide-custom-amp.png)

1. Klicke **+** im **CUSTOM**-Teil der Amp-Liste ("Create a custom amp").
2. Gib dem Amp einen Namen und klicke **+ Add .nam files**.
3. Gib jeder Datei einen Cab-Slot und einen Kanal. Dateien mit Namen wie `V30-MeinAmp-2.nam` füllen das selbst aus: der erste Teil ist das Cab (`AMP`, `DI` oder `DIRECT` für kein Cab, `G12`, `G65`, `V30`), die letzte Zahl der Kanal.
4. Klicke **Save amp**.

Jede Datei ist eine aufgenommene Kombination aus Kanal und Cab; VoLum schaltet zwischen ihnen um. NAM-A1- und A2-Captures funktionieren beide. Kann eine Datei nicht gelesen werden, wird nichts gespeichert, und der Builder nennt die Datei.

Eigene Amps spielen wie mitgelieferte, auch als Dual-Amp-SUPPORT. Mit dem Stift- und dem Papierkorb-Symbol in der Amp-Liste bearbeitest oder löschst du einen. Fehlt später eine Datei, sagt die Fußzeile das, und VoLum behält das letzte Capture, das funktioniert hat.

### Eigene IRs

![VoLum Menü für eigene IRs](user-guide-custom-ir.png)

Ein eigenes IR ist dein eigenes Lautsprecher-Cab. Es ersetzt das Cab und braucht daher ein Capture nur vom Amp (No Cab): jeder mitgelieferte Amp hat eines, eigene Amps haben eines, wenn du eine `DIRECT`-Datei hinzugefügt hast. Wo es fehlt, sind **Custom IR** und **No Cab** ausgegraut; fahr mit der Maus darüber, um den Grund zu sehen.

1. Klicke **Custom IR** in der Cab-Reihe.
2. Klicke im Menü **Manage custom IRs...**, dann **+ Import IR (.wav)**.
3. Wähle das IR im **Custom IR**-Menü.

Jede Dual-Amp-Spur hat ihr eigenes IR. VoLum bringt importierte IRs auf eine ähnliche Lautheit wie die eingebauten Cabs und lehnt sehr große Dateien ab (etwa einen versehentlich gewählten ganzen Song).

Zum Feinabstimmen eines IRs klickst du das **Zahnrad** in seiner Zeile unter **Manage custom IRs**. **Level** (±24 dB), **Low cut** (20 bis 800 Hz) und **High cut** (1 bis 20 kHz) stellst du mit **+** / **−** ein, oder du klickst einen Wert an und tippst ihn, zum Beispiel `2.5k` oder `-3 dB`. `0` oder `off` schaltet einen Filter aus. Diese Einstellungen gehören zum IR, wo immer du es nutzt. Ein goldenes Zahnrad markiert ein angepasstes IR.

### Eigene Pedale

![VoLum Menü für eigene Pedale](user-guide-custom-pedal.png)

Klicke im Capture-Menü der NAM-Pedale **Manage custom pedals...**, dann **+ Import pedal (.nam)**. Importierte Pedale stehen im selben Menü unter **CUSTOM**.

### Inhalte Löschen

Löschst du etwas, das gerade spielt, macht VoLum im selben Schritt weiter: ein gelöschter Amp fällt auf einen mitgelieferten zurück, ein gelöschtes Pedal lässt seinen Slot leer, ein gelöschtes IR kehrt zum Cab des Amps zurück. Die Rückfrage sagt dir vorher, was passiert. Ein anderes offenes VoLum spielt weiter, bis es das gelöschte Teil das nächste Mal braucht.

## Sichern Und Teilen Mit Packs

![VoLum Pack-Export](user-guide-pack-export.png)

Ein Pack ist eine `.volumpack`-Datei mit deinen eigenen Amps, IRs, Pedalen und Presets. Damit sicherst du deine Bibliothek, ziehst sie auf einen anderen Rechner um oder teilst einen Teil davon. Öffne **Einstellungen > SYSTEM** und nutze **Export Pack...** oder **Import Pack...** auf der Karte **Back up your library**.

### Export

Wähle, was hinein soll:

- **Everything:** deine ganze Bibliothek und deine PLAY-Programmliste. Aus der Standalone-App kommen auch deine Rechner-Einstellungen mit.
- **Sounds:** hake Presets an. Deine PLAY-Sounds stehen oben, in Programmreihenfolge.
- **A whole amp:** ein eigener Amp mit allen darauf gespeicherten Presets.

Das Band **ALSO INCLUDING** listet, was deine Auswahl braucht, etwa ein IR oder ein Pedal. Es kommt immer mit, damit nichts kaputt ankommt. Ein Preset auf einem mitgelieferten Amp bringt sein eigenes IR und Pedal mit, aber keinen Amp, denn der andere Rechner hat den mitgelieferten Amp schon.

### Import

![VoLum Pack-Import-Vorschau](user-guide-pack-import.png)

Die Vorschau listet alles im Pack, alles angehakt. Entferne den Haken bei dem, was du nicht willst; was ein angehaktes Preset braucht, bleibt angehakt. Jede Zeile sagt, was passiert: **Add**, **Replace**, **Keep mine**, **Reloads** (spielt gerade) oder **Skip**. Ein Teil mit demselben Namen wie eines von dir kommt daneben dazu, und beide bleiben.

Ein **Everything**-Pack bietet drei Arten zu mischen:

- **Overwrite:** das Pack gewinnt, wo beide ein Teil haben. Deine anderen Teile bleiben.
- **Add:** deine Version gewinnt. Nur neue Teile kommen dazu.
- **Reset:** das Pack ersetzt deine Bibliothek. Teile, die nicht im Pack sind, werden gelöscht. Nur möglich, solange alles angehakt ist.

Ein **Sounds**- oder **A whole amp**-Pack mischt immer wie Overwrite und bietet nie Reset an. Ein Pack von Freunden kann deine Bibliothek also nicht löschen.

In der Standalone-App bietet ein Everything-Pack außerdem **Also restore machine settings** (letzter Amp, Einstellungen pro Amp, Lite, Eingangskalibrierung und MIDI-Programmliste).

Deine vorherige Bibliothek bleibt daneben als `volum-content.json.packbak` erhalten. Ein beschädigtes Pack ändert nichts und meldet **This Pack is damaged.** Ein Pack aus einer neueren VoLum-Version wird abgelehnt.

## Einstellungen

Öffne die Einstellungen mit dem Zahnrad oder `H`. Schließe sie mit dem Zahnrad, dem X, `H`, `Esc` oder einem Klick neben das Panel. Die Einstellungen haben drei Tabs und öffnen sich wieder auf dem zuletzt genutzten.

### SIGNAL-Tab

![VoLum Einstellungen, SIGNAL-Tab](user-guide-settings-signal.png)

- **Input calibration:** gib den Eingangspegel deines Interfaces in dBu ein und schalte **Calibrate input** ein. VoLum fährt den Amp dann so hart an wie bei der Aufnahme. Das braucht ein Capture, das seinen Aufnahmepegel mitspeichert. Die mitgelieferten Amps tun das nicht, für sie zeigt die Karte "This model has no capture level".
- **Output mode:** **Raw** lässt den Pegel, wie er ist, **Normalized** (Standard) bringt alle Amps auf eine ähnliche Lautstärke, **Calibrated** passt zu deiner Eingangskalibrierung (nur für Captures, die einen Ausgangspegel mitbringen).
- **Performance:** **FULL** (Standard) oder **LITE**. LITE spielt eine kleinere Version der A2-Captures auf jeder Amp- und NAM-Pedal-Spur, für weniger CPU bei etwas geringerer Qualität. Das Pitch-Pedal und ältere Nicht-A2-Captures betrifft es nicht. **Animate art in PLAY** (Standard ON) schaltet das bewegte Bild ein oder aus; der Klang ändert sich nicht.
- **Audio & MIDI devices...** (nur Standalone) öffnet das Fenster für Audio- und MIDI-Geräte. Im Plug-in nimmst du die Audio-Einstellungen deiner DAW.

LITE und Animate art gelten für deinen Rechner, nicht pro Projekt. Die Eingangskalibrierung ist der Standard für jedes neue VoLum auf diesem Rechner.

### MIDI-Tab

Siehe [MIDI-Fußcontroller](#der-midi-tab).

### SYSTEM-Tab

![VoLum Einstellungen, SYSTEM-Tab](user-guide-settings-system.png)

- **Keyboard shortcuts:** eine kurze Tastenliste.
- **Model information:** Details zum geladenen Capture.
- **Back up your library:** **Export Pack...** und **Import Pack...** (siehe [Packs](#sichern-und-teilen-mit-packs)).
- **About:** Version, ein Link **Read the manual** zu diesem Handbuch (englisch), **Check for updates automatically** und **Check now**.

### Update-Prüfung Und Datenschutz

VoLum sucht höchstens einmal am Tag nach einer neuen Version. Ein goldener Punkt am Zahnrad heißt, dass eine verfügbar ist; öffne die Einstellungen, um sie zu lesen, und folge dem Link zur Release-Seite. VoLum lädt oder installiert nie selbst etwas.

Die Prüfung ist eine einfache Anfrage an `https://guitarlum.github.io/VoLum/appcast.json`, ohne Tracking und ohne Kennung. Abschalten kannst du sie mit **Check for updates automatically**.

### Audio- Und MIDI-Geräte (Standalone)

Öffne das Fenster mit **Audio & MIDI devices...** auf dem SIGNAL-Tab. Unter Windows geht auch `Strg+,`; unter macOS **VoLum > Preferences...**.

- Wähle Treiber, Ein- und Ausgabegerät (sie dürfen verschieden sein), Samplerate, Puffergröße und MIDI-Eingang.
- Nimm einen Mono-Eingangskanal für die Gitarre.
- Puffergrößen sind 48, 64, 96, 128, 256, 512, 1024, 2048, 4096 und 8192 Samples. Gewährt der Treiber eine andere Größe, zeigt und behält VoLum die, die wirklich läuft.
- Die angezeigte Samplerate ist die, mit der der Treiber wirklich läuft. Lehnt dein Interface eine Rate ab, sagt VoLum dir, welche es stattdessen nutzt.

Die Zeile **Latency** zeigt, was du hörst, wenn der Treiber seine eigene Verzögerung meldet (ASIO tut das). Sonst zeigt sie VoLums Anteil plus Puffer und sagt, dass der echte Wert höher ist. VoLum selbst fügt keine Verzögerung hinzu, außer ein Capture läuft mit einer anderen Samplerate als dein Interface (etwa 1,4 ms bei 44,1 kHz) oder das Pitch-Pedal ist an.

## VoLum In Einer DAW

- VoLum läuft als VST3-Plug-in und unter macOS als AU. Logic Pro und GarageBand brauchen das AU.
- **Deine Standalone-Sounds sind der Startpunkt.** Eine neue Plug-in-Instanz startet mit den Einstellungen pro Amp aus der Standalone-App. Danach speichert das DAW-Projekt die Einstellungen dieser Instanz. Plug-ins schreiben nie Einstellungen pro Amp zurück, zwei Spuren können sich also nicht gegenseitig die Sounds überschreiben.
- **Pro Rechner:** LITE und Animate art werden nicht im Projekt gespeichert. Ein Projekt von einem schnellen Rechner läuft auf einem langsamen also trotzdem mit LITE. Eine in irgendeinem VoLum geänderte Eingangskalibrierung wird zum Standard für neue Instanzen; ein gespeichertes Projekt behält seine eigene.
- **Pro Instanz:** BUILD oder PLAY, die MIDI-Empfangseinstellung und der Recall CC. Eine neue Instanz startet in BUILD, auf All channels und CC 102.
- **Geteilt:** die eigene Bibliothek und die PLAY-Programmliste. Ein Projekt speichert Verweise auf deine eigenen Amps, IRs, Pedale und Presets und findet sie wieder, solange sie noch in deiner Bibliothek sind.
- Im Plug-in schaltet `B` das gewählte Pedal ein und aus, damit `Space` für Play/Stop deiner DAW frei bleibt.
- VoLum meldet seine Verzögerung an die DAW, die sie automatisch ausgleicht.

## Tastenkürzel

Tasten wirken, solange kein Textfeld offen ist. Solange Einstellungen, ein Pack-Fenster oder ein anderes Panel offen ist, ändern Tasten weder den Amp noch PLAY dahinter.

**Überall**

| Taste | Aktion |
| --- | --- |
| `P` | Zwischen BUILD und PLAY wechseln |
| `T` / `M` / `H` | Tuner / Metronom / Einstellungen |
| `Strg+S` | Preset oder Sound speichern |
| `Esc` | Offenes Panel oder Menü schließen |

**BUILD**

| Taste | Aktion |
| --- | --- |
| `1` / `2` / `3` | PRE / AMP / POST öffnen |
| `Up` / `Down` | Vorheriger / nächster Amp |
| `Left` / `Right` | AMP: vorheriger / nächster Kanal. PRE und POST: vorheriges / nächstes Pedal |
| `Tab` / `Shift+Tab` | Nächstes / vorheriges Pedal; mit Dual Amp zwischen MAIN und SUPPORT wechseln |
| `S` / `Shift+S` | Nächstes / vorheriges Cab (AMP-Bereich) |
| `Enter` | Einen Regler des gewählten Pedals oder Amps auswählen |
| `Space` (Standalone), `B` (Plug-in) | Gewähltes Pedal ein- oder ausschalten; im AMP-Bereich Dual Amp |

**Ausgewählter Regler**

| Taste | Aktion |
| --- | --- |
| `Up` / `Down` | Regler drehen (`Shift` für feine Schritte) |
| `Left` / `Right` | Vorherigen / nächsten Regler auswählen |
| `Enter` | Genauen Wert eintippen |
| `Delete` / `Backspace` | Auf Standard zurücksetzen |
| `Esc` | Regler abwählen |

Beim Eintippen eines Werts gilt ein Komma als Dezimalpunkt, und du darfst die Einheit anhängen (`dB`, `%`, `ms`, `Hz`, `s`, `st`). Werte außerhalb des Bereichs springen auf das nächste Ende.

**PLAY**

| Taste | Aktion |
| --- | --- |
| `Up` / `Down`, `Left` / `Right` | Vorheriger / nächster Sound |
| `1` bis `8` | Die acht Stomps von links nach rechts schalten |

**MIDI-Tab der Einstellungen:** `PageUp` / `PageDown` wechseln die Bank. `Esc` schließt zuerst eine offene Sound-Auswahl, dann die Einstellungen.

Volle Screenreader-Unterstützung gibt es noch nicht.

## Fehlerbehebung

- **"Output safety active - lower output or wet mix" in der Fußzeile, rote OUT-Anzeige:** VoLums Ausgangslimiter hat ausufernde Spitzen abgefangen. Nimm den Amp-**OUTPUT**, Delay-**MIX** oder Reverb-**MIX** zurück.
- **Kein Ton bei offenem Tuner:** das ist Absicht; schließe den Tuner.
- **Dual Amp klingt dünn:** schalte **Ø** auf der SUPPORT-Spur um.
- **Fußcontroller in der DAW ignoriert:** sende den Recall CC (102) statt Program Change und prüfe den Kanal im MIDI-Tab.
- **Der gewählte Treiber hat kein Gerät** (zum Beispiel ASIO ohne ASIO-Interface): VoLum zeigt einen Fehler und kehrt zum letzten funktionierenden Setup zurück.
- **Interface beim Start nicht angeschlossen:** VoLum öffnet ohne Audio und behält deine Einstellungen. Schließ das Interface an und starte VoLum neu.
- **Windows: VoLum meldet, dass schon eine Kopie läuft:** diese Kopie ist wirklich noch offen. Beende sie im Task-Manager und starte VoLum neu.
- **macOS: das Plug-in erscheint nach der Installation aus dem Zip nicht:** siehe [README](../README.de.md#wichtiger-sicherheitshinweis).

## Wo VoLum Deine Dateien Ablegt

| Was | Windows | macOS |
| --- | --- | --- |
| Einstellungen | `%LOCALAPPDATA%\VoLum\volum-settings.json` | `~/Library/Application Support/VoLum/volum-settings.json` |
| Eigene Bibliothek | `%LOCALAPPDATA%\VoLum\content` | `~/Library/Application Support/VoLum/content` |
| Diagnose-Log | `%LOCALAPPDATA%\VoLum\volum.log` | `~/Library/Application Support/VoLum/volum.log` |

Kann VoLum die Bibliothek nicht lesen, behält es die beschädigte Datei als `volum-content.json.bak` und sagt es dir beim nächsten Öffnen eines Fensters. Das Log ist klein, kürzt sich selbst und hält Start, Audio-Setup, jedes Laden von Amp und IR sowie alle Fehler fest.

## Fehler Melden Oder Feature Vorschlagen

Öffne ein [Issue auf GitHub](https://github.com/guitarlum/VoLum/issues/new/choose). Nimm **Bug report** für Abstürze oder falsches Verhalten und **Feature request** für Ideen. Häng nach Möglichkeit `volum.log` an (siehe [Wo VoLum Deine Dateien Ablegt](#wo-volum-deine-dateien-ablegt)).
