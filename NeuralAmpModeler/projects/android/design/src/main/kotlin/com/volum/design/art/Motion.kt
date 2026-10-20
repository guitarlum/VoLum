package com.volum.design.art

import kotlin.math.exp
import kotlin.math.max
import kotlin.math.min
import kotlin.math.sin
import kotlin.math.sqrt

/**
 * `art/VoLumArtMotion.h` and the art half of `VoLumPlayLight.h`, ported 1:1.
 * Pure (no Android), unit-tested in `MotionTest` against the desktop doctests.
 */
object ArtTiming {
    const val NO_ONSET = 1e9f
    const val MAX_EVENT_AGE = 1.5f
    const val ONSET_SLOTS = 4
    const val ONSET_ATTACK = 0.4f
    const val ONSET_REARM = 0.2f
    const val ONSET_REJUMP = 0.3f
    const val ONSET_REFRACTORY = 0.09f
    const val CLOCK_RATE_FLOOR = 0.35f
    const val MAX_MOTION_DT = 0.05f

    /** `kPlayBloomGain`. */
    const val BLOOM_GAIN = 0.45f

    /** `PlaySurface::Tick` adds 0.015 rad to the idle phase per 60 Hz tick. */
    const val IDLE_PHASE_PER_SECOND = 0.015f * 60f

    /** `kPlayAttackDecay` per 60 Hz tick is exp(-(1/60) / 0.12 s). */
    const val ATTACK_TAU_S = 0.12f
}

/** One frame's motion input (`volumart::ArtMotion`). Mutable and reused: nothing is allocated per frame. */
class ArtMotion {
    var clock = 0.0
    var energy = 0f
    var pick = 0f
    var bloom = 0f
    val onsetAge = FloatArray(ArtTiming.ONSET_SLOTS) { ArtTiming.NO_ONSET }
    var onsets = 0
    var seed = 0

    val isRest: Boolean get() = energy <= 0f && pick <= 0f && onsetAge[0] >= ArtTiming.MAX_EVENT_AGE

    /** 0 at rest, 1 once anything sounds: the weight a crossfade-to-live gives the live element. */
    val wake: Float get() = smoothstep(0f, 0.25f, max(energy, pick))

    fun copyFrom(o: ArtMotion) {
        clock = o.clock
        energy = o.energy
        pick = o.pick
        bloom = o.bloom
        o.onsetAge.copyInto(onsetAge)
        onsets = o.onsets
        seed = o.seed
    }

    fun setRest(seed: Int) {
        clock = 0.0
        energy = 0f
        pick = 0f
        bloom = 0f
        onsetAge.fill(ArtTiming.NO_ONSET)
        onsets = 0
        this.seed = seed
    }
}

/** `volumart::ArtMotionState` plus `AdvanceArtMotion` / `LiveArtMotion`. */
class ArtMotionState {
    val m = ArtMotion()
    private var prevAttack = 0f
    private var sinceOnset = ArtTiming.NO_ONSET
    private var armed = true

    fun advance(energy: Float, attack: Float, bloom: Float, wallDt: Float) {
        val dt = wallDt.coerceIn(0f, ArtTiming.MAX_MOTION_DT)
        m.energy = energy.coerceIn(0f, 1f)
        m.pick = attack.coerceIn(0f, 1f)
        m.bloom = max(bloom, 0f)
        for (k in m.onsetAge.indices) m.onsetAge[k] = min(m.onsetAge[k] + dt, ArtTiming.NO_ONSET)
        sinceOnset = min(sinceOnset + dt, ArtTiming.NO_ONSET)
        if (m.pick < ArtTiming.ONSET_REARM || m.pick - prevAttack >= ArtTiming.ONSET_REJUMP) armed = true
        if (armed && m.pick >= ArtTiming.ONSET_ATTACK && sinceOnset >= ArtTiming.ONSET_REFRACTORY) {
            for (k in ArtTiming.ONSET_SLOTS - 1 downTo 1) m.onsetAge[k] = m.onsetAge[k - 1]
            m.onsetAge[0] = 0f
            m.onsets++
            sinceOnset = 0f
            armed = false
        }
        prevAttack = m.pick
        val a = max(m.energy, m.pick)
        if (a > 0f) m.clock += dt.toDouble() * (ArtTiming.CLOCK_RATE_FLOOR + (1f - ArtTiming.CLOCK_RATE_FLOOR) * a)
    }

    /** The frame to draw; snapped to the exact rest frame in silence so nothing stale leaks into the static art. */
    fun live(seed: Int, into: ArtMotion): ArtMotion {
        if (m.isRest) {
            val clock = m.clock
            val onsets = m.onsets
            into.setRest(seed)
            into.clock = clock
            into.onsets = onsets
        } else {
            into.copyFrom(m)
            into.seed = seed
        }
        return into
    }
}

/** `VoLumPlayLight.h` functions the stage and the art read. */
object PlayLight {
    /** `PlayGlowAmount`: sustained energy plus a short lift on each pick. */
    fun glow(energy: Float, attack: Float): Float = (energy + 0.35f * attack * (1f - energy)).coerceIn(0f, 1f)

    /** `PlayBloomWeight`: the Add weight whose square is the bloom gain. */
    fun bloomWeight(glow: Float): Float = sqrt(ArtTiming.BLOOM_GAIN * glow.coerceIn(0f, 1f))

    fun idlePulse(phase: Float): Float = 0.5f + 0.5f * sin(phase)

    /** `PlayCoronaOpacity`: the stage frame's opacity. */
    fun coronaOpacity(glow: Float, idlePulse: Float): Float {
        val g = glow.coerceIn(0f, 1f)
        return 0.05f + 0.03f * idlePulse.coerceIn(0f, 1f) * (1f - g) + 0.40f * g
    }

    /** Attack envelope of a pick [ageMs] ago (decays 0.87 per 60 Hz tick). */
    fun attackFromPickAge(ageMs: Long): Float = if (ageMs < 0) 0f else exp(-(ageMs / 1000f) / ArtTiming.ATTACK_TAU_S)
}

/** `ArtMotionSeed`: fractal case 0..14, or 100 + custom style. */
fun artMotionSeed(custom: Boolean, caseOrStyle: Int): Int = if (custom) 100 + caseOrStyle else caseOrStyle
