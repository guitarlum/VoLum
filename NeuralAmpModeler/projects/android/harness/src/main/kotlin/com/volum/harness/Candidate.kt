package com.volum.harness

import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.animation.core.withInfiniteAnimationFrameMillis
import com.volum.core.VoLumStore

/**
 * What a candidate (`:ui-a`, `:ui-b`, `:ui-c`) or the design gallery hands the
 * host. [App] renders whatever `store.state` says (view, section, overlays,
 * confirm, banner) and changes it only through `store.dispatch`. The same
 * entry point serves the launcher activity, the capture activity and the
 * instrumented harness tests.
 *
 * Contract for the harness tests:
 * - every operable control carries `Modifier.featureId(...)`; every surface root carries `Modifier.screen(...)`;
 * - knobs and sliders expose `ProgressBarRangeInfo` + `SetProgress`, text fields `SetText`;
 * - reorderable rows expose a "Move down" `CustomAccessibilityAction`, knobs a "Reset" action;
 * - perpetual animation (art, meters) runs on `withInfiniteAnimationFrameNanos` / `rememberInfiniteTransition`,
 *   which the test clock skips, never on a bare `withFrameNanos` loop;
 * - no system intents: links, file picks and permission requests go through `store.dispatch`.
 */
interface CandidateUi {
    val name: String

    @Composable
    fun App(store: VoLumStore)
}

/** Implemented by the app's `Application`, so harness activities and tests find the flavor's candidate. */
interface CandidateHost {
    val candidate: CandidateUi
}

/** Drives the fake engine's telemetry from the frame clock. Skipped by the harness test clock. */
@Composable
fun EngineClock(store: VoLumStore) {
    LaunchedEffect(store) {
        var start = -1L
        while (true) {
            withInfiniteAnimationFrameMillis { t ->
                if (start < 0) start = t
                store.tick(t - start)
            }
        }
    }
}
