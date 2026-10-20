package com.volum.ui.a

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
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
import androidx.compose.ui.semantics.CustomAccessibilityAction
import androidx.compose.ui.semantics.customActions
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.Overlay
import com.volum.core.PickKind
import com.volum.core.SettingsTab

@Composable
internal fun Card(title: String, modifier: Modifier = Modifier, content: @Composable ColumnScope.() -> Unit) {
    Frame(modifier.fillMaxHeight()) {
        Column(Modifier.padding(10.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            SectionLabel(title)
            content()
        }
    }
}

@Composable
internal fun Settings(c: Ctx, tab: SettingsTab) {
    Sheet("Settings", listOf("settings.close"), c, actions = {
        SettingsTab.entries.forEach { t ->
            Chip(t.name, { c.d(Action.SetSettingsTab(t)) }, Modifier.width(88.dp), listOf("settings.tab.${t.name.lowercase()}"), selected = t == tab)
        }
    }) {
        Row(Modifier.fillMaxWidth().weight(1f).padding(bottom = 8.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            when (tab) {
                SettingsTab.SIGNAL -> SignalTab(c)
                SettingsTab.MIDI -> MidiTab(c)
                SettingsTab.SYSTEM -> SystemTab(c)
            }
        }
    }
}

@Composable
private fun androidx.compose.foundation.layout.RowScope.SignalTab(c: Ctx) {
    val s = c.s
    val sig = s.signal
    Card("INPUT CALIBRATION", Modifier.weight(1f)) {
        Switch("CALIBRATE INPUT", sig.calibrate, { c.d(Action.SetCalibrate(!sig.calibrate)) }, Modifier.fillMaxWidth(), listOf("signal.calibration.on"))
        var text by remember(sig.calibrationDbu) { mutableStateOf(f("%.1f", sig.calibrationDbu)) }
        Row(verticalAlignment = Alignment.CenterVertically) {
            NameField(text, { text = it }, { parseNumber(text)?.let { c.d(Action.SetCalibrationDbu(it)) } }, listOf("signal.calibration.level"), Modifier.weight(1f))
            T("  dBu", Type.label(11.sp))
        }
        if (s.amp?.inputLevelDbu == null) {
            T("${s.amp?.name ?: "This amp"} has no recorded input level.", Type.label(10.sp, Ink.faint), Modifier.ids("signal.calibration.unavailable"), maxLines = 2)
        } else {
            T("Capture level ${f("%.1f", s.amp?.inputLevelDbu ?: 0.0)} dBu", Type.label(10.sp))
        }
    }
    Card("OUTPUT MODE", Modifier.weight(1f)) {
        val hasOut = s.amp?.outputLevelDbu != null
        listOf("RAW", "NORMALIZED", "CALIBRATED").forEachIndexed { i, label ->
            val enabled = i != 2 || hasOut
            Chip(if (enabled) label else "$label (N/A)", { c.d(Action.SetOutputMode(i)) }, Modifier.fillMaxWidth(),
                listOf("signal.output." + label.lowercase()), selected = sig.outputMode == i, enabled = enabled)
        }
    }
    Card("PERFORMANCE", Modifier.weight(1f)) {
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            Chip("FULL", { c.d(Action.SetLite(false)) }, Modifier.weight(1f), listOf("signal.perf.full"), selected = !sig.lite)
            Chip("LITE", { c.d(Action.SetLite(true)) }, Modifier.weight(1f), listOf("signal.perf.lite"), selected = sig.lite)
        }
        Switch("ANIMATE ART", sig.animateArt, { c.d(Action.SetAnimateArt(!sig.animateArt)) }, Modifier.fillMaxWidth(), listOf("signal.animate_art"))
        Chip("AUDIO & MIDI DEVICES", { c.d(Action.Open(Overlay.AudioDevices)) }, Modifier.fillMaxWidth(), listOf("signal.audio_devices"))
    }
}

@Composable
private fun androidx.compose.foundation.layout.RowScope.MidiTab(c: Ctx) {
    val s = c.s
    val m = s.midi
    Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            T("CHANNEL", Type.label(11.sp))
            Chip("ALL", { c.d(Action.SetMidiChannel(0)) }, ids = listOf("midi.channel.all"), selected = m.channel == 0)
            Chip("ONE", { c.d(Action.SetMidiChannel(m.lastOneChannel)) }, ids = listOf("midi.channel.one"), selected = m.channel != 0)
            val ch = if (m.channel == 0) m.lastOneChannel else m.channel
            val chIds = listOf("midi.channel.number")
            Stepper(null, "$ch", { c.d(Action.SetMidiChannel(if (ch <= 1) 16 else ch - 1)) }, { c.d(Action.SetMidiChannel(if (ch >= 16) 1 else ch + 1)) },
                prevIds = chIds, nextIds = chIds, enabled = m.channel != 0, valueWidth = 32.dp)
            androidx.compose.foundation.layout.Spacer(Modifier.weight(1f))
            val ccIds = listOf("midi.recall_cc")
            Stepper("RECALL CC", "${m.recallCc}", { c.d(Action.SetRecallCc(if (m.recallCc <= 0) 119 else m.recallCc - 1)) },
                { c.d(Action.SetRecallCc(if (m.recallCc >= 119) 0 else m.recallCc + 1)) }, prevIds = ccIds, nextIds = ccIds, valueWidth = 40.dp)
            Stepper("BANK", "${m.bank + 1}", { c.d(Action.SetMidiBank(m.bank - 1)) }, { c.d(Action.SetMidiBank(m.bank + 1)) },
                prevIds = listOf("midi.bank.prev"), nextIds = listOf("midi.bank.next"), valueWidth = 32.dp)
        }
        SectionLabel("PROGRAMS ${m.bank * 8}–${m.bank * 8 + 7}  ·  TAP A SLOT TO ASSIGN A SOUND")
        (0 until 8).chunked(4).forEach { row ->
            Row(Modifier.fillMaxWidth().weight(1f), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                row.forEach { i -> MidiSlot(c, m.bank * 8 + i, Modifier.weight(1f).fillMaxHeight()) }
            }
        }
    }
}

@Composable
private fun MidiSlot(c: Ctx, p: Int, modifier: Modifier) {
    val s = c.s
    val sound = s.library.sounds[p]
    val move = Modifier.semantics { customActions = listOf(CustomAccessibilityAction("Move down") { c.d(Action.MoveSound(p, 1)); true }) }
    Box(modifier) {
        Tap({ c.d(Action.Open(Overlay.SoundPicker(p, replace = true))) }, Modifier.fillMaxWidth().fillMaxHeight().then(move),
            listOf("midi.slot.assign", "midi.slot.move"), selected = s.library.live == p, pick = Pick.TEAL) {
            Column(Modifier.fillMaxWidth().padding(end = if (sound != null) 48.dp else 0.dp)) {
                T(program(p), Type.value(14.sp, Ink.dim))
                T(sound?.let { s.library.preset(it.presetId)?.name } ?: if (sound != null) "Invalid slot" else "empty",
                    Type.value(13.sp, if (sound == null) Ink.faint else if (s.library.soundValid(p)) Ink.text else Ink.alert))
            }
        }
        if (sound != null) IconTap(Glyph.CLOSE, { c.d(Action.ClearSound(p)) }, Modifier.align(Alignment.CenterEnd), listOf("midi.slot.clear"), desc = "Clear program $p")
    }
}

@Composable
private fun androidx.compose.foundation.layout.RowScope.SystemTab(c: Ctx) {
    val s = c.s
    Card("ABOUT", Modifier.weight(1f)) {
        T("VoLum 1.3 for Android (shootout build)", Type.value(13.sp), Modifier.ids("system.about.version"), maxLines = 2)
        T("Playing: ${s.amp?.name ?: "-"}  ·  ${if (s.signal.lite) "A2 Lite" else "A2 Full"}", Type.label(10.sp), Modifier.ids("system.model_info"), maxLines = 2)
        Chip("USER GUIDE", { c.d(Action.OpenLink("https://github.com/guitarlum/VoLum/blob/main/docs/user-guide.en.md")) }, Modifier.fillMaxWidth(), listOf("system.about.manual"))
        Chip("SHARE LOG", { c.d(Action.ShareLog) }, Modifier.fillMaxWidth(), listOf("system.share_log"))
    }
    Card("UPDATES", Modifier.weight(1f)) {
        Switch("CHECK AUTOMATICALLY", s.update.auto, { c.d(Action.SetAutoUpdate(!s.update.auto)) }, Modifier.fillMaxWidth(), listOf("system.update.auto"))
        Chip("CHECK NOW", { c.d(Action.CheckUpdatesNow) }, Modifier.fillMaxWidth(), listOf("system.update.check_now"))
        val available = s.update.available
        if (available != null) {
            Chip("GET VoLum $available", { c.d(Action.OpenLink("https://github.com/guitarlum/VoLum/releases/latest")) }, Modifier.fillMaxWidth(), listOf("system.update.link"), selected = true)
        } else if (s.update.checks > 0) {
            T("You have the latest version.", Type.label(10.sp))
        }
    }
    Card("PACKS", Modifier.weight(1f)) {
        T("Back up or share amps, IRs, pedals and presets as one file.", Type.label(10.sp, Ink.faint), maxLines = 2)
        Chip("EXPORT PACK", { c.d(Action.Open(Overlay.PackExport)) }, Modifier.fillMaxWidth(), listOf("pack.export.open"))
        Chip("IMPORT PACK", { c.d(Action.PickFiles(PickKind.PACK_OPEN)) }, Modifier.fillMaxWidth(), listOf("pack.import.open"))
    }
}

@Composable
internal fun AudioDevices(c: Ctx) {
    val a = c.s.audio
    Sheet("Audio & MIDI devices", listOf("audio.devices.close"), c) {
        Row(Modifier.fillMaxWidth().weight(1f).padding(bottom = 8.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Card("INPUT", Modifier.weight(1f)) {
                a.inputs.forEach { d -> Chip(d.name, { c.d(Action.SelectInput(d.id)) }, Modifier.fillMaxWidth(), listOf("audio.input.device"), selected = d.id == a.inputId) }
                val dev = a.inputs.firstOrNull { it.id == a.inputId }
                if (dev != null && dev.channels > 1) {
                    Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                        (0 until dev.channels).forEach { ch ->
                            Chip("IN ${ch + 1}", { c.d(Action.SelectInputChannel(ch)) }, Modifier.weight(1f), listOf("audio.input.channel"), selected = a.inputChannel == ch)
                        }
                    }
                }
            }
            Card("OUTPUT", Modifier.weight(1f)) {
                a.outputs.forEach { d -> Chip(d.name, { c.d(Action.SelectOutput(d.id)) }, Modifier.fillMaxWidth(), listOf("audio.output.device"), selected = d.id == a.outputId) }
            }
            Card("FORMAT", Modifier.weight(1f)) {
                Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                    a.sampleRates.forEach { r -> Chip("${r / 1000.0}k".replace(".0k", "k"), { c.d(Action.SetSampleRate(r)) }, Modifier.weight(1f), listOf("audio.sample_rate"), selected = r == a.sampleRate, size = 11.sp) }
                }
                SectionLabel("BUFFER  ·  ${f("%.1f", a.bufferFrames * 1000.0 / a.sampleRate)} ms")
                a.buffers.chunked(2).forEach { row ->
                    Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                        row.forEach { b -> Chip("$b", { c.d(Action.SetBuffer(b)) }, Modifier.weight(1f), listOf("audio.buffer"), selected = b == a.bufferFrames) }
                    }
                }
            }
            Card("MIDI CONTROLLER", Modifier.weight(1f)) {
                if (a.midiLost) Notice("Controller disconnected. Plug it back in or pick another.", alert = true, ids = listOf("midi.device.lost"))
                a.midiDevices.forEach { d ->
                    Chip(d.name + if (d.ble) "  · BLE" else "  · USB", { c.d(Action.SelectMidiDevice(d.id)) }, Modifier.fillMaxWidth(), listOf("midi.device.select"), selected = d.id == a.midiDeviceId && !a.midiLost)
                }
                Chip(if (a.bleScanning) "SCANNING…" else "FIND BLUETOOTH MIDI", { c.d(Action.ScanBleMidi) }, Modifier.fillMaxWidth(), listOf("midi.device.ble_scan"))
            }
        }
    }
}

@Composable
internal fun AudioStatus(c: Ctx) {
    val s = c.s
    val a = s.audio
    Sheet("Input", listOf("audio.status.close"), c, width = 640.dp) {
        Column(Modifier.fillMaxWidth().weight(1f).padding(bottom = 10.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
            when (a.permission) {
                com.volum.core.Permission.DENIED -> {
                    Notice("VoLum needs the microphone permission to hear your guitar.", alert = true, ids = listOf("audio.permission.denied"))
                    Chip("ALLOW MICROPHONE", { c.d(Action.RequestMicPermission) }, Modifier.fillMaxWidth(), listOf("audio.permission.grant"), selected = true)
                }
                com.volum.core.Permission.DENIED_PERMANENT -> {
                    Notice("Microphone access is turned off for VoLum. Turn it on in Android settings.", alert = true, ids = listOf("audio.permission.denied"))
                    Chip("OPEN APP SETTINGS", { c.d(Action.OpenAppSettings) }, Modifier.fillMaxWidth(), listOf("audio.permission.settings"), selected = true)
                }
                com.volum.core.Permission.GRANTED -> Unit
            }
            when (a.status) {
                com.volum.core.InterfaceStatus.NO_INTERFACE -> {
                    Notice("No USB audio interface found. Connect one, or play through the phone microphone.", alert = true, ids = listOf("audio.no_interface"))
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        Chip("RESCAN", { c.d(Action.RescanAudio) }, Modifier.weight(1f), listOf("audio.no_interface.rescan"), selected = true)
                        Chip("USE PHONE MIC", { c.d(Action.UseBuiltinInput) }, Modifier.weight(1f), listOf("audio.no_interface.builtin"))
                    }
                }
                com.volum.core.InterfaceStatus.UNPLUGGED -> {
                    Notice("The USB interface was unplugged. Output is muted until it is back.", alert = true, ids = listOf("audio.unplugged"))
                    Chip("RECONNECT", { c.d(Action.ReconnectInterface) }, Modifier.fillMaxWidth(), listOf("audio.unplugged.reconnect"), selected = true)
                }
                com.volum.core.InterfaceStatus.OK -> if (a.permission == com.volum.core.Permission.GRANTED) {
                    val input = a.inputs.firstOrNull { it.id == a.inputId }?.name ?: "-"
                    val output = a.outputs.firstOrNull { it.id == a.outputId }?.name ?: "-"
                    Notice("In: $input (input ${a.inputChannel + 1})\nOut: $output\n${a.sampleRate} Hz · ${a.bufferFrames} samples")
                }
            }
            Chip("AUDIO & MIDI DEVICES", { c.d(Action.Open(Overlay.AudioDevices)) }, Modifier.fillMaxWidth())
        }
    }
}
