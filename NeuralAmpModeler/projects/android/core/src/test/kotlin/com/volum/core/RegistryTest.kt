package com.volum.core

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class RegistryTest {
    @Test
    fun everyReferencedFixtureExists() {
        val referenced = FeatureRegistry.ui.map { it.fixture } +
            ScreenRegistry.specs.flatMap { it.captures + it.fixture } +
            MotionRegistry.all.map { it.fixture }
        val missing = referenced.distinct().filter { id -> Fixtures.all.none { it.id == id } }
        assertEquals(emptyList<String>(), missing)
    }

    @Test
    fun everyCandidateCaptureLandsOnItsScreen() {
        val wrong = ScreenRegistry.inScope(Scope.CANDIDATE).flatMap { spec ->
            (spec.captures + spec.fixture).mapNotNull { f ->
                val landed = VoLumStore.forScreen(spec.screen, f).current.screen()
                if (landed == spec.screen) null else "${spec.screen.id} in $f lands on ${landed.id}"
            }
        }
        assertEquals(emptyList<String>(), wrong)
    }

    @Test
    fun everyScreenHasASpecAndEveryCandidateScreenHasFeatures() {
        assertEquals(ScreenId.entries.toSet(), ScreenRegistry.specs.map { it.screen }.toSet())
        val featureScreens = FeatureRegistry.ui.mapNotNull { it.screen }.toSet()
        val bare = ScreenRegistry.inScope(Scope.CANDIDATE).map { it.screen }.filter { it !in featureScreens }
        assertEquals("candidate screens with no feature on them", emptyList<ScreenId>(), bare)
    }

    @Test
    fun homeIsPlayAndTheDefaultFixtureStartsThere() {
        assertEquals(ScreenId.PLAY, ScreenRegistry.HOME)
        assertEquals(ScreenId.PLAY, Fixtures.build("default").screen())
    }

    @Test
    fun motionScriptsAreOrderedAndInsideTheirDuration() {
        MotionRegistry.all.forEach { m ->
            assertTrue(m.id, m.steps.zipWithNext().all { (a, b) -> a.atMs <= b.atMs })
            assertTrue(m.id, m.steps.all { it.atMs < m.durationMs })
        }
        val refs = listOf("build_idle", "play_energy", "pedal_toggle", "section_switch")
        assertTrue(refs.all { r -> MotionRegistry.inScope(Scope.CANDIDATE).any { it.id == r } })
    }

    @Test
    fun telemetryIsDeterministic() {
        val s = Fixtures.build("default")
        assertEquals(FakeEngine.telemetry(s, 12_345), FakeEngine.telemetry(s, 12_345))
        assertEquals(0.0, FakeEngine.telemetry(Fixtures.build("mic_denied"), 5_000).energy, 0.0)
        assertTrue(FakeEngine.telemetry(Reducer.reduce(s, Action.Open(Overlay.Tuner)), 5_000).muted)
    }
}
