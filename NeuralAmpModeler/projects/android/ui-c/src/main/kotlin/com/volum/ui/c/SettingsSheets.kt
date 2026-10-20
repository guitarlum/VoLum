package com.volum.ui.c

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.semantics.CustomAccessibilityAction
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.customActions
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.InterfaceStatus
import com.volum.core.Overlay
import com.volum.core.Permission
import com.volum.core.PickKind
import com.volum.core.SettingsTab
import com.volum.design.components.CrossButton
import com.volum.design.components.VoLumChannelStepper
import com.volum.design.components.VoLumSegmentSwitch
import com.volum.design.components.WarnGlyph
import com.volum.design.components.hairlineFrame
import com.volum.design.components.insetWell
import com.volum.design.theme.VFitText
import com.volum.design.theme.VText
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumType
import com.volum.design.theme.withOpacity

@Composable
private fun Columns(left: @Composable ColumnScope.() -> Unit, right: @Composable ColumnScope.() -> Unit) {
    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(24.dp)) {
        Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(8.dp), content = left)
        Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(8.dp), content = right)
    }
}

@Composable
private fun Labeled(title: String, modifier: Modifier = Modifier, content: @Composable ColumnScope.() -> Unit) {
    Column(modifier, verticalArrangement = Arrangement.spacedBy(4.dp)) {
        Caption(title)
        content()
    }
}

@Composable
internal fun Settings(ui: Ui, o: Overlay.Settings, modifier: Modifier) {
    val tabs = listOf(SettingsTab.SIGNAL, SettingsTab.MIDI, SettingsTab.SYSTEM)
    PanelFrame(ui, "Settings", "settings.close", { ui.back() }, modifier, actions = {
        Pills(listOf("SIGNAL", "MIDI", "SYSTEM"), tabs.indexOf(o.tab), { "settings.tab." + tabs[it].name.lowercase() }, { ui.d(Action.SetSettingsTab(tabs[it])) }, Modifier.width(330.dp))
        Gap(w = 12.dp)
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
                if (amp?.inputLevelDbu == null) Note("${amp?.name ?: "This amp"} stores no capture input level, so calibration has nothing to match.", Modifier.ids("signal.calibration.unavailable"), maxLines = 2)
                Spaced(gap = 10) {
                    Field("%.1f".format(sig.calibrationDbu), listOf("signal.calibration.level"), { t -> t.replace(',', '.').toDoubleOrNull()?.let { ui.d(Action.SetCalibrationDbu(it)) } }, Modifier.width(96.dp).alpha(if (sig.calibrate) 1f else 0.4f), numeric = true)
                    VText("dBu at 0 dBFS", style = VoLumType.label(10.sp, if (sig.calibrate) VoLumColors.TextDim else VoLumColors.CreamDim.withOpacity(0.6f)))
                    Toggle("Calibrate", sig.calibrate, listOf("signal.calibration.on"), { ui.d(Action.SetCalibrate(!sig.calibrate)) })
                }
            }
            Labeled("Output level") {
                Spaced(gap = 4) {
                    Brass("Raw", sig.outputMode == 0, listOf("signal.output.raw"), { ui.d(Action.SetOutputMode(0)) })
                    Brass("Normalized", sig.outputMode == 1, listOf("signal.output.normalized"), { ui.d(Action.SetOutputMode(1)) })
                    Brass("Calibrated", sig.outputMode == 2, listOf("signal.output.calibrated"), { ui.d(Action.SetOutputMode(2)) }, enabled = amp?.outputLevelDbu != null)
                }
                if (amp?.outputLevelDbu == null) Note("Calibrated needs a capture that stores its output level.", maxLines = 1, size = 11.sp)
            }
        },
        right = {
            Labeled("Performance") {
                Spaced(gap = 4) {
                    Brass("Full", !sig.lite, listOf("signal.perf.full"), { ui.d(Action.SetLite(false)) }, minWidth = 80.dp)
                    Brass("Lite", sig.lite, listOf("signal.perf.lite"), { ui.d(Action.SetLite(true)) }, minWidth = 80.dp)
                    Note("Lite uses less CPU on slower phones.", maxLines = 1, size = 11.sp)
                }
            }
            Labeled("Display") { Toggle("Animate art", sig.animateArt, listOf("signal.animate_art"), { ui.d(Action.SetAnimateArt(!sig.animateArt)) }) }
            Labeled("Audio") {
                val a = s.audio
                val input = a.inputs.firstOrNull { it.id == a.inputId }?.name ?: "no input"
                Spaced(gap = 10) {
                    Btn("Audio & MIDI devices", listOf("signal.audio_devices"), { ui.d(Action.Open(Overlay.AudioDevices)) })
                    VFitText("$input  ·  ${a.sampleRate} Hz  ·  ${a.bufferFrames}", Modifier.weight(1f), style = VoLumType.label(10.sp, VoLumColors.TealDim), minSize = 7.sp)
                }
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
                Spaced(gap = 4) {
                    Brass("All channels", midi.channel == 0, listOf("midi.channel.all"), { ui.d(Action.SetMidiChannel(0)) })
                    Brass("One channel", midi.channel != 0, listOf("midi.channel.one"), { ui.d(Action.SetMidiChannel(midi.lastOneChannel)) })
                }
                if (midi.channel != 0) {
                    Spaced(gap = 8) {
                        Caption("Channel", color = VoLumColors.CreamDim)
                        VoLumChannelStepper(
                            "${midi.channel}",
                            onPrev = { ui.d(Action.SetMidiChannel((midi.channel - 2 + 16) % 16 + 1)) },
                            onNext = { ui.d(Action.SetMidiChannel(midi.channel % 16 + 1)) },
                            prevModifier = Modifier.ids("midi.channel.number"),
                            nextModifier = Modifier.ids("midi.channel.number"),
                        )
                    }
                }
            }
            Labeled("Recall by control change") {
                Spaced(gap = 8) {
                    Caption("CC", color = VoLumColors.CreamDim)
                    VoLumChannelStepper(
                        "${midi.recallCc}",
                        onPrev = { ui.d(Action.SetRecallCc(midi.recallCc - 1)) },
                        onNext = { ui.d(Action.SetRecallCc(midi.recallCc + 1)) },
                        prevModifier = Modifier.ids("midi.recall_cc"),
                        nextModifier = Modifier.ids("midi.recall_cc"),
                    )
                }
                Note("Program Change recalls the Sound on that program; this CC does the same with its value.", maxLines = 2, size = 11.sp)
            }
        },
        right = {
            Spaced(gap = 6) {
                Caption("Program slots")
                Fill()
                VoLumChannelStepper(
                    "%03d-%03d".format(midi.bank * 8, midi.bank * 8 + 7),
                    onPrev = { if (midi.bank > 0) ui.d(Action.SetMidiBank(midi.bank - 1)) },
                    onNext = { if (midi.bank < 15) ui.d(Action.SetMidiBank(midi.bank + 1)) },
                    prevModifier = Modifier.semantics { contentDescription = "Previous bank" }.ids("midi.bank.prev"),
                    nextModifier = Modifier.semantics { contentDescription = "Next bank" }.ids("midi.bank.next"),
                )
            }
            Row(Modifier.fillMaxWidth().padding(bottom = 2.dp), horizontalArrangement = Arrangement.spacedBy(5.dp, Alignment.End), verticalAlignment = Alignment.CenterVertically) {
                VText("BANK ${midi.bank + 1}", style = VoLumType.label(9.sp, VoLumColors.GoldDim))
                repeat(16) { i ->
                    Spacer(Modifier.size(6.dp).drawBehind { drawCircle(if (i == midi.bank) VoLumColors.Gold else VoLumColors.Frame) })
                }
            }
            Grid((midi.bank * 8 until midi.bank * 8 + 8).toList(), 2) { p, m ->
                val sound = lib.sounds[p]
                Row(m, verticalAlignment = Alignment.CenterVertically) {
                    ListRow(
                        sound?.let { lib.preset(it.presetId)?.name ?: "Invalid slot" } ?: "Empty",
                        listOf("midi.slot.assign", "midi.slot.move"),
                        { ui.d(Action.Open(Overlay.SoundPicker(p, replace = sound != null))) },
                        Modifier.weight(1f).semantics {
                            customActions = listOf(
                                CustomAccessibilityAction("Move down") { ui.d(Action.MoveSound(p, 1)); true },
                                CustomAccessibilityAction("Move up") { ui.d(Action.MoveSound(p, -1)); true },
                            )
                        },
                        sub = "%03d".format(p) + if (lib.live == p) "  ·  LIVE" else "",
                        selected = lib.live == p,
                    )
                    if (sound != null) {
                        Gap(w = 14.dp)
                        CrossButton({ ui.d(Action.ClearSound(p)) }, Modifier.semantics { contentDescription = "Clear program $p" }.ids("midi.slot.clear"), VoLumColors.Danger.withOpacity(0.8f))
                    } else Gap(w = 62.dp)
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
                VText("VoLum 0.1-shootout  ·  Android", Modifier.ids("system.about.version"), style = VoLumType.label(14.sp, VoLumColors.TextBright))
                VFitText(
                    "Model: " + (s.amp?.let { "${it.name}  ·  CH ${s.rig.channel + 1}  ·  ${if (it.custom) "custom capture" else "A2 capture"}" } ?: "none"),
                    Modifier.ids("system.model_info"), style = VoLumType.body(12.sp, VoLumColors.TextDim), minSize = 9.sp,
                )
                Btn("User guide", listOf("system.about.manual"), { ui.d(Action.OpenLink("https://github.com/guitarlum/VoLum/blob/main/docs/user-guide.en.md")) })
            }
            Labeled("Updates") {
                Spaced(gap = 8) {
                    Toggle("Check automatically", u.auto, listOf("system.update.auto"), { ui.d(Action.SetAutoUpdate(!u.auto)) })
                    Btn("Check now", listOf("system.update.check_now"), { ui.d(Action.CheckUpdatesNow) })
                }
                if (u.available != null) Btn("Get VoLum ${u.available}", listOf("system.update.link"), { ui.d(Action.OpenLink("https://github.com/guitarlum/VoLum/releases/latest")) }, primary = true)
                else if (u.checks > 0) Note("You are up to date.")
            }
        },
        right = {
            Labeled("Packs") {
                Spaced(gap = 8) {
                    Btn("Export pack", listOf("pack.export.open"), { ui.d(Action.Open(Overlay.PackExport)) })
                    Btn("Import pack", listOf("pack.import.open"), { ui.d(Action.PickFiles(PickKind.PACK_OPEN)) })
                }
                Note("A Pack carries presets, custom amps, IRs and pedals in one file, to back up or share.", maxLines = 2)
            }
            Labeled("Support") {
                Spaced(gap = 8) {
                    Btn("Share log", listOf("system.share_log"), { ui.d(Action.ShareLog) })
                    Note("Sends volum.log to whoever helps you.", maxLines = 1)
                }
            }
        },
    )
}

@Composable
internal fun AudioDevices(ui: Ui, modifier: Modifier) {
    val a = ui.s.audio
    PanelFrame(ui, "Audio & MIDI devices", "audio.devices.close", { ui.back() }, modifier, crumb = "Settings  ·  Signal") {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(18.dp)) {
            Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                val dev = a.inputs.firstOrNull { it.id == a.inputId }
                Spaced(gap = 4) {
                    Caption("Input", Modifier.weight(1f))
                    if (dev != null && dev.channels > 1) (0 until dev.channels).forEach { ch -> Brass("${ch + 1}", a.inputChannel == ch, listOf("audio.input.channel"), { ui.d(Action.SelectInputChannel(ch)) }, minWidth = 48.dp) }
                }
                a.inputs.forEach { d -> ListRow(d.name, listOf("audio.input.device"), { ui.d(Action.SelectInput(d.id)) }, Modifier.fillMaxWidth(), sub = if (d.usb) "USB" else "BUILT-IN", selected = a.inputId == d.id) }
                Caption("Output")
                a.outputs.forEach { d -> ListRow(d.name, listOf("audio.output.device"), { ui.d(Action.SelectOutput(d.id)) }, Modifier.fillMaxWidth(), sub = if (d.usb) "USB" else "BUILT-IN", selected = a.outputId == d.id) }
            }
            Column(Modifier.weight(0.85f), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                Caption("Sample rate")
                Spaced(gap = 4) { a.sampleRates.forEach { r -> Brass("%.1f k".format(r / 1000.0), a.sampleRate == r, listOf("audio.sample_rate"), { ui.d(Action.SetSampleRate(r)) }) } }
                Caption("Buffer")
                Spaced(gap = 4) { a.buffers.forEach { b -> Brass("$b", a.bufferFrames == b, listOf("audio.buffer"), { ui.d(Action.SetBuffer(b)) }) } }
                Note("About %.1f ms. Smaller answers faster; raise it if you hear crackles.".format(a.bufferFrames * 1000.0 / a.sampleRate), maxLines = 2, size = 11.sp)
            }
            Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                Caption("MIDI controllers")
                if (a.midiLost) Alert("Controller disconnected", "Plug it back in or pick another.", "midi.device.lost")
                a.midiDevices.forEach { d ->
                    ListRow(d.name, listOf("midi.device.select"), { ui.d(Action.SelectMidiDevice(d.id)) }, Modifier.fillMaxWidth(), sub = (if (d.ble) "BLUETOOTH" else "USB") + if (a.midiDeviceId == d.id) "  ·  IN USE" else "", selected = a.midiDeviceId == d.id && !a.midiLost)
                }
                if (a.midiDevices.isEmpty()) Note("No MIDI controller connected.")
                Btn(if (a.bleScanning) "Scanning..." else "Scan for Bluetooth MIDI", listOf("midi.device.ble_scan"), { ui.d(Action.ScanBleMidi) })
            }
        }
    }
}

@Composable
internal fun AudioStatus(ui: Ui, modifier: Modifier) {
    val a = ui.s.audio
    PanelFrame(ui, "Audio input", "audio.status.close", { ui.back() }, modifier) {
        when {
            a.permission != Permission.GRANTED -> {
                Alert("Microphone access is off", if (a.permission == Permission.DENIED_PERMANENT) "Android will not ask again. Allow it in the app settings, then come back." else "VoLum needs the microphone permission to hear your guitar, even through a USB interface.", "audio.permission.denied")
                if (a.permission == Permission.DENIED_PERMANENT) Btn("Open app settings", listOf("audio.permission.settings"), { ui.d(Action.OpenAppSettings) }, primary = true)
                else Btn("Allow microphone", listOf("audio.permission.grant"), { ui.d(Action.RequestMicPermission) }, primary = true)
            }
            a.status == InterfaceStatus.NO_INTERFACE -> {
                Alert("No USB audio interface", "Connect your interface (USB-C, or with an OTG adapter) and rescan, or play through the phone microphone for now.", "audio.no_interface")
                Spaced(gap = 8) {
                    Btn("Rescan USB", listOf("audio.no_interface.rescan"), { ui.d(Action.RescanAudio) }, primary = true)
                    Btn("Use phone mic", listOf("audio.no_interface.builtin"), { ui.d(Action.UseBuiltinInput) })
                }
            }
            a.status == InterfaceStatus.UNPLUGGED -> {
                Alert("Interface unplugged", "The USB interface went away mid-session. Sound is stopped so nothing blasts out of the phone speaker.", "audio.unplugged")
                Spaced(gap = 8) {
                    Btn("Reconnect", listOf("audio.unplugged.reconnect"), { ui.d(Action.ReconnectInterface) }, primary = true)
                    Btn("Use phone mic", listOf("audio.no_interface.builtin"), { ui.d(Action.UseBuiltinInput) })
                }
            }
            else -> {
                val input = a.inputs.firstOrNull { it.id == a.inputId }
                Alert("Input is live", "${input?.name ?: "No input"}  ·  input ${a.inputChannel + 1}  ·  ${a.sampleRate} Hz  ·  ${a.bufferFrames} samples", null, ok = true)
                Btn("Audio & MIDI devices", listOf("signal.audio_devices"), { ui.d(Action.Open(Overlay.AudioDevices)) })
            }
        }
    }
}

/** A recessed notice with a red (or teal) edge and the caution glyph. */
@Composable
internal fun Alert(title: String, body: String, id: String?, ok: Boolean = false) {
    val accent = if (ok) VoLumColors.Teal else VoLumColors.Danger
    Row(
        Modifier.fillMaxWidth().insetWell(4.dp)
            .drawBehind { if (!ok) drawRoundRect(VoLumColors.DangerFill.withOpacity(0.22f), cornerRadius = CornerRadius(4.dp.toPx())) }
            .hairlineFrame(accent.withOpacity(0.7f), radius = 4.dp).padding(12.dp)
            .then(if (id != null) Modifier.ids(id) else Modifier),
        horizontalArrangement = Arrangement.spacedBy(12.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        if (!ok) WarnGlyph(accent) else Box(Modifier.size(22.dp), contentAlignment = Alignment.Center) { com.volum.design.components.VoLumLed(true, color = accent) }
        Column(Modifier.weight(1f)) {
            VText(title, style = VoLumType.label(15.sp, VoLumColors.TextBright))
            Note(body, color = VoLumColors.Cream, maxLines = 3)
        }
    }
}
