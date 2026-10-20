package com.volum.design.art

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

/** The desktop doctests of `test_volum_art_anim.cpp` that cover what `:design` ported. */
class MotionTest {
    private val tick = 1f / 60f

    private fun ArtMotionState.run(seconds: Float, energy: Float, attack: (Int) -> Float = { 0f }) {
        val n = (seconds / tick).toInt()
        for (i in 0 until n) advance(energy, attack(i), PlayLight.bloomWeight(PlayLight.glow(energy, attack(i))), tick)
    }

    @Test
    fun silenceStaysAtRest() {
        val s = ArtMotionState()
        s.run(5f, 0f)
        assertTrue(s.m.isRest)
        assertEquals(0.0, s.m.clock, 0.0)
        assertEquals(0, s.m.onsets)
        assertEquals(0f, s.m.bloom, 0f)
        assertEquals(0f, s.m.wake, 0f)
    }

    @Test
    fun aPickIsOneOnsetAndAHeldNoteDoesNotRepeatIt() {
        val s = ArtMotionState()
        var attack = 1f
        s.advance(1f, attack, 0f, tick)
        assertEquals(1, s.m.onsets)
        assertEquals(0f, s.m.onsetAge[0], 0f)
        repeat(180) {
            attack *= 0.87f
            s.advance(1f, attack, 0f, tick)
        }
        assertEquals(1, s.m.onsets)
        assertTrue(s.m.pick < 0.05f)
        assertEquals(180 * tick, s.m.onsetAge[0], 0.001f)
    }

    @Test
    fun separatePicksStampOnsetsNewestFirst() {
        val s = ArtMotionState()
        repeat(3) {
            var attack = 1f
            repeat(60) { s.advance(0.5f, attack, 0f, tick); attack *= 0.87f }
        }
        assertEquals(3, s.m.onsets)
        assertTrue(s.m.onsetAge[0] < s.m.onsetAge[1] && s.m.onsetAge[1] < s.m.onsetAge[2])
        assertEquals(1f, s.m.onsetAge[1] - s.m.onsetAge[0], 0.02f)
        assertTrue(s.m.onsetAge[3] >= ArtTiming.NO_ONSET)
    }

    @Test
    fun theMotionClockRunsOnlyWhileSoundingAndNeverFasterThanWallTime() {
        val s = ArtMotionState()
        s.run(2f, 1f)
        val full = s.m.clock
        assertEquals(2.0, full, 0.02)
        val quiet = ArtMotionState().apply { run(2f, 0.01f) }.m.clock
        assertTrue(quiet >= 2.0 * ArtTiming.CLOCK_RATE_FLOOR * 0.99 && quiet < 0.8)
        s.run(1f, 0f)
        assertEquals(full, s.m.clock, 0.0)
        s.advance(1f, 0f, 0f, 3f)
        assertTrue(s.m.clock - full <= ArtTiming.MAX_MOTION_DT + 1e-9)
    }

    @Test
    fun theLiveFrameSnapsToTheExactRestFrameInSilence() {
        val s = ArtMotionState()
        s.run(1f, 0.8f) { if (it == 0) 1f else 0f }
        s.advance(0f, 0f, 0.3f, tick)
        repeat(120) { s.advance(0f, 0f, 1e-4f, tick) }
        val frame = s.live(13, ArtMotion())
        assertTrue(frame.isRest)
        assertEquals(0f, frame.bloom, 0f)
        assertEquals(13, frame.seed)
        assertEquals(s.m.clock, frame.clock, 0.0)
    }

    @Test
    fun playLightMatchesTheDesktopCurve() {
        val full = PlayLight.bloomWeight(1f)
        assertEquals(0.45f, full * full, 1e-5f)
        assertEquals(0f, PlayLight.bloomWeight(0f), 0f)
        assertEquals(0.05f, PlayLight.coronaOpacity(0f, 0f), 1e-6f)
        assertEquals(0.08f, PlayLight.coronaOpacity(0f, 1f), 1e-6f)
        assertEquals(0.45f, PlayLight.coronaOpacity(1f, 1f), 1e-6f)
        assertEquals(0.35f, PlayLight.glow(0f, 1f), 1e-6f)
        assertEquals(1f, PlayLight.attackFromPickAge(0), 0f)
        assertEquals(0.87f, PlayLight.attackFromPickAge(1000L / 60), 0.01f)
        assertEquals(0f, PlayLight.attackFromPickAge(-1), 0f)
    }

    private fun refHash(v: UInt): Float {
        var x = v
        x = x xor (x shr 16)
        x *= 0x7feb352du
        x = x xor (x shr 15)
        x *= 0x846ca68bu
        x = x xor (x shr 16)
        return (x shr 8).toFloat() * (1f / 16777216f)
    }

    @Test
    fun hashAndLcgAreTheDesktopUint32Streams() {
        for (v in listOf(0u, 1u, 0x51u, 747796405u, 2654435761u, 0xFFFFFFFFu)) assertEquals(refHash(v), hash01(v.toInt()), 0f)
        var ref = 31u
        val f = Frand(31)
        repeat(500) {
            ref = ref * 1664525u + 1013904223u
            assertEquals(ref.toFloat() / 4294967295f, f.next(), 0f)
        }
        for (i in 0 until 2000) {
            val n = noise1(i * 0.137f, 7)
            assertTrue(n >= -0.5f && n <= 0.5f)
        }
    }

    @Test
    fun colourHelpersTruncateLikeIColor() {
        assertEquals(argb(127, 252, 222, 145), withA(argb(255, 252, 222, 145), 0.5f))
        assertEquals(argb(255, 0, 0, 0), withA(argb(0, 0, 0, 0), 2f))
        val m = mix(argb(255, 233, 138, 90), argb(255, 201, 74, 140), 0.3f)
        assertEquals(argb(255, 223, 118, 105), m)
        assertEquals(argb(255, 225, 187, 103), contrast(argb(255, 200, 162, 78), 0.1f))
    }
}
