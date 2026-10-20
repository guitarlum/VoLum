package com.volum.ui.c

import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInVertically
import androidx.compose.animation.slideOutVertically
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.Overlay
import com.volum.core.ScreenId
import com.volum.core.SettingsTab
import com.volum.core.View
import com.volum.design.components.cornerBrackets
import com.volum.design.components.hairlineFrame
import com.volum.design.components.panelDepth
import com.volum.design.theme.VText
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumType
import com.volum.harness.screen

/**
 * Renders the top panel of the overlay stack where it came from:
 * - a menu opened from a BUILD sheet row rises above that row, which the sheet folds
 *   down to; a panel opened from that menu stacks on the menu's folded header;
 * - the Sound picker docks beside the Sound rail it was opened from;
 * - everything opened from the top bar hangs under its control, sized to its content.
 */
@Composable
internal fun OverlayHost(ui: Ui, sheet: Overlay?) {
    BoxWithConstraints(Modifier.fillMaxSize()) {
        val room = maxHeight - Metrics.top
        AnimatedContent(
            sheet,
            contentKey = { it?.let { o -> o::class } },
            transitionSpec = {
                val rising = (targetState ?: initialState)?.let { ui.s.folded() || it is Overlay.SoundPicker } ?: true
                val dir = if (rising) 1 else -1
                (slideInVertically(tween(260)) { it / 4 * dir } + fadeIn(tween(220))) togetherWith (slideOutVertically(tween(200)) { it / 5 * dir } + fadeOut(tween(160)))
            },
            label = "panel",
        ) { o -> if (o != null) Panel(ui, o, room) }
    }
}

@Composable
private fun Panel(ui: Ui, o: Overlay, room: Dp) {
    val s = ui.s
    val chainRoot = s.ui.overlays.firstOrNull()?.takeIf { s.folded() }
    when {
        o is Overlay.Tuner -> Box(Modifier.fillMaxSize().padding(top = Metrics.top).screen(ScreenId.TUNER)) { Tuner(ui, Modifier.fillMaxSize()) }
        chainRoot != null && o.fromSheet -> Box(Modifier.fillMaxSize().padding(top = Metrics.top + 4.dp, bottom = Metrics.tray + 10.dp, start = 8.dp, end = 8.dp), contentAlignment = Alignment.BottomCenter) {
            Body(ui, o, Modifier.fillMaxWidth().heightIn(max = room - Metrics.tray - 14.dp).screen(screenOf(o)))
        }
        chainRoot != null -> Column(
            Modifier.fillMaxSize().padding(top = Metrics.top + 4.dp, bottom = 6.dp, start = 8.dp, end = 8.dp).screen(screenOf(o)),
            verticalArrangement = Arrangement.spacedBy(4.dp, Alignment.Bottom),
        ) {
            Body(ui, o, Modifier.fillMaxWidth().heightIn(max = room - Metrics.pickerHead - 18.dp))
            PickerHead(ui, chainRoot, o)
        }
        o is Overlay.SoundPicker && s.ui.view == View.PLAY -> Box(Modifier.fillMaxSize().padding(top = Metrics.top + 6.dp, end = Metrics.rail + 14.dp, start = 8.dp, bottom = 8.dp), contentAlignment = Alignment.TopEnd) {
            Body(ui, o, Modifier.fillMaxWidth().screen(screenOf(o)))
        }
        else -> {
            val (width, align) = hang(o, s.ui.view == View.BUILD)
            Box(Modifier.fillMaxSize().padding(top = Metrics.top + 4.dp, start = 8.dp, end = 8.dp, bottom = 6.dp), contentAlignment = align) {
                Body(ui, o, Modifier.width(width).heightIn(max = room - 10.dp).screen(screenOf(o)))
            }
        }
    }
}

/** Where a top-bar panel hangs: under the preset bar, the input chip, or the tool cluster. */
private fun hang(o: Overlay, build: Boolean): Pair<Dp, Alignment> = when (o) {
    is Overlay.PresetMenu, is Overlay.PresetManage -> 760.dp to if (build) Alignment.TopCenter else Alignment.TopEnd
    is Overlay.Metronome -> 790.dp to Alignment.TopEnd
    is Overlay.AudioStatus -> 600.dp to Alignment.TopEnd
    else -> 898.dp to Alignment.TopEnd
}

@Composable
private fun Body(ui: Ui, o: Overlay, m: Modifier) {
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
        is Overlay.Tuner -> Unit
        is Overlay.Metronome -> Metronome(ui, m)
        is Overlay.Settings -> Settings(ui, o, m)
        is Overlay.AudioDevices -> AudioDevices(ui, m)
        is Overlay.AudioStatus -> AudioStatus(ui, m)
        is Overlay.PackExport -> PackExport(ui, m)
        is Overlay.PackImport -> PackImport(ui, m)
        is Overlay.NameDialog -> Unit
    }
}

/**
 * A picker folded to its header while a panel opened from it is up: the control
 * that opened the panel stays in view, lit, and a tap on it closes the panel.
 */
@Composable
private fun PickerHead(ui: Ui, root: Overlay, top: Overlay) {
    val s = ui.s
    Row(
        Modifier.fillMaxWidth().height(Metrics.pickerHead).panelDepth(6.dp).hairlineFrame(VoLumColors.Frame, radius = 6.dp).cornerBrackets(VoLumColors.Corner, 8.dp, 4.dp)
            .padding(horizontal = 18.dp, vertical = 6.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        val (title, label, id) = when (root) {
            is Overlay.AmpPicker -> Triple("Choose amp", if (s.builder.editAmpId != null) "Editing " + (s.library.amp(s.builder.editAmpId)?.name ?: "amp") else "+ Add your amp", "custom.amp.add")
            is Overlay.IrMenu -> Triple("Custom IR", "Manage IRs", "ir.menu.manage")
            is Overlay.CaptureMenu -> Triple("NAM ${root.slot} capture", "Manage pedals", "pre.capture.manage")
            else -> Triple("Support amp", "", "")
        }
        VText(title, style = VoLumType.display(20.sp, VoLumColors.Gold))
        Fill()
        VText("OPEN", style = VoLumType.label(9.sp, VoLumColors.Amber))
        if (id.isNotEmpty()) Btn(label, listOf(id), { ui.d(Action.CloseOverlay(top)) }, primary = true)
    }
}

internal fun screenOf(o: Overlay): ScreenId = when (o) {
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
        SettingsTab.SIGNAL -> ScreenId.SETTINGS_SIGNAL
        SettingsTab.MIDI -> ScreenId.SETTINGS_MIDI
        SettingsTab.SYSTEM -> ScreenId.SETTINGS_SYSTEM
    }
    is Overlay.AudioDevices -> ScreenId.SETTINGS_AUDIO
    is Overlay.AudioStatus -> ScreenId.AUDIO_STATUS
    is Overlay.PackExport -> ScreenId.PACK_EXPORT
    is Overlay.PackImport -> ScreenId.PACK_IMPORT
    is Overlay.SoundPicker -> ScreenId.PLAY_PICKER
}

internal val back: Ui.() -> Unit = { d(Action.Back) }
