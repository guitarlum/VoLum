package com.volum.ui.a

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.isTraversalGroup
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.traversalIndex
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.InterfaceStatus
import com.volum.core.NamePurpose
import com.volum.core.Overlay
import com.volum.core.Permission
import com.volum.core.Reducer
import com.volum.core.SettingsTab
import com.volum.core.View
import com.volum.core.VoLumState
import com.volum.design.components.VoLumBuildTip
import com.volum.design.components.VoLumButton
import com.volum.design.components.VoLumHeaderCluster
import com.volum.design.components.VoLumLed
import com.volum.design.components.VoLumLogo
import com.volum.design.components.VoLumPresetBar
import com.volum.design.components.hairlineFrame
import com.volum.design.components.volumHeaderPlate
import com.volum.design.theme.VFitText
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumType
import com.volum.design.theme.withOpacity

internal val HeaderH = 48.dp

/**
 * One header for both views, the desktop's: logo; the preset bar in BUILD (the
 * first-run "Tweak your sound" pointer in PLAY); SAVE; the input status chip; and
 * the PLAY / BUILD toggle, tuner, metronome and gear. It is composed after the
 * surface below it so the surface's controls come first in the tree; TalkBack
 * still reads it first.
 */
@Composable
internal fun Header(c: Ctx) {
    val s = c.s
    val inPlay = s.ui.view == View.PLAY
    Row(
        Modifier.fillMaxWidth().height(HeaderH).volumHeaderPlate().padding(start = 14.dp, end = 4.dp)
            .semantics { isTraversalGroup = true; traversalIndex = -1f },
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        VoLumLogo(compact = true)
        Fill()
        if (!inPlay) {
            val bank = s.library.presetsFor(s.ampId)
            VoLumPresetBar(
                s.presetName, s.rig.dirty,
                onOpen = { c.d(Action.Open(Overlay.PresetMenu)) },
                onPrev = { c.d(Action.StepPreset(-1)) },
                onNext = { c.d(Action.StepPreset(1)) },
                modifier = Modifier.width(276.dp),
                canStep = bank.isNotEmpty(),
                openModifier = Modifier.ids(listOfNotNull("header.preset.menu", "header.preset_name".takeIf { s.presetName != null }, "header.unsaved".takeIf { s.rig.dirty })),
                prevModifier = Modifier.ids("header.preset.prev"),
                nextModifier = Modifier.ids("header.preset.next"),
            )
            Fill()
        }
        VoLumButton("Save", { c.d(Action.Open(Overlay.NameDialog(NamePurpose.SAVE_PRESET, Reducer.nameSuggestion(s)))) }, Modifier.ids(if (inPlay) "play.save" else "presets.save.open"))
        InputChip(c)
        if (inPlay && !s.ui.buildTipSeen) VoLumBuildTip({ c.d(Action.SetView(View.BUILD)) }, Modifier.ids("tip.build"))
        VoLumHeaderCluster(
            inPlay,
            onSwitchView = { c.d(Action.SetView(if (inPlay) View.BUILD else View.PLAY)) },
            onTuner = { c.d(Action.Open(Overlay.Tuner)) },
            onMetronome = { c.d(Action.Open(Overlay.Metronome)) },
            onSettings = { c.d(Action.Open(Overlay.Settings(SettingsTab.SIGNAL))) },
            metronomeOn = s.metronome.on,
            updateBadge = s.update.available != null,
            viewModifier = Modifier.ids(if (inPlay) "nav.view.build" else "nav.view.play"),
            tunerModifier = Modifier.ids("nav.tuner.open"),
            metronomeModifier = Modifier.ids("nav.metronome.open"),
            settingsModifier = Modifier.ids(listOfNotNull("nav.settings.open", "system.update.badge".takeIf { s.update.available != null })),
        )
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

/** What feeds VoLum, at a glance: a teal lamp and the source, or a red lamp and the problem. A tap opens the input panel. */
@Composable
private fun InputChip(c: Ctx) {
    val live = inputLive(c.s)
    val ink = if (live) VoLumColors.Cream else VoLumColors.Danger
    Row(
        Modifier.ids("nav.audio_status.open", "audio.input.indicator").heightIn(min = 48.dp).widthIn(min = 96.dp)
            .clickable(role = Role.Button, onClickLabel = "Input status") { c.d(Action.Open(Overlay.AudioStatus)) }
            .padding(vertical = 10.dp)
            .hairlineFrame(if (live) VoLumColors.BtnOffBorder else VoLumColors.Danger.withOpacity(0.6f), radius = 3.dp)
            .padding(horizontal = 8.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(4.dp),
    ) {
        VoLumLed(true, color = if (live) VoLumColors.Teal else VoLumColors.Danger)
        VFitText(inputSummary(c.s), style = VoLumType.label(11.sp, ink), minSize = 8.sp)
    }
}
