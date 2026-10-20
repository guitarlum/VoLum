package com.volum.ui.c

import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInVertically
import androidx.compose.animation.slideOutVertically
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.volum.core.Action
import com.volum.core.Overlay
import com.volum.core.ScreenId
import com.volum.core.View
import com.volum.harness.screen

/**
 * Renders the top panel of the overlay stack. Panels rise to [Metrics.sheetTop] so the
 * top bar and the block row (BUILD) or the current Sound (PLAY) stay readable; the
 * Sound picker docks right of the Sound rail so the row it was opened from stays in view.
 */
@Composable
internal fun OverlayHost(ui: Ui, sheet: Overlay?) {
    Box(Modifier.fillMaxSize()) {
        if (sheet != null && sheet.blockLevel) {
            val src = remember { MutableInteractionSource() }
            Box(
                Modifier
                    .fillMaxWidth()
                    .padding(top = Metrics.top)
                    .height(Metrics.band)
                    .background(Look.ground.copy(alpha = 0.35f))
                    .clickable(src, null) { ui.d(Action.Back) },
            )
        }
        AnimatedContent(
            sheet,
            contentKey = { it?.let { o -> o::class } },
            transitionSpec = {
                val fromBlocks = targetState?.blockLevel ?: initialState?.blockLevel ?: true
                val dir = if (fromBlocks) 1 else -1
                (slideInVertically(tween(260)) { it / 3 * dir } + fadeIn(tween(200))) togetherWith (slideOutVertically(tween(180)) { it / 4 * dir } + fadeOut(tween(150)))
            },
            label = "panel",
        ) { o ->
            if (o != null) Panel(ui, o)
        }
    }
}

@Composable
private fun Panel(ui: Ui, o: Overlay) {
    val play = ui.s.ui.view == View.PLAY
    val area = when {
        o is Overlay.Tuner -> Modifier.fillMaxSize().padding(top = Metrics.top)
        o is Overlay.SoundPicker && play -> Modifier.fillMaxSize().padding(top = Metrics.top + 4.dp, start = 328.dp)
        o.blockLevel -> Modifier.fillMaxSize().padding(top = Metrics.sheetTop)
        else -> Modifier.fillMaxSize().padding(top = Metrics.top + 4.dp)
    }
    Box(area.screen(screenOf(o))) {
        val m = Modifier.fillMaxSize().padding(horizontal = 8.dp)
        when (o) {
            is Overlay.AmpPicker -> AmpPicker(ui, m)
            is Overlay.IrMenu -> IrMenu(ui, m)
            is Overlay.CaptureMenu -> CaptureMenu(ui, o, m)
            is Overlay.SupportAmpMenu -> SupportAmpMenu(ui, m)
            is Overlay.SoundPicker -> SoundPicker(ui, o, m)
            is Overlay.PresetMenu -> PresetMenu(ui, m)
            is Overlay.PresetManage -> PresetManage(ui, m)
            is Overlay.CustomAmpBuilder -> Builder(ui, m)
            is Overlay.IrManage -> IrManage(ui, m)
            is Overlay.PedalManage -> PedalManage(ui, m)
            is Overlay.Tuner -> Tuner(ui, Modifier.fillMaxSize())
            is Overlay.Metronome -> Metronome(ui, m)
            is Overlay.Settings -> Settings(ui, o, m)
            is Overlay.AudioDevices -> AudioDevices(ui, m)
            is Overlay.AudioStatus -> AudioStatus(ui, m)
            is Overlay.PackExport -> PackExport(ui, m)
            is Overlay.PackImport -> PackImport(ui, m)
            is Overlay.NameDialog -> Unit
        }
    }
}

private fun screenOf(o: Overlay): ScreenId = when (o) {
    is Overlay.AmpPicker -> ScreenId.AMP_PICKER
    is Overlay.IrMenu -> ScreenId.CAB_IR_MENU
    is Overlay.CaptureMenu -> ScreenId.PRE_CAPTURE_MENU
    is Overlay.SupportAmpMenu -> ScreenId.SUPPORT_AMP_MENU
    is Overlay.PresetMenu -> ScreenId.PRESETS_MENU
    is Overlay.PresetManage -> ScreenId.PRESETS_MANAGE
    is Overlay.NameDialog -> ScreenId.DIALOG_NAME
    is Overlay.CustomAmpBuilder -> ScreenId.CUSTOM_AMP_BUILDER
    is Overlay.IrManage -> ScreenId.CUSTOM_IR_MANAGE
    is Overlay.PedalManage -> ScreenId.CUSTOM_PEDAL_MANAGE
    is Overlay.Tuner -> ScreenId.TUNER
    is Overlay.Metronome -> ScreenId.METRONOME
    is Overlay.Settings -> when (o.tab) {
        com.volum.core.SettingsTab.SIGNAL -> ScreenId.SETTINGS_SIGNAL
        com.volum.core.SettingsTab.MIDI -> ScreenId.SETTINGS_MIDI
        com.volum.core.SettingsTab.SYSTEM -> ScreenId.SETTINGS_SYSTEM
    }
    is Overlay.AudioDevices -> ScreenId.SETTINGS_AUDIO
    is Overlay.AudioStatus -> ScreenId.AUDIO_STATUS
    is Overlay.PackExport -> ScreenId.PACK_EXPORT
    is Overlay.PackImport -> ScreenId.PACK_IMPORT
    is Overlay.SoundPicker -> ScreenId.PLAY_PICKER
}

internal val back: Ui.() -> Unit = { d(Action.Back) }

/** Menus opened from a block's sheet rise from the block row; everything opened from the top bar hangs from it. */
private val Overlay.blockLevel: Boolean
    get() = this is Overlay.AmpPicker || this is Overlay.IrMenu || this is Overlay.CaptureMenu || this is Overlay.SupportAmpMenu
