package com.volum.design.art.amps

import com.volum.design.art.AdditiveArtAnimator
import com.volum.design.art.ArtCanvas
import com.volum.design.art.ArtCommon
import com.volum.design.art.ArtInk
import com.volum.design.art.ArtMotion
import com.volum.design.art.Box
import com.volum.design.art.argb
import com.volum.design.art.mix
import kotlin.math.floor
import kotlin.math.min
import kotlin.math.pow
import kotlin.math.sin

/**
 * H&K TriAmp (`VoLumArtHkTriamp.h`, class A): three scope beams (clean,
 * crunch, lead) chase each other round the Lissajous knot, their phosphor
 * trails lengthening with the level; the static trace dims under them.
 */
class HkTriampAnimator : AdditiveArtAnimator(NebulaHeroes::hkTriamp) {
    private val knot = FloatArray(2 * SAMPLES)
    private val trail = FloatArray(2 * (SAMPLES + 8))
    private var trailN = 0
    private val white = argb(255, 240, 250, 252)
    private val beamCol = intArrayOf(mix(ArtInk.Teal, white, 0.15f), argb(255, 120, 160, 255), ArtInk.Gold)
    private val blooms = Array(BEAMS) { ArtCommon.MovingBloom(beamCol[it], 0.55f) }
    private val xy = FloatArray(2)

    override fun prepareExtras(r: Box, d: Float) {
        for (i in 0 until SAMPLES) {
            val t = i * 6.28318f / 720f
            knot[2 * i] = r.mw + sin(3f * t + 0.5f) * r.w * 0.4f
            knot[2 * i + 1] = r.mh + sin(4f * t) * r.h * 0.4f * 1.06f
        }
    }

    override fun baseDim(m: ArtMotion): Float = 0.3f * m.wake

    override fun drawExtras(g: ArtCanvas, r: Box, m: ArtMotion) {
        val wake = m.wake
        val amp = min(1f, wake * (0.55f + 0.45f * m.energy) + 0.35f * m.pick)
        if (amp <= 0f) return
        val trailLen = 36f + 90f * m.energy + 70f * m.pick
        val head0 = (m.clock * SAMPLES_PER_SECOND) % SAMPLES.toDouble()
        for (b in 0 until BEAMS) {
            val head = head0.toFloat() + b * (SAMPLES / BEAMS.toFloat())
            val tail = head - trailLen
            val col = beamCol[b]
            trail(tail, head)
            g.batchPolyline(col, 8f + 4f * m.pick, trail, trailN, 0.5f * amp, add = true)
            for (k in 0 until TIERS) {
                val f0 = k.toFloat() / TIERS
                val f1 = (k + 1).toFloat() / TIERS
                trail(tail + trailLen * f0, tail + trailLen * f1)
                g.batchPolyline(col, 2f + 1.2f * f1, trail, trailN, min(0.85f, amp * f1.pow(1.4f) * (0.8f + 0.25f * m.pick)), add = true, buttCap = true)
            }
            at(head)
            blooms[b].draw(g, xy[0], xy[1], 16f + 10f * m.energy + 12f * m.pick, amp)
            ArtCommon.glowDotAdd(g, col, white, xy[0], xy[1], 1.5f + 1.8f * m.pick, 4f + 3f * m.energy + 4f * m.pick, amp)
        }
    }

    private fun at(s: Float) {
        val wrapped = s - floor(s / SAMPLES) * SAMPLES
        val i0 = wrapped.toInt() % SAMPLES
        val i1 = (i0 + 1) % SAMPLES
        val f = wrapped - floor(wrapped)
        xy[0] = knot[2 * i0] + (knot[2 * i1] - knot[2 * i0]) * f
        xy[1] = knot[2 * i0 + 1] + (knot[2 * i1 + 1] - knot[2 * i0 + 1]) * f
    }

    private fun push() {
        if (2 * trailN + 1 >= trail.size) return
        trail[2 * trailN] = xy[0]
        trail[2 * trailN + 1] = xy[1]
        trailN++
    }

    private fun trail(s0: Float, s1: Float) {
        trailN = 0
        at(s0)
        push()
        var s = floor(s0) + 1f
        while (s < s1) {
            at(s)
            push()
            s += 1f
        }
        at(s1)
        push()
    }

    companion object {
        const val SAMPLES = 720
        const val BEAMS = 3
        const val TIERS = 8
        const val SAMPLES_PER_SECOND = 110.0
    }
}
