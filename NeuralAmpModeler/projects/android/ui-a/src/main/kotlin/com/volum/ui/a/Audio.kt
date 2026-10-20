package com.volum.ui.a

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.InterfaceStatus
import com.volum.core.Overlay
import com.volum.core.Permission
import com.volum.design.components.VoLumBanner
import com.volum.design.components.VoLumBannerKind
import com.volum.design.components.VoLumButton
import com.volum.design.components.VoLumListCaption
import com.volum.design.components.VoLumSelectionStyle
import com.volum.design.theme.VText
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumType

@Composable
internal fun AudioDevices(c: Ctx) {
    val a = c.s.audio
    Sheet("Audio & MIDI devices", listOf("audio.devices.close"), c) {
        Row(Modifier.fillMaxWidth().weight(1f), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            Card("Input", Modifier.weight(1f).fillMaxHeight()) {
                a.inputs.forEach { d ->
                    Choice(d.name, d.id == a.inputId, { c.d(Action.SelectInput(d.id)) }, Modifier.fillMaxWidth().ids("audio.input.device"), VoLumSelectionStyle.ListTeal)
                }
                val dev = a.inputs.firstOrNull { it.id == a.inputId }
                if (dev != null && dev.channels > 1) {
                    Caption("GUITAR ON")
                    Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                        (0 until dev.channels).forEach { ch ->
                            Choice("IN ${ch + 1}", a.inputChannel == ch, { c.d(Action.SelectInputChannel(ch)) }, Modifier.weight(1f).ids("audio.input.channel"))
                        }
                    }
                }
            }
            Card("Output", Modifier.weight(1f).fillMaxHeight()) {
                a.outputs.forEach { d ->
                    Choice(d.name, d.id == a.outputId, { c.d(Action.SelectOutput(d.id)) }, Modifier.fillMaxWidth().ids("audio.output.device"), VoLumSelectionStyle.ListTeal)
                }
            }
            Card("Format", Modifier.weight(1f).fillMaxHeight()) {
                Caption("SAMPLE RATE")
                Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                    a.sampleRates.forEach { r -> Choice("${r / 1000.0}k".replace(".0k", "k"), r == a.sampleRate, { c.d(Action.SetSampleRate(r)) }, Modifier.weight(1f).ids("audio.sample_rate"), size = 11.sp, minWidth = 48.dp) }
                }
                Caption("BUFFER  ·  ${f("%.1f", a.bufferFrames * 1000.0 / a.sampleRate)} MS")
                a.buffers.chunked(2).forEach { row ->
                    Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                        row.forEach { b -> Choice("$b", b == a.bufferFrames, { c.d(Action.SetBuffer(b)) }, Modifier.weight(1f).ids("audio.buffer"), size = 11.sp) }
                    }
                }
            }
            Card("MIDI controller", Modifier.weight(1f).fillMaxHeight()) {
                if (a.midiLost) VoLumBanner("Controller disconnected", VoLumBannerKind.ERROR, null, Modifier.fillMaxWidth().ids("midi.device.lost"))
                a.midiDevices.forEach { d ->
                    Choice("${d.name}  ·  ${if (d.ble) "BLE" else "USB"}", d.id == a.midiDeviceId && !a.midiLost, { c.d(Action.SelectMidiDevice(d.id)) },
                        Modifier.fillMaxWidth().ids("midi.device.select"), VoLumSelectionStyle.ListTeal)
                }
                if (a.midiLost) Hint("Plug it back in, or pick another controller.")
                VoLumButton(if (a.bleScanning) "Scanning..." else "Scan for Bluetooth MIDI", { c.d(Action.ScanBleMidi) }, Modifier.fillMaxWidth().ids("midi.device.ble_scan"))
            }
        }
    }
}

/** What feeds VoLum and how to fix it: each problem in words, its recovery as the primary action, sized to its content. */
@Composable
internal fun AudioStatus(c: Ctx) {
    val s = c.s
    val a = s.audio
    Sheet("Input", listOf("audio.status.close"), c, width = 640.dp, fill = false) {
        Column(Modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            when (a.permission) {
                Permission.DENIED -> {
                    Note("VoLum needs the microphone permission to hear your guitar, through the phone or a USB interface.", alert = true, ids = listOf("audio.permission.denied"))
                    VoLumButton("Allow microphone", { c.d(Action.RequestMicPermission) }, Modifier.fillMaxWidth().ids("audio.permission.grant"), primary = true)
                }
                Permission.DENIED_PERMANENT -> {
                    Note("Microphone access is turned off for VoLum. Turn it on in Android settings, then come back.", alert = true, ids = listOf("audio.permission.denied"))
                    VoLumButton("Open app settings", { c.d(Action.OpenAppSettings) }, Modifier.fillMaxWidth().ids("audio.permission.settings"), primary = true)
                }
                Permission.GRANTED -> Unit
            }
            when (a.status) {
                InterfaceStatus.NO_INTERFACE -> {
                    Note("No USB audio interface found. Connect one and rescan, or play through the phone's microphone.", alert = true, ids = listOf("audio.no_interface"))
                    Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                        VoLumButton("Rescan", { c.d(Action.RescanAudio) }, Modifier.weight(1f).ids("audio.no_interface.rescan"), primary = true)
                        VoLumButton("Use phone mic", { c.d(Action.UseBuiltinInput) }, Modifier.weight(1f).ids("audio.no_interface.builtin"))
                    }
                }
                InterfaceStatus.UNPLUGGED -> {
                    Note("The USB interface was unplugged. Output is muted until it is back.", alert = true, ids = listOf("audio.unplugged"))
                    VoLumButton("Reconnect", { c.d(Action.ReconnectInterface) }, Modifier.fillMaxWidth().ids("audio.unplugged.reconnect"), primary = true)
                }
                InterfaceStatus.OK -> if (a.permission == Permission.GRANTED) {
                    val input = a.inputs.firstOrNull { it.id == a.inputId }?.name ?: "-"
                    val output = a.outputs.firstOrNull { it.id == a.outputId }?.name ?: "-"
                    VoLumListCaption("ALL GOOD")
                    StatusLine("IN", "$input  ·  input ${a.inputChannel + 1}")
                    StatusLine("OUT", output)
                    StatusLine("FORMAT", "${a.sampleRate} Hz  ·  ${a.bufferFrames} samples")
                }
            }
            VoLumButton("Audio & MIDI devices", { c.d(Action.Open(Overlay.AudioDevices)) }, Modifier.fillMaxWidth().padding(top = 4.dp).ids("signal.audio_devices"))
        }
    }
}

@Composable
private fun StatusLine(label: String, value: String) {
    Row(verticalAlignment = Alignment.CenterVertically) {
        VText(label, Modifier.padding(end = 12.dp).fillMaxWidth(0.2f), style = VoLumType.label(10.sp, VoLumColors.GoldDim))
        VText(value, style = VoLumType.body(13.sp, VoLumColors.Cream))
    }
}
