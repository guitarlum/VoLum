package com.volum.harness.tests

import androidx.compose.ui.test.junit4.createAndroidComposeRule
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.volum.core.FeatureRegistry
import com.volum.core.VoLumStore
import com.volum.harness.HarnessActivity
import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

/**
 * Every non-N/A, non-engine feature id is found on its screen in its fixture,
 * operated through its semantics node, and changes the store's state the way
 * `FeatureRegistry` specifies. The test never dispatches to the store itself.
 */
@RunWith(AndroidJUnit4::class)
class CoverageTest {
    @get:Rule
    val rule = createAndroidComposeRule<HarnessActivity>()

    @Test
    fun everyFeatureIsOperableOnItsScreen() {
        val host = SessionHost(rule)
        val features = FeatureRegistry.ui
        val exerciser = FeatureExerciser(rule, host)
        val failures = JSONArray()
        val missing = mutableListOf<String>()
        var passed = 0
        host.start(VoLumStore.forScreen(features.first().screen!!, features.first().fixture))
        for (spec in features) {
            val reason = runCatching { exerciser.exercise(spec) }.getOrElse { "exception: ${it::class.simpleName}: ${it.message?.take(200)}" }
            if (reason == null) passed++ else {
                missing += spec.id
                failures.put(JSONObject().put("id", spec.id).put("screen", spec.screen?.id).put("fixture", spec.fixture).put("reason", reason))
            }
        }
        Reports.write("coverage.json", JSONObject()
            .put("candidate", Reports.candidateName)
            .put("total", features.size)
            .put("passed", passed)
            .put("missing", JSONArray(missing))
            .put("failures", failures))
        assertTrue("${missing.size} of ${features.size} features not operable: ${missing.take(40)}", missing.isEmpty())
    }
}
