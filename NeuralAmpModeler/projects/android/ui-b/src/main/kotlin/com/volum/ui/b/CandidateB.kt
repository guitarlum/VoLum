package com.volum.ui.b

import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.scaleIn
import androidx.compose.animation.slideInHorizontally
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.semantics.clearAndSetSemantics
import com.volum.core.Overlay
import com.volum.core.View
import com.volum.core.VoLumState
import com.volum.core.VoLumStore
import com.volum.harness.CandidateUi

/** Candidate B, Stage Floor: PLAY is a floor of big Sound pads and stomps; BUILD is a pedalboard you tap into. */
object CandidateB : CandidateUi {
    override val name = "B Stage Floor"

    @Composable
    override fun App(store: VoLumStore) {
        val state by store.state.collectAsState()
        val tele = store.telemetry.collectAsState()
        val stage = remember(store) { Stage(store, tele) }
        CompositionLocalProvider(LocalStage provides stage) { Root(state) }
    }
}

@Composable
private fun Root(s: VoLumState) {
    val top = s.ui.top
    val confirm = s.ui.confirm != null
    Box(Modifier.fillMaxSize().background(Ink.ground)) {
        Box(Modifier.fillMaxSize().then(if (top != null || confirm) Modifier.clearAndSetSemantics {} else Modifier)) {
            AnimatedContent(
                targetState = s.ui.view,
                transitionSpec = {
                    val dir = if (targetState == View.BUILD) 1 else -1
                    (fadeIn(tween(200)) + slideInHorizontally(tween(240)) { it / 10 * dir }) togetherWith fadeOut(tween(120))
                },
                label = "view",
            ) { v -> if (v == View.PLAY) PlaySurface(s) else BuildSurface(s) }
        }
        val under = if (top is Overlay.NameDialog) s.ui.overlays.dropLast(1).lastOrNull() else null
        if (under != null) Box(Modifier.fillMaxSize().clearAndSetSemantics {}) { OverlayContent(s, under) }
        AnimatedContent(
            targetState = top,
            contentKey = { o -> o?.let { it::class } },
            transitionSpec = { (fadeIn(tween(160)) + scaleIn(tween(180), initialScale = 0.97f)) togetherWith fadeOut(tween(110)) },
            label = "overlay",
        ) { o ->
            if (o != null) {
                val live = s.ui.top?.takeIf { it::class == o::class } ?: o
                Box(Modifier.fillMaxSize().then(if (confirm) Modifier.clearAndSetSemantics {} else Modifier)) { OverlayContent(s, live) }
            }
        }
        ConfirmLayer(s)
        BannerStrip(s)
    }
}

@Composable
private fun OverlayContent(s: VoLumState, o: Overlay) {
    when (o) {
        Overlay.AmpPicker -> AmpPicker(s)
        Overlay.IrMenu -> IrMenu(s)
        is Overlay.CaptureMenu -> CaptureMenu(s, o.slot)
        Overlay.SupportAmpMenu -> SupportAmpMenu(s)
        Overlay.PresetMenu -> PresetMenu(s)
        Overlay.PresetManage -> PresetManage(s)
        is Overlay.NameDialog -> NameDialog(s, o)
        Overlay.CustomAmpBuilder -> AmpBuilder(s)
        Overlay.IrManage -> IrManage(s)
        Overlay.PedalManage -> PedalManage(s)
        Overlay.Tuner -> Tuner(s)
        Overlay.Metronome -> Metronome(s)
        is Overlay.Settings -> SettingsPanel(s, o.tab)
        Overlay.AudioDevices -> AudioDevices(s)
        Overlay.AudioStatus -> AudioStatus(s)
        Overlay.PackExport -> PackExport(s)
        Overlay.PackImport -> PackImport(s)
        is Overlay.SoundPicker -> SoundPicker(s, o)
    }
}
