package com.volum.design.art.amps

import com.volum.design.art.AdditiveArtAnimator
import com.volum.design.art.ArtCanvas
import com.volum.design.art.ArtCommon
import com.volum.design.art.ArtInk
import com.volum.design.art.ArtMotion
import com.volum.design.art.ArtTiming
import com.volum.design.art.Box
import com.volum.design.art.argb
import com.volum.design.art.hash01
import com.volum.design.art.mix
import com.volum.design.art.noise1
import com.volum.design.art.smoothstep
import com.volum.design.art.withA
import kotlin.math.hypot
import kotlin.math.max
import kotlin.math.min
import kotlin.math.pow

/**
 * Lichtlaerm Prometheus (`VoLumArtLichtlaerm.h`, class A): embers climb the
 * six flanks of the anti-snowflake from its heart, higher with the level,
 * each pick shoots a spark to the tips, and the tips catch the fire.
 */
class LichtlaermAnimator : AdditiveArtAnimator(NebulaHeroes::lichtlaerm) {
    private var v = FloatArray(0)
    private var cx = 0f
    private var cy = 0f
    private var rr = 1f
    private val xy = FloatArray(2)
    private val halo = FloatArray(3 * SIDES)
    private val core = FloatArray(3 * SIDES)
    private val rays = FloatArray(36)
    private val rayCores = FloatArray(36)
    private val tipHalo = FloatArray(9)
    private val tipCore = FloatArray(9)
    private val ember = argb(255, 178, 74, 34)
    private val hot = argb(255, 255, 244, 214)
    private val heart = ArtCommon.MovingBloom(ArtInk.Gold, 0.6f)
    private val tipBloom = ArtCommon.MovingBloom(ArtInk.Gold, 0.55f)

    override fun prepareExtras(r: Box, d: Float) {
        v = NebulaHeroes.lichtlaermPath(r)
        cx = r.mw
        cy = r.mh
        rr = min(r.w * 0.46f, r.h * 0.5f)
    }

    override fun baseDim(m: ArtMotion): Float = 0.15f * m.wake

    override fun drawExtras(g: ArtCanvas, r: Box, m: ArtMotion) {
        if (v.size < 2 * (SEGS + 1)) return
        val wake = m.wake
        val e = m.energy
        val p = m.pick
        val t = m.clock.toFloat()
        val emberAmp = wake * (0.35f + 0.65f * e)
        val reach = 0.3f + e
        val emberLen = 0.05f + 0.08f * e
        var tipGlow = 0f
        if (emberAmp > 0f) for (j in 0 until EMBERS) {
            val u = (m.clock * EMBER_SPEED + j / EMBERS.toDouble()) % 1.0
            val head = u.toFloat() * EMBER_RUN
            val flick = 0.85f + 0.3f * noise1(t * 3.1f, 40 + j)
            comet(g, head, emberLen, ember, ArtInk.Gold, emberAmp * flick, reach, 7f + 3f * e, 3f + 1.5f * e)
            tipGlow += 0.4f * e * e * wake * smoothstep(0.93f, 1f, head) * (1f - smoothstep(1f, EMBER_RUN, head))
        }
        for (k in 0 until ArtTiming.ONSET_SLOTS) {
            val age = m.onsetAge[k]
            if (age >= ArtTiming.MAX_EVENT_AGE) continue
            val x = age / SPARK_RISE
            if (x < 1.4f) {
                val vary = hash01((m.onsets - k) * 747796405 + m.seed)
                comet(g, x * (1.25f - 0.25f * x), SPARK_LEN, mix(ember, ArtInk.Gold, 0.5f), mix(ArtInk.Gold, ArtInk.GoldHi, 0.2f + 0.4f * vary), 0.95f, 100f, 9f, 4f)
            }
            tipGlow += smoothstep(0.84f * SPARK_RISE, 1.04f * SPARK_RISE, age) * (1f - smoothstep(1.04f * SPARK_RISE, 1.4f, age))
        }
        tipGlow = min(1f, tipGlow)
        if (tipGlow > 0.01f) tips(g, tipGlow, t)
        val flick = 0.8f + 0.3f * noise1(t * 5.3f, 11) + 0.15f * noise1(t * 13.7f, 23)
        val heat = min(1f, wake * (0.4f + 0.6f * e) * flick + 0.7f * p)
        heart.draw(g, cx, cy, rr * (0.2f + 0.1f * e + 0.12f * p), heat)
        ArtCommon.glowDotAdd(g, ArtInk.Gold, ArtInk.GoldHi, cx, cy, 3.5f + 3f * e + 2f * p, 10f + 8f * e, heat)
    }

    private fun heatAt(s: Float, reach: Float) = smoothstep(0f, 0.035f, s) * (1f - smoothstep(0.5f * reach, reach, s))

    private fun index(side: Int, j: Int) = HALF + 2 * HALF * (side / 2) + if (side % 2 == 0) j else -j

    private fun at(side: Int, q: Float) {
        val qc = q.coerceIn(0f, HALF.toFloat())
        val j0 = min(qc.toInt(), HALF - 1)
        val f = qc - j0
        val i0 = index(side, j0)
        val i1 = index(side, j0 + 1)
        xy[0] = v[2 * i0] + (v[2 * i1] - v[2 * i0]) * f
        xy[1] = v[2 * i0 + 1] + (v[2 * i1 + 1] - v[2 * i0 + 1]) * f
    }

    private fun strokeSides(g: ArtCanvas, s0In: Float, s1In: Float, c: Int, width: Float, w: Float, butt: Boolean, stride: Int) {
        val s0 = max(0f, s0In)
        val s1 = min(1f, s1In)
        if (s1 <= s0) return
        val q0 = s0 * HALF
        val q1 = s1 * HALF
        g.clearPath()
        for (side in 0 until SIDES) {
            at(side, q0)
            g.moveTo(xy[0], xy[1])
            var j = q0.toInt() + stride
            while (j < q1) {
                val i = index(side, j)
                g.lineTo(v[2 * i], v[2 * i + 1])
                j += stride
            }
            at(side, q1)
            g.lineTo(xy[0], xy[1])
        }
        g.strokePath(c, width, w, add = true, roundJoin = true, buttCap = butt)
    }

    private fun comet(g: ArtCanvas, head: Float, len: Float, tailCol: Int, headCol: Int, amp: Float, reach: Float, glowW: Float, headR: Float) {
        val tail = head - len
        if (amp <= 0f || len <= 0f || head <= 0f || tail >= 1f) return
        strokeSides(g, tail, head, mix(tailCol, headCol, 0.7f), glowW, min(1f, 0.5f * amp * heatAt((head - 0.4f * len).coerceIn(0f, 1f), reach)), false, 3)
        for (k in 0 until TIERS) {
            val f0 = k.toFloat() / TIERS
            val f1 = (k + 1).toFloat() / TIERS
            val a = tail + len * f0
            val b = tail + len * f1
            if (b <= 0f || a >= 1f) continue
            val alpha = min(0.9f, amp * f1.pow(1.4f) * heatAt((0.5f * (a + b)).coerceIn(0f, 1f), reach))
            if (alpha < 0.01f) continue
            strokeSides(g, a, b, mix(tailCol, headCol, f1), 1.6f + 1.4f * f1, alpha, true, 1)
        }
        val ha = min(1f, amp * heatAt(head, reach) * (1f - smoothstep(0.96f, 1f, head)))
        if (ha < 0.01f) return
        for (side in 0 until SIDES) {
            at(side, head * HALF)
            halo[3 * side] = xy[0]; halo[3 * side + 1] = xy[1]; halo[3 * side + 2] = headR
            core[3 * side] = xy[0]; core[3 * side + 1] = xy[1]; core[3 * side + 2] = 0.35f * headR
        }
        g.batchDots(withA(headCol, 0.6f), halo, SIDES, ha, add = true)
        g.batchDots(hot, core, SIDES, ha, add = true)
    }

    private fun tips(g: ArtCanvas, glow: Float, t: Float) {
        val c30 = 0.8660254f
        for (i in 0 until 3) {
            val vi = i * 2 * HALF
            val tx = v[2 * vi]
            val ty = v[2 * vi + 1]
            val ox = tx - cx
            val oy = ty - cy
            val ol = max(1f, hypot(ox, oy))
            val bR = 12f + 16f * glow
            tipBloom.draw(g, tx - ox / ol * bR * 0.45f, ty - oy / ol * bR * 0.45f, bR, glow)
            val l = (4f + 14f * glow) * (0.85f + 0.3f * noise1(t * 7f, 60 + i))
            for (d in 0 until 3) {
                val dx = when (d) { 0 -> 0f; 1 -> c30; else -> -c30 }
                val dy = if (d == 0) 1f else 0.5f
                val o = 4 * (3 * i + d)
                rays[o] = tx - dx * l; rays[o + 1] = ty - dy * l; rays[o + 2] = tx + dx * l; rays[o + 3] = ty + dy * l
                rayCores[o] = tx - dx * l * 0.55f; rayCores[o + 1] = ty - dy * l * 0.55f; rayCores[o + 2] = tx + dx * l * 0.55f; rayCores[o + 3] = ty + dy * l * 0.55f
            }
            tipHalo[3 * i] = tx; tipHalo[3 * i + 1] = ty; tipHalo[3 * i + 2] = 2.5f + 6f * glow
            tipCore[3 * i] = tx; tipCore[3 * i + 1] = ty; tipCore[3 * i + 2] = 1f + 1.2f * glow
        }
        g.batchLines(ArtInk.Gold, 2.5f, rays, 9, 0.6f * glow, add = true)
        g.batchLines(hot, 1.2f, rayCores, 9, min(1f, 0.9f * glow), add = true)
        g.batchDots(withA(ArtInk.Gold, 0.7f), tipHalo, 3, min(1f, 0.85f * glow), add = true)
        g.batchDots(hot, tipCore, 3, min(1f, 0.85f * glow), add = true)
    }

    companion object {
        const val SEGS = 3072
        const val HALF = 512
        const val SIDES = 6
        const val TIERS = 5
        const val EMBERS = 3
        const val EMBER_SPEED = 0.3
        const val EMBER_RUN = 1.14f
        const val SPARK_RISE = 0.5f
        const val SPARK_LEN = 0.2f
    }
}
