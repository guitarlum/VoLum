package com.volum.app

import android.app.Application
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.BackHandler
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import com.volum.core.Action
import com.volum.core.Fixtures
import com.volum.core.VoLumStore
import com.volum.harness.CandidateHost
import com.volum.harness.CandidateUi
import com.volum.harness.EngineClock

class VoLumApplication : Application(), CandidateHost {
    override val candidate: CandidateUi = flavorCandidate
}

/** Launcher: the flavor's candidate on the fake engine, starting from the desktop screenshot seed. */
class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        enableEdgeToEdge()
        super.onCreate(savedInstanceState)
        WindowCompat.getInsetsController(window, window.decorView).apply {
            systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
            hide(WindowInsetsCompat.Type.systemBars())
        }
        val candidate = (application as CandidateHost).candidate
        setContent {
            val store = remember { VoLumStore(Fixtures.build("default"), energyOverride = null) }
            val state by store.state.collectAsState()
            BackHandler(enabled = state.ui.overlays.isNotEmpty() || state.ui.confirm != null) { store.dispatch(Action.Back) }
            EngineClock(store)
            candidate.App(store)
        }
    }
}
