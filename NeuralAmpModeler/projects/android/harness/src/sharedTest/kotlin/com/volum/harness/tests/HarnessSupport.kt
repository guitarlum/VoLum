package com.volum.harness.tests

import androidx.compose.runtime.Composable
import androidx.compose.runtime.MutableState
import androidx.compose.runtime.key
import androidx.compose.runtime.mutableStateOf
import androidx.compose.ui.geometry.Rect
import androidx.compose.ui.layout.findRootCoordinates
import androidx.compose.ui.semantics.SemanticsNode
import androidx.compose.ui.semantics.SemanticsProperties
import androidx.compose.ui.semantics.getOrNull
import androidx.compose.ui.test.SemanticsMatcher
import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import androidx.test.ext.junit.rules.ActivityScenarioRule
import androidx.test.platform.app.InstrumentationRegistry
import com.volum.core.ScreenId
import com.volum.core.VoLumStore
import com.volum.harness.CandidateHost
import com.volum.harness.CandidateUi
import com.volum.harness.FeatureIdsKey
import com.volum.harness.HarnessActivity
import com.volum.harness.ScreenIdsKey
import org.json.JSONObject
import java.io.File

typealias HarnessRule = AndroidComposeTestRule<ActivityScenarioRule<HarnessActivity>, HarnessActivity>

/** A fresh store composed from scratch; replacing it resets the candidate's composition and local state. */
class Session(val store: VoLumStore, val generation: Int)

class SessionHost(private val rule: HarnessRule, private val override: CandidateUi? = null) {
    private var generation = 0
    private lateinit var session: MutableState<Session>
    val candidate: CandidateUi get() = override ?: (rule.activity.application as CandidateHost).candidate
    val store: VoLumStore get() = session.value.store

    fun start(store: VoLumStore) {
        session = mutableStateOf(Session(store, generation++))
        val ui = candidate
        rule.setContent { Host(session, ui) }
        rule.waitForIdle()
    }

    fun reset(store: VoLumStore) {
        rule.runOnUiThread { session.value = Session(store, generation++) }
        rule.waitForIdle()
    }

    @Composable
    private fun Host(state: MutableState<Session>, ui: CandidateUi) {
        val s = state.value
        key(s.generation) { ui.App(s.store) }
    }
}

fun hasFeatureId(id: String) = SemanticsMatcher("featureId=$id") { it.config.getOrNull(FeatureIdsKey)?.contains(id) == true }
val hasAnyFeatureId = SemanticsMatcher("has featureId") { it.config.getOrNull(FeatureIdsKey)?.isNotEmpty() == true }
val hasScreenId = SemanticsMatcher("has screen id") { it.config.getOrNull(ScreenIdsKey)?.isNotEmpty() == true }

/** Placed, non-empty and at least partly inside its window. */
fun SemanticsNode.isVisible(): Boolean {
    if (!layoutInfo.isPlaced) return false
    val b = boundsInRoot
    if (b.width <= 0f || b.height <= 0f) return false
    val root = layoutInfo.coordinates.findRootCoordinates().size
    return b.overlaps(Rect(0f, 0f, root.width.toFloat(), root.height.toFloat()))
}

fun HarnessRule.visibleScreens(): Set<String> =
    onAllNodes(hasScreenId, useUnmergedTree = true).fetchSemanticsNodes(atLeastOneRootRequired = false)
        .filter { it.isVisible() }
        .flatMap { it.config.getOrNull(ScreenIdsKey).orEmpty() }
        .toSet()

fun HarnessRule.isScreenVisible(screen: ScreenId): Boolean = screen.id in visibleScreens()

/** Stable-ish identity of a clickable node across recompositions, for replaying a click path. */
fun SemanticsNode.signature(): String {
    config.getOrNull(FeatureIdsKey)?.takeIf { it.isNotEmpty() }?.let { return "f:" + it.joinToString(",") }
    config.getOrNull(SemanticsProperties.TestTag)?.let { return "t:$it" }
    val text = config.getOrNull(SemanticsProperties.Text)?.joinToString(" ") { it.text }
    val desc = config.getOrNull(SemanticsProperties.ContentDescription)?.joinToString(" ")
    if (!text.isNullOrBlank() || !desc.isNullOrBlank()) return "x:${text.orEmpty()}|${desc.orEmpty()}"
    val b = boundsInRoot
    return "b:${(b.left / 8).toInt()},${(b.top / 8).toInt()},${(b.width / 8).toInt()},${(b.height / 8).toInt()}"
}

object Reports {
    private val dir: File by lazy {
        File(InstrumentationRegistry.getInstrumentation().targetContext.filesDir, "harness").apply { mkdirs() }
    }

    fun write(name: String, json: JSONObject) {
        File(dir, name).writeText(json.toString(2))
    }

    val candidateName: String
        get() = ((InstrumentationRegistry.getInstrumentation().targetContext.applicationContext) as? CandidateHost)?.candidate?.name ?: "?"
}
