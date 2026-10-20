package com.volum.ui.a

import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.Crossfade
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.scaleIn
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.runtime.Composable
import androidx.compose.runtime.State
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.semantics.clearAndSetSemantics
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.BannerKind
import com.volum.core.ConfirmKind
import com.volum.core.Overlay
import com.volum.core.ScreenId
import com.volum.core.ScreenRegistry
import com.volum.core.Section
import com.volum.core.Telemetry
import com.volum.core.View
import com.volum.core.VoLumState
import com.volum.core.VoLumStore
import com.volum.harness.CandidateUi
import com.volum.harness.screen

/** What every surface of candidate A reads: the domain state, the one way to change it, and per-frame telemetry. */
internal class Ctx(val s: VoLumState, val d: (Action) -> Unit, val tel: State<Telemetry>) {
    fun energy(): Float = tel.value.energy.toFloat()
    fun inLevel(): Float = tel.value.meter(tel.value.inDb).toFloat()
    fun outLevel(): Float = tel.value.meter(tel.value.outDb).toFloat()
}

/**
 * Candidate A, Signal Flow. BUILD keeps the desktop's PRE / AMP / POST chain strip on
 * screen at all times and opens the focused section or block in an editor below it,
 * whose knob row swaps in place. PLAY is the second surface: hero art, Sounds, stomps.
 * Every panel is modal: while one is open, the surfaces under it leave the semantics
 * tree, the way TalkBack and the harness should see a modal.
 */
object CandidateA : CandidateUi {
    override val name = "A Signal Flow"

    @Composable
    override fun App(store: VoLumStore) {
        val s by store.state.collectAsState()
        val tel = store.telemetry.collectAsState()
        val c = Ctx(s, store::dispatch, tel)
        val modal = s.ui.overlays.isNotEmpty() || s.ui.confirm != null
        Box(Modifier.fillMaxSize().background(Ink.bg)) {
            Column(Modifier.fillMaxSize().then(if (modal) Modifier.clearAndSetSemantics { } else Modifier.screen(baseScreen(s)))) {
                Header(c)
                Crossfade(s.ui.view, Modifier.weight(1f), tween(180), label = "view") { view ->
                    when (view) {
                        View.PLAY -> PlaySurface(c)
                        View.BUILD -> BuildSurface(c)
                    }
                }
            }
            OverlayLayer(c)
            s.ui.confirm?.let { ConfirmLayer(c) }
            s.ui.banner?.let { BannerLayer(c) }
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
        transitionSpec = { (fadeIn(tween(160)) + scaleIn(tween(160), initialScale = 0.97f)) togetherWith fadeOut(tween(110)) },
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
    Box(Modifier.fillMaxSize().background(Ink.scrim).pointerInput(Unit) { detectTapGestures { onTap() } })
}

internal fun Modifier.swallowTaps(): Modifier = pointerInput(Unit) { detectTapGestures { } }

/** The panel every overlay is built on: title, optional header actions, close, content. */
@Composable
internal fun Sheet(
    title: String,
    closeIds: List<String>,
    c: Ctx,
    width: Dp = 880.dp,
    align: Alignment = Alignment.Center,
    fullHeight: Boolean = true,
    actions: @Composable RowScope.() -> Unit = {},
    content: @Composable ColumnScope.() -> Unit,
) {
    Box(Modifier.fillMaxSize().padding(6.dp), contentAlignment = align) {
        Frame(Modifier.width(width).then(if (fullHeight) Modifier.fillMaxHeight() else Modifier).swallowTaps(), framed = true) {
            Column(Modifier.padding(horizontal = 10.dp, vertical = 4.dp)) {
                Row(Modifier.fillMaxWidth().height(52.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    T(title, Type.display(20.sp), Modifier.weight(1f))
                    actions()
                    IconTap(Glyph.CLOSE, { c.d(Action.Back) }, ids = closeIds, desc = "Close")
                }
                content()
            }
        }
    }
}

@Composable
private fun ConfirmLayer(c: Ctx) {
    val confirm = c.s.ui.confirm ?: return
    Box(Modifier.fillMaxSize()) {
        Scrim { c.d(Action.CancelConfirm) }
        Box(Modifier.fillMaxSize().screen(c.s.screen()), contentAlignment = Alignment.Center) {
            Frame(Modifier.width(420.dp).swallowTaps(), framed = true) {
                Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(14.dp)) {
                    T(confirm.message, Type.display(18.sp), Modifier.ids("dialog.confirm.message"), maxLines = 3)
                    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(10.dp, Alignment.End)) {
                        Chip("CANCEL", { c.d(Action.CancelConfirm) }, ids = listOf("dialog.confirm.cancel"))
                        Chip(acceptLabel(confirm.kind), { c.d(Action.AcceptConfirm) }, ids = listOf("dialog.confirm.accept"), selected = true)
                    }
                }
            }
        }
    }
}

private fun acceptLabel(k: ConfirmKind) = when (k) {
    ConfirmKind.OVERWRITE_PRESET -> "OVERWRITE"
    else -> "DELETE"
}

@Composable
private fun BannerLayer(c: Ctx) {
    val banner = c.s.ui.banner ?: return
    val error = banner.kind == BannerKind.ERROR
    Box(Modifier.fillMaxSize().padding(top = 56.dp), contentAlignment = Alignment.TopCenter) {
        Frame(Modifier.width(620.dp).swallowTaps(), framed = true) {
            Row(Modifier.padding(horizontal = 12.dp, vertical = 4.dp), verticalAlignment = Alignment.CenterVertically) {
                T(banner.message, Type.value(13.sp, if (error) Ink.alert else Ink.text),
                    Modifier.weight(1f).then(if (error) Modifier.ids("banner.error") else Modifier), maxLines = 2)
                Spacer(Modifier.width(8.dp))
                Chip("DISMISS", { c.d(Action.DismissBanner) }, ids = listOf("banner.dismiss"))
            }
        }
    }
}
