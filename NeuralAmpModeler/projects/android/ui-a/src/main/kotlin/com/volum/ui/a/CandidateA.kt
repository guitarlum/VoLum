package com.volum.ui.a

import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.core.FastOutSlowInEasing
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.scaleIn
import androidx.compose.animation.slideInVertically
import androidx.compose.animation.slideOutVertically
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.runtime.Composable
import androidx.compose.runtime.State
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.semantics.clearAndSetSemantics
import com.volum.core.Action
import com.volum.core.ConfirmKind
import com.volum.core.Overlay
import com.volum.core.ScreenId
import com.volum.core.ScreenRegistry
import com.volum.core.Section
import com.volum.core.Telemetry
import com.volum.core.View
import com.volum.core.VoLumState
import com.volum.core.VoLumStore
import com.volum.design.components.VoLumConfirmDialog
import com.volum.design.components.volumCanvas
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumMotion
import com.volum.design.theme.VoLumTheme
import com.volum.harness.CandidateUi
import com.volum.harness.screen

/** What every surface of candidate A reads: the domain state, the one way to change it, and per-frame telemetry. */
internal class Ctx(val s: VoLumState, val d: (Action) -> Unit, val tel: State<Telemetry>) {
    fun inLevel(): Float = tel.value.meter(tel.value.inDb).toFloat()
    fun outLevel(): Float = tel.value.meter(tel.value.outDb).toFloat()
}

/** PLAY to BUILD and back: the new surface rises a little as it fades in, so the switch reads as a move, not a cut. */
private const val ViewMs = 300

/**
 * Candidate A, Signal Flow. BUILD keeps the desktop's PRE / AMP / POST chain strip on
 * screen at all times and opens the focused section or block in an editor below it,
 * whose knob row swaps in place. PLAY is the second surface: the moving stage, the
 * Sound banner, the stomps and the Sounds list. Every panel is modal: while one is
 * open, the surfaces under it leave the semantics tree.
 */
object CandidateA : CandidateUi {
    override val name = "A Signal Flow"

    @Composable
    override fun App(store: VoLumStore) {
        val s by store.state.collectAsState()
        val tel = store.telemetry.collectAsState()
        val c = Ctx(s, store::dispatch, tel)
        val modal = s.ui.overlays.isNotEmpty() || s.ui.confirm != null
        VoLumTheme {
            Box(Modifier.fillMaxSize().volumCanvas()) {
                Box(Modifier.fillMaxSize().then(if (modal) Modifier.clearAndSetSemantics { } else Modifier.screen(baseScreen(s)))) {
                    AnimatedContent(
                        s.ui.view,
                        Modifier.fillMaxSize().padding(top = HeaderH),
                        transitionSpec = {
                            val rise = if (targetState == View.BUILD) 1 else -1
                            (fadeIn(tween(ViewMs, easing = FastOutSlowInEasing)) + slideInVertically(tween(ViewMs, easing = FastOutSlowInEasing)) { rise * it / 14 }) togetherWith
                                (fadeOut(tween(ViewMs / 2)) + slideOutVertically(tween(ViewMs)) { -rise * it / 28 })
                        },
                        label = "view",
                    ) { view ->
                        when (view) {
                            View.PLAY -> PlaySurface(c)
                            View.BUILD -> BuildSurface(c)
                        }
                    }
                    Header(c)
                }
                OverlayLayer(c)
                s.ui.confirm?.let { ConfirmLayer(c) }
            }
        }
    }
}

private fun baseScreen(s: VoLumState): ScreenId = when (s.ui.view) {
    View.PLAY -> ScreenId.PLAY
    View.BUILD -> when (s.ui.section) {
        Section.PRE -> ScreenId.BUILD_PRE
        Section.AMP -> ScreenId.BUILD_AMP
        Section.POST -> ScreenId.BUILD_POST
    }
}

@Composable
private fun OverlayLayer(c: Ctx) {
    AnimatedContent(
        c.s.ui.top,
        transitionSpec = {
            (fadeIn(tween(VoLumMotion.PanelMs)) + scaleIn(tween(VoLumMotion.PanelMs), initialScale = 0.97f)) togetherWith fadeOut(tween(VoLumMotion.PanelMs * 2 / 3))
        },
        contentKey = { it?.let { o -> o::class } },
        label = "overlay",
    ) { o ->
        if (o == null) return@AnimatedContent
        val screen = ScreenRegistry.screenOf(c.s.copy(ui = c.s.ui.copy(overlays = listOf(o))))
        Box(Modifier.fillMaxSize()) {
            Scrim { c.d(Action.Back) }
            Box(Modifier.fillMaxSize().then(if (c.s.ui.confirm != null) Modifier.clearAndSetSemantics { } else Modifier.screen(screen))) {
                OverlayContent(o, c)
            }
        }
    }
}

@Composable
private fun OverlayContent(o: Overlay, c: Ctx) = when (o) {
    is Overlay.AmpPicker -> AmpPicker(c)
    is Overlay.IrMenu -> IrMenu(c)
    is Overlay.CaptureMenu -> CaptureMenu(c, o.slot)
    is Overlay.SupportAmpMenu -> SupportAmpMenu(c)
    is Overlay.PresetMenu -> PresetMenu(c)
    is Overlay.PresetManage -> PresetManage(c)
    is Overlay.NameDialog -> NameDialog(c, o)
    is Overlay.CustomAmpBuilder -> AmpBuilder(c)
    is Overlay.IrManage -> IrManage(c)
    is Overlay.PedalManage -> PedalManage(c)
    is Overlay.Tuner -> Tuner(c)
    is Overlay.Metronome -> Metronome(c)
    is Overlay.Settings -> Settings(c, o.tab)
    is Overlay.AudioDevices -> AudioDevices(c)
    is Overlay.AudioStatus -> AudioStatus(c)
    is Overlay.PackExport -> PackExport(c)
    is Overlay.PackImport -> PackImport(c)
    is Overlay.SoundPicker -> SoundPicker(c, o)
}

/** Tap outside a panel goes Back; not a semantics click, so the modal stays the only operable layer. */
@Composable
private fun Scrim(onTap: () -> Unit) {
    Box(Modifier.fillMaxSize().background(VoLumColors.Scrim).pointerInput(Unit) { detectTapGestures { onTap() } })
}

@Composable
private fun ConfirmLayer(c: Ctx) {
    val confirm = c.s.ui.confirm ?: return
    val (title, label) = when (confirm.kind) {
        ConfirmKind.DELETE_PRESET -> "Delete preset" to "Delete"
        ConfirmKind.OVERWRITE_PRESET -> "Overwrite preset" to "Overwrite"
        ConfirmKind.DELETE_AMP -> "Delete custom amp" to "Delete"
        ConfirmKind.DELETE_IR -> "Delete IR" to "Delete"
        ConfirmKind.DELETE_PEDAL -> "Delete pedal" to "Delete"
    }
    Box(Modifier.fillMaxSize().screen(c.s.screen())) {
        VoLumConfirmDialog(
            title, confirm.message, label,
            onConfirm = { c.d(Action.AcceptConfirm) },
            onCancel = { c.d(Action.CancelConfirm) },
            modifier = Modifier.ids("dialog.confirm.message"),
            confirmModifier = Modifier.ids("dialog.confirm.accept"),
            cancelModifier = Modifier.ids("dialog.confirm.cancel"),
        )
    }
}
