package com.volum.harness.tests

import android.os.SystemClock
import androidx.compose.ui.test.hasClickAction
import androidx.compose.ui.test.junit4.createAndroidComposeRule
import androidx.compose.ui.test.performClick
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

/**
 * Every candidate screen id is reached from home (PLAY, in the screen's own
 * starting fixture) by clicks only, in at most [MAX_TAPS] taps. Breadth-first
 * over the set of visible screen ids; a click that leaves the visible screens
 * unchanged does not open a new branch.
 */
@RunWith(AndroidJUnit4::class)
class ReachabilityTest {
    @get:Rule
    val rule = createAndroidComposeRule<HarnessActivity>()

    private data class Branch(val path: List<String>, val screens: Set<String>)

    @Test
    fun everyScreenIsWithinThreeTapsOfHome() {
        val host = SessionHost(rule)
        val targets = ScreenRegistry.inScope(Scope.CANDIDATE)
        val reached = linkedMapOf<String, JSONObject>()
        val started = SystemClock.elapsedRealtime()
        var clicks = 0
        host.start(home(targets.first().fixture))

        for (fixture in targets.map { it.fixture }.distinct()) {
            val wanted = targets.filter { it.fixture == fixture }.map { it.screen.id }.toSet()
            if (wanted.all { it in reached }) continue
            host.reset(home(fixture))
            val homeScreens = rule.visibleScreens()
            homeScreens.forEach { reached.putIfAbsent(it, entry(fixture, emptyList())) }
            val seen = mutableSetOf(homeScreens)
            var frontier = listOf(Branch(emptyList(), homeScreens))
            for (depth in 1..MAX_TAPS) {
                val next = mutableListOf<Branch>()
                for (branch in frontier) {
                    if (wanted.all { it in reached } || SystemClock.elapsedRealtime() - started > BUDGET_MS) break
                    replay(host, fixture, branch.path) ?: continue
                    val candidates = clickableSignatures()
                    var dirty = false
                    for (sig in candidates) {
                        if (SystemClock.elapsedRealtime() - started > BUDGET_MS) break
                        if (dirty) {
                            replay(host, fixture, branch.path) ?: break
                            dirty = false
                        }
                        if (!click(sig)) continue
                        clicks++
                        val screens = rule.visibleScreens()
                        if (screens != branch.screens) {
                            dirty = true
                            val path = branch.path + sig
                            screens.filter { it !in reached }.forEach { reached[it] = entry(fixture, path) }
                            if (seen.add(screens) && depth < MAX_TAPS) next += Branch(path, screens)
                        }
                    }
                }
                frontier = next
                if (frontier.isEmpty()) break
            }
        }

        val unreached = targets.map { it.screen.id }.filter { it !in reached }
        Reports.write("reachability.json", JSONObject()
            .put("candidate", Reports.candidateName)
            .put("pass", unreached.isEmpty())
            .put("total", targets.size)
            .put("reached", JSONObject().apply { reached.filterKeys { k -> targets.any { it.screen.id == k } }.forEach { (k, v) -> put(k, v) } })
            .put("unreached", JSONArray(unreached))
            .put("clicks", clicks)
            .put("elapsed_ms", SystemClock.elapsedRealtime() - started)
            .put("budget_exhausted", SystemClock.elapsedRealtime() - started > BUDGET_MS))
        assertTrue("screens not reachable from home in $MAX_TAPS taps: $unreached", unreached.isEmpty())
    }

    private fun home(fixture: String): VoLumStore =
        VoLumStore(ScreenRegistry.place(Fixtures.build(fixture), ScreenRegistry.HOME), Fixtures.spec(fixture).energy)

    private fun entry(fixture: String, path: List<String>) =
        JSONObject().put("taps", path.size).put("fixture", fixture).put("path", JSONArray(path))

    private fun clickableSignatures(): List<String> =
        rule.onAllNodes(hasClickAction()).fetchSemanticsNodes(atLeastOneRootRequired = false)
            .filter { it.isVisible() }
            .map { it.signature() }
            .distinct()

    private fun click(signature: String): Boolean {
        val nodes = rule.onAllNodes(hasClickAction()).fetchSemanticsNodes(atLeastOneRootRequired = false)
        val index = nodes.indexOfFirst { it.isVisible() && it.signature() == signature }
        if (index < 0) return false
        return runCatching {
            rule.onAllNodes(hasClickAction())[index].performClick()
            rule.waitForIdle()
        }.isSuccess
    }

    /** Fresh home in [fixture], then the clicks of [path]; null when a step can no longer be found. */
    private fun replay(host: SessionHost, fixture: String, path: List<String>): Unit? {
        host.reset(home(fixture))
        for (sig in path) if (!click(sig)) return null
        return Unit
    }

    companion object {
        const val MAX_TAPS = 3
        const val BUDGET_MS = 8 * 60 * 1000L
    }
}
