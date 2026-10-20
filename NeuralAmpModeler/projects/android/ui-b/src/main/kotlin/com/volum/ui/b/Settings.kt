package com.volum.ui.b

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.semantics.CustomAccessibilityAction
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.customActions
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.Overlay
import com.volum.core.PickKind
import com.volum.core.ScreenId
import com.volum.core.SettingsTab
import com.volum.core.VoLumState
import com.volum.design.components.CrossButton
import com.volum.design.components.VoLumModePicker
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumType
import com.volum.harness.featureId

private const val MANUAL = "https://github.com/guitarlum/VoLum/blob/main/docs/user-guide.en.md"
private const val RELEASES = "https://github.com/guitarlum/VoLum/releases/latest"

@Composable
internal fun SettingsPanel(s: VoLumState, tab: SettingsTab) {
    val st = LocalStage.current
    val screen = when (tab) {
        SettingsTab.SIGNAL -> ScreenId.SETTINGS_SIGNAL
        SettingsTab.MIDI -> ScreenId.SETTINGS_MIDI
        SettingsTab.SYSTEM -> ScreenId.SETTINGS_SYSTEM
    }
    Panel(s, screen, "Settings", arrayOf("settings.close"), actions = {
        Choice(listOf("SIGNAL", "MIDI", "SYSTEM"), tab.ordinal, { st.go(Action.SetSettingsTab(SettingsTab.entries[it])) },
            Modifier.width(390.dp).height(48.dp), { i -> arrayOf("settings.tab.${SettingsTab.entries[i].name.lowercase()}") }, size = 13.sp)
    }) {
        when (tab) {
            SettingsTab.SIGNAL -> SignalTab(s)
            SettingsTab.MIDI -> MidiTab(s)
            SettingsTab.SYSTEM -> SystemTab(s)
        }
    }
}

/** Number entry that keeps what is being typed and follows the store when it changes elsewhere. */
@Composable
private fun NumberField(value: Double, format: (Double) -> String, onSet: (Double) -> Unit, modifier: Modifier, ids: Array<String>) {
    var local by remember { mutableStateOf(format(value)) }
    val parsed = local.replace(',', '.').toDoubleOrNull()
    val shown = if (parsed == null || parsed == value) local else format(value)
    Field(shown, { local = it; it.replace(',', '.').toDoubleOrNull()?.let(onSet) }, modifier, ids, number = true, onDone = { parsed?.let(onSet) })
}

@Composable
private fun SignalTab(s: VoLumState) {
    val st = LocalStage.current
    val sig = s.signal
    Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
        Card(Modifier.weight(1f).fillMaxHeight(), "INPUT CALIBRATION") {
            Row(Modifier.fillMaxWidth().height(52.dp), horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
                TextPad("−", { st.go(Action.SetCalibrationDbu(sig.calibrationDbu - 0.5)) }, Modifier.width(52.dp).fillMaxHeight(), arrayOf("signal.calibration.level"), size = 22.sp)
                NumberField(sig.calibrationDbu, { String.format("%.1f", it) }, { st.go(Action.SetCalibrationDbu(it)) }, Modifier.weight(1f).height(52.dp),
                    arrayOf("signal.calibration.level"))
                TextPad("+", { st.go(Action.SetCalibrationDbu(sig.calibrationDbu + 0.5)) }, Modifier.width(52.dp).fillMaxHeight(), arrayOf("signal.calibration.level"), size = 22.sp)
            }
            FitText("dBu your interface reads at 0 dBFS", VoLumType.body(11.sp, VoLumColors.CreamDim), Modifier.fillMaxWidth().height(16.dp), align = TextAlign.Start)
            Switch("CALIBRATE INPUT", sig.calibrate, { st.go(Action.SetCalibrate(!sig.calibrate)) }, Modifier.fillMaxWidth().height(48.dp), arrayOf("signal.calibration.on"))
            if (s.amp?.inputLevelDbu == null) {
                Box(Modifier.fillMaxWidth().featureId("signal.calibration.unavailable")) {
                    Status("${s.amp?.name ?: "This amp"} stores no input level: no effect", Modifier.fillMaxWidth().height(20.dp), VoLumColors.CreamDim)
                }
            }
        }
        Card(Modifier.weight(1f).fillMaxHeight(), "OUTPUT LEVEL") {
            val calOk = s.amp?.outputLevelDbu != null
            val ids = listOf("signal.output.raw", "signal.output.normalized", "signal.output.calibrated")
            VoLumModePicker(listOf("RAW", "NORMALIZED", "CALIBRATED"), sig.outputMode, { st.go(Action.SetOutputMode(it)) }, Modifier.fillMaxWidth(),
                itemModifier = { i -> Modifier.fillMaxWidth().featureId(ids[i]).alpha(if (i == 2 && !calOk) 0.4f else 1f) })
            if (!calOk) FitText("CALIBRATED needs a capture that stores its output level", VoLumType.body(10.sp, VoLumColors.CreamDim), Modifier.fillMaxWidth().height(16.dp), align = TextAlign.Start)
        }
        Card(Modifier.weight(1f).fillMaxHeight(), "PERFORMANCE") {
            Choice(listOf("FULL", "LITE"), if (sig.lite) 1 else 0, { st.go(Action.SetLite(it == 1)) }, Modifier.fillMaxWidth().height(48.dp),
                { i -> arrayOf(if (i == 1) "signal.perf.lite" else "signal.perf.full") }, style = Sel.Brass)
            FitText(if (sig.lite) "LITE: lighter models, less CPU" else "FULL: every capture at full size", VoLumType.body(11.sp, VoLumColors.CreamDim), Modifier.fillMaxWidth().height(16.dp), align = TextAlign.Start)
            Switch("ANIMATE ART", sig.animateArt, { st.go(Action.SetAnimateArt(!sig.animateArt)) }, Modifier.fillMaxWidth().height(48.dp), arrayOf("signal.animate_art"))
            Box(Modifier.weight(1f))
            TextPad("AUDIO & MIDI DEVICES  >", { st.go(Action.Open(Overlay.AudioDevices)) }, Modifier.fillMaxWidth().height(52.dp), arrayOf("signal.audio_devices"), tone = Tone.PRIMARY)
        }
    }
}

@Composable
private fun MidiTab(s: VoLumState) {
    val st = LocalStage.current
    val midi = s.midi
    val bank = (0..7).map { midi.bank * 8 + it }
    val names = bank.map { p -> s.library.sounds[p]?.let { s.library.preset(it.presetId)?.name ?: "Invalid slot" } ?: "+ assign" }
    val slotStyle = rememberSharedStyle(names, VoLumType.display(16.sp, VoLumColors.Cream), 96.dp)
    Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
        Card(Modifier.width(290.dp).fillMaxHeight(), "LISTEN ON") {
            Choice(listOf("ALL CHANNELS", "ONE"), if (midi.channel == 0) 0 else 1, { st.go(Action.SetMidiChannel(if (it == 0) 0 else midi.lastOneChannel)) },
                Modifier.fillMaxWidth().height(48.dp), { i -> arrayOf(if (i == 0) "midi.channel.all" else "midi.channel.one") }, style = Sel.Brass)
            val ch = if (midi.channel == 0) midi.lastOneChannel else midi.channel
            Stepper("CHANNEL $ch", { st.go(Action.SetMidiChannel(if (ch <= 1) 16 else ch - 1)) }, { st.go(Action.SetMidiChannel(if (ch >= 16) 1 else ch + 1)) },
                Modifier.fillMaxWidth().height(48.dp), arrayOf("midi.channel.number"), arrayOf("midi.channel.number"),
                prevEnabled = midi.channel != 0, nextEnabled = midi.channel != 0, valueStyle = VoLumType.value(13.sp, if (midi.channel != 0) VoLumColors.Gold else VoLumColors.CreamDim), description = "MIDI channel")
            FitText("RECALL CC · its value picks the program", VoLumType.SectionHeader, Modifier.fillMaxWidth().height(16.dp), align = TextAlign.Start)
            Stepper("CC ${midi.recallCc}", { st.go(Action.SetRecallCc(midi.recallCc - 1)) }, { st.go(Action.SetRecallCc(midi.recallCc + 1)) },
                Modifier.fillMaxWidth().height(48.dp), arrayOf("midi.recall_cc"), arrayOf("midi.recall_cc"), valueStyle = VoLumType.value(13.sp, VoLumColors.Gold), description = "recall CC")
            FitText("Program Change n recalls program n. No MIDI Learn:", VoLumType.body(10.sp, VoLumColors.CreamDim), Modifier.fillMaxWidth().height(14.dp), align = TextAlign.Start)
            FitText("put Sounds on the programs you send.", VoLumType.body(10.sp, VoLumColors.CreamDim), Modifier.fillMaxWidth().height(14.dp), align = TextAlign.Start)
        }
        Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Stepper("BANK ${midi.bank + 1}  ·  PROGRAMS ${twoDigit(midi.bank * 8)}–${twoDigit(midi.bank * 8 + 7)}", { st.go(Action.SetMidiBank(midi.bank - 1)) },
                { st.go(Action.SetMidiBank(midi.bank + 1)) }, Modifier.fillMaxWidth().height(48.dp), arrayOf("midi.bank.prev"), arrayOf("midi.bank.next"),
                prevEnabled = midi.bank > 0, nextEnabled = midi.bank < 15, valueStyle = VoLumType.label(13.sp, VoLumColors.Gold), description = "bank")
            (0..1).forEach { r ->
                Row(Modifier.fillMaxWidth().weight(1f), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    (0..3).forEach { c -> MidiSlot(s, midi.bank * 8 + r * 4 + c, slotStyle, Modifier.weight(1f).fillMaxHeight()) }
                }
            }
        }
    }
}

@Composable
private fun MidiSlot(s: VoLumState, program: Int, nameStyle: androidx.compose.ui.text.TextStyle, modifier: Modifier) {
    val st = LocalStage.current
    val sound = s.library.sounds[program]
    val valid = s.library.soundValid(program)
    Box(modifier) {
        val move = if (sound != null) moveActions(st, program) else Modifier
        Pad({ st.go(Action.Open(Overlay.SoundPicker(program, replace = true))) }, Modifier.fillMaxSize().then(move),
            if (sound != null) arrayOf("midi.slot.assign", "midi.slot.move") else arrayOf("midi.slot.assign"),
            selected = s.library.live == program, style = Sel.Brass, tone = if (sound != null && !valid) Tone.DANGER else Tone.FRAMED,
            description = "Assign program $program") { _ ->
            Column(Modifier.fillMaxSize().padding(start = 10.dp, top = 6.dp, bottom = 8.dp, end = 44.dp)) {
                FitText(twoDigit(program), VoLumType.value(14.sp, if (sound != null) VoLumColors.Gold else VoLumColors.CreamDim), Modifier.width(48.dp).height(20.dp), align = TextAlign.Start)
                Box(Modifier.weight(1f))
                FitText(when {
                    sound == null -> "+ assign"
                    !valid -> "Invalid slot"
                    else -> s.library.preset(sound.presetId)?.name ?: ""
                }, if (sound != null && !valid) VoLumType.label(13.sp, VoLumColors.Danger) else nameStyle.copy(color = if (sound == null) VoLumColors.CreamDim else VoLumColors.Cream),
                    Modifier.fillMaxWidth().height(24.dp), align = TextAlign.Start)
            }
        }
        if (sound != null) {
            CrossButton({ st.go(Action.ClearSound(program)) }, Modifier.align(Alignment.TopEnd).featureId("midi.slot.clear")
                .semantics { contentDescription = "Clear program $program" }, VoLumColors.CreamDim)
        }
    }
}

private fun moveActions(st: Stage, program: Int): Modifier = Modifier.semantics {
    customActions = listOf(
        CustomAccessibilityAction("Move down") { st.go(Action.MoveSound(program, 1)); true },
        CustomAccessibilityAction("Move up") { st.go(Action.MoveSound(program, -1)); true },
    )
}

@Composable
private fun SystemTab(s: VoLumState) {
    val st = LocalStage.current
    Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
        Card(Modifier.weight(1f).fillMaxHeight(), "ABOUT") {
            Box(Modifier.fillMaxWidth().featureId("system.about.version")) {
                FitText("VoLum 0.1-shootout · Android", VoLumType.display(20.sp, VoLumColors.Gold), Modifier.fillMaxWidth().height(28.dp), align = TextAlign.Start)
            }
            Column(Modifier.fillMaxWidth().featureId("system.model_info")) {
                FitText("PLAYING NOW", VoLumType.SectionHeader, Modifier.fillMaxWidth().height(16.dp), align = TextAlign.Start)
                FitText("${s.amp?.name ?: "—"} · CH ${s.rig.channel + 1} · ${s.rig.cab.label}", VoLumType.label(13.sp, VoLumColors.Cream), Modifier.fillMaxWidth().height(20.dp), align = TextAlign.Start)
                FitText("${s.audio.sampleRate / 1000.0} kHz · ${s.audio.bufferFrames} samples · ${if (s.signal.lite) "LITE" else "FULL"} models",
                    VoLumType.body(12.sp, VoLumColors.CreamDim), Modifier.fillMaxWidth().height(18.dp), align = TextAlign.Start)
            }
            Box(Modifier.weight(1f))
            TextPad("USER GUIDE  ↗", { st.go(Action.OpenLink(MANUAL)) }, Modifier.fillMaxWidth().height(52.dp), arrayOf("system.about.manual"))
        }
        Card(Modifier.weight(1f).fillMaxHeight(), "UPDATES") {
            Switch("CHECK AUTOMATICALLY", s.update.auto, { st.go(Action.SetAutoUpdate(!s.update.auto)) }, Modifier.fillMaxWidth().height(48.dp), arrayOf("system.update.auto"))
            TextPad("CHECK NOW", { st.go(Action.CheckUpdatesNow) }, Modifier.fillMaxWidth().height(48.dp), arrayOf("system.update.check_now"))
            val available = s.update.available
            if (available != null) {
                FitText("VoLum $available is available", VoLumType.display(19.sp, VoLumColors.Gold), Modifier.fillMaxWidth().height(26.dp), align = TextAlign.Start)
                TextPad("GET $available  ↗", { st.go(Action.OpenLink(RELEASES)) }, Modifier.fillMaxWidth().height(52.dp), arrayOf("system.update.link"), tone = Tone.PRIMARY)
            } else {
                FitText(if (s.update.checks > 0) "You have the latest version." else "Not checked yet.", VoLumType.body(13.sp, VoLumColors.CreamDim), Modifier.fillMaxWidth().height(20.dp), align = TextAlign.Start)
            }
        }
        Card(Modifier.weight(1f).fillMaxHeight(), "LIBRARY") {
            TextPad("EXPORT A PACK…", { st.go(Action.Open(Overlay.PackExport)) }, Modifier.fillMaxWidth().height(52.dp), arrayOf("pack.export.open"))
            TextPad("IMPORT A PACK…", { st.go(Action.PickFiles(PickKind.PACK_OPEN)) }, Modifier.fillMaxWidth().height(52.dp), arrayOf("pack.import.open"))
            FitText("Packs carry amps, IRs, pedals and presets", VoLumType.body(11.sp, VoLumColors.CreamDim), Modifier.fillMaxWidth().height(16.dp), align = TextAlign.Start)
            Box(Modifier.weight(1f))
            TextPad("SHARE LOG", { st.go(Action.ShareLog) }, Modifier.fillMaxWidth().height(52.dp), arrayOf("system.share_log"))
        }
    }
}

@Composable
internal fun AudioDevices(s: VoLumState) {
    val st = LocalStage.current
    val a = s.audio
    val input = a.inputs.firstOrNull { it.id == a.inputId }
    Panel(s, ScreenId.SETTINGS_AUDIO, "Audio & MIDI devices", arrayOf("audio.devices.close"), subtitle = "USB audio interfaces come first; the phone mic and speaker are a fallback") {
        Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            Card(Modifier.weight(1f).fillMaxHeight(), "AUDIO") {
                DeviceRow("INPUT") {
                    Choice(a.inputs.map { it.name }, a.inputs.indexOfFirst { it.id == a.inputId }, { st.go(Action.SelectInput(a.inputs[it].id)) },
                        Modifier.fillMaxSize(), { arrayOf("audio.input.device") }, style = Sel.ListTeal)
                }
                DeviceRow("IN CHANNEL") {
                    Choice((1..(input?.channels ?: 1)).map { "$it" }, a.inputChannel, { st.go(Action.SelectInputChannel(it)) }, Modifier.fillMaxSize(),
                        { arrayOf("audio.input.channel") }, enabled = { input != null })
                }
                DeviceRow("OUTPUT") {
                    Choice(a.outputs.map { it.name }, a.outputs.indexOfFirst { it.id == a.outputId }, { st.go(Action.SelectOutput(a.outputs[it].id)) },
                        Modifier.fillMaxSize(), { arrayOf("audio.output.device") }, style = Sel.ListTeal)
                }
                DeviceRow("SAMPLE RATE") {
                    Choice(a.sampleRates.map { "${it / 1000.0} k" }, a.sampleRates.indexOf(a.sampleRate), { st.go(Action.SetSampleRate(a.sampleRates[it])) },
                        Modifier.fillMaxSize(), { arrayOf("audio.sample_rate") })
                }
                DeviceRow("BUFFER") {
                    Choice(a.buffers.map { "$it" }, a.buffers.indexOf(a.bufferFrames), { st.go(Action.SetBuffer(a.buffers[it])) },
                        Modifier.fillMaxSize(), { arrayOf("audio.buffer") })
                }
            }
            Card(Modifier.width(310.dp).fillMaxHeight(), "MIDI CONTROLLERS", accent = if (a.midiLost) VoLumColors.Danger else VoLumColors.Corner) {
                if (a.midiLost) {
                    Box(Modifier.fillMaxWidth().featureId("midi.device.lost")) {
                        Status("Controller disconnected", Modifier.fillMaxWidth().height(20.dp), VoLumColors.Danger)
                    }
                    FitText("Plug it back in, then reconnect, or scan for another.", VoLumType.body(11.sp, VoLumColors.Cream), Modifier.fillMaxWidth().height(16.dp), align = TextAlign.Start)
                }
                a.midiDevices.forEach { d ->
                    val lost = a.midiLost && a.midiDeviceId == d.id
                    TextPad("${d.name} · ${if (d.ble) "BLE" else "USB"}${if (lost) " · DISCONNECTED" else ""}", { st.go(Action.SelectMidiDevice(d.id)) },
                        Modifier.fillMaxWidth().height(48.dp).alpha(if (lost) 0.55f else 1f), arrayOf("midi.device.select"),
                        selected = a.midiDeviceId == d.id && !a.midiLost, style = Sel.ListTeal, tone = if (lost) Tone.DANGER else Tone.FRAMED, size = 11.sp)
                }
                if (a.midiLost && a.midiDeviceId != null) {
                    TextPad("RECONNECT", { st.go(Action.SelectMidiDevice(a.midiDeviceId!!)) }, Modifier.fillMaxWidth().height(52.dp), arrayOf("midi.device.select"), tone = Tone.PRIMARY)
                }
                Box(Modifier.weight(1f))
                TextPad(if (a.bleScanning) "SCANNING…" else "SCAN FOR BLUETOOTH MIDI", { st.go(Action.ScanBleMidi) }, Modifier.fillMaxWidth().height(52.dp),
                    arrayOf("midi.device.ble_scan"))
            }
        }
    }
}

@Composable
private fun ColumnScope.DeviceRow(label: String, content: @Composable () -> Unit) {
    Row(Modifier.fillMaxWidth().height(48.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        FitText(label, VoLumType.label(11.sp, VoLumColors.TextBright), Modifier.width(96.dp), align = TextAlign.Start)
        Box(Modifier.weight(1f).fillMaxHeight()) { content() }
    }
}
