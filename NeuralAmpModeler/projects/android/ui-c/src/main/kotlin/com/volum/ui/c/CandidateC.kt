package com.volum.ui.c

import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.scaleIn
import androidx.compose.animation.togetherWith
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
import com.volum.design.components.volumCanvas
import com.volum.design.theme.VoLumTheme
import com.volum.harness.CandidateUi

/**
 * C, Studio Instrument: the art is the instrument. PLAY is the stage with the Sound
 * rail floating on it; BUILD keeps a living art window with the PRE / AMP / POST
 * blocks floating on it above a sheet capped under half the screen. A picker folds
 * that sheet down to the row it came from and rises above it; panels opened from
 * the top bar drop from it, never ending across a row of controls.
 */
object CandidateC : CandidateUi {
    override val name = "C Studio Instrument"

    @Composable
    override fun App(store: VoLumStore) {
        val s by store.state.collectAsState()
        VoLumTheme { Studio(Ui(s, store::dispatch, store.telemetry)) }
    }
}

@Composable
internal fun Studio(ui: Ui) {
    val s = ui.s
    val dialog = s.ui.top as? Overlay.NameDialog
    val sheet = s.ui.overlays.lastOrNull { it !is Overlay.NameDialog }
    val modal = dialog != null || s.ui.confirm != null
    val covered = sheet != null || modal
    val tuner by animateFloatAsState(if (sheet is Overlay.Tuner) 0f else 1f, tween(240), label = "tuner")
    val fade = { tuner }
    Box(Modifier.fillMaxSize().volumCanvas()) {
        Box(Modifier.fillMaxSize().then(if (covered) Modifier.clearAndSetSemantics { } else Modifier)) {
            AnimatedContent(
                s.ui.view,
                transitionSpec = { (fadeIn(tween(240)) + scaleIn(tween(240), initialScale = 0.985f)) togetherWith fadeOut(tween(180)) },
                label = "view",
            ) { view ->
                if (view == View.PLAY) PlayView(ui, fade, banner = sheet == null && !modal) else BuildView(ui, fade, banner = sheet == null && !modal)
            }
        }
        Box(Modifier.fillMaxSize().then(if (modal) Modifier.clearAndSetSemantics { } else Modifier)) {
            OverlayHost(ui, sheet)
            TopBar(ui)
        }
        if (dialog != null) NameDialog(ui, dialog)
        s.ui.confirm?.let { ConfirmDialog(ui, it) }
    }
}
