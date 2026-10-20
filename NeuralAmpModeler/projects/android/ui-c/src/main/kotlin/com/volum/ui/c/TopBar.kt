package com.volum.ui.c

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.CornerRadius
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
import com.volum.design.components.VoLumBuildTip
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

/** The desktop header on the `:design` plate: logo, BUILD's preset bar, input state, SAVE and the tool cluster. Above every panel. */
@Composable
internal fun TopBar(ui: Ui) {
    val s = ui.s
    val build = s.ui.view == View.BUILD
    Row(
        Modifier.fillMaxWidth().height(Metrics.top).volumHeaderPlate().padding(start = 14.dp, end = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        VoLumLogo(compact = true)
        Gap(w = 6.dp)
        Box(Modifier.weight(1f), contentAlignment = Alignment.Center) {
            if (build) {
                VoLumPresetBar(
                    s.presetName,
                    s.rig.dirty,
                    onOpen = { ui.d(if (s.ui.top is Overlay.PresetMenu) Action.Back else Action.Open(Overlay.PresetMenu)) },
                    onPrev = { ui.d(Action.StepPreset(-1)) },
                    onNext = { ui.d(Action.StepPreset(1)) },
                    modifier = Modifier.width(320.dp),
                    canStep = s.library.presetsFor(s.ampId).isNotEmpty(),
                    openModifier = Modifier.ids(listOfNotNull("header.preset.menu", "header.preset_name".takeIf { s.presetName != null }, "header.unsaved".takeIf { s.rig.dirty })),
                    prevModifier = Modifier.ids("header.preset.prev"),
                    nextModifier = Modifier.ids("header.preset.next"),
                )
            }
        }
        InputIndicator(ui)
        val saving = (s.ui.top as? Overlay.NameDialog)?.purpose == NamePurpose.SAVE_PRESET && Overlay.PresetMenu !in s.ui.overlays
        Btn("Save", listOf(if (build) "presets.save.open" else "play.save"), {
            ui.d(Action.Open(Overlay.NameDialog(NamePurpose.SAVE_PRESET, Reducer.nameSuggestion(s))))
        }, primary = saving)
        val update = s.update.available
        if (update != null) {
            Tappable({ ui.d(Action.Open(Overlay.Settings(SettingsTab.SYSTEM))) }, Modifier.height(Metrics.target).widthIn(min = 72.dp), listOf("system.update.badge")) { pressed ->
                Row(
                    Modifier.fillMaxHeight().padding(vertical = 10.dp)
                        .drawBehind { drawRoundRect(VoLumColors.Gold.withOpacity(if (pressed) 0.22f else 0.12f), cornerRadius = CornerRadius(4.dp.toPx())) }
                        .hairlineFrame(VoLumColors.GoldDim, radius = 4.dp)
                        .padding(horizontal = 10.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) { VFitText("$update ready", style = VoLumType.label(11.sp, VoLumColors.Gold), minSize = 8.sp) }
            }
        }
        if (!build && !s.ui.buildTipSeen) VoLumBuildTip({ ui.d(Action.SetView(View.BUILD)) }, Modifier.ids("tip.build"))
        VoLumHeaderCluster(
            inPlay = !build,
            onSwitchView = { ui.d(Action.SetView(if (build) View.PLAY else View.BUILD)) },
            onTuner = { ui.d(if (s.tunerOpen) Action.Back else Action.Open(Overlay.Tuner)) },
            onMetronome = { ui.d(if (s.ui.top is Overlay.Metronome) Action.Back else Action.Open(Overlay.Metronome)) },
            onSettings = { ui.d(Action.Open(Overlay.Settings(SettingsTab.SIGNAL))) },
            metronomeOn = s.metronome.on,
            updateBadge = update != null,
            viewModifier = Modifier.ids(if (build) "nav.view.play" else "nav.view.build"),
            tunerModifier = Modifier.ids("nav.tuner.open"),
            metronomeModifier = Modifier.ids("nav.metronome.open"),
            settingsModifier = Modifier.ids("nav.settings.open"),
        )
    }
}

/** Input state at a glance; red with TAP TO FIX when the guitar cannot be heard. */
@Composable
private fun InputIndicator(ui: Ui) {
    val a = ui.s.audio
    val bad = a.permission != Permission.GRANTED || a.status != InterfaceStatus.OK
    val (label, sub) = when {
        a.permission != Permission.GRANTED -> "MIC OFF" to "TAP TO FIX"
        a.status == InterfaceStatus.NO_INTERFACE -> "NO USB" to "TAP TO FIX"
        a.status == InterfaceStatus.UNPLUGGED -> "UNPLUGGED" to "TAP TO FIX"
        else -> (a.inputs.firstOrNull { it.id == a.inputId }?.let { if (it.usb) "USB IN" else "PHONE MIC" } ?: "INPUT") to "%d kHz  %d".format(a.sampleRate / 1000, a.bufferFrames)
    }
    val accent = if (bad) VoLumColors.Danger else VoLumColors.Teal
    Tappable({ ui.d(Action.Open(Overlay.AudioStatus)) }, Modifier.height(Metrics.target).width(118.dp), listOf("nav.audio_status.open", "audio.input.indicator")) { pressed ->
        Row(
            Modifier.fillMaxHeight().padding(vertical = 5.dp)
                .drawBehind {
                    val fill = if (bad) VoLumColors.DangerFill else if (pressed) VoLumColors.SelBg else VoLumColors.BtnOffBg
                    drawRoundRect(fill, cornerRadius = CornerRadius(4.dp.toPx()))
                }
                .hairlineFrame(if (bad) VoLumColors.Danger else if (pressed) VoLumColors.GoldDim else VoLumColors.BtnOffBorder, radius = 4.dp)
                .padding(horizontal = 6.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            VoLumLed(true, color = accent)
            Column(Modifier.padding(start = 2.dp)) {
                VFitText(label, style = VoLumType.label(11.sp, VoLumColors.TextBright), minSize = 8.sp)
                VFitText(sub, style = VoLumType.label(8.sp, if (bad) VoLumColors.Danger else VoLumColors.TealDim), minSize = 6.sp)
            }
        }
    }
}
