package com.volum.harness.tests

import androidx.compose.ui.semantics.getOrNull
import androidx.compose.ui.test.junit4.createAndroidComposeRule
import androidx.compose.ui.unit.Density
import androidx.compose.ui.unit.dp
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.volum.core.FeatureRegistry
import com.volum.core.VoLumStore
import com.volum.harness.FeatureIdsKey
import com.volum.harness.HarnessActivity
import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

/** Every node carrying an operable feature id is at least 48 x 48 dp, on every screen and fixture the coverage test visits. */
@RunWith(AndroidJUnit4::class)
class TouchTargetTest {
    @get:Rule
    val rule = createAndroidComposeRule<HarnessActivity>()

    @Test
    fun operableFeatureNodesAreAtLeast48dp() {
        val host = SessionHost(rule)
        val density = Density(rule.activity)
        val minPx = with(density) { MIN_DP.dp.toPx() }
        val operable = FeatureRegistry.ui.filter { !it.isView }.map { it.id }.toSet()
        val placements = FeatureRegistry.ui.map { it.screen!! to it.fixture }.distinct()
        val violations = JSONArray()
        val seen = mutableSetOf<String>()
        var checked = 0
        host.start(VoLumStore.forScreen(placements.first().first, placements.first().second))
        for ((screen, fixture) in placements) {
            host.reset(VoLumStore.forScreen(screen, fixture))
            for (node in rule.onAllNodes(hasAnyFeatureId).fetchSemanticsNodes(atLeastOneRootRequired = false)) {
                val ids = node.config.getOrNull(FeatureIdsKey).orEmpty().filter { it in operable }
                if (ids.isEmpty() || !node.isVisible()) continue
                checked++
                val b = node.boundsInRoot
                if (b.width + 0.5f < minPx || b.height + 0.5f < minPx) {
                    val key = "${ids.joinToString(",")}@${screen.id}"
                    if (seen.add(key)) {
                        violations.put(JSONObject()
                            .put("ids", JSONArray(ids))
                            .put("screen", screen.id)
                            .put("fixture", fixture)
                            .put("w_dp", (b.width / density.density).toDouble())
                            .put("h_dp", (b.height / density.density).toDouble()))
                    }
                }
            }
        }
        Reports.write("touch.json", JSONObject()
            .put("candidate", Reports.candidateName)
            .put("pass", violations.length() == 0)
            .put("checked", checked)
            .put("min_dp", MIN_DP)
            .put("violations", violations))
        assertTrue("${violations.length()} feature nodes smaller than $MIN_DP dp: $violations", violations.length() == 0)
    }

    companion object {
        const val MIN_DP = 48
    }
}
