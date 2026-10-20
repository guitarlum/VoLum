package com.volum.harness.tests

import androidx.compose.ui.test.junit4.createAndroidComposeRule
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.volum.core.Fixtures
import com.volum.core.Scope
import com.volum.core.ScreenRegistry
import com.volum.core.VoLumStore
import com.volum.harness.HarnessActivity
import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

/** Every candidate screen id is reached from home (PLAY) by clicks only, in at most three taps. */
@RunWith(AndroidJUnit4::class)
class ReachabilityTest {
    @get:Rule
    val rule = createAndroidComposeRule<HarnessActivity>()

    @Test
    fun everyScreenIsWithinThreeTapsOfHome() {
        val host = SessionHost(rule)
        val targets = ScreenRegistry.inScope(Scope.CANDIDATE)
        host.start(VoLumStore(Fixtures.build(targets.first().fixture)))
        val result = ReachabilityWalker(rule, host, MAX_TAPS).walk(targets, BUDGET_MS)
        val ids = targets.map { it.screen.id }
        val unreached = ids.filter { it !in result.reached }
        Reports.write("reachability.json", JSONObject()
            .put("candidate", Reports.candidateName)
            .put("pass", unreached.isEmpty())
            .put("total", targets.size)
            .put("reached", JSONObject().apply { result.reached.filterKeys { it in ids }.forEach { (k, v) -> put(k, v) } })
            .put("unreached", JSONArray(unreached))
            .put("clicks", result.clicks)
            .put("elapsed_ms", result.elapsedMs)
            .put("budget_exhausted", result.budgetExhausted))
        assertTrue("screens not reachable from home in $MAX_TAPS taps: $unreached", unreached.isEmpty())
    }

    companion object {
        const val MAX_TAPS = 3
        const val BUDGET_MS = 8 * 60 * 1000L
    }
}
