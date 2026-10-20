package com.volum.harness.tests

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.text.BasicText
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.semantics.CustomAccessibilityAction
import androidx.compose.ui.semantics.ProgressBarRangeInfo
import androidx.compose.ui.semantics.customActions
import androidx.compose.ui.semantics.progressBarRangeInfo
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.setProgress
import androidx.compose.ui.semantics.setText
import androidx.compose.ui.test.junit4.createAndroidComposeRule
import androidx.compose.ui.unit.dp
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.volum.core.Action
import com.volum.core.FeatureRegistry
import com.volum.core.Overlay
import com.volum.core.Param
import com.volum.core.ScreenId
import com.volum.core.ScreenRegistry
import com.volum.core.View
import com.volum.core.VoLumStore
import com.volum.harness.CandidateUi
import com.volum.harness.HarnessActivity
import com.volum.harness.featureId
import com.volum.harness.screen
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

/**
 * Proves the harness can pass a correct UI and catch a wrong one, independent of
 * the flavor's candidate: a probe UI implements a few features right (including
 * Custom IR on the Dual Amp SUPPORT lane), a stomp and a knob wired to the wrong
 * parameter, Next controls that step back, one tagged but inert, one target too
 * small, and a three-tap-deep screen.
 */
@RunWith(AndroidJUnit4::class)
class HarnessSelfTest {
    @get:Rule
    val rule = createAndroidComposeRule<HarnessActivity>()

    private fun host() = SessionHost(rule, ProbeCandidate).also { it.start(VoLumStore.forScreen(ScreenId.PLAY)) }

    @Test
    fun exerciserPassesCorrectControlsAndRejectsWrongOnes() {
        val exerciser = FeatureExerciser(rule, host())
        val shouldPass = listOf(
            "nav.view.build", "nav.tuner.open", "play.stomp.delay", "play.art", "header.preset.menu",
            "amp.knob.bass", "amp.knob.reset", "amp.knob.exact", "presets.menu.manage", "tuner.close",
            "header.preset.prev", "play.sound.prev", "dual.support.cab.custom_ir", "dual.support.ir.select", "ir.menu.select",
        )
        val failed = shouldPass.associateWith { exerciser.exercise(FeatureRegistry.byId.getValue(it)) }.filterValues { it != null }
        assertEquals("probe features the harness should accept", emptyMap<String, String?>(), failed)
        assertNotNull("a stomp that toggles Delay must not pass as Reverb", exerciser.exercise(FeatureRegistry.byId.getValue("play.stomp.reverb")))
        assertNotNull("a knob bound to BASS must not pass as MID", exerciser.exercise(FeatureRegistry.byId.getValue("amp.knob.mid")))
        assertNotNull("a tagged but inert node must not pass", exerciser.exercise(FeatureRegistry.byId.getValue("amp.eq.toggle")))
        assertNotNull("an absent feature must not pass", exerciser.exercise(FeatureRegistry.byId.getValue("amp.knob.gate")))
        assertNotNull("a preset Next that steps back must not pass", exerciser.exercise(FeatureRegistry.byId.getValue("header.preset.next")))
        assertNotNull("a Sound Next that steps back must not pass", exerciser.exercise(FeatureRegistry.byId.getValue("play.sound.next")))
        assertNotNull("a MIDI bank Next that pages back must not pass", exerciser.exercise(FeatureRegistry.byId.getValue("midi.bank.next")))
        assertNotNull("a MIDI bank Prev that pages forward must not pass", exerciser.exercise(FeatureRegistry.byId.getValue("midi.bank.prev")))
    }

    @Test
    fun walkerFindsScreensWithinTheTapBudgetOnly() {
        val targets = listOf(ScreenId.PLAY, ScreenId.BUILD_AMP, ScreenId.TUNER, ScreenId.PRESETS_MENU, ScreenId.PRESETS_MANAGE, ScreenId.SETTINGS_SIGNAL)
            .map { ScreenRegistry.spec(it) }
        val host = host()
        val three = ReachabilityWalker(rule, host, maxTaps = 3).walk(targets, 120_000).reached
        assertEquals(0, three["play"]?.getInt("taps"))
        assertEquals(1, three["build.amp"]?.getInt("taps"))
        assertEquals(1, three["tuner"]?.getInt("taps"))
        assertEquals(2, three["presets.menu"]?.getInt("taps"))
        assertEquals(3, three["presets.manage"]?.getInt("taps"))
        assertNull("nothing leads to settings", three["settings.signal"])
        val two = ReachabilityWalker(rule, host, maxTaps = 2).walk(targets, 120_000).reached
        assertNull("three taps deep is out of a two-tap budget", two["presets.manage"])
    }

    @Test
    fun touchScanFlagsOnlyTheUndersizedTarget() {
        val operable = setOf("amp.knob.bass", "amp.knob.mid", "amp.gate.toggle", "header.preset.menu")
        val result = TouchTargetScan(rule, host()).scan(listOf(ScreenId.BUILD_AMP to "default"), operable)
        assertEquals(1, result.violations.length())
        assertEquals("amp.gate.toggle", result.violations.getJSONObject(0).getJSONArray("ids").getString(0))
    }
}

private object ProbeCandidate : CandidateUi {
    override val name = "harness probe"

    @Composable
    override fun App(store: VoLumStore) {
        val state by store.state.collectAsState()
        when (val screen = state.screen()) {
            ScreenId.PLAY -> Row(Modifier.fillMaxSize().screen(screen)) {
                Button("BUILD", "nav.view.build") { store.dispatch(Action.SetView(View.BUILD)) }
                Button("TUNER", "nav.tuner.open") { store.dispatch(Action.Open(Overlay.Tuner)) }
                Button("DLY", "play.stomp.delay") { store.dispatch(Action.ToggleParam(Param.DELAY_ON)) }
                Button("REV", "play.stomp.reverb") { store.dispatch(Action.ToggleParam(Param.DELAY_ON)) }
                Button("PREV", "play.sound.prev") { store.dispatch(Action.StepSound(-1)) }
                Button("NEXT", "play.sound.next") { store.dispatch(Action.StepSound(-1)) }
                Box(Modifier.size(96.dp).featureId("play.art"))
            }
            ScreenId.BUILD_AMP -> Row(Modifier.fillMaxSize().screen(screen)) {
                Button("PRESET", "header.preset.menu") { store.dispatch(Action.Open(Overlay.PresetMenu)) }
                Button("<", "header.preset.prev") { store.dispatch(Action.StepPreset(-1)) }
                Button(">", "header.preset.next") { store.dispatch(Action.StepPreset(-1)) }
                Button("IR", "dual.support.cab.custom_ir") { store.dispatch(Action.Open(Overlay.IrMenu)) }
                Knob(store, Param.BASS, state.param(Param.BASS), "amp.knob.bass", "amp.knob.reset", "amp.knob.exact")
                Knob(store, Param.BASS, state.param(Param.BASS), "amp.knob.mid")
                Box(Modifier.size(30.dp).featureId("amp.gate.toggle").clickable { store.dispatch(Action.ToggleParam(Param.GATE_ON)) })
                Box(Modifier.size(64.dp).featureId("amp.eq.toggle"))
            }
            ScreenId.PRESETS_MENU -> Row(Modifier.fillMaxSize().screen(screen)) {
                Button("MANAGE", "presets.menu.manage") { store.dispatch(Action.Open(Overlay.PresetManage)) }
            }
            ScreenId.TUNER -> Row(Modifier.fillMaxSize().screen(screen)) {
                Button("CLOSE", "tuner.close") { store.dispatch(Action.Back) }
            }
            ScreenId.CAB_IR_MENU -> Row(Modifier.fillMaxSize().screen(screen)) {
                Button("ROYER", "ir.menu.select", "dual.support.ir.select") { store.dispatch(Action.SelectIr("ir:royer")) }
            }
            ScreenId.SETTINGS_MIDI -> Row(Modifier.fillMaxSize().screen(screen)) {
                Button("BANK-", "midi.bank.prev") { store.dispatch(Action.SetMidiBank(state.midi.bank + 1)) }
                Button("BANK+", "midi.bank.next") { store.dispatch(Action.SetMidiBank(state.midi.bank - 1)) }
            }
            else -> Box(Modifier.fillMaxSize().screen(screen)) { BasicText(screen.id) }
        }
    }

    @Composable
    private fun Button(label: String, vararg ids: String, onClick: () -> Unit) {
        Box(Modifier.size(64.dp).featureId(*ids).clickable(onClick = onClick)) { BasicText(label) }
    }

    @Composable
    private fun Knob(store: VoLumStore, p: Param, value: Double, vararg ids: String) {
        Box(
            Modifier.size(64.dp).featureId(*ids).semantics {
                progressBarRangeInfo = ProgressBarRangeInfo(value.toFloat(), p.min.toFloat()..p.max.toFloat())
                setProgress { v -> store.dispatch(Action.SetParam(p, v.toDouble())); true }
                setText { t -> t.text.toDoubleOrNull()?.let { store.dispatch(Action.SetParam(p, it)) } != null }
                customActions = listOf(CustomAccessibilityAction("Reset") { store.dispatch(Action.ResetParam(p)); true })
            },
        )
    }
}
