package com.volum.ui.c

import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.semantics.clearAndSetSemantics
import com.volum.core.Overlay
import com.volum.core.VoLumStore
import com.volum.core.View
import com.volum.harness.CandidateUi

/**
 * C, Studio Instrument: the art fills the screen and the instrument floats over it.
 * PLAY is the stage (Sound rail, the current Sound, the chain of stomps); BUILD lifts
 * a contextual sheet under the floating PRE / AMP / POST blocks. Every deeper panel
 * rises to the same line, so the top bar and the block row stay in view.
 */
object CandidateC : CandidateUi {
    override val name = "C Studio Instrument"

    @Composable
    override fun App(store: VoLumStore) {
        val s by store.state.collectAsState()
        Studio(Ui(s, store::dispatch, store.telemetry))
    }
}

@Composable
internal fun Studio(ui: Ui) {
    val s = ui.s
    val dialog = s.ui.top as? Overlay.NameDialog
    val sheet = s.ui.overlays.lastOrNull { it !is Overlay.NameDialog }
    val modal = dialog != null || s.ui.confirm != null
    val covered = sheet != null || modal
    val stepBack by animateFloatAsState(if (sheet is Overlay.Tuner) 0f else 1f, tween(240), label = "tuner")
    Box(Modifier.fillMaxSize().background(Look.ground)) {
        StageArt(ui.tel, s.amp?.art ?: 0, s.signal.animateArt, Modifier.fillMaxSize().ids(if (s.ui.view == View.PLAY) "play.art" else "amp.hero"))
        Box(Modifier.fillMaxSize().graphicsLayer { alpha = stepBack }.then(if (covered) Modifier.clearAndSetSemantics { } else Modifier)) {
            AnimatedContent(s.ui.view, transitionSpec = { fadeIn(tween(220)) togetherWith fadeOut(tween(160)) }, label = "view") { view ->
                if (view == View.PLAY) PlayView(ui) else BuildView(ui)
            }
        }
        Box(Modifier.fillMaxSize().then(if (modal) Modifier.clearAndSetSemantics { } else Modifier)) {
            OverlayHost(ui, sheet)
            TopBar(ui)
        }
        BannerStrip(ui)
        if (dialog != null) NameDialog(ui, dialog)
        s.ui.confirm?.let { ConfirmDialog(ui, it) }
    }
}
