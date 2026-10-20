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
import com.volum.design.art.smoothstep
import kotlin.math.exp
import kotlin.math.floor
import kotlin.math.max
import kotlin.math.min
import kotlin.math.pow

/**
 * Marshall JVM (`VoLumArtJvm210.h`, class A): eight comets run the Levy
 * curve's congruent level-3 copies at once, the seams flash as they cross,
 * and each pick re-folds the curve from a few creases up to its full depth.
 */
class Jvm210Animator : AdditiveArtAnimator(SceneHeroes::jvm210) {
    private var p = FloatArray(0)
    private val dots = FloatArray(3 * (1 shl MAX_FOLD_LEVEL))
    private val path = FloatArray(2 * (2 * (1 shl MAX_FOLD_LEVEL) + 1))
    private val xy = FloatArray(2)
    private val white = argb(255, 240, 250, 252)
    private val hot = argb(255, 255, 240, 200)
    private val binCol = IntArray(BINS) { mix(mix(ArtInk.Teal, ArtInk.Gold, (it + 0.5f) / BINS), white, 0.12f) }
    private val headBloom = Array(BINS) { ArtCommon.MovingBloom(binCol[it], 0.6f) }

    override fun prepareExtras(r: Box, d: Float) {
        p = SceneHeroes.jvmLevy(r)
    }

    override fun baseDim(m: ArtMotion): Float = 0.25f * m.wake

    override fun drawExtras(g: ArtCanvas, r: Box, m: ArtMotion) {
        if (p.size != 2 * (SEGS + 1)) return
        comets(g, m)
        folds(g, m)
    }

    private fun at(s: Float) {
        val i0 = s.toInt().coerceIn(0, SEGS - 1)
        val f = s - i0
        xy[0] = p[2 * i0] + (p[2 * i0 + 2] - p[2 * i0]) * f
        xy[1] = p[2 * i0 + 1] + (p[2 * i0 + 3] - p[2 * i0 + 1]) * f
    }

    private fun span(g: ArtCanvas, s0: Float, s1: Float) {
        if (s1 - s0 < 1e-3f) return
        at(s0)
        g.moveTo(xy[0], xy[1])
        var s = floor(s0) + 1f
        while (s < s1) {
            at(s)
            g.lineTo(xy[0], xy[1])
            s += 1f
        }
        at(s1)
        g.lineTo(xy[0], xy[1])
    }

    private fun run(g: ArtCanvas, s0: Float, s1: Float) {
        val n = SEGS.toFloat()
        when {
            s1 <= 0f -> span(g, s0 + n, s1 + n)
            s0 < 0f -> { span(g, s0 + n, n); span(g, 0f, s1) }
            else -> span(g, s0, s1)
        }
    }

    private fun comets(g: ArtCanvas, m: ArtMotion) {
        val wake = m.wake
        val amp = min(1f, wake * (0.5f + 0.5f * m.energy) + 0.35f * m.pick)
        if (amp <= 0f) return
        val head = ((m.clock * SEGS_PER_SECOND) % COPY_SEGS.toDouble()).toFloat()
        val trail = 32f + 56f * m.energy + 40f * m.pick
        for (bin in 0 until BINS) {
            val col = binCol[bin]
            val c0 = bin * COPIES / BINS
            val c1 = (bin + 1) * COPIES / BINS
            g.clearPath()
            for (c in c0 until c1) run(g, c * COPY_SEGS + head - trail, c * COPY_SEGS + head)
            g.strokePath(col, 7f + 4f * m.pick, 0.42f * amp, add = true, roundJoin = true)
            for (k in 0 until TIERS) {
                val f0 = k.toFloat() / TIERS
                val f1 = (k + 1).toFloat() / TIERS
                g.clearPath()
                for (c in c0 until c1) {
                    val tail = c * COPY_SEGS + head - trail
                    run(g, tail + trail * f0, tail + trail * f1)
                }
                g.strokePath(col, 1.8f + 1.4f * f1, min(0.9f, amp * f1.pow(1.3f) * (0.85f + 0.3f * m.pick)), add = true, roundJoin = true, buttCap = true)
            }
            var n = 0
            for (c in c0 until c1) {
                at(c * COPY_SEGS + head)
                dots[3 * n] = xy[0]; dots[3 * n + 1] = xy[1]; dots[3 * n + 2] = 3.5f + 2.5f * m.energy + 4f * m.pick
                n++
            }
            g.batchDots(col, dots, n, 0.5f * amp, add = true)
        }
        for (c in 0 until COPIES) {
            at(c * COPY_SEGS + head)
            dots[3 * c] = xy[0]; dots[3 * c + 1] = xy[1]; dots[3 * c + 2] = 1.3f + 1.5f * m.pick
            if (m.pick > 0.01f) headBloom[c * BINS / COPIES].draw(g, xy[0], xy[1], 12f + 14f * m.pick, m.pick)
        }
        g.batchDots(white, dots, COPIES, amp, add = true)
        val into = 1f - (COPY_SEGS - head) / 12f
        val beat = max(exp(-head / 20f), if (into > 0f) into * into else 0f)
        val jw = amp * beat * (0.55f + 0.45f * m.energy)
        if (jw <= 0.01f) return
        for (bin in 0 until BINS) {
            var n = 0
            for (j in 0..COPIES) if (min(BINS - 1, j * BINS / COPIES) == bin) {
                dots[3 * n] = p[2 * j * COPY_SEGS]; dots[3 * n + 1] = p[2 * j * COPY_SEGS + 1]; dots[3 * n + 2] = 5f + 4f * m.energy
                n++
            }
            g.batchDots(binCol[bin], dots, n, min(1f, 0.6f * jw), add = true)
        }
        for (j in 0..COPIES) {
            dots[3 * j] = p[2 * j * COPY_SEGS]; dots[3 * j + 1] = p[2 * j * COPY_SEGS + 1]; dots[3 * j + 2] = 1.8f
        }
        g.batchDots(white, dots, COPIES + 1, min(1f, jw), add = true)
    }

    private fun folds(g: ArtCanvas, m: ArtMotion) {
        for (k in 0 until ArtTiming.ONSET_SLOTS) {
            val age = m.onsetAge[k]
            if (age >= FOLD_END) continue
            val older = if (k + 1 < ArtTiming.ONSET_SLOTS) m.onsetAge[k + 1] else ArtTiming.NO_ONSET
            var w = smoothstep(0f, 0.12f, age) * (1f - smoothstep(0.8f, FOLD_END, age))
            w *= 0.2f + 0.8f * smoothstep(0.12f, 0.5f, older - age)
            if (k > 0) w *= 1f - smoothstep(0f, 0.2f, m.onsetAge[k - 1])
            if (w <= 0.004f) continue
            val level = min(FOLD_DEPTH - 0.001f, FOLD_DEPTH * (age / 1.1f).pow(0.85f))
            val tint = hash01((m.onsets - k) * 747796405 + m.seed)
            fold(g, level, w, mix(ArtInk.Gold, ArtInk.Teal, 0.35f * tint))
        }
    }

    private fun fold(g: ArtCanvas, level: Float, w: Float, col: Int) {
        val lev = min(MAX_FOLD_LEVEL, level.toInt())
        val s = smoothstep(0f, 1f, level - lev)
        val n = 1 shl lev
        val step = SEGS shr lev
        val crease = 1f - smoothstep(3f, 6f, level)
        var pn = 0
        for (j in 0 until n) {
            val a = j * step
            val b = a + step
            val c = a + step / 2
            val mx = 0.5f * (p[2 * a] + p[2 * b])
            val my = 0.5f * (p[2 * a + 1] + p[2 * b + 1])
            val vx = mx + (p[2 * c] - mx) * s
            val vy = my + (p[2 * c + 1] - my) * s
            path[2 * pn] = p[2 * a]; path[2 * pn + 1] = p[2 * a + 1]; pn++
            path[2 * pn] = vx; path[2 * pn + 1] = vy; pn++
            dots[3 * j] = vx; dots[3 * j + 1] = vy; dots[3 * j + 2] = 3.5f
        }
        path[2 * pn] = p[2 * SEGS]; path[2 * pn + 1] = p[2 * SEGS + 1]; pn++
        g.batchPolyline(col, 6f, path, pn, 0.4f * w, add = true)
        g.batchPolyline(col, 1.7f, path, pn, min(1f, 0.95f * w), add = true)
        if (crease <= 0f) return
        g.batchDots(col, dots, n, min(1f, 0.6f * w * crease), add = true)
        for (j in 0 until n) dots[3 * j + 2] = 1.2f
        g.batchDots(hot, dots, n, min(1f, w * crease), add = true)
    }

    companion object {
        const val SEGS = 2048
        const val COPIES = 8
        const val COPY_SEGS = SEGS / COPIES
        const val BINS = 4
        const val TIERS = 4
        const val MAX_FOLD_LEVEL = 8
        const val FOLD_DEPTH = 9f
        const val FOLD_END = 1.3f
        const val SEGS_PER_SECOND = 160.0
    }
}
