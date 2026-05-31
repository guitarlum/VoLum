# VoLum Benutzerhandbuch

**Sprachen:** [English](user-guide.en.md) | Deutsch

Dieses Handbuch erklärt die VoLum-1.0-Oberfläche nach der Installation. Downloads, Hinweise zu unsignierten Builds und Installationspfade stehen in der [Haupt-README](../README.de.md).

## Hauptansicht

![VoLum Hauptansicht](user-guide-main.png)

1. **Amp-Browser:** wähle einen der mitgelieferten Amps.
2. **Amp-Panel:** zeigt den fokussierten Amp. Im Dual-Amp-Modus teilt es sich in MAIN und SUPPORT.
3. **Speaker- und Channel-Steuerung:** wähle `AMP`, `G12`, `G65` oder `V30`, dann den Gain-Stage-Kanal.
4. **Reglerzeile:** bearbeitet den fokussierten Amp, das PRE-Pedal oder den POST-Effekt.
5. **PRE | AMP | POST-Leiste:** öffnet immer genau einen Bereich.
6. **Toolbar:** Tuner, Metronom und Einstellungen sitzen oben rechts.

Die mitgelieferten NAM-Profile wurden mit einem Interface-Eingangspegel um +4 dBu aufgenommen. Nutze einen ähnlichen Pro-Line-Eingangspegel in VoLum, um den aufgenommenen Sounds möglichst nah zu kommen.

VoLum speichert die meisten Spiel-Einstellungen pro Amp. Wenn du zu einem Amp zurückkehrst, stellt VoLum Speaker, Kanal, Regler, PRE-Pedale, POST-Effekte und Dual Amp wieder her.

## Amp Wählen

Die linke Seitenleiste enthält die 15 mitgelieferten Amps. Jeder Amp hat vier Speaker-Modi und eine eigene Anzahl an Gain-Stage-Kanälen. VoLum lädt Modelle im Hintergrund, deshalb ist das Zurückwechseln zu einem bereits geladenen Amp schnell.

Kurze Orientierung:

- **Vintage:** Orange ORS100, Orange OD120, Marshall JMP 2203, Marshall 2204.
- **Modern/High Gain:** Diezel Herbert, Soldano SLO100, Marshall JVM, H&K TriAmp.
- **Boutique/Charakter:** Ampete One, Bad Cat Mini Cat, Brunetti XL 2, Lichtlaerm Prometheus, Sebago Texas Flood, THC Sunset, Fryette Deliverance.

Amp-EQ und Pedal-EQ sind zusätzliche Klangregler. Sie müssen nicht den physischen Reglerstellungen entsprechen, mit denen das Profil aufgenommen wurde.

Der Amp-Regler **OUTPUT** bleibt bei `0.0 dB` auf Unity-Gain. Ganz gegen den Uhrzeigersinn zeigt er `-∞ dB` und schaltet den Amp-Ausgang vollständig stumm.

## PRE-Bereich

![VoLum PRE-Bereich](user-guide-pre.png)

PRE liegt vor dem Amp. Der Bereich enthält einen Kompressor und zwei frei belegbare NAM-Pedal-Slots.

1. Klick auf **PRE**.
2. Klick auf **COMP**, **NAM 1** oder **NAM 2**, um eine Karte zu fokussieren.
3. Nutze die Reglerzeile für diese Karte.
4. Klick eine fokussierte NAM-Karte erneut an, um ein Capture zu wählen.

Pedal-Captures sind nach Typ gruppiert und von weniger zu mehr Gain sortiert. Gute Startpunkte:

- Clean- oder Low-Gain-Amps: Nuke, Bender, Myth, Mash.
- Edge-of-Breakup-Amps: Revival Drive.
- Mid-/High-Gain-Amps: Klon, TS, TS+, Fatbee.

PRE-Einstellungen werden pro Amp gespeichert.

Klicke auf das **Schloss** im PRE-Kopf, um die aktuelle PRE-Szene als globale Overlay-Szene beim Amp-Wechsel mitzunehmen. Solange PRE gesperrt ist, lädt VoLum keine PRE-Werte von anderen Amps, und deine live geänderten PRE-Einstellungen werden nicht in deren gespeicherte Daten geschrieben. Unterscheidet sich die Live-Szene vom gespeicherten PRE des aktiven Amps, erscheint ein **Store**-Pfeil — damit schreibst du das Overlay nur in den aktuellen Amp. Nochmal auf das Schloss klicken entsperrt; VoLum stellt still das gespeicherte PRE dieses Amps wieder her und verwirft ungespeicherte Overlay-Änderungen.

Der Kompressor-Regler **OUTPUT** und beide NAM-Pedal-Regler **LEVEL** schalten ihre jeweilige Stufe bei der ganz linken Einstellung `-∞ dB` vollständig stumm.

## Dual Amp

![VoLum Dual-Amp-Ansicht](user-guide-dual-amp.png)

Dual Amp kombiniert den Haupt-Amp mit einem Support-Amp.

1. Öffne die AMP-Ansicht.
2. Klick auf die geteilte **Dual Amp**-Schaltfläche.
3. Klick auf die SUPPORT-Seite, um den zweiten Amp zu wählen.
4. Klick MAIN oder SUPPORT, um eine Spur zu fokussieren.
5. Stelle Speaker, Kanal, Regler und Pan für die fokussierte Spur ein.

Die Support-Spur hat einen `Ø`-Polaritätsschalter. Er ist bei neuen Dual-Amp-Setups standardmäßig aktiv, weil manche mittigen Amp-Stacks so besser summieren. Wenn ein Stack dünn oder phasig klingt, schalte `Ø` um und prüfe danach Speaker und Kanal beider Spuren.

VoLum gleicht MAIN- und SUPPORT-NAM-Latenzen aus, bevor die Spuren gepannt und summiert werden.

Der **OUTPUT**-Regler der SUPPORT-Spur schaltet diese Spur bei der ganz linken Einstellung `-∞ dB` ebenfalls vollständig stumm.

## POST-Bereich

![VoLum POST-Bereich](user-guide-post.png)

POST liegt hinter dem Amp. Der Bereich enthält Delay- und Reverb-Karten.

1. Klick auf **POST**.
2. Klick auf **DELAY** oder **REVERB**.
3. Nutze den Karten-Button oder die Leertaste zum Ein- und Ausschalten.
4. Bearbeite die fokussierte Karte in der Reglerzeile.

**Delay** bietet Digital, Analog und Reverse. Die Regler sind Time, Feedback, Mix, Tone und ein modusspezifischer Charakterregler: `Grit`, `Wear` oder `Bloom`. Ping-Pong gibt es für Digital und Analog.

**Reverb** bietet Hall, Plate und Oktaverb. Hall und Plate liefern klassische Räume. Oktaverb ergänzt die Pitch-Wash-Stimmen `HALO`, `SHIMMER` und `BLOOM` mit Intensity-Regler.

Die LED auf jeder Karte zeigt, ob sie aktiv ist. Das Label zeigt den aktuellen Modus oder eine kurze Preset-Zusammenfassung. POST-Einstellungen werden pro Amp gespeichert, genau wie PRE. Das **Schloss** im POST-Kopf funktioniert wie bei PRE, damit du eine Delay/Reverb-Szene beim Durchklicken der Amps mitnimmst; der **Store**-Pfeil erscheint, wenn du das Overlay im aktuellen Amp speichern kannst. Entsperren stellt das gespeicherte POST dieses Amps ohne Rückfrage wieder her. Ein Doppelklick auf einen POST-Regler stellt dessen Default wieder her.

Beim Wechsel von Delay-Modus, Ping-Pong, Reverb-Modus oder Oktaverb-Stimme wird der alte Tail gelöscht, damit Wiederholungen und Raum aus dem vorherigen Modus nicht in den neuen Modus bluten.

## Tuner Und Metronom

![VoLum Tuner-Overlay](user-guide-tuner.png)

Öffne den Tuner über die Toolbar. Solange er geöffnet ist, schaltet VoLum den Ausgang stumm, damit du lautlos stimmen kannst. Klick außerhalb des Fensters oder drücke `Esc`, um ihn zu schließen.

![VoLum Metronom-Steuerung](user-guide-metronome.png)

Öffne das Metronom über die Toolbar. Du kannst es einschalten, BPM mit `+` / `-` oder direkter Eingabe setzen, die Lautstärke ändern und `1/4`, `2/4`, `3/4`, `4/4` oder `6/8` wählen.

## Tastatur

- Ohne gewählten Regler: `Hoch` / `Runter` wechselt den Amp, `Links` / `Rechts` wechselt den Kanal in der AMP-Ansicht.
- `1` / `2` / `3` wechselt PRE / AMP / POST.
- `Tab` / `Umschalt+Tab` bewegt den Fokus im aktuellen Bereich; `Links` / `Rechts` auch in PRE/POST.
- `Enter` bearbeitet das fokussierte Ziel; `Leertaste` schaltet es ein/aus, wenn möglich.
- In manchen DAWs (unter anderem REAPER) kommt die Leertaste zuerst beim Host an. Rechtsklick auf den FX-Header des Plugins und **Send all keyboard input to plug-in** aktivieren, damit Shortcuts VoLum erreichen.
- `S` wechselt Speaker/Cab der fokussierten Amp-Spur; `Umschalt+S` rückwärts.
- `T` öffnet den Tuner; `M` öffnet das Metronom; `H` öffnet Einstellungen.
- Gewählter Regler: `Hoch` / `Runter` ändert den Wert, `Links` / `Rechts` wählt einen anderen Regler, `Umschalt` macht kleinere Schritte.
- `Enter` gibt einen exakten Wert ein, `Entf` / `Rücktaste` setzt zurück, `Esc` beendet die Reglerbearbeitung.

Das deckt den wichtigsten Spiel- und Bearbeitungsablauf ab. Vollständige Screenreader-Unterstützung gibt es noch nicht.

## Einstellungen Und Sicherheit

Öffne Einstellungen über das Zahnrad oben rechts oder mit `H`. Das Overlay enthält auch die Tastaturübersicht und globale Einstellungen.

VoLum speichert Benutzereinstellungen automatisch:

- **Windows:** `%LOCALAPPDATA%\VoLum\volum-settings.json`
- **macOS:** `~/Library/Application Support/VoLum/volum-settings.json`

Nutze die Standalone-App als Editor für deine Klangbibliothek. Sie schreibt die globalen Defaults pro Amp in diese Datei, inklusive Speaker, Kanal, Regler, PRE-Pedale, POST-Effekte und Dual-Amp-Setup.

Neue VST3-Instanzen lesen diese Defaults, wenn du VoLum auf eine Spur lädst. Danach gehört der Zustand dieser Plugin-Instanz dem DAW-Projekt. Reaper, Cubase, Live und andere Hosts speichern und laden den VST3-Zustand mit dem Projekt und mit ihren normalen Plugin-Preset-Systemen. VST3-Instanzen schreiben die globale VoLum-Einstellungsdatei nicht, deshalb können zwei Spuren einander die Defaults nicht überschreiben.

### Standalone-Audioeinstellungen

In der Standalone-App öffnest du **File -> Preferences** oder drückst `Ctrl+,`, um Audiotreiber, getrennte Ein- und Ausgabegeräte, Samplerate und Kanalrouting zu wählen. In der VST3-Version nutzt du stattdessen die Audioeinstellungen deiner DAW.

Wähle Eingabe- und Ausgabegerät unabhängig voneinander. Unter macOS erscheinen Mikrofon und Lautsprecher oft als getrennte Geräte. Route Input L/R und Output L/R nach Bedarf; beide Eingangskanäle sind in 1.0.1 wieder verfügbar (ein geplanter Single-Device-/Mono-Input-Shortcut wurde verworfen, weil er die macOS-Gerätelisten leer ließ). Die Standalone-Bufferliste nutzt eine stabile Auswahl gängiger Pro-Audio-Größen: 48, 64, 96, 128, 256, 512, 1024, 2048, 4096 und 8192 Samples. Ältere gespeicherte Werte unterhalb der sichtbaren Liste werden auf die nächste sichtbare Größe angehoben.

Wenn du einen Treiber ohne nutzbares Gerät auswählst, zum Beispiel ASIO auf einem Laptop ohne ASIO-Interface, zeigt VoLum eine Fehlermeldung und stellt die vorher funktionierende Audiokonfiguration wieder her, statt sich zu schließen.

VoLum besitzt außerdem eine immer aktive Ausgangs-Schutzstufe nach Delay und Reverb. Normales Spielen bleibt unverändert. Wenn ein heißes Rig und starke POST-Effekte durchgehende Peaks erzeugen, wird das OUT-Meter rot und der Footer zeigt `Output safety active - lower output or wet mix`. Drehe Output, Delay Mix oder Reverb Mix zurück, wenn du das oft siehst.

## Fehler Melden Oder Feature Vorschlagen

Erstelle ein [Issue auf GitHub](https://github.com/guitarlum/VoLum/issues/new/choose). Nutze die Vorlage **Bug report** für Abstürze oder Fehlverhalten und **Feature request** für Ideen.
