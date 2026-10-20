package com.volum.ui.b

import androidx.compose.runtime.State
import androidx.compose.runtime.staticCompositionLocalOf
import com.volum.core.Action
import com.volum.core.Telemetry
import com.volum.core.VoLumStore

/** What every candidate B composable needs: the store to dispatch to and the per-frame telemetry, read only in draw. */
internal class Stage(val store: VoLumStore, val tele: State<Telemetry>) {
    fun go(a: Action) = store.dispatch(a)
}

internal val LocalStage = staticCompositionLocalOf<Stage> { error("candidate B composable outside CandidateB.App") }
