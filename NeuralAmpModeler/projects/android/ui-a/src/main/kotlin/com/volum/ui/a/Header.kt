package com.volum.ui.a

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.InterfaceStatus
import com.volum.core.Overlay
import com.volum.core.Permission
import com.volum.core.SettingsTab
import com.volum.core.View
import com.volum.core.VoLumState

/** One header for both views: logo, PLAY / BUILD, the preset bar (BUILD), input status, tuner, metronome, settings. */
@Composable
internal fun Header(c: Ctx) {
    val s = c.s
    Row(
        Modifier.fillMaxWidth().height(54.dp).padding(horizontal = 8.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        T("VoLum", Type.display(20.sp), Modifier.width(66.dp))
        Chip("PLAY", { c.d(Action.SetView(View.PLAY)) }, Modifier.width(68.dp), listOf("nav.view.play"), selected = s.ui.view == View.PLAY)
        Chip("BUILD", { c.d(Action.SetView(View.BUILD)) }, Modifier.width(72.dp), listOf("nav.view.build"), selected = s.ui.view == View.BUILD)
        if (s.ui.view == View.BUILD) PresetBar(c, Modifier.weight(1f)) else Spacer(Modifier.weight(1f))
        InputChip(c)
        IconTap(Glyph.TUNER, { c.d(Action.Open(Overlay.Tuner)) }, ids = listOf("nav.tuner.open"), desc = "Tuner")
        IconTap(Glyph.METRONOME, { c.d(Action.Open(Overlay.Metronome)) }, ids = listOf("nav.metronome.open"), desc = "Metronome")
        Tap({ c.d(Action.Open(Overlay.Settings(SettingsTab.SIGNAL))) }, Modifier.semanticsDesc("Settings"), listOf("nav.settings.open")) {
            GlyphIcon(Glyph.GEAR, Modifier.padding(12.dp))
            if (s.update.available != null) Led(true, Modifier.align(Alignment.TopEnd).size(10.dp).ids("system.update.badge"))
        }
    }
}

@Composable
private fun PresetBar(c: Ctx, modifier: Modifier) {
    val s = c.s
    Row(modifier, verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(4.dp)) {
        IconTap(Glyph.PREV, { c.d(Action.StepPreset(-1)) }, ids = listOf("header.preset.prev"), desc = "Previous preset")
        Tap({ c.d(Action.Open(Overlay.PresetMenu)) }, Modifier.weight(1f), listOf("header.preset.menu")) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                T(s.presetName ?: "No preset", Type.value(14.sp), Modifier.weight(1f, fill = false).then(if (s.presetName != null) Modifier.ids("header.preset_name") else Modifier))
                if (s.rig.dirty) T("  UNSAVED", Type.label(10.sp, Ink.dim), Modifier.ids("header.unsaved"))
            }
        }
        IconTap(Glyph.NEXT, { c.d(Action.StepPreset(1)) }, ids = listOf("header.preset.next"), desc = "Next preset")
        Chip("SAVE", { c.d(Action.Open(Overlay.NameDialog(com.volum.core.NamePurpose.SAVE_PRESET, com.volum.core.Reducer.nameSuggestion(s)))) },
            Modifier.width(64.dp), listOf("presets.save.open"))
    }
}

internal fun inputLive(s: VoLumState) = s.audio.permission == Permission.GRANTED && s.audio.status == InterfaceStatus.OK

internal fun inputSummary(s: VoLumState): String = when {
    s.audio.permission != Permission.GRANTED -> "NO MIC"
    s.audio.status == InterfaceStatus.NO_INTERFACE -> "NO USB"
    s.audio.status == InterfaceStatus.UNPLUGGED -> "UNPLUGGED"
    s.audio.inputs.firstOrNull { it.id == s.audio.inputId }?.usb == true -> "USB IN"
    else -> "PHONE MIC"
}

@Composable
private fun InputChip(c: Ctx) {
    val live = inputLive(c.s)
    Tap({ c.d(Action.Open(Overlay.AudioStatus)) }, Modifier.width(104.dp), listOf("nav.audio_status.open")) {
        Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.ids("audio.input.indicator")) {
            Box(Modifier.size(12.dp)) { Led(live) }
            Spacer(Modifier.width(6.dp))
            T(inputSummary(c.s), Type.label(11.sp, if (live) Ink.text else Ink.alert))
        }
    }
}
