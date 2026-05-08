# VoLum Benutzerhandbuch

**Sprachen:** [English](user-guide.en.md) | Deutsch

Dieses Handbuch erklärt die VoLum-Oberfläche nach der Installation. Download und Einrichtung stehen in der [Haupt-README](../README.de.md).

## Hauptansicht

![VoLum Hauptansicht](user-guide-main.png)

1. **Amp-Browser:** wähle einen der mitgelieferten Amp-Modelle in der linken Seitenleiste.
2. **Amp-Panel:** zeigt den aktuell gewählten Amp. Im Dual-Amp-Modus teilt es sich in MAIN- und SUPPORT-Spur.
3. **Speaker- und Channel-Steuerung:** wähle Cab/Speaker-Variante und Gain-Stufe/Kanal für den fokussierten Amp.
4. **Reglerzeile:** bearbeitet den aktuell fokussierten Amp, das PRE-Pedal oder den POST-Effekt.
5. **PRE | AMP | POST-Leiste:** klick auf einen Bereich, um ihn auszuklappen. Es ist immer nur ein Bereich geöffnet.
6. **Toolbar:** Tuner, Metronom und Einstellungen sitzen oben rechts.

Die meisten Amp-Regler werden pro Amp gespeichert. Wenn du zu einem Amp zurückkehrst, stellt VoLum Speaker, Kanal, Regler, PRE-Pedale und Dual-Amp-Setup wieder her.

## Amp-Notizen

VoLum enthält eine Mischung aus Vintage-, modernen und Boutique-Amp-Captures. Diese Notizen sind kurze Orientierungshilfen, keine festen Regeln.

- **Orange ORS100 1972:** alter Picture-Panel-Orange-Sound ohne Master Volume. Erwarte großen, clean-bis-lauten Power-Amp-Charakter statt moderner Preamp-Zerre.
- **Orange OD120 1975:** die Overdrive-Linie der klassischen Orange-Schaltung. Sie ergänzt eine Master-Volume-Gain-Struktur und kann dadurch mehr Gain liefern als der frühere Picture-Panel-ORS100.
- **Marshall JMP 2203 1976:** früher Master-Volume-Marshall und Übergangsmodell in Richtung JCM800-2203-Sound.
- **Marshall 2204 1982:** Vertical-Input-50W-Marshall aus der JCM800-Ära.
- **Marshall JVM 210H:** modifiziert mit der OD1-Voice aus dem JVM 410.
- **Lichtlaerm Prometheus:** KT88/EL34-Endstufe mit viel Headroom.
- **Diezel Herbert Mk1:** später Mk1-Herbert, nahe am Mk2-Feature-Set, aber mit älterem Mk1-Sound.
- **Soldano SLO100:** SLO im 2021-Stil mit Deep/Depth-Control-Version.
- **Sebago Texas Flood:** High-End-100W-Steel-String-Singer-inspirierter Amp mit Premium-Optionen und Transformer-Auswahl.
- **THC Sunset:** deutscher Boutique-Amp mit Trainwreck-inspirierter Richtung.

## Tuner Und Metronom

![VoLum Tuner-Overlay](user-guide-tuner.png)

Öffne den Tuner über die Toolbar oben rechts. Solange der Tuner geöffnet ist, schaltet VoLum den Ausgang stumm, damit du lautlos stimmen kannst. Klick außerhalb des Tuners oder drücke `Esc`, um ihn zu schließen.

![VoLum Metronom-Steuerung](user-guide-metronome.png)

Öffne das Metronom über die Toolbar oben rechts. Du kannst es ein- oder ausschalten, BPM mit `+` und `-` oder per direkter Eingabe setzen, die Lautstärke ändern und `1/4`, `2/4`, `3/4`, `4/4` oder `6/8` wählen.

## Dual Amp

![VoLum Dual-Amp-Ansicht](user-guide-dual-amp.png)

Dual Amp kombiniert den Haupt-Amp mit einem Support-Amp.

1. Klick auf die geteilte Dual-Amp-Schaltfläche im Amp-Panel.
2. Klick auf die SUPPORT-Seite, um einen zweiten Amp zu wählen.
3. Klick auf MAIN oder SUPPORT, um diese Spur zu fokussieren. Speaker, Channel und Reglerzeile folgen der fokussierten Spur.
4. Nutze die Pan-Regler der Spuren, um die beiden Amps im Stereobild zu platzieren.

Wenn Dual Amp aktiv ist, zeigt die OUT-Anzeige getrennte linke und rechte Balken. MAIN- und SUPPORT-Einstellungen werden mit dem aktuellen Haupt-Amp gespeichert, sodass jeder Amp sein eigenes Pairing haben kann.

VoLum gleicht unterschiedliche NAM-Resampler-Latenzen zwischen MAIN und SUPPORT aus, bevor die Spuren gepannt und summiert werden. Das `Ø`-Symbol oben rechts am SUPPORT-Amp invertiert dessen Polarität gegenüber MAIN und ist bei neuen Dual-Amp-Setups standardmäßig aktiv; das kann mittige Mono-Stacks bei manchen Capture-Paaren verbessern. Wenn ein mittig gestackter Dual-Amp-Sound trotzdem phasig klingt, prüfe, ob beide Spuren die gewünschte Speaker-/Channel-Datei nutzen, und vergleiche Gate-, EQ-, IR- und Output-Einstellungen.

## PRE-Bereich

![VoLum PRE-Bereich](user-guide-pre.png)

Der PRE-Bereich liegt vor dem Amp. Er enthält einen Kompressor und zwei frei belegbare NAM-Pedal-Slots. Klick auf PRE in der Triptych-Leiste, um den Bereich auszuklappen, und dann auf eine Karte, um ihre Regler in der Reglerzeile zu fokussieren.

Klick auf eine NAM-Pedalkarte, während sie fokussiert ist, um die Capture-Auswahl zu öffnen. Pedal-Captures sind nach Pedaltyp gruppiert und innerhalb jeder Gruppe nach Gain sortiert.

Empfohlene Startpunkte:

- **Clean- oder Low-Gain-Amps:** Nuke, Bender, Myth und Mash.
- **Clean- bis Low-Gain-Amps:** Revival Drive.
- **Mid- bis High-Gain-Amps:** Minotaur Klon, TS und Fatbee.

Die EQ-Regler in VoLum zeigen nicht die echten Amp- oder Pedal-Einstellungen an, mit denen das Profil aufgenommen wurde. Für den beabsichtigten Capture-Sound musst du Amp-EQ und Pedal-EQ nicht anfassen. Nutze diese EQs nur, wenn du nach dem Profil zusätzlich formen möchtest.

PRE-Einstellungen sind lokal für den aktuellen Amp. So kann jeder Amp eigene Compressor-, Pedal- und Pedal-EQ-Einstellungen behalten.

## POST-Bereich

![VoLum POST-Bereich](user-guide-post.png)

Der POST-Bereich liegt hinter dem Amp. Er enthält Delay- und Reverb-Karten. Klick auf POST in der Triptych-Leiste, um den Bereich auszuklappen, und dann auf eine Karte, um ihre Regler in der Reglerzeile zu fokussieren.

- **Delay:** Digital-, Analog-, Tape- und Reverse-Modus mit Time, Feedback, Mix, Tone und einem fünften Regler, dessen Bezeichnung und Verhalten je nach Modus wechselt: **Grit** (Digital, Bit-Crush + Rauschteppich), **Wear** (Analog, BBD-Höhenrolloff + Chorus-Tiefe + Compander-Weichheit), **Age** (Tape, Wow/Flutter + Sättigung + Höhenrolloff-Skalierung), **Bloom** (Reverse, Weichheit der Einblendkurve von harter Dreiecksform bis zu sanftem sin²-Swell). Ein globaler Ping-Pong-Schalter wechselt die Wiederholungen zwischen links und rechts (im Reverse-Modus automatisch ausgeblendet). Der Tape-Modus zeigt zusätzlich einen Studio / Vintage / Broken Unter-Schalter für den Bandcharakter. Beim Überfahren des Reglers erscheint ein Tooltip, der das Verhalten im aktuellen Modus erklärt.
- **Reverb:** Hall-, Plate-, Oktaverb- und TremVerb-Modus mit Mix, Decay, Tone, Pre-Delay und Shimmer. Ein 3-Weg Unter-Schalter ändert den Reverb-Charakter pro Modus: Hall = Studio / Concert / Cathedral, Plate = Steel / Brass / Copper, Oktaverb = Oct / Oct+5th / Oct+SubOct (Oktav-Aufwärts-Shimmer mit optionaler Quint- oder Sub-Oktav-Stimme). TremVerb ist ein kurzer Plate-artiger Raum mit Photozellen-Tremolo nur auf dem Wet-Signal — der Shimmer-Regler wird zu Trem Depth und ein neuer Trem-Rate-Regler ersetzt Pre-Delay; der Unter-Schalter ist in diesem Modus ausgeblendet.

Die kleine LED auf jeder Karte zeigt, ob der Effekt aktiv ist. Das Label unten zeigt den aktuellen Modus oder eine kurze Preset-Zusammenfassung. POST-Effekte sind geteilt/global statt pro Amp gespeichert, also wie finale Effekte hinter der Amp-Sektion.

## Tastatur

- Ohne gewählten Regler: `Hoch` / `Runter` wechselt den Amp, `Links` / `Rechts` wechselt den Kanal in der AMP-Ansicht.
- Klick einen Regler an, um ihn per Tastatur zu steuern.
- Gewählter Regler: `Hoch` / `Runter` ändert den Wert, `Links` / `Rechts` wählt einen anderen Regler.
- Halte `Shift` für feinere Änderungen.
- `Enter` öffnet die direkte Zahleneingabe.
- `Entf` oder `Rücktaste` setzt den gewählten Regler zurück.
- `Esc` beendet den Regler-Tastaturmodus.

## Einstellungen

VoLum speichert Benutzereinstellungen automatisch:

- **Windows:** `%LOCALAPPDATA%\VoLum\volum-settings.json`
- **macOS:** `~/Library/Application Support/VoLum/volum-settings.json`

Standalone-App und VST3-Plugin verwenden auf jedem System denselben VoLum-Einstellungsort.