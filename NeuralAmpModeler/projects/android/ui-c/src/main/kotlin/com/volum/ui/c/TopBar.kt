package com.volum.ui.c

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.text.BasicText
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import com.volum.core.Action
import com.volum.core.InterfaceStatus
import com.volum.core.NamePurpose
import com.volum.core.Overlay
import com.volum.core.Permission
import com.volum.core.Reducer
import com.volum.core.SettingsTab
import com.volum.core.View

/** Global chrome: view switch, BUILD's preset bar, input state and the tools. Always above every sheet. */
@Composable
internal fun TopBar(ui: Ui) {
    val s = ui.s
    val build = s.ui.view == View.BUILD
    Row(
        Modifier
            .fillMaxWidth()
            .height(Metrics.top)
            .background(Brush.verticalGradient(listOf(Look.ground, Look.ground.copy(alpha = 0.75f), Color.Transparent)))
            .padding(horizontal = 8.dp, vertical = 2.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        BasicText("VoLum", style = displayStyle(22f, cap = 1f), modifier = Modifier.padding(horizontal = 4.dp))
        Choice("PLAY", !build, listOf("nav.view.play"), { ui.d(Action.SetView(View.PLAY)) })
        Choice("BUILD", build, listOf("nav.view.build"), { ui.d(Action.SetView(View.BUILD)) })
        if (build) PresetBar(ui, Modifier.weight(1f)) else Fill()
        InputIndicator(ui)
        Btn("TUNER", listOf("nav.tuner.open"), { ui.d(Action.Open(Overlay.Tuner)) }, strong = s.tunerOpen, cap = 1f, pad = 10.dp)
        Btn("METRO", listOf("nav.metronome.open"), { ui.d(Action.Open(Overlay.Metronome)) }, sub = if (s.metronome.on) "%.0f BPM".format(s.metronome.bpm) else null, strong = s.ui.top is Overlay.Metronome, cap = 1f, pad = 10.dp)
        Btn("SAVE", listOf(if (build) "presets.save.open" else "play.save"), {
            ui.d(Action.Open(Overlay.NameDialog(NamePurpose.SAVE_PRESET, Reducer.nameSuggestion(s))))
        }, cap = 1f, pad = 10.dp)
        if (s.update.available != null) {
            Btn(s.update.available ?: "", listOf("system.update.badge"), { ui.d(Action.Open(Overlay.Settings(SettingsTab.SYSTEM))) }, sub = "UPDATE", strong = true, cap = 1f, pad = 8.dp)
        }
        IconBtn("Settings", listOf("nav.settings.open"), { ui.d(Action.Open(Overlay.Settings(SettingsTab.SIGNAL))) }, active = s.ui.overlays.any { it is Overlay.Settings }) { gear(it) }
    }
}

@Composable
private fun PresetBar(ui: Ui, modifier: Modifier) {
    val s = ui.s
    val name = s.presetName ?: "Default"
    Row(modifier, verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(4.dp)) {
        Btn("‹", listOf("header.preset.prev"), { ui.d(Action.StepPreset(-1)) })
        val src = remember { MutableInteractionSource() }
        val scale = pressScale(src)
        Box(
            Modifier
                .weight(1f)
                .heightIn(min = Metrics.target)
                .graphicsLayer { scaleX = scale; scaleY = scale }
                .clip(Corner)
                .background(if (s.ui.top is Overlay.PresetMenu) Look.raised else Look.glass)
                .border(1.dp, if (s.ui.top is Overlay.PresetMenu) Look.pick else Look.line, Corner)
                .clickable(src, null, role = Role.Button) { ui.d(Action.Open(Overlay.PresetMenu)) }
                .ids("header.preset.menu")
                .padding(horizontal = 10.dp),
            contentAlignment = Alignment.Center,
        ) {
            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    BasicText(name, style = displayStyle(17f), maxLines = 1, overflow = TextOverflow.Ellipsis, modifier = Modifier.weight(1f, fill = false).ids("header.preset_name"))
                    if (s.rig.dirty) BasicText("(unsaved)", style = labelStyle(10f, Look.warn), maxLines = 1, modifier = Modifier.ids("header.unsaved"))
                    BasicText("▾", style = labelStyle(10f))
                }
                BasicText(
                    (s.currentPreset?.let { if (it.factory) "FACTORY" else "USER" } ?: "NO PRESET") + " · " + (s.amp?.name ?: ""),
                    style = labelStyle(8f, Look.dim, bold = false),
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                )
            }
        }
        Btn("›", listOf("header.preset.next"), { ui.d(Action.StepPreset(1)) })
    }
}

@Composable
private fun InputIndicator(ui: Ui) {
    val a = ui.s.audio
    val (label, sub, color) = when {
        a.permission != Permission.GRANTED -> Triple("MIC OFF", "TAP TO FIX", Look.danger)
        a.status == InterfaceStatus.NO_INTERFACE -> Triple("NO USB", "TAP TO FIX", Look.danger)
        a.status == InterfaceStatus.UNPLUGGED -> Triple("UNPLUGGED", "TAP TO FIX", Look.danger)
        else -> Triple(a.inputs.firstOrNull { it.id == a.inputId }?.let { if (it.usb) "USB IN" else "PHONE MIC" } ?: "INPUT", "%d kHz · %d".format(a.sampleRate / 1000, a.bufferFrames), Look.ok)
    }
    val src = remember { MutableInteractionSource() }
    val scale = pressScale(src)
    Row(
        Modifier
            .heightIn(min = Metrics.target)
            .widthIn(min = 48.dp)
            .graphicsLayer { scaleX = scale; scaleY = scale }
            .clip(Corner)
            .background(if (color == Look.danger) Look.danger.copy(alpha = 0.18f) else Look.raised)
            .border(1.dp, if (color == Look.danger) Look.danger else Look.line, Corner)
            .clickable(src, null, role = Role.Button) { ui.d(Action.Open(Overlay.AudioStatus)) }
            .ids("nav.audio_status.open", "audio.input.indicator")
            .padding(horizontal = 10.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        Led(true, 8.dp, color)
        Column {
            BasicText(label, style = labelStyle(11f, Look.text), maxLines = 1)
            BasicText(sub, style = labelStyle(8f, Look.dim, bold = false), maxLines = 1)
        }
    }
}
