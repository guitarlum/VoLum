package com.volum.harness.tests

import androidx.compose.ui.test.junit4.createAndroidComposeRule
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.volum.core.FeatureRegistry
import com.volum.core.VoLumStore
import com.volum.harness.HarnessActivity
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
        val operable = FeatureRegistry.ui.filter { !it.isView }.map { it.id }.toSet()
        val placements = FeatureRegistry.ui.map { it.screen!! to it.fixture }.distinct()
        host.start(VoLumStore.forScreen(placements.first().first, placements.first().second))
        val result = TouchTargetScan(rule, host, MIN_DP).scan(placements, operable)
        Reports.write("touch.json", JSONObject()
            .put("candidate", Reports.candidateName)
            .put("pass", result.violations.length() == 0)
            .put("checked", result.checked)
            .put("min_dp", MIN_DP)
            .put("violations", result.violations))
        assertTrue("${result.violations.length()} feature nodes smaller than $MIN_DP dp: ${result.violations}", result.violations.length() == 0)
    }

    companion object {
        const val MIN_DP = 48
    }
}
