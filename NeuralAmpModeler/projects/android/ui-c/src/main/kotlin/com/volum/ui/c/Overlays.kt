package com.volum.ui.c

import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInHorizontally
import androidx.compose.animation.slideInVertically
import androidx.compose.animation.slideOutHorizontally
import androidx.compose.animation.slideOutVertically
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.Overlay
import com.volum.core.ScreenId
import com.volum.core.SettingsTab
import com.volum.core.View
import com.volum.core.VoLumState
import com.volum.design.components.cornerBrackets
import com.volum.design.components.hairlineFrame
import com.volum.design.components.panelDepth
import com.volum.design.theme.VFitText
import com.volum.design.theme.VText
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumType
import com.volum.harness.screen

/**
 * Renders the top panel of the overlay stack where it came from:
 * - a menu opened from a BUILD sheet row rises above that row, which the sheet folds
 *   down to; a panel opened from that menu stacks on the menu's folded header;
 * - the support amp list opened from the SUPPORT lane fills the MAIN half beside it;
 * - the Sound picker docks beside the Sound rail it was opened from;
 * - everything opened from the top bar drops from it across the width, as tall as
 *   its content up to the next clean edge of the view under it.
 */
@Composable
internal fun OverlayHost(ui: Ui, sheet: Overlay?) {
    BoxWithConstraints(Modifier.fillMaxSize()) {
        val screenH = maxHeight
        val screenW = maxWidth
        AnimatedContent(
            sheet,
            contentKey = { it?.let { o -> o::class } },
            transitionSpec = {
                val o = targetState ?: initialState
                if (o is Overlay.SupportAmpMenu && ui.s.supportFromLane()) {
                    (slideInHorizontally(tween(260)) { it / 6 } + fadeIn(tween(220))) togetherWith (slideOutHorizontally(tween(200)) { it / 6 } + fadeOut(tween(160)))
                } else {
                    val rising = o?.let { ui.s.folded() || it is Overlay.SoundPicker } ?: true
                    val dir = if (rising) 1 else -1
                    (slideInVertically(tween(260)) { it / 4 * dir } + fadeIn(tween(220))) togetherWith (slideOutVertically(tween(200)) { it / 5 * dir } + fadeOut(tween(160)))
                }
            },
            label = "panel",
        ) { o -> if (o != null) Panel(ui, o, screenH, screenW) }
    }
}

@Composable
private fun Panel(ui: Ui, o: Overlay, screenH: Dp, screenW: Dp) {
    val s = ui.s
    val room = screenH - Metrics.top
    val chainRoot = s.ui.overlays.firstOrNull()?.takeIf { s.folded() }
    when {
        o is Overlay.Tuner -> Box(Modifier.fillMaxSize().padding(top = Metrics.top).pointerInput(Unit) { detectTapGestures { } }.screen(ScreenId.TUNER)) {
            Tuner(ui, Modifier.fillMaxSize().padding(bottom = if (s.ui.view == View.PLAY) PlayChainH else 0.dp))
        }
        o is Overlay.SupportAmpMenu && s.supportFromLane() -> Box(Modifier.fillMaxSize().padding(top = Metrics.top + 4.dp, start = 8.dp, bottom = 6.dp)) {
            Body(ui, o, Modifier.width(laneWidth(screenW) - 4.dp).fillMaxHeight().screen(screenOf(o)))
        }
        chainRoot != null && o.fromSheet -> Box(Modifier.fillMaxSize().padding(top = Metrics.top + 4.dp, bottom = Metrics.tray + 10.dp, start = 8.dp, end = 8.dp), contentAlignment = Alignment.BottomCenter) {
            Body(ui, o, Modifier.fillMaxWidth().heightIn(max = room - Metrics.tray - 14.dp).screen(screenOf(o)))
        }
        chainRoot != null -> Column(
            Modifier.fillMaxSize().padding(top = Metrics.top + 4.dp, bottom = 6.dp, start = 8.dp, end = 8.dp).screen(screenOf(o)),
            verticalArrangement = Arrangement.Bottom,
        ) {
            Body(ui, o, Modifier.fillMaxWidth().heightIn(max = room - Metrics.pickerHead - 14.dp))
            pickerHead(s, chainRoot)?.let { FoldedHead(ui, it, o) }
        }
        parentHead(s, o) != null -> Column(Modifier.fillMaxSize().padding(top = Metrics.top + 4.dp, bottom = 6.dp, start = 8.dp, end = 8.dp), horizontalAlignment = Alignment.CenterHorizontally) {
            FoldedHead(ui, parentHead(s, o)!!, o, Modifier.widthIn(max = HangWidth))
            CompositionLocalProvider(LocalPanelFloor provides panelFloor(s.ui.view == View.BUILD, screenH, Metrics.top + 4.dp + Metrics.pickerHead)) {
                Body(ui, o, Modifier.widthIn(max = HangWidth).fillMaxWidth().heightIn(max = room - 10.dp - Metrics.pickerHead).screen(screenOf(o)))
            }
        }
        o is Overlay.SoundPicker && s.ui.view == View.PLAY -> Box(Modifier.fillMaxSize().padding(top = Metrics.top + 6.dp, end = Metrics.rail + 14.dp, start = 8.dp, bottom = 6.dp), contentAlignment = Alignment.TopEnd) {
            CompositionLocalProvider(LocalPanelFloor provides panelFloor(false, screenH, Metrics.top + 6.dp)) {
                Body(ui, o, Modifier.fillMaxWidth().screen(screenOf(o)))
            }
        }
        else -> {
            val build = s.ui.view == View.BUILD
            Box(Modifier.fillMaxSize().padding(top = Metrics.top + 4.dp, start = 8.dp, end = 8.dp, bottom = 6.dp), contentAlignment = Alignment.TopCenter) {
                CompositionLocalProvider(LocalPanelFloor provides panelFloor(build, screenH, Metrics.top + 4.dp)) {
                    Body(ui, o, Modifier.widthIn(max = HangWidth).fillMaxWidth().heightIn(max = room - 10.dp).screen(screenOf(o)))
                }
            }
        }
    }
}

/** One Dual Amp lane of the BUILD art window ([ArtWindow]: 4 dp inset, 6 dp between the lanes). */
internal fun laneWidth(screenW: Dp): Dp = (screenW - 14.dp) / 2

/**
 * Where a panel hanging from the top bar at [top] may end: never across the rows
 * that stay under it (what floats on the art steps back). In BUILD those are the
 * sheet's origin row and knob row, in PLAY the stomp chain. A panel whose floor
 * would cut one is carried down to the next clean edge.
 */
@Composable
private fun panelFloor(build: Boolean, screenH: Dp, top: Dp): (Int) -> Int {
    val d = LocalDensity.current
    return remember(build, screenH, top, d) {
        val edges = if (build) {
            val sheetTop = screenH - Metrics.sheet
            listOf(sheetTop to (sheetTop + 68.dp), (sheetTop + 68.dp) to (screenH - 6.dp))
        } else {
            listOf((screenH - PlayChainH - 4.dp) to (screenH - 6.dp))
        }
        val zones = edges.map { (from, to) -> with(d) { (from - top).roundToPx() to (to - top).roundToPx() } }
        val floor: (Int) -> Int = { h -> zones.firstOrNull { (from, to) -> h > from && h < to }?.second ?: h }
        floor
    }
}

/**
 * A top-bar panel drops from the bar across the whole width, so its sides never
 * cut a floating block or the PLAY title strip; its floor is [panelFloor]'s.
 */
private val HangWidth = 898.dp

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
 * A parent folded to its head: its title and, lit, the control that opened what is
 * on top of it. [restore] puts the parent back when the panel was opened without it
 * underneath (a deep link), so the lit control always lands where it says.
 */
private class Head(val title: String, val label: String, val id: String, val restore: Overlay? = null)

private fun pickerHead(s: VoLumState, root: Overlay): Head? = when (root) {
    is Overlay.AmpPicker -> Head("Choose amp", if (s.builder.editAmpId != null) "Editing " + (s.library.amp(s.builder.editAmpId)?.name ?: "amp") else "+ Add your amp", "custom.amp.add")
    is Overlay.IrMenu -> Head("Custom IR", "Manage IRs", "ir.menu.manage")
    is Overlay.CaptureMenu -> Head("NAM ${root.slot} capture", "Manage pedals", "pre.capture.manage")
    else -> null
}

/** A top-bar panel opened from another one: the parent folds to a head above it instead of vanishing. */
private fun parentHead(s: VoLumState, top: Overlay): Head? {
    val stack = s.ui.overlays.filter { it !is Overlay.NameDialog }
    if (top != stack.lastOrNull()) return null
    if (top is Overlay.PresetManage) return Head("Presets", "Manage", "presets.menu.manage", Overlay.PresetMenu.takeIf { it !in stack })
    val parent = stack.getOrNull(stack.size - 2) ?: return null
    if (parent.fromSheet) return null
    return when {
        parent is Overlay.Settings && top is Overlay.AudioDevices -> Head("Settings  ·  Signal", "Audio & MIDI devices", "signal.audio_devices")
        parent is Overlay.AudioStatus && top is Overlay.AudioDevices -> Head("Audio input", "Audio & MIDI devices", "signal.audio_devices")
        parent is Overlay.Settings && top is Overlay.PackExport -> Head("Settings  ·  System", "Export pack", "pack.export.open")
        parent is Overlay.Settings && top is Overlay.PackImport -> Head("Settings  ·  System", "Import pack", "pack.import.open")
        parent is Overlay.Settings && top is Overlay.SoundPicker -> Head("Settings  ·  MIDI", "Program %03d".format(top.program), "midi.slot.assign")
        parent is Overlay.PackImport && top is Overlay.PedalManage -> Head("Import pack", "Manage pedals", "pre.capture.manage")
        else -> null
    }
}

/**
 * A parent folded to its head while a panel opened from it is up: the control
 * that opened the panel stays in view, lit, and a tap on it closes the panel.
 * Pickers fold under what they opened (they rise from the sheet); top-bar panels
 * fold above it (they drop from the bar).
 */
@Composable
private fun FoldedHead(ui: Ui, head: Head, top: Overlay, modifier: Modifier = Modifier) {
    Row(
        modifier.fillMaxWidth().height(Metrics.pickerHead).panelDepth(6.dp).hairlineFrame(VoLumColors.Frame, radius = 6.dp).cornerBrackets(VoLumColors.Corner, 8.dp, 4.dp)
            .padding(horizontal = 18.dp, vertical = 6.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        VFitText(head.title, Modifier.weight(1f, fill = false), style = VoLumType.display(20.sp, VoLumColors.Gold), minSize = 13.sp)
        Fill()
        VText("OPEN", style = VoLumType.label(9.sp, VoLumColors.Amber))
        Btn(head.label, listOf(head.id), {
            ui.d(Action.CloseOverlay(top))
            head.restore?.let { ui.d(Action.Open(it)) }
        }, primary = true)
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
