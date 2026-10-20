package com.volum.ui.c

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.BasicText
import androidx.compose.foundation.verticalScroll
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.semantics.customActions
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import com.volum.core.Action
import com.volum.core.InterfaceStatus
import com.volum.core.Overlay
import com.volum.core.Permission
import com.volum.core.PickKind
import com.volum.core.SettingsTab

@Composable
private fun Columns(left: @Composable ColumnScope.() -> Unit, right: @Composable ColumnScope.() -> Unit) {
    Row(Modifier.fillMaxWidth().fillMaxHeight(), horizontalArrangement = Arrangement.spacedBy(20.dp)) {
        Column(Modifier.weight(1f).fillMaxHeight().verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(10.dp), content = left)
        Column(Modifier.weight(1f).fillMaxHeight().verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(10.dp), content = right)
    }
}

@Composable
internal fun Settings(ui: Ui, o: Overlay.Settings, modifier: Modifier) {
    PanelFrame("", null, "settings.close", { ui.back() }, modifier, lead = {
        BasicText("SETTINGS", style = labelStyle(13f, Look.text), modifier = Modifier.padding(end = 12.dp))
        Choice("SIGNAL", o.tab == SettingsTab.SIGNAL, listOf("settings.tab.signal"), { ui.d(Action.SetSettingsTab(SettingsTab.SIGNAL)) }, minWidth = 104.dp)
        Choice("MIDI", o.tab == SettingsTab.MIDI, listOf("settings.tab.midi"), { ui.d(Action.SetSettingsTab(SettingsTab.MIDI)) }, minWidth = 104.dp)
        Choice("SYSTEM", o.tab == SettingsTab.SYSTEM, listOf("settings.tab.system"), { ui.d(Action.SetSettingsTab(SettingsTab.SYSTEM)) }, minWidth = 104.dp)
    }) {
        when (o.tab) {
            SettingsTab.SIGNAL -> SignalTab(ui)
            SettingsTab.MIDI -> MidiTab(ui)
            SettingsTab.SYSTEM -> SystemTab(ui)
        }
    }
}

@Composable
private fun SignalTab(ui: Ui) {
    val s = ui.s
    val sig = s.signal
    val amp = s.amp
    Columns(
        left = {
            Labeled("Input calibration") {
                if (amp?.inputLevelDbu == null) Note("${amp?.name ?: "This amp"} stores no capture input level, so calibration has nothing to match.", Modifier.ids("signal.calibration.unavailable"))
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.CenterVertically) {
                    Field("%.1f".format(sig.calibrationDbu), listOf("signal.calibration.level"), { t -> t.replace(',', '.').toDoubleOrNull()?.let { ui.d(Action.SetCalibrationDbu(it)) } }, Modifier.width(110.dp), numeric = true)
                    BasicText("dBu at 0 dBFS", style = labelStyle(10f))
                    Switch("CALIBRATE", sig.calibrate, listOf("signal.calibration.on"), { ui.d(Action.SetCalibrate(!sig.calibrate)) })
                }
            }
            Labeled("Output level") {
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    Choice("RAW", sig.outputMode == 0, listOf("signal.output.raw"), { ui.d(Action.SetOutputMode(0)) })
                    Choice("NORMALIZED", sig.outputMode == 1, listOf("signal.output.normalized"), { ui.d(Action.SetOutputMode(1)) })
                    Choice("CALIBRATED", sig.outputMode == 2, listOf("signal.output.calibrated"), { ui.d(Action.SetOutputMode(2)) }, enabled = amp?.outputLevelDbu != null)
                }
                if (amp?.outputLevelDbu == null) Note("CALIBRATED needs a capture that stores its output level.", maxLines = 1)
            }
        },
        right = {
            Labeled("Performance") {
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    Choice("FULL", !sig.lite, listOf("signal.perf.full"), { ui.d(Action.SetLite(false)) }, minWidth = 80.dp)
                    Choice("LITE", sig.lite, listOf("signal.perf.lite"), { ui.d(Action.SetLite(true)) }, sub = "LESS CPU", minWidth = 80.dp)
                }
            }
            Labeled("Display") { Switch("ANIMATE ART", sig.animateArt, listOf("signal.animate_art"), { ui.d(Action.SetAnimateArt(!sig.animateArt)) }) }
            Labeled("Audio") {
                val a = s.audio
                val input = a.inputs.firstOrNull { it.id == a.inputId }?.name ?: "no input"
                Btn("AUDIO & MIDI DEVICES  ▸", listOf("signal.audio_devices"), { ui.d(Action.Open(Overlay.AudioDevices)) }, sub = "$input · ${a.sampleRate} Hz · ${a.bufferFrames}", modifier = Modifier.fillMaxWidth())
            }
        },
    )
}

@Composable
private fun MidiTab(ui: Ui) {
    val s = ui.s
    val midi = s.midi
    val lib = s.library
    Columns(
        left = {
            Labeled("Listen on") {
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
                    Choice("ALL CHANNELS", midi.channel == 0, listOf("midi.channel.all"), { ui.d(Action.SetMidiChannel(0)) })
                    Choice("ONE", midi.channel != 0, listOf("midi.channel.one"), { ui.d(Action.SetMidiChannel(midi.lastOneChannel)) }, minWidth = 64.dp)
                }
                if (midi.channel != 0) {
                    Stepper("CH ${midi.channel}", listOf("midi.channel.number"), { ui.d(Action.SetMidiChannel((midi.channel + it).coerceIn(1, 16))) }, label = "CHANNEL")
                }
            }
            Labeled("Recall by control change") {
                Stepper("CC ${midi.recallCc}", listOf("midi.recall_cc"), { ui.d(Action.SetRecallCc(midi.recallCc + it)) }, valueWidth = 72.dp)
                Note("Program Change recalls the Sound on that program; this CC does the same with its value.", maxLines = 2)
            }
        },
        right = {
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                Caption("Program slots")
                Fill()
                Btn("‹", listOf("midi.bank.prev"), { ui.d(Action.SetMidiBank(midi.bank - 1)) }, enabled = midi.bank > 0)
                BasicText("BANK ${midi.bank + 1} · %03d–%03d".format(midi.bank * 8, midi.bank * 8 + 7), style = techStyle(11f), maxLines = 1)
                Btn("›", listOf("midi.bank.next"), { ui.d(Action.SetMidiBank(midi.bank + 1)) }, enabled = midi.bank < 15)
            }
            Grid((midi.bank * 8 until midi.bank * 8 + 8).toList(), 2) { p, m ->
                val sound = lib.sounds[p]
                Row(m, horizontalArrangement = Arrangement.spacedBy(4.dp), verticalAlignment = Alignment.CenterVertically) {
                    ListRow(
                        sound?.let { lib.preset(it.presetId)?.name ?: "Invalid slot" } ?: "— empty —",
                        listOf("midi.slot.assign", "midi.slot.move"),
                        { ui.d(Action.Open(Overlay.SoundPicker(p, replace = sound != null))) },
                        Modifier.weight(1f).semantics {
                            customActions = listOf(
                                androidx.compose.ui.semantics.CustomAccessibilityAction("Move down") { ui.d(Action.MoveSound(p, 1)); true },
                                androidx.compose.ui.semantics.CustomAccessibilityAction("Move up") { ui.d(Action.MoveSound(p, -1)); true },
                            )
                        },
                        sub = "PROGRAM %03d".format(p) + if (lib.live == p) " · LIVE" else "",
                        selected = lib.live == p,
                    )
                    if (sound != null) Btn("×", listOf("midi.slot.clear"), { ui.d(Action.ClearSound(p)) })
                }
            }
        },
    )
}

@Composable
private fun SystemTab(ui: Ui) {
    val s = ui.s
    val u = s.update
    Columns(
        left = {
            Labeled("About") {
                BasicText("VoLum 0.1-shootout · Android", style = bodyStyle(14f), modifier = Modifier.ids("system.about.version"))
                BasicText(
                    "Model: " + (s.amp?.let { "${it.name} · CH ${s.rig.channel + 1} · ${if (it.custom) "custom capture" else "A2 capture"}" } ?: "none"),
                    style = bodyStyle(12f, Look.dim, cap = 1.15f), maxLines = 1, overflow = TextOverflow.Ellipsis, modifier = Modifier.ids("system.model_info"),
                )
                Btn("USER GUIDE  ↗", listOf("system.about.manual"), { ui.d(Action.OpenLink("https://github.com/guitarlum/VoLum/blob/main/docs/user-guide.en.md")) })
            }
            Labeled("Updates") {
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
                    Switch("CHECK AUTOMATICALLY", u.auto, listOf("system.update.auto"), { ui.d(Action.SetAutoUpdate(!u.auto)) })
                    Btn("CHECK NOW", listOf("system.update.check_now"), { ui.d(Action.CheckUpdatesNow) })
                }
                if (u.available != null) Btn("GET ${u.available}  ↗", listOf("system.update.link"), { ui.d(Action.OpenLink("https://github.com/guitarlum/VoLum/releases/latest")) }, strong = true, sub = "A NEWER VOLUM IS OUT")
                else if (u.checks > 0) Note("You are up to date.")
            }
        },
        right = {
            Labeled("Packs") {
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    Btn("EXPORT PACK…", listOf("pack.export.open"), { ui.d(Action.Open(Overlay.PackExport)) }, sub = "BACK UP OR SHARE")
                    Btn("IMPORT PACK…", listOf("pack.import.open"), { ui.d(Action.PickFiles(PickKind.PACK_OPEN)) }, sub = "OPEN A .VOLUMPACK")
                }
                Note("A Pack carries presets, custom amps, IRs and pedals in one file.")
            }
            Labeled("Support") {
                Btn("SHARE LOG", listOf("system.share_log"), { ui.d(Action.ShareLog) }, sub = "SEND VOLUM.LOG")
            }
        },
    )
}

@Composable
internal fun AudioDevices(ui: Ui, modifier: Modifier) {
    val a = ui.s.audio
    PanelFrame("Audio & MIDI devices", "SETTINGS · SIGNAL", "audio.devices.close", { ui.back() }, modifier) {
        Row(Modifier.fillMaxWidth().fillMaxHeight(), horizontalArrangement = Arrangement.spacedBy(20.dp)) {
            Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                Labeled("Input") {
                    Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                        a.inputs.forEach { d -> Choice(d.name, a.inputId == d.id, listOf("audio.input.device"), { ui.d(Action.SelectInput(d.id)) }, Modifier.weight(1f), sub = if (d.usb) "USB" else "BUILT-IN") }
                    }
                    val dev = a.inputs.firstOrNull { it.id == a.inputId }
                    if (dev != null && dev.channels > 1) {
                        Row(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
                            (0 until dev.channels).forEach { ch -> Choice("INPUT ${ch + 1}", a.inputChannel == ch, listOf("audio.input.channel"), { ui.d(Action.SelectInputChannel(ch)) }, Modifier.weight(1f)) }
                        }
                    }
                }
                Labeled("Output") {
                    Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                        a.outputs.forEach { d -> Choice(d.name, a.outputId == d.id, listOf("audio.output.device"), { ui.d(Action.SelectOutput(d.id)) }, Modifier.weight(1f), sub = if (d.usb) "USB" else "BUILT-IN") }
                    }
                }
            }
            Column(Modifier.weight(0.8f), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                Labeled("Sample rate") {
                    Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                        a.sampleRates.forEach { r -> Choice("%.1f k".format(r / 1000.0), a.sampleRate == r, listOf("audio.sample_rate"), { ui.d(Action.SetSampleRate(r)) }, Modifier.weight(1f)) }
                    }
                }
                Labeled("Buffer") {
                    Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                        a.buffers.forEach { b -> Choice("$b", a.bufferFrames == b, listOf("audio.buffer"), { ui.d(Action.SetBuffer(b)) }, Modifier.weight(1f)) }
                    }
                    Note("≈ %.1f ms. Smaller answers faster; raise it if you hear crackles.".format(a.bufferFrames * 1000.0 / a.sampleRate), maxLines = 2)
                }
            }
            Column(Modifier.weight(1f).fillMaxHeight().verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                Labeled("MIDI controllers") {
                    if (a.midiLost) {
                        Box(Modifier.background(Look.danger.copy(alpha = 0.2f), Corner).padding(10.dp).ids("midi.device.lost")) {
                            Note("Your MIDI controller disconnected. Plug it back in or pick another.", color = Look.text)
                        }
                    }
                    a.midiDevices.forEach { d ->
                        ListRow(d.name, listOf("midi.device.select"), { ui.d(Action.SelectMidiDevice(d.id)) }, Modifier.fillMaxWidth(), sub = (if (d.ble) "BLUETOOTH" else "USB") + if (a.midiDeviceId == d.id) " · IN USE" else "", selected = a.midiDeviceId == d.id && !a.midiLost)
                    }
                    if (a.midiDevices.isEmpty()) Note("No MIDI controller connected.")
                    Btn(if (a.bleScanning) "SCANNING…" else "SCAN FOR BLUETOOTH MIDI", listOf("midi.device.ble_scan"), { ui.d(Action.ScanBleMidi) })
                }
            }
        }
    }
}

@Composable
internal fun AudioStatus(ui: Ui, modifier: Modifier) {
    val a = ui.s.audio
    PanelFrame("Audio input", null, "audio.status.close", { ui.back() }, modifier) {
        Column(Modifier.fillMaxWidth().padding(horizontal = 8.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
            when {
                a.permission != Permission.GRANTED -> {
                    Alert("Microphone access is off", if (a.permission == Permission.DENIED_PERMANENT) "Android will not ask again. Allow it in the app settings, then come back." else "VoLum needs the microphone permission to hear your guitar, even through a USB interface.", "audio.permission.denied")
                    if (a.permission == Permission.DENIED_PERMANENT) Btn("OPEN APP SETTINGS  ↗", listOf("audio.permission.settings"), { ui.d(Action.OpenAppSettings) }, strong = true)
                    else Btn("ALLOW MICROPHONE", listOf("audio.permission.grant"), { ui.d(Action.RequestMicPermission) }, strong = true)
                }
                a.status == InterfaceStatus.NO_INTERFACE -> {
                    Alert("No USB audio interface", "Connect your interface (USB-C or with an OTG adapter) and rescan, or play through the phone microphone for now.", "audio.no_interface")
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        Btn("RESCAN USB", listOf("audio.no_interface.rescan"), { ui.d(Action.RescanAudio) }, strong = true)
                        Btn("USE PHONE MIC", listOf("audio.no_interface.builtin"), { ui.d(Action.UseBuiltinInput) })
                    }
                }
                a.status == InterfaceStatus.UNPLUGGED -> {
                    Alert("Interface unplugged", "The USB interface went away mid-session. Sound is stopped so nothing blasts out of the phone speaker.", "audio.unplugged")
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        Btn("RECONNECT", listOf("audio.unplugged.reconnect"), { ui.d(Action.ReconnectInterface) }, strong = true)
                        Btn("USE PHONE MIC", listOf("audio.no_interface.builtin"), { ui.d(Action.UseBuiltinInput) })
                    }
                }
                else -> {
                    val input = a.inputs.firstOrNull { it.id == a.inputId }
                    Alert("Input is live", "${input?.name ?: "No input"} · input ${a.inputChannel + 1} · ${a.sampleRate} Hz · ${a.bufferFrames} samples", null, ok = true)
                    Btn("AUDIO & MIDI DEVICES  ▸", emptyList(), { ui.d(Action.Open(Overlay.AudioDevices)) })
                }
            }
        }
    }
}

@Composable
private fun Alert(title: String, body: String, id: String?, ok: Boolean = false) {
    val color = if (ok) Look.ok else Look.danger
    Row(
        Modifier.fillMaxWidth().background(color.copy(alpha = 0.14f), Corner).padding(12.dp).then(if (id != null) Modifier.ids(id) else Modifier),
        horizontalArrangement = Arrangement.spacedBy(12.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Led(true, 12.dp, color)
        Column {
            BasicText(title, style = bodyStyle(17f))
            Note(body, color = Look.text, maxLines = 2)
        }
    }
}
