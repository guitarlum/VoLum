package com.volum.ui.b

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.InterfaceStatus
import com.volum.core.Overlay
import com.volum.core.Permission
import com.volum.core.ScreenId
import com.volum.core.VoLumState
import com.volum.design.components.VoLumLed
import com.volum.design.components.VoLumMeter
import com.volum.design.components.WarnGlyph
import com.volum.design.components.cornerBrackets
import com.volum.design.components.hairlineFrame
import com.volum.design.components.panelDepth
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumType
import com.volum.design.theme.withOpacity
import com.volum.harness.featureId

/** What is wrong with the input and the one tap that fixes it, beside what VoLum sees of the audio path right now. */
@Composable
internal fun AudioStatus(s: VoLumState) {
    val st = LocalStage.current
    val a = s.audio
    val input = a.inputs.firstOrNull { it.id == a.inputId }
    val actions = rememberFitGroup()
    Panel(s, ScreenId.AUDIO_STATUS, "Audio input", arrayOf("audio.status.close"), subtitle = "what VoLum hears and how to fix it") {
        Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(14.dp)) {
            if (a.permission != Permission.GRANTED) {
                Problem(Modifier.weight(1f).fillMaxHeight(), "audio.permission.denied", "Microphone access is off",
                    if (a.permission == Permission.DENIED) listOf("VoLum needs it to hear your guitar,", "through the interface or the phone mic.")
                    else listOf("Android will not ask again.", "Turn on Microphone for VoLum in the app settings.")) {
                    if (a.permission == Permission.DENIED) {
                        TextPad("ALLOW MICROPHONE", { st.go(Action.RequestMicPermission) }, Modifier.weight(1f).fillMaxHeight(), arrayOf("audio.permission.grant"), tone = Tone.PRIMARY, size = 14.sp, group = actions)
                    } else {
                        TextPad("OPEN APP SETTINGS  ↗", { st.go(Action.OpenAppSettings) }, Modifier.weight(1f).fillMaxHeight(), arrayOf("audio.permission.settings"), tone = Tone.PRIMARY, size = 14.sp, group = actions)
                    }
                }
            }
            when (a.status) {
                InterfaceStatus.NO_INTERFACE -> Problem(Modifier.weight(1f).fillMaxHeight(), "audio.no_interface", "No USB interface",
                    listOf("Connect a USB audio interface and rescan,", "or play through the phone mic for now.")) {
                    TextPad("RESCAN USB", { st.go(Action.RescanAudio) }, Modifier.weight(1f).fillMaxHeight(), arrayOf("audio.no_interface.rescan"), tone = Tone.PRIMARY, size = 14.sp, group = actions)
                    TextPad("USE PHONE MIC", { st.go(Action.UseBuiltinInput) }, Modifier.weight(1f).fillMaxHeight(), arrayOf("audio.no_interface.builtin"), size = 14.sp, group = actions)
                }
                InterfaceStatus.UNPLUGGED -> Problem(Modifier.weight(1f).fillMaxHeight(), "audio.unplugged", "Interface unplugged",
                    listOf("The output is muted so nothing pops.", "Plug it back in and reconnect.")) {
                    TextPad("RECONNECT", { st.go(Action.ReconnectInterface) }, Modifier.weight(1f).fillMaxHeight(), arrayOf("audio.unplugged.reconnect"), tone = Tone.PRIMARY, size = 14.sp, group = actions)
                    TextPad("USE PHONE MIC", { st.go(Action.UseBuiltinInput) }, Modifier.weight(1f).fillMaxHeight(), arrayOf("audio.no_interface.builtin"), size = 14.sp, group = actions)
                }
                InterfaceStatus.OK -> if (a.permission == Permission.GRANTED) {
                    Card(Modifier.weight(1f).fillMaxHeight(), "ALL GOOD", accent = VoLumColors.Teal) {
                        FitText(input?.name ?: "No input", VoLumType.display(26.sp, VoLumColors.TextBright), Modifier.fillMaxWidth().height(34.dp), align = TextAlign.Start)
                        FitText("VoLum hears your guitar and plays it out.", hint(size = 13.sp), Modifier.fillMaxWidth().height(20.dp), align = TextAlign.Start)
                        Box(Modifier.weight(1f))
                        Column(Modifier.fillMaxWidth().height(56.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                            VoLumMeter({ st.level(true) }, "IN", Modifier.fillMaxWidth().height(24.dp), horizontal = true, frame = { st.frames.longValue })
                            VoLumMeter({ st.level(false) }, "OUT", Modifier.fillMaxWidth().height(24.dp), out = true, horizontal = true, frame = { st.frames.longValue })
                        }
                    }
                }
            }
            PathCard(s, Modifier.width(300.dp).fillMaxHeight())
        }
    }
}

/** The audio path at a glance, one lamp per link: teal when it works, red where it breaks. */
@Composable
private fun PathCard(s: VoLumState, modifier: Modifier) {
    val st = LocalStage.current
    val a = s.audio
    val input = a.inputs.firstOrNull { it.id == a.inputId }
    val output = a.outputs.firstOrNull { it.id == a.outputId }
    val values = rememberFitGroup()
    Card(modifier, "WHAT VOLUM SEES") {
        val mic = when (a.permission) {
            Permission.GRANTED -> "allowed" to VoLumColors.Teal
            Permission.DENIED -> "not allowed yet" to VoLumColors.Danger
            else -> "blocked in Android settings" to VoLumColors.Danger
        }
        PathLine("MICROPHONE ACCESS", mic.first, mic.second, values)
        val usb = when (a.status) {
            InterfaceStatus.OK -> (input?.name ?: "none") to VoLumColors.Teal
            InterfaceStatus.NO_INTERFACE -> "none connected" to VoLumColors.Danger
            InterfaceStatus.UNPLUGGED -> "unplugged" to VoLumColors.Danger
        }
        PathLine("INPUT", usb.first, usb.second, values)
        PathLine("OUTPUT", if (a.status == InterfaceStatus.UNPLUGGED) "muted" else (output?.name ?: "none"),
            if (a.status == InterfaceStatus.UNPLUGGED) VoLumColors.Amber else VoLumColors.Teal, values)
        PathLine("AUDIO", "${a.sampleRate / 1000.0} kHz · ${a.bufferFrames} samples · in ${a.inputChannel + 1}", VoLumColors.TextDim, values)
        Box(Modifier.weight(1f))
        TextPad("AUDIO & MIDI DEVICES  >", { st.go(Action.Open(Overlay.AudioDevices)) }, Modifier.fillMaxWidth().height(52.dp), arrayOf("signal.audio_devices"))
    }
}

@Composable
private fun ColumnScope.PathLine(caption: String, value: String, ink: Color, values: FitGroup) {
    Column(Modifier.fillMaxWidth()) {
        FitText(caption, VoLumType.SectionHeader, Modifier.fillMaxWidth().height(15.dp), align = TextAlign.Start)
        Row(Modifier.fillMaxWidth().height(22.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            VoLumLed(true, color = ink, animate = false)
            FitText(value, VoLumType.label(12.sp, if (ink == VoLumColors.TextDim) VoLumColors.Cream else ink), Modifier.weight(1f).height(20.dp), align = TextAlign.Start, group = values)
        }
    }
}

@Composable
private fun Problem(modifier: Modifier, id: String, title: String, body: List<String>, actions: @Composable RowScope.() -> Unit) {
    Column(
        modifier.panelDepth(4.dp).hairlineFrame(VoLumColors.Danger.withOpacity(0.55f), radius = 4.dp).cornerBrackets(VoLumColors.Danger.withOpacity(0.8f), 8.dp, 4.dp)
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        Column(Modifier.fillMaxWidth().featureId(id), verticalArrangement = Arrangement.spacedBy(4.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                WarnGlyph(VoLumColors.Danger)
                FitText(title, VoLumType.display(24.sp, VoLumColors.TextBright), Modifier.weight(1f).height(32.dp), align = TextAlign.Start)
            }
            val lines = rememberFitGroup()
            body.forEach { FitText(it, hint(VoLumColors.Cream, 13.sp), Modifier.fillMaxWidth().height(20.dp), align = TextAlign.Start, group = lines) }
        }
        Box(Modifier.weight(1f))
        Row(Modifier.fillMaxWidth().height(64.dp), horizontalArrangement = Arrangement.spacedBy(10.dp)) { actions() }
    }
}