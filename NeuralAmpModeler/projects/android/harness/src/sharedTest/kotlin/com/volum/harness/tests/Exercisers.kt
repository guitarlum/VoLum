package com.volum.harness.tests

import android.os.SystemClock
import androidx.compose.ui.semantics.ProgressBarRangeInfo
import androidx.compose.ui.semantics.SemanticsActions
import androidx.compose.ui.semantics.SemanticsProperties
import androidx.compose.ui.semantics.getOrNull
import androidx.compose.ui.test.SemanticsNodeInteraction
import androidx.compose.ui.test.hasClickAction
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performScrollTo
import androidx.compose.ui.test.performSemanticsAction
import androidx.compose.ui.test.performTouchInput
import androidx.compose.ui.test.swipeRight
import androidx.compose.ui.test.swipeUp
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.unit.Density
import androidx.compose.ui.unit.dp
import com.volum.core.FeatureSpec
import com.volum.core.Fixtures
import com.volum.core.ScreenId
import com.volum.core.ScreenRegistry
import com.volum.core.ScreenSpec
import com.volum.core.VoLumState
import com.volum.core.VoLumStore
import com.volum.harness.FeatureIdsKey
import org.json.JSONArray
import org.json.JSONObject

/** Operates one feature through its semantics node and judges the store's state change. Never dispatches itself. */
class FeatureExerciser(private val rule: HarnessRule, private val host: SessionHost) {
    private val maxNodesTried = 8

    private fun storeFor(spec: FeatureSpec) = VoLumStore.forScreen(spec.screen!!, spec.fixture)

    /** Null when the feature passed, otherwise why it did not. */
    fun exercise(spec: FeatureSpec): String? {
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
            val outcome = operate(rule.onAllNodes(matcher)[i], spec, before)
            if (outcome == null) return null
            last = outcome
        }
        return last
    }

    private fun operate(node: SemanticsNodeInteraction, spec: FeatureSpec, before: VoLumState): String? {
        runCatching { node.performScrollTo() }
        val config = node.fetchSemanticsNode().config
        when {
            spec.interaction == "drag" -> {
                val range = config.getOrNull(SemanticsProperties.ProgressBarRangeInfo)
                if (config.getOrNull(SemanticsActions.SetProgress) != null && range != null) {
                    val (value, fraction) = dragTarget(range)
                    node.performSemanticsAction(SemanticsActions.SetProgress) { it(value) }
                    rule.waitForIdle()
                    return judgeDrag(spec, before, host.store.current, fraction)
                }
                node.performTouchInput { swipeUp() }
                rule.waitForIdle()
                if (spec.check(before, host.store.current)) return null
                node.performTouchInput { swipeRight() }
            }
            spec.interaction == "text" -> {
                if (config.getOrNull(SemanticsActions.SetText) == null) return "node has no SetText action"
                node.performSemanticsAction(SemanticsActions.SetText) { it(AnnotatedString(spec.text!!)) }
                rule.waitForIdle()
                if (spec.check(before, host.store.current)) return null
                if (config.getOrNull(SemanticsActions.OnImeAction) != null) node.performSemanticsAction(SemanticsActions.OnImeAction)
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

    /** SetProgress target at 85 % of the range, or 15 % when the control already sits in the upper half: (value, fraction). */
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

/**
 * Breadth-first walk over the set of visible screen ids from home (PLAY in each
 * screen's starting fixture), clicks only, at most [maxTaps] deep. A click that
 * leaves the visible screens unchanged does not open a branch.
 */
class ReachabilityWalker(private val rule: HarnessRule, private val host: SessionHost, private val maxTaps: Int = 3) {
    data class Result(val reached: Map<String, JSONObject>, val clicks: Int, val elapsedMs: Long, val budgetExhausted: Boolean)

    private data class Branch(val path: List<String>, val screens: Set<String>)

    fun walk(targets: List<ScreenSpec>, budgetMs: Long): Result {
        val reached = linkedMapOf<String, JSONObject>()
        val started = SystemClock.elapsedRealtime()
        fun overBudget() = SystemClock.elapsedRealtime() - started > budgetMs
        var clicks = 0
        for (fixture in targets.map { it.fixture }.distinct()) {
            val wanted = targets.filter { it.fixture == fixture }.map { it.screen.id }.toSet()
            if (wanted.all { it in reached }) continue
            host.reset(home(fixture))
            val homeScreens = rule.visibleScreens()
            homeScreens.forEach { reached.putIfAbsent(it, entry(fixture, emptyList())) }
            val seen = mutableSetOf(homeScreens)
            var frontier = listOf(Branch(emptyList(), homeScreens))
            for (depth in 1..maxTaps) {
                val next = mutableListOf<Branch>()
                for (branch in frontier) {
                    if (wanted.all { it in reached } || overBudget()) break
                    replay(fixture, branch.path) ?: continue
                    var dirty = false
                    for (sig in clickableSignatures()) {
                        if (overBudget()) break
                        if (dirty) {
                            replay(fixture, branch.path) ?: break
                            dirty = false
                        }
                        if (!click(sig)) continue
                        clicks++
                        val screens = rule.visibleScreens()
                        if (screens != branch.screens) {
                            dirty = true
                            val path = branch.path + sig
                            screens.filter { it !in reached }.forEach { reached[it] = entry(fixture, path) }
                            if (seen.add(screens) && depth < maxTaps) next += Branch(path, screens)
                        }
                    }
                }
                frontier = next
                if (frontier.isEmpty()) break
            }
        }
        return Result(reached, clicks, SystemClock.elapsedRealtime() - started, overBudget())
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

    private fun replay(fixture: String, path: List<String>): Unit? {
        host.reset(home(fixture))
        for (sig in path) if (!click(sig)) return null
        return Unit
    }
}

/** Every visible node carrying an operable feature id is at least [minDp] x [minDp]. */
class TouchTargetScan(private val rule: HarnessRule, private val host: SessionHost, private val minDp: Int = 48) {
    data class Result(val checked: Int, val violations: JSONArray)

    fun scan(placements: List<Pair<ScreenId, String>>, operable: Set<String>): Result {
        val density = Density(rule.activity)
        val minPx = with(density) { minDp.dp.toPx() }
        val violations = JSONArray()
        val seen = mutableSetOf<String>()
        var checked = 0
        for ((screen, fixture) in placements) {
            host.reset(VoLumStore.forScreen(screen, fixture))
            for (node in rule.onAllNodes(hasAnyFeatureId).fetchSemanticsNodes(atLeastOneRootRequired = false)) {
                val ids = node.config.getOrNull(FeatureIdsKey).orEmpty().filter { it in operable }
                if (ids.isEmpty() || !node.isVisible()) continue
                checked++
                val b = node.boundsInRoot
                if (b.width + 0.5f < minPx || b.height + 0.5f < minPx) {
                    if (seen.add("${ids.joinToString(",")}@${screen.id}")) {
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
        return Result(checked, violations)
    }
}
