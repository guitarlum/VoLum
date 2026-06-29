# VoLum Benutzerhandbuch

**Sprachen:** [English](user-guide.en.md) | Deutsch

Dieses Handbuch erklärt die VoLum-1.2-Oberfläche nach der Installation. Downloads, Hinweise zu unsignierten Builds und Installationspfade stehen in der [Haupt-README](../README.de.md).

## Inhalt

- [Hauptansicht](#hauptansicht)
- [Amp Wählen](#amp-wählen)
- [PRE-Bereich](#pre-bereich)
- [Dual Amp](#dual-amp)
- [POST-Bereich](#post-bereich)
- [Presets](#presets)
- [Eigene Inhalte (Bring Your Own)](#eigene-inhalte-bring-your-own)
- [Tuner Und Metronom](#tuner-und-metronom)
- [Tastatur](#tastatur)
- [Einstellungen Und Sicherheit](#einstellungen-und-sicherheit)
- [Fehler Melden Oder Feature Vorschlagen](#fehler-melden-oder-feature-vorschlagen)

## Hauptansicht

![VoLum Hauptansicht](user-guide-main.png)

1. **Amp-Browser:** wähle einen der mitgelieferten Amps.
2. **Amp-Panel:** zeigt den fokussierten Amp. Im Dual-Amp-Modus teilt es sich in MAIN und SUPPORT.
3. **Kanal- und Cab-Steuerung:** wähle zuerst den Gain-Stage-Kanal, dann das Speaker-Cab (`AMP`/`No Cab`, `G12`, `G65`, `V30`). Bei eigenen Amps kommt der Kanal zuerst: die Reihe zeigt nur die Cabs, die es für den gewählten Kanal gibt, und ein Kanalwechsel behält dein Cab, wenn es noch passt, oder springt auf ein verfügbares.
4. **Reglerzeile:** bearbeitet den fokussierten Amp, das PRE-Pedal oder den POST-Effekt.
5. **PRE | AMP | POST-Leiste:** öffnet immer genau einen Bereich.
6. **Toolbar:** Tuner, Metronom und Einstellungen sitzen oben rechts.

Die mitgelieferten NAM-Profile wurden mit einem Interface-Eingangspegel um +4 dBu aufgenommen. Nutze einen ähnlichen Pro-Line-Eingangspegel in VoLum, um den aufgenommenen Sounds möglichst nah zu kommen. Jedes mitgelieferte Amp-, Cab- und PRE-NAM-Capture ist ein NAM-Architecture-2-(A2)-Profil, trainiert auf den besten Sitz zwischen 700 und 1200 Epochen. A2-Captures werden immer in voller Größe gespielt; VoLum fällt nie auf die Lite-Variante zurück.

VoLum speichert die meisten Spiel-Einstellungen pro Amp. Wenn du zu einem Amp zurückkehrst, stellt VoLum Speaker, Kanal, Regler, PRE-Pedale, POST-Effekte und Dual Amp wieder her.

## Amp Wählen

Die linke Seitenleiste enthält die 15 mitgelieferten Amps. Jeder Amp hat vier Speaker-Modi und eine eigene Anzahl an Gain-Stage-Kanälen. VoLum lädt Modelle im Hintergrund, deshalb ist das Zurückwechseln zu einem bereits geladenen Amp schnell.

Kurze Orientierung:

- **Clean, Blues und dynamische Boutique:** Sebago Texas Flood (ein Pedal-Platform im Stil des Dumble Steel String Singer), THC Sunset, Bad Cat Mini Cat.
- **Vintage und Classic-Rock-Crunch:** Orange ORS100, Orange OD120, Marshall JMP 2203, Marshall 2204.
- **Modern und High Gain:** Soldano SLO100, Diezel Herbert, Marshall JVM, H&K TriAmp, Fryette Deliverance, Lichtlaerm Prometheus, Brunetti XL 2.
- **Allrounder:** Der Ampete One vereint eine amerikanische und eine britische Stimme in einem Amp.

Amp-EQ und Pedal-EQ sind zusätzliche Klangregler. Sie müssen nicht den physischen Reglerstellungen entsprechen, mit denen das Profil aufgenommen wurde.

Der Amp-Regler **OUTPUT** bleibt bei `0.0 dB` auf Unity-Gain. Ganz gegen den Uhrzeigersinn zeigt er `-∞ dB` und schaltet den Amp-Ausgang vollständig stumm.

## PRE-Bereich

![VoLum PRE-Bereich](user-guide-pre.png)

PRE liegt vor dem Amp. Der Bereich enthält ein Pitch-Pedal, einen Kompressor und zwei frei belegbare NAM-Pedal-Slots.

1. Klick auf **PRE**.
2. Klick auf **PITCH**, **COMP**, **NAM 1** oder **NAM 2**, um eine Karte zu fokussieren.
3. Nutze die Reglerzeile für diese Karte.
4. Klick eine fokussierte NAM-Karte erneut an, um ein Capture zu wählen.

### Pitch (Transpose + Octaver)

![VoLum Pitch-Pedal — Transpose-Modus](user-guide-pitch-transpose.png)

Das **PITCH**-Pedal sitzt ganz am Anfang der Signalkette. Wähle mit dem **TRANSPOSE / OCTAVER**-Umschalter den Modus:

- **TRANSPOSE** verschiebt das gesamte Signal nach oben oder unten. **SEMI** legt das Intervall in Halbtönen fest (−12 bis +7) — abgestimmt auf Drop-Tunings und Capo-artige Verschiebungen, **MIX** mischt das verschobene Signal mit dem Dry-Sound, und **LEVEL** trimmt den Ausgang. Die **DROP / INSTANT**-Pille wählt den Charakter: **INSTANT** (Standard) nutzt die kürzeste Überblendung für die geringste Latenz und das direkteste Live-Gefühl; **DROP** führt eine zusätzliche Wellenform-Ausrichtungssuche für die genaueste Tonhöhe und das stabilste Sustain selbst an den Extremen (±12) durch, bei etwas höherer Latenz. Beide folgen der Tonhöhe über den gesamten Bereich gleich gut — INSTANTs Splices sind bei tiefen Abwärts-Verschiebungen nur etwas körniger.
- **OCTAVER** ist ein polyphoner (akkordtauglicher) Oktaver. **OCT DN** und **OCT UP** stellen den Pegel der Unter- und Oberoktave ein, **DRY** behält dein Originalsignal in der Mischung, **LEVEL** trimmt den Ausgang, und die **VINTAGE / MODERN**-Pille wählt die Klangfarbe — Vintage fügt Grit und einen dunkleren Low-Pass für einen analogen Charakter hinzu, Modern bleibt clean.

![VoLum Pitch-Pedal — Octaver-Modus](user-guide-pitch-octaver.png)

Die Pitch-Engine ist ein latenzarmer Zeitbereichs-Shifter, der für Gitarre gebaut ist: Er folgt schnell und hält seine Stimmung über ein langes Sustain stabil, statt beim Ausklingen der Note tonal wegzudriften. VoLum meldet seine Pitch-Latenz an deinen Host zur Plugin-Latenzkompensation, sodass das verschobene Signal zeitlich zum restlichen Mix ausgerichtet bleibt; der Wert hängt vom Transpose-Charakter ab (etwa 8,6 ms bei INSTANT, etwa 17 ms bei DROP) und wird beim Wechsel von Charakter oder Modus neu gemeldet.

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

Jede Spur behält ihren eigenen Speaker/Cab, Kanal und ihre eigene Custom IR. Fokussiere eine Spur und stelle dann deren Cab oder Custom IR ein; die andere Spur bleibt unverändert. Ein eigener Amp als SUPPORT-Partner merkt sich sein Cab und seinen Kanal zusammen mit dem restlichen Rig — über Preset-Aufruf, App-Neustarts und DAW-Projekte hinweg.

VoLum gleicht MAIN- und SUPPORT-NAM-Latenzen aus, bevor die Spuren gepannt und summiert werden.

Der **OUTPUT**-Regler der SUPPORT-Spur schaltet diese Spur bei der ganz linken Einstellung `-∞ dB` ebenfalls vollständig stumm.

## POST-Bereich

![VoLum POST-Bereich](user-guide-post.png)

POST liegt hinter dem Amp. Der Bereich enthält Delay-, Reverb- und Tremolo-Karten.

1. Klick auf **POST**.
2. Klick auf **DELAY**, **REVERB** oder **TREM**.
3. Nutze den Karten-Button oder die Leertaste zum Ein- und Ausschalten.
4. Bearbeite die fokussierte Karte in der Reglerzeile.

**Delay** bietet Digital, Analog und Reverse. Die Regler sind Time, Feedback, Mix, Tone und ein modusspezifischer Charakterregler: `Grit`, `Wear` oder `Bloom`. Ping-Pong gibt es für Digital und Analog.

**Reverb** bietet Hall, Plate und Oktaverb. Hall und Plate liefern klassische Räume. Oktaverb ergänzt die Pitch-Wash-Stimmen `HALO`, `SHIMMER` und `BLOOM` mit Intensity-Regler.

**Tremolo** läuft als Letztes, hinter dem Reverb, und moduliert so den gesamten Effektklang. Der **OPTICAL / BIAS / HARMONIC**-Wähler bestimmt den Charakter:

- **Optical** ist ein hackender Photozellen-Lautstärke-Gate.
- **Bias** ist eine weiche, symmetrische Sinus-Modulation — das klassische „Bang Bang (My Baby Shot Me Down)"-Tremolo und die Werks-Standardstimme.
- **Harmonic** teilt das Signal an einer Trennfrequenz und moduliert tiefes und hohes Band gegenphasig für einen phasigen Sweep.

Die gemeinsamen Regler sind **RATE**, **DEPTH**, **SHAPE** (formt den LFO von weichem Sinus hin zu hartem Rechteck) und **MIX**. Im Harmonic-Modus erscheint ein zusätzlicher **X-OVER**-Regler für die Trennfrequenz. Mit **TEMPO SYNC** koppelst du die Rate ans Tempo: im DAW folgt sie dem Host-Tempo, in der Standalone-App dem Metronom-BPM. Bei aktivem Sync wird der RATE-Regler zu einem musikalischen **DIVISION**-Stepper (1/2 bis 1/16, inklusive punktierter und Triolen-Werte). Linker und rechter Kanal bleiben phasengekoppelt für ein kohärentes Stereo-Tremolo.

Die LED auf jeder Karte zeigt, ob sie aktiv ist. Das Label zeigt den aktuellen Modus oder eine kurze Preset-Zusammenfassung. POST-Einstellungen werden pro Amp gespeichert, genau wie PRE. Das **Schloss** im POST-Kopf funktioniert wie bei PRE, damit du eine Delay/Reverb/Tremolo-Szene beim Durchklicken der Amps mitnimmst; der **Store**-Pfeil erscheint, wenn du das Overlay im aktuellen Amp speichern kannst. Entsperren stellt das gespeicherte POST dieses Amps ohne Rückfrage wieder her. Ein Doppelklick auf einen POST-Regler stellt dessen Default wieder her.

Beim Wechsel von Delay-Modus, Ping-Pong, Reverb-Modus oder Oktaverb-Stimme wird der alte Tail gelöscht, damit Wiederholungen und Raum aus dem vorherigen Modus nicht in den neuen Modus bluten.

## Presets

![VoLum Preset-Verwaltung](user-guide-presets.png)

Ein Preset ist eine benannte Momentaufnahme des gesamten Rigs für den fokussierten Amp: Speaker/Cab, Kanal, alle Regler, PRE-Pedale, POST-Effekte und das Dual-Amp-Setup.

1. Stelle einen Sound ein und öffne die Preset-Leiste in der AMP-Kopfzeile.
2. Mit **Save current as new** speicherst du ihn unter einem Namen.
3. Mit den Pfeilen `<` / `>` blätterst du gespeicherte Presets direkt durch, oder du wählst eines aus der Liste.
4. **Update** überschreibt das gewählte Preset mit dem aktuellen Rig (mit Rückfrage); **Rename** und **Delete** verwalten die Liste.

Presets sind pro Amp: Jeder Amp (Werk oder eigen) hat seine eigene Preset-Liste. Die Leiste zeigt **(unsaved)**, sobald das aktuelle Rig vom geladenen Preset abweicht, und wird wieder sauber, sobald das Rig wieder übereinstimmt. Die fest angeheftete Zeile **Default (factory settings)** setzt den fokussierten Amp auf seine Auslieferungswerte zurück.

## Eigene Inhalte (Bring Your Own)

VoLum kann eigene NAM-Amp-Captures, Impulsantworten und Pedal-Captures laden. Importierte Dateien werden in eine VoLum-eigene Inhaltsbibliothek kopiert, damit sie auch nach dem Verschieben oder Löschen der Originale funktionieren, und alle Formate sehen dieselbe Bibliothek:

- **Windows:** `%LOCALAPPDATA%\VoLum\content`
- **macOS:** `~/Library/Application Support/VoLum/content`

**Eigene Amps.** Klicke auf das **+** im CUSTOM-Bereich des Amp-Browsers, um den Builder zu öffnen. Benenne den Amp, füge eine oder mehrere `.nam`-Dateien hinzu und ordne jede einem Cab-Slot und Kanal zu (Dateien im Schema `PREFIX-CODE-CHANNEL.nam` füllen das automatisch aus). Sowohl NAM-Architecture-1-(A1)- als auch Architecture-2-(A2)-Captures laden, und A2-Container werden in voller Größe gespielt wie die mitgelieferten Profile. Gespeicherte eigene Amps erscheinen in der CUSTOM-Liste und werden genau wie Werk-Amps geladen und gespielt — auch als Dual-Amp-SUPPORT-Partner. Mit den Stift-/Papierkorb-Symbolen bearbeitest oder löschst du einen.

![VoLum Builder für eigene Amps](user-guide-custom-amp.png)

**Eigene IRs.** Eine Custom IR faltet das **DIRECT**-Capture (nur Amp) des Amps — den rohen Amp ohne eingebackenen Speaker. Sie ist für einen eigenen Amp gedacht, der ein DIRECT-Capture enthält; die Auswahl des Cabs **Custom IR** schaltet den Amp zuerst auf sein DIRECT/No-Cab-Capture. Ein eigener Amp, der nur aus vollen Amp-plus-Cab-Captures gebaut ist, hat kein rohes Signal, das eine IR formen könnte — auf einem Kanal ohne DIRECT-Capture sind die Schaltflächen **Custom IR** und **No Cab** ausgegraut (fahre mit der Maus darüber für den Grund) und lassen sich nicht auswählen; wechsle auf einen Kanal mit DIRECT-Capture, um sie zu nutzen. Wähle in der Speaker-Reihe das Cab **Custom IR** und importiere dann über dessen Dropdown eine `.wav`-Impulsantwort. Die Custom IR gehört zur **fokussierten Spur**: im Dual-Amp-Modus haben die MAIN- und die SUPPORT-Spur jeweils ihre eigene Custom IR, das Ändern der einen wirkt sich nie auf die andere aus. Impulsantworten sind kurze Box-Captures — nur der erste Sekundenbruchteil wird genutzt — daher lehnt VoLum sehr große WAV-Dateien (z. B. ein versehentlich gewählter ganzer Song) mit einer Meldung ab, statt sie zu laden.

Namen für eigene Inhalte (Amps, IRs, Pedale und Presets) haben sinnvolle Längenbegrenzungen, damit sie immer in ihre Beschriftungen passen — lange Namen werden bereits bei der Eingabe gekürzt.

![VoLum Verwaltung eigener IRs](user-guide-custom-ir.png)

**Eigene Pedale.** Im PRE-NAM-Capture-Dropdown kannst du in der **CUSTOM**-Gruppe eigene `.nam`-Pedal-Captures importieren und verwalten; ein importiertes Capture lädt wie ein Werk-Capture in seinen PRE-Slot.

![VoLum Verwaltung eigener Pedale](user-guide-custom-pedal.png)

Die Inhaltsbibliothek wird von allen geöffneten Instanzen und Spuren geteilt. In einer DAW speichert das Projekt stabile Referenzen (IDs) auf deine eigenen Amps, IRs, Pedale und das aktive Preset, sodass das erneute Öffnen eines Projekts sie wiederherstellt, solange die Einträge noch in deiner Bibliothek vorhanden sind.

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

### A2 Lite-Modus (Performance)

Die **Performance**-Karte der Einstellungen hat einen **FULL / LITE**-Schalter; der aktive Modus ist hervorgehoben (FULL ist die Voreinstellung), du siehst also stets, welcher Qualitätsmodus läuft. Lite tauscht ein wenig Qualität gegen geringere CPU-Last. VoLums A2-Amp- und Pedal-Captures sind so gepackt, dass jede Datei sowohl eine volle Version als auch eine kleinere „Lite“-Version enthält. Standardmäßig spielt VoLum immer die volle Version (beste Qualität). Schaltest du auf Lite, nutzt VoLum die kleinere Version auf jeder NAM-Spur: beide PRE-Pedale, der Haupt-Amp und die Dual-Amp-Support-Spur.

Der Lite-Modus ist eine Einstellung pro Rechner: Er wird in `volum-settings.json` gespeichert, nicht im Projekt. Er bleibt also für jedes Projekt und jede DAW-Sitzung auf diesem Rechner aktiv, und ein auf einem schnellen Rechner gespeichertes Projekt spielt auf einem langsamen weiterhin Lite. Captures, die keine A2-Container sind (ältere Modelle mit nur einer Größe und die meisten eigenen Importe), bleiben unberührt — der Schalter hat dort einfach keine Wirkung. Standard ist Full.

### Standalone-Audioeinstellungen

In der Standalone-App öffnest du **File -> Preferences** oder drückst `Ctrl+,`, um Audiotreiber, getrennte Ein- und Ausgabegeräte, Samplerate und Kanalrouting zu wählen. In der VST3-Version nutzt du stattdessen die Audioeinstellungen deiner DAW.

Wähle Eingabe- und Ausgabegerät unabhängig voneinander. Unter macOS erscheinen Mikrofon und Lautsprecher oft als getrennte Geräte. Wähle einen Mono-Eingangskanal für das Gitarrensignal und route Output L/R nach Bedarf. Die Standalone-Bufferliste nutzt eine stabile Auswahl gängiger Pro-Audio-Größen: 48, 64, 96, 128, 256, 512, 1024, 2048, 4096 und 8192 Samples. Ältere gespeicherte Werte unterhalb der sichtbaren Liste werden auf die nächste sichtbare Größe angehoben.

Wenn du einen Treiber ohne nutzbares Gerät auswählst, zum Beispiel ASIO auf einem Laptop ohne ASIO-Interface, zeigt VoLum eine Fehlermeldung und stellt die vorher funktionierende Audiokonfiguration wieder her, statt sich zu schließen.

VoLum besitzt außerdem eine immer aktive Ausgangs-Schutzstufe nach Delay und Reverb. Normales Spielen bleibt unverändert. Wenn ein heißes Rig und starke POST-Effekte durchgehende Peaks erzeugen, wird das OUT-Meter rot und der Footer zeigt `Output safety active - lower output or wet mix`. Drehe Output, Delay Mix oder Reverb Mix zurück, wenn du das oft siehst.

## Fehler Melden Oder Feature Vorschlagen

Erstelle ein [Issue auf GitHub](https://github.com/guitarlum/VoLum/issues/new/choose). Nutze die Vorlage **Bug report** für Abstürze oder Fehlverhalten und **Feature request** für Ideen.
