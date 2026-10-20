package com.volum.harness.tests

import androidx.compose.ui.semantics.ProgressBarRangeInfo
import androidx.compose.ui.semantics.SemanticsActions
import androidx.compose.ui.semantics.SemanticsProperties
import androidx.compose.ui.semantics.getOrNull
import androidx.compose.ui.test.SemanticsNodeInteraction
import androidx.compose.ui.test.junit4.createAndroidComposeRule
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performImeAction
import androidx.compose.ui.test.performScrollTo
import androidx.compose.ui.test.performSemanticsAction
import androidx.compose.ui.test.performTextReplacement
import androidx.compose.ui.test.performTouchInput
import androidx.compose.ui.test.swipeRight
import androidx.compose.ui.test.swipeUp
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.volum.core.FeatureRegistry
import com.volum.core.FeatureSpec
import com.volum.core.VoLumState
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

    private val maxNodesTried = 8

    @Test
    fun everyFeatureIsOperableOnItsScreen() {
        val host = SessionHost(rule)
        val features = FeatureRegistry.ui
        val failures = JSONArray()
        val missing = mutableListOf<String>()
        var passed = 0
        host.start(storeFor(features.first()))
        for (spec in features) {
            val reason = runCatching { exercise(host, spec) }.getOrElse { "exception: ${it::class.simpleName}: ${it.message?.take(200)}" }
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

    private fun storeFor(spec: FeatureSpec) = VoLumStore.forScreen(spec.screen!!, spec.fixture)

    /** Returns null when the feature passed, otherwise why it did not. */
    private fun exercise(host: SessionHost, spec: FeatureSpec): String? {
        host.reset(storeFor(spec))
        val matcher = hasFeatureId(spec.id)
        val count = rule.onAllNodes(matcher).fetchSemanticsNodes(atLeastOneRootRequired = false).size
        if (count == 0) return "no node carries the id on ${spec.screen!!.id} (fixture ${spec.fixture})"
        if (spec.isView) {
            val visible = rule.onAllNodes(matcher).fetchSemanticsNodes(atLeastOneRootRequired = false).any { it.isVisible() }
            return if (visible) null else "node is not visible"
        }
        var last = "state did not change as specified"
        for (i in 0 until minOf(count, maxNodesTried)) {
            if (i > 0) host.reset(storeFor(spec))
            val before = host.store.current
            val node = rule.onAllNodes(matcher)[i]
            val outcome = operate(host, node, spec, before)
            if (outcome == null) return null
            last = outcome
        }
        return last
    }

    private fun operate(host: SessionHost, node: SemanticsNodeInteraction, spec: FeatureSpec, before: VoLumState): String? {
        runCatching { node.performScrollTo() }
        val config = node.fetchSemanticsNode().config
        val interaction = spec.interaction
        when {
            interaction == "drag" -> {
                val range = config.getOrNull(SemanticsProperties.ProgressBarRangeInfo)
                if (config.getOrNull(SemanticsActions.SetProgress) != null && range != null) {
                    val target = dragTarget(range)
                    node.performSemanticsAction(SemanticsActions.SetProgress) { it(target.first) }
                    rule.waitForIdle()
                    return judgeDrag(spec, before, host.store.current, target.second)
                }
                node.performTouchInput { swipeUp() }
                rule.waitForIdle()
                if (spec.check(before, host.store.current)) return null
                node.performTouchInput { swipeRight() }
            }
            interaction == "text" -> {
                if (config.getOrNull(SemanticsActions.SetText) == null) return "node has no SetText action"
                node.performTextReplacement(spec.text!!)
                rule.waitForIdle()
                if (spec.check(before, host.store.current)) return null
                if (config.getOrNull(SemanticsActions.OnImeAction) != null) node.performImeAction()
            }
            spec.actionLabel != null -> {
                val action = config.getOrNull(SemanticsActions.CustomActions)?.firstOrNull { it.label.equals(spec.actionLabel, ignoreCase = true) }
                    ?: return "node has no '${spec.actionLabel}' accessibility action"
                rule.runOnIdle { action.action() }
            }
            else -> {
                if (config.getOrNull(SemanticsActions.OnClick) == null) return "node is not clickable"
                node.performClick()
            }
        }
        rule.waitForIdle()
        return if (spec.check(before, host.store.current)) null else "state did not change as specified"
    }

    /** SetProgress target at 85 % of the range, or 15 % when the control already sits in the upper half; returns (value, fraction). */
    private fun dragTarget(range: ProgressBarRangeInfo): Pair<Float, Float> {
        val lo = range.range.start
        val hi = range.range.endInclusive
        val frac = if (hi > lo) (range.current - lo) / (hi - lo) else 0f
        val targetFrac = if (frac < 0.5f) 0.85f else 0.15f
        return (lo + targetFrac * (hi - lo)) to targetFrac
    }

    private fun judgeDrag(spec: FeatureSpec, before: VoLumState, after: VoLumState, targetFrac: Float): String? {
        if (!spec.check(before, after)) return "SetProgress did not change the bound value"
        val p = spec.param ?: return null
        val moved = p.normalized(after.param(p)) - p.normalized(before.param(p))
        val wanted = targetFrac - p.normalized(before.param(p))
        return if (moved * wanted > 0) null else "SetProgress moved ${p.name} away from the target"
    }
}
