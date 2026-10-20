package com.volum.ui.b

import androidx.compose.runtime.LongState
import androidx.compose.runtime.MutableState
import androidx.compose.runtime.State
import androidx.compose.runtime.staticCompositionLocalOf
import com.volum.core.Action
import com.volum.core.Sound
import com.volum.core.Telemetry
import com.volum.core.VoLumStore
import com.volum.design.art.ArtInput

/** A Sound taken off the floor by CLEAR, kept so the toast can put it back. */
internal data class Cleared(val program: Int, val sound: Sound, val name: String)

/**
 * What every candidate B composable needs: the store to dispatch to, the per-frame
 * telemetry and frame clock (read only in draw), and the last cleared Sound for undo.
 */
internal class Stage(
    val store: VoLumStore,
    val tele: State<Telemetry>,
    val frames: LongState,
    val cleared: MutableState<Cleared?>,
) {
    val input = ArtInput { tele.value }
    fun go(a: Action) = store.dispatch(a)
    fun level(input: Boolean): Float = tele.value.let { it.meter(if (input) it.inDb else it.outDb).toFloat() }
}

internal val LocalStage = staticCompositionLocalOf<Stage> { error("candidate B composable outside CandidateB.App") }
