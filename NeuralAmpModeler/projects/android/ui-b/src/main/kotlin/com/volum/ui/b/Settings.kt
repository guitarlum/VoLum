package com.volum.ui.b

import androidx.compose.foundation.background
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
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicText
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.semantics.CustomAccessibilityAction
import androidx.compose.ui.semantics.customActions
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.InterfaceStatus
import com.volum.core.Overlay
import com.volum.core.Permission
import com.volum.core.PickKind
import com.volum.core.ScreenId
import com.volum.core.SettingsTab
import com.volum.core.VoLumState
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
    Panel(screen, "Settings", arrayOf("settings.close"), actions = {
        Choice(listOf("SIGNAL", "MIDI", "SYSTEM"), tab.ordinal, { st.go(Action.SetSettingsTab(SettingsTab.entries[it])) },
            Modifier.width(360.dp).height(48.dp), { i -> arrayOf("settings.tab.${SettingsTab.entries[i].name.lowercase()}") })
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
            Row(Modifier.fillMaxWidth().height(52.dp), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                TextPad("−", { st.go(Action.SetCalibrationDbu(sig.calibrationDbu - 0.5)) }, Modifier.width(52.dp).fillMaxHeight(), style = Type.display(24.sp))
                NumberField(sig.calibrationDbu, { String.format("%.1f", it) }, { st.go(Action.SetCalibrationDbu(it)) }, Modifier.weight(1f).fillMaxHeight(),
                    arrayOf("signal.calibration.level"))
                TextPad("+", { st.go(Action.SetCalibrationDbu(sig.calibrationDbu + 0.5)) }, Modifier.width(52.dp).fillMaxHeight(), style = Type.display(24.sp))
            }
            FitText("dBu your interface reads at 0 dBFS", Type.value(11.sp, Ink.dim), Modifier.fillMaxWidth().height(16.dp), align = TextAlign.Start)
            Switch("CALIBRATE INPUT", sig.calibrate, { st.go(Action.SetCalibrate(!sig.calibrate)) }, Modifier.fillMaxWidth().height(48.dp), arrayOf("signal.calibration.on"))
            if (s.amp?.inputLevelDbu == null) {
                Box(Modifier.fillMaxWidth().clip(RoundedCornerShape(6.dp)).background(Ink.raised).padding(8.dp).featureId("signal.calibration.unavailable")) {
                    BasicText("${s.amp?.name ?: "This amp"} stores no input level, so calibration has no effect on it.", style = Type.value(12.sp, Ink.dim))
                }
            }
        }
        Card(Modifier.weight(1f).fillMaxHeight(), "OUTPUT LEVEL") {
            val calOk = s.amp?.outputLevelDbu != null
            listOf("RAW" to "signal.output.raw", "NORMALIZED" to "signal.output.normalized", "CALIBRATED" to "signal.output.calibrated").forEachIndexed { i, (label, id) ->
                TextPad(label, { st.go(Action.SetOutputMode(i)) }, Modifier.fillMaxWidth().height(48.dp), arrayOf(id), selected = sig.outputMode == i,
                    enabled = i != 2 || calOk, sel = Sel.AMBER_PICKER)
            }
            if (!calOk) FitText("CALIBRATED needs a capture with an output level", Type.value(11.sp, Ink.faint), Modifier.fillMaxWidth().height(16.dp), align = TextAlign.Start)
        }
        Card(Modifier.weight(1f).fillMaxHeight(), "PERFORMANCE") {
            Choice(listOf("FULL", "LITE"), if (sig.lite) 1 else 0, { st.go(Action.SetLite(it == 1)) }, Modifier.fillMaxWidth().height(48.dp),
                { i -> arrayOf(if (i == 0) "signal.perf.full" else "signal.perf.lite") })
            FitText(if (sig.lite) "LITE: lighter models, less CPU" else "FULL: every capture at full size", Type.value(11.sp, Ink.dim), Modifier.fillMaxWidth().height(16.dp), align = TextAlign.Start)
            Switch("ANIMATE ART", sig.animateArt, { st.go(Action.SetAnimateArt(!sig.animateArt)) }, Modifier.fillMaxWidth().height(48.dp), arrayOf("signal.animate_art"))
            Box(Modifier.weight(1f))
            TextPad("AUDIO & MIDI DEVICES ›", { st.go(Action.Open(Overlay.AudioDevices)) }, Modifier.fillMaxWidth().height(56.dp), arrayOf("signal.audio_devices"))
        }
    }
}

@Composable
private fun MidiTab(s: VoLumState) {
    val st = LocalStage.current
    val midi = s.midi
    Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
        Card(Modifier.width(290.dp).fillMaxHeight(), "LISTEN ON") {
            Choice(listOf("ALL CHANNELS", "ONE"), if (midi.channel == 0) 0 else 1, { st.go(Action.SetMidiChannel(if (it == 0) 0 else midi.lastOneChannel)) },
                Modifier.fillMaxWidth().height(48.dp), { i -> arrayOf(if (i == 0) "midi.channel.all" else "midi.channel.one") })
            val ch = if (midi.channel == 0) midi.lastOneChannel else midi.channel
            Stepper("CHANNEL $ch", { st.go(Action.SetMidiChannel(if (ch <= 1) 16 else ch - 1)) }, { st.go(Action.SetMidiChannel(if (ch >= 16) 1 else ch + 1)) },
                Modifier.fillMaxWidth().height(48.dp), arrayOf("midi.channel.number"), arrayOf("midi.channel.number"),
                prevEnabled = midi.channel != 0, nextEnabled = midi.channel != 0, description = "MIDI channel")
            Caption("RECALL CC (value = program)", Modifier.fillMaxWidth())
            Stepper("CC ${midi.recallCc}", { st.go(Action.SetRecallCc(midi.recallCc - 1)) }, { st.go(Action.SetRecallCc(midi.recallCc + 1)) },
                Modifier.fillMaxWidth().height(48.dp), arrayOf("midi.recall_cc"), arrayOf("midi.recall_cc"), description = "recall CC")
            BasicText("Program Change n recalls the Sound on program n. No MIDI Learn: assign Sounds to the slots.", style = Type.value(11.sp, Ink.dim))
        }
        Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Stepper("BANK ${midi.bank + 1} · PROGRAMS ${midi.bank * 8}–${midi.bank * 8 + 7}", { st.go(Action.SetMidiBank(midi.bank - 1)) },
                { st.go(Action.SetMidiBank(midi.bank + 1)) }, Modifier.fillMaxWidth().height(48.dp), arrayOf("midi.bank.prev"), arrayOf("midi.bank.next"),
                prevEnabled = midi.bank > 0, nextEnabled = midi.bank < 15, valueStyle = Type.label(13.sp, Ink.text), description = "bank")
            (0..1).forEach { r ->
                Row(Modifier.fillMaxWidth().weight(1f), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    (0..3).forEach { c -> MidiSlot(s, midi.bank * 8 + r * 4 + c, Modifier.weight(1f).fillMaxHeight()) }
                }
            }
        }
    }
}

@Composable
private fun MidiSlot(s: VoLumState, program: Int, modifier: Modifier) {
    val st = LocalStage.current
    val sound = s.library.sounds[program]
    val valid = s.library.soundValid(program)
    Box(modifier) {
        val move = if (sound != null) Modifier.semantics {
            customActions = listOf(
                CustomAccessibilityAction("Move down") { st.go(Action.MoveSound(program, 1)); true },
                CustomAccessibilityAction("Move up") { st.go(Action.MoveSound(program, -1)); true },
            )
        } else Modifier
        Pad({ st.go(Action.Open(Overlay.SoundPicker(program, replace = true))) }, Modifier.fillMaxSize().then(move),
            if (sound != null) arrayOf("midi.slot.assign", "midi.slot.move") else arrayOf("midi.slot.assign"),
            selected = s.library.live == program, sel = Sel.LIST_TEAL, fill = if (sound != null && !valid) Ink.alert.copy(alpha = 0.18f) else Ink.panel,
            description = "Assign program $program") {
            Column(Modifier.fillMaxSize().padding(start = 10.dp, top = 8.dp, bottom = 8.dp, end = 52.dp)) {
                BasicText(program.toString().padStart(3, '0'), style = Type.tech(14.sp, if (sound != null) Ink.text else Ink.faint))
                Box(Modifier.weight(1f))
                FitText(when {
                    sound == null -> "+ assign"
                    !valid -> "Invalid slot"
                    else -> s.library.preset(sound.presetId)?.name ?: ""
                }, Type.display(16.sp, if (sound == null) Ink.faint else Ink.text), Modifier.fillMaxWidth().height(24.dp), align = TextAlign.Start)
            }
        }
        if (sound != null) {
            Pad({ st.go(Action.ClearSound(program)) }, Modifier.align(Alignment.TopEnd).size(48.dp), arrayOf("midi.slot.clear"), fill = Ink.ground.copy(alpha = 0.6f),
                description = "Clear program $program") {
                BasicText("✕", style = Type.display(16.sp, Ink.dim))
            }
        }
    }
}

@Composable
private fun SystemTab(s: VoLumState) {
    val st = LocalStage.current
    Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
        Card(Modifier.weight(1f).fillMaxHeight(), "ABOUT") {
            Box(Modifier.fillMaxWidth().featureId("system.about.version")) {
                FitText("VoLum 0.1-shootout · Android", Type.display(20.sp), Modifier.fillMaxWidth().height(28.dp), align = TextAlign.Start)
            }
            Column(Modifier.fillMaxWidth().featureId("system.model_info")) {
                Caption("PLAYING NOW")
                FitText("${s.amp?.name ?: "—"} · CH ${s.rig.channel + 1} · ${s.rig.cab.label}", Type.value(13.sp), Modifier.fillMaxWidth().height(20.dp), align = TextAlign.Start)
                FitText("${s.audio.sampleRate / 1000.0} kHz · ${s.audio.bufferFrames} samples · ${if (s.signal.lite) "LITE" else "FULL"} models",
                    Type.value(12.sp, Ink.dim), Modifier.fillMaxWidth().height(18.dp), align = TextAlign.Start)
            }
            Box(Modifier.weight(1f))
            TextPad("USER GUIDE ↗", { st.go(Action.OpenLink(MANUAL)) }, Modifier.fillMaxWidth().height(52.dp), arrayOf("system.about.manual"))
        }
        Card(Modifier.weight(1f).fillMaxHeight(), "UPDATES") {
            Switch("CHECK AUTOMATICALLY", s.update.auto, { st.go(Action.SetAutoUpdate(!s.update.auto)) }, Modifier.fillMaxWidth().height(48.dp), arrayOf("system.update.auto"))
            TextPad("CHECK NOW", { st.go(Action.CheckUpdatesNow) }, Modifier.fillMaxWidth().height(48.dp), arrayOf("system.update.check_now"))
            val available = s.update.available
            if (available != null) {
                FitText("VoLum $available is available", Type.display(18.sp, Ink.hot), Modifier.fillMaxWidth().height(26.dp), align = TextAlign.Start)
                TextPad("GET $available ↗", { st.go(Action.OpenLink(RELEASES)) }, Modifier.fillMaxWidth().height(52.dp), arrayOf("system.update.link"), selected = true)
            } else {
                FitText(if (s.update.checks > 0) "You have the latest version." else "Not checked yet.", Type.value(13.sp, Ink.dim), Modifier.fillMaxWidth().height(20.dp), align = TextAlign.Start)
            }
        }
        Card(Modifier.weight(1f).fillMaxHeight(), "LIBRARY") {
            TextPad("EXPORT PACK…", { st.go(Action.Open(Overlay.PackExport)) }, Modifier.fillMaxWidth().height(52.dp), arrayOf("pack.export.open"))
            TextPad("IMPORT PACK…", { st.go(Action.PickFiles(PickKind.PACK_OPEN)) }, Modifier.fillMaxWidth().height(52.dp), arrayOf("pack.import.open"))
            FitText("Packs carry amps, IRs, pedals and presets", Type.value(11.sp, Ink.dim), Modifier.fillMaxWidth().height(16.dp), align = TextAlign.Start)
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
    Panel(ScreenId.SETTINGS_AUDIO, "Audio & MIDI devices", arrayOf("audio.devices.close"), subtitle = "USB audio interfaces come first; the phone mic and speaker are a fallback") {
        Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            Card(Modifier.weight(1f).fillMaxHeight(), "AUDIO") {
                DeviceRow("INPUT") {
                    Choice(a.inputs.map { it.name }, a.inputs.indexOfFirst { it.id == a.inputId }, { st.go(Action.SelectInput(a.inputs[it].id)) },
                        Modifier.fillMaxSize(), { arrayOf("audio.input.device") }, sel = Sel.LIST_TEAL)
                }
                DeviceRow("IN CHANNEL") {
                    Choice((1..(input?.channels ?: 1)).map { "$it" }, a.inputChannel, { st.go(Action.SelectInputChannel(it)) }, Modifier.fillMaxSize(),
                        { arrayOf("audio.input.channel") }, enabled = { input != null })
                }
                DeviceRow("OUTPUT") {
                    Choice(a.outputs.map { it.name }, a.outputs.indexOfFirst { it.id == a.outputId }, { st.go(Action.SelectOutput(a.outputs[it].id)) },
                        Modifier.fillMaxSize(), { arrayOf("audio.output.device") }, sel = Sel.LIST_TEAL)
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
            Card(Modifier.width(300.dp).fillMaxHeight(), "MIDI CONTROLLERS") {
                if (a.midiLost) {
                    Box(Modifier.fillMaxWidth().clip(RoundedCornerShape(6.dp)).background(Ink.alert.copy(alpha = 0.3f)).padding(8.dp).featureId("midi.device.lost")) {
                        BasicText("Controller disconnected. Plug it back in or pick another one.", style = Type.value(12.sp, Ink.hot))
                    }
                }
                a.midiDevices.forEach { d ->
                    TextPad("${d.name} · ${if (d.ble) "BLE" else "USB"}", { st.go(Action.SelectMidiDevice(d.id)) }, Modifier.fillMaxWidth().height(48.dp),
                        arrayOf("midi.device.select"), selected = a.midiDeviceId == d.id && !a.midiLost, sel = Sel.LIST_TEAL)
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
        FitText(label, Type.label(11.sp), Modifier.width(96.dp), align = TextAlign.Start)
        Box(Modifier.weight(1f).fillMaxHeight()) { content() }
    }
}

/** What is wrong with the input and the one tap that fixes it. */
@Composable
internal fun AudioStatus(s: VoLumState) {
    val st = LocalStage.current
    val a = s.audio
    val input = a.inputs.firstOrNull { it.id == a.inputId }
    Panel(ScreenId.AUDIO_STATUS, "Audio input", arrayOf("audio.status.close"), subtitle = "what VoLum hears and how to fix it") {
        Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            if (a.permission != Permission.GRANTED) {
                Problem(Modifier.weight(1f).fillMaxHeight(), "audio.permission.denied", "Microphone access is off",
                    if (a.permission == Permission.DENIED) "VoLum needs it to hear your guitar through the interface or the phone mic."
                    else "Android will not ask again. Turn on Microphone for VoLum in the app settings.") {
                    if (a.permission == Permission.DENIED) {
                        TextPad("ALLOW MICROPHONE", { st.go(Action.RequestMicPermission) }, Modifier.fillMaxWidth().height(64.dp), arrayOf("audio.permission.grant"), selected = true)
                    } else {
                        TextPad("OPEN APP SETTINGS ↗", { st.go(Action.OpenAppSettings) }, Modifier.fillMaxWidth().height(64.dp), arrayOf("audio.permission.settings"), selected = true)
                    }
                }
            }
            when (a.status) {
                InterfaceStatus.NO_INTERFACE -> Problem(Modifier.weight(1f).fillMaxHeight(), "audio.no_interface", "No USB interface",
                    "Connect a USB audio interface and rescan, or play through the phone mic for now.") {
                    TextPad("RESCAN USB", { st.go(Action.RescanAudio) }, Modifier.fillMaxWidth().height(64.dp), arrayOf("audio.no_interface.rescan"), selected = true)
                    TextPad("USE PHONE MIC", { st.go(Action.UseBuiltinInput) }, Modifier.fillMaxWidth().height(56.dp), arrayOf("audio.no_interface.builtin"))
                }
                InterfaceStatus.UNPLUGGED -> Problem(Modifier.weight(1f).fillMaxHeight(), "audio.unplugged", "Interface unplugged",
                    "The output is muted so nothing pops. Plug it back in and reconnect.") {
                    TextPad("RECONNECT", { st.go(Action.ReconnectInterface) }, Modifier.fillMaxWidth().height(64.dp), arrayOf("audio.unplugged.reconnect"), selected = true)
                    TextPad("USE PHONE MIC", { st.go(Action.UseBuiltinInput) }, Modifier.fillMaxWidth().height(56.dp))
                }
                InterfaceStatus.OK -> if (a.permission == Permission.GRANTED) {
                    Card(Modifier.weight(1f).fillMaxHeight(), "ALL GOOD") {
                        FitText(input?.name ?: "No input", Type.display(26.sp, Ink.hot), Modifier.fillMaxWidth().height(36.dp), align = TextAlign.Start)
                        FitText("${a.sampleRate / 1000.0} kHz · ${a.bufferFrames} samples · input ${a.inputChannel + 1}", Type.value(13.sp, Ink.dim),
                            Modifier.fillMaxWidth().height(20.dp), align = TextAlign.Start)
                        Row(Modifier.fillMaxWidth().height(28.dp), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                            Meter(input = true, Modifier.weight(1f).fillMaxHeight())
                            Meter(input = false, Modifier.weight(1f).fillMaxHeight())
                        }
                        Box(Modifier.weight(1f))
                        TextPad("AUDIO & MIDI DEVICES ›", { st.go(Action.Open(Overlay.AudioDevices)) }, Modifier.fillMaxWidth().height(56.dp))
                    }
                }
            }
        }
    }
}

@Composable
private fun Problem(modifier: Modifier, id: String, title: String, body: String, actions: @Composable ColumnScope.() -> Unit) {
    Column(modifier.clip(RoundedCornerShape(8.dp)).background(Ink.panel).brackets(Ink.alert).padding(14.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
        Column(Modifier.fillMaxWidth().featureId(id)) {
            Box(Modifier.size(10.dp).clip(RoundedCornerShape(5.dp)).background(Ink.alert))
            FitText(title, Type.display(24.sp, Ink.hot), Modifier.fillMaxWidth().height(34.dp), align = TextAlign.Start)
            BasicText(body, style = Type.value(13.sp, Ink.dim))
        }
        Box(Modifier.weight(1f))
        actions()
    }
}
