package com.volum.harness

import android.content.Intent
import android.os.Bundle
import android.util.Log
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.key
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.runtime.withFrameNanos
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import com.volum.core.Fixtures
import com.volum.core.MotionRegistry
import com.volum.core.ScreenId
import com.volum.core.ScreenRegistry
import com.volum.core.VoLumStore
import kotlinx.coroutines.delay

/**
 * Renders one screen in one fixture, or plays one motion script, for the box's
 * `Capture` and `Motion` steps:
 *
 *     am start -n <pkg>/com.volum.harness.CaptureActivity --es screen play --es fixture default --ei seq 7
 *     am start -n <pkg>/com.volum.harness.CaptureActivity --es motion play_energy --ei seq 8
 *
 * Logs `VoLumCapture: ready seq=<n>` once the frame has settled and
 * `VoLumCapture: done seq=<n>` when a motion script has finished.
 */
class CaptureActivity : ComponentActivity() {
    private data class Request(val screen: ScreenId, val fixture: String, val motion: String?, val seq: Int)

    private var request by mutableStateOf<Request?>(null)

    override fun onCreate(savedInstanceState: Bundle?) {
        enableEdgeToEdge()
        super.onCreate(savedInstanceState)
        hideSystemBars()
        request = parse(intent)
        val candidate = (application as CandidateHost).candidate
        setContent { request?.let { CaptureHost(it, candidate) } }
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
        request = parse(intent)
    }

    private fun hideSystemBars() {
        WindowCompat.getInsetsController(window, window.decorView).apply {
            systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
            hide(WindowInsetsCompat.Type.systemBars())
        }
    }

    private fun parse(intent: Intent): Request {
        val seq = intent.getIntExtra("seq", 0)
        val motion = intent.getStringExtra("motion")
        if (motion != null) {
            val m = MotionRegistry.spec(motion)
            return Request(m.screen, m.fixture, motion, seq)
        }
        val screen = ScreenId.of(intent.getStringExtra("screen") ?: ScreenRegistry.HOME.id)
        val fixture = intent.getStringExtra("fixture") ?: ScreenRegistry.spec(screen).fixture
        return Request(screen, fixture, null, seq)
    }

    @Composable
    private fun CaptureHost(r: Request, candidate: CandidateUi) {
        key(r) {
            val store = remember { VoLumStore.forScreen(r.screen, r.fixture) }
            EngineClock(store)
            LaunchedEffect(Unit) {
                withFrameNanos { }
                withFrameNanos { }
                delay(SETTLE_MS)
                Log.i(TAG, "ready seq=${r.seq} screen=${r.screen.id} fixture=${r.fixture}")
            }
            if (r.motion != null) {
                LaunchedEffect(Unit) {
                    val script = MotionRegistry.spec(r.motion)
                    val fixtureEnergy = Fixtures.spec(script.fixture).energy
                    store.energyOverride = fixtureEnergy
                    delay(SETTLE_MS)
                    Log.i(TAG, "motion-start seq=${r.seq} id=${script.id}")
                    var elapsed = 0L
                    for (step in script.steps) {
                        if (step.atMs > elapsed) delay(step.atMs - elapsed)
                        elapsed = step.atMs
                        step.energy?.let { store.energyOverride = it }
                        step.action?.let { store.dispatch(it) }
                    }
                    delay((script.durationMs - elapsed).coerceAtLeast(0))
                    Log.i(TAG, "done seq=${r.seq} id=${script.id}")
                }
            }
            candidate.App(store)
        }
    }

    companion object {
        const val TAG = "VoLumCapture"
        private const val SETTLE_MS = 700L
    }
}

/** Hosts the harness tests' compositions; declared here so every flavor's test APK can launch it. */
class HarnessActivity : ComponentActivity()
