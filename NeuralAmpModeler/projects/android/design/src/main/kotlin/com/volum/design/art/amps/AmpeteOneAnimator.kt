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
import kotlin.math.atan2
import kotlin.math.ceil
import kotlin.math.cos
import kotlin.math.floor
import kotlin.math.hypot
import kotlin.math.max
import kotlin.math.min
import kotlin.math.pow
import kotlin.math.sin
import kotlin.math.sqrt

/**
 * Ampete One (`VoLumArtAmpeteOne.h`, class A): flames run the dragon's body
 * from tail to head, warming from teal to gold; embers rise off it behind the
 * front, the mouth exhales a fan of sparks, and each pick sends a fast breath
 * through the whole dragon.
 */
class AmpeteOneAnimator : AdditiveArtAnimator(NebulaHeroes::ampeteOne) {
    private var pts = FloatArray(0)
    private var n = 0
    private var unit = 1f
    private var plume = 1f
    private var glowW = 7.5f
    private var coreW = 2.5f
    private var tailX = 0f
    private var tailY = 0f
    private var headX = 0f
    private var headY = 0f
    private var mouth = 3.14159265f
    private val path = FloatArray(2 * 4200)
    private var pathN = 0
    private val xy = FloatArray(2)

    private val cHead = FloatArray(2 + ArtTiming.ONSET_SLOTS)
    private val cLen = FloatArray(2 + ArtTiming.ONSET_SLOTS)
    private val cAmp = FloatArray(2 + ArtTiming.ONSET_SLOTS)
    private val cSpeed = FloatArray(2 + ArtTiming.ONSET_SLOTS)
    private val cShed = FloatArray(2 + ArtTiming.ONSET_SLOTS)
    private val cSalt = IntArray(2 + ArtTiming.ONSET_SLOTS)

    private val spX = FloatArray(MAX_SPARKS)
    private val spY = FloatArray(MAX_SPARKS)
    private val spR = FloatArray(MAX_SPARKS)
    private val spA = FloatArray(MAX_SPARKS)
    private val spHue = IntArray(MAX_SPARKS)
    private var spN = 0
    private val dots = FloatArray(3 * MAX_SPARKS)
    private val white = argb(255, 240, 250, 252)
    private val halo = intArrayOf(mix(ArtInk.Teal, ArtInk.Gold, 0.3f), ArtInk.Gold)
    private val core = intArrayOf(mix(ArtInk.Teal, ArtInk.GoldHi, 0.4f), mix(ArtInk.Gold, ArtInk.GoldHi, 0.6f))
    private val tailBloom = ArtCommon.MovingBloom(ArtInk.Teal, 0.6f)
    private val headBloom = ArtCommon.MovingBloom(ArtInk.Gold, 0.55f)
    private val frontBloom = Array(GLOW_BUCKETS) { ArtCommon.MovingBloom(mix(ArtInk.Teal, ArtInk.Gold, 0.5f * it / (GLOW_BUCKETS - 1)), 0.5f) }

    override fun prepareExtras(r: Box, d: Float) {
        pts = NebulaHeroes.ampetePath(r)
        n = pts.size / 2
        if (n < 2) return
        var mnx = 1e9f; var mny = 1e9f; var mxx = -1e9f; var mxy = -1e9f
        for (i in 0 until n) { mnx = min(mnx, pts[2 * i]); mny = min(mny, pts[2 * i + 1]); mxx = max(mxx, pts[2 * i]); mxy = max(mxy, pts[2 * i + 1]) }
        val acx = (mnx + mxx) * 0.5f
        val acy = (mny + mxy) * 0.5f
        // One dragon step is the hero's scale S: ~4 px in the mono panel, ~2 px in a dual lane.
        unit = (hypot(pts[2] - pts[0], pts[3] - pts[1]) / 4f).coerceIn(0.45f, 1.25f)
        plume = max(0.7f, unit)
        glowW = 2.5f + 5f * unit
        coreW = 1.1f + 1.4f * unit
        tailX = pts[0]; tailY = pts[1]
        headX = pts[2 * (n - 1)]; headY = pts[2 * (n - 1) + 1]
        var sx = headX - pts[2 * (n - 2)]
        var sy = headY - pts[2 * (n - 2) + 1]
        val sl = hypot(sx, sy)
        sx = if (sl > 0f) sx / sl else 0f
        sy = if (sl > 0f) sy / sl else 0f
        var ax = headX - acx
        var ay = headY - acy
        val al = hypot(ax, ay)
        ax = if (al > 1e-3f) ax / al else 0f
        ay = if (al > 1e-3f) ay / al else 0f
        val dx = sx + ax
        val dy = sy + ay
        mouth = if (dx == 0f && dy == 0f) 3.14159265f else atan2(dy, dx)
    }

    override fun baseDim(m: ArtMotion): Float = 0.2f * m.wake

    override fun drawExtras(g: ArtCanvas, r: Box, m: ArtMotion) {
        if (n < 2) return
        val wake = m.wake
        val last = (n - 1).toFloat()
        val cycle = n.toDouble() + OFFSTAGE
        var nc = 0
        val clockAmp = min(1f, wake * (0.45f + 0.55f * m.energy) + 0.3f * m.pick)
        for (c in 0 until 2) {
            val amp = clockAmp * (if (c == 0) 1f else smoothstep(0.3f, 0.75f, m.energy))
            if (amp <= 0f) continue
            val run = m.clock * CLOCK_SPEED + c * 0.5 * cycle
            val lap = floor(run / cycle)
            cHead[nc] = (run - lap * cycle).toFloat(); cLen[nc] = 140f + 110f * m.energy + 60f * m.pick
            cAmp[nc] = amp; cSpeed[nc] = CLOCK_SPEED; cShed[nc] = 0.4f
            cSalt[nc] = (lap.toLong().toInt() * 2 + c) * 2246822519L.toInt() xor 0x5eed
            nc++
        }
        var ignite = 0f
        for (k in 0 until ArtTiming.ONSET_SLOTS) {
            val age = m.onsetAge[k]
            if (age >= ArtTiming.MAX_EVENT_AGE) continue
            val pickId = m.onsets - k
            val hv = hash01(pickId * 747796405 + m.seed)
            val hs = hash01(pickId * 2891336453L.toInt() xor (m.seed + 77))
            val amp = (0.75f + 0.25f * hv) * (0.7f + 0.3f * m.energy) * (1f - smoothstep(1.15f, 1.45f, age))
            if (amp <= 0f) continue
            val speed = PICK_SPEED * (0.92f + 0.16f * hs)
            cHead[nc] = age * speed; cLen[nc] = PICK_LEN; cAmp[nc] = amp; cSpeed[nc] = speed; cShed[nc] = 0.18f
            cSalt[nc] = pickId * 2654435761L.toInt() xor 0xf1a3
            nc++
            ignite = max(ignite, amp * (1f - smoothstep(0f, 0.3f, age)))
        }
        spN = 0
        var flare = 0f
        for (i in 0 until nc) {
            comet(g, i, last)
            shed(i, last)
            exhale(i)
            val since = max(0f, (cHead[i] - last - cLen[i]) / cSpeed[i])
            flare += cAmp[i] * smoothstep(last - 200f, last, cHead[i]) * (1f - smoothstep(0f, 0.3f, since))
        }
        sparks(g)
        if (ignite > 0f) {
            tailBloom.draw(g, tailX, tailY, 12f + 10f * ignite, ignite)
            ArtCommon.glowDotAdd(g, ArtInk.Teal, white, tailX, tailY, 1.5f + 1.5f * ignite, 4f + 5f * ignite, ignite)
        }
        val ph = ((m.clock * 2.0) % 6.283185307179586).toFloat()
        val breath = wake * (0.1f + 0.2f * m.energy) * (0.5f + 0.5f * sin(ph))
        flare = min(1f, flare)
        val heat = min(1f, flare + breath)
        if (heat > 0f) {
            headBloom.draw(g, headX, headY, 16f + 26f * flare, heat)
            ArtCommon.glowDotAdd(g, ArtInk.Gold, ArtInk.GoldHi, headX, headY, 2.5f + 2.5f * flare, 6f + 8f * flare, 0.85f * heat)
        }
    }

    private fun at(s: Float) {
        val i0 = min(s.toInt(), n - 2)
        val f = s - i0
        xy[0] = pts[2 * i0] + (pts[2 * i0 + 2] - pts[2 * i0]) * f
        xy[1] = pts[2 * i0 + 1] + (pts[2 * i0 + 3] - pts[2 * i0 + 1]) * f
    }

    private fun span(s0: Float, s1: Float) {
        pathN = 0
        at(s0)
        path[0] = xy[0]; path[1] = xy[1]; pathN = 1
        var i = s0.toInt() + 1
        while (i < s1 && pathN < path.size / 2 - 1) {
            path[2 * pathN] = pts[2 * i]; path[2 * pathN + 1] = pts[2 * i + 1]; pathN++
            i++
        }
        at(s1)
        path[2 * pathN] = xy[0]; path[2 * pathN + 1] = xy[1]; pathN++
    }

    private fun comet(g: ArtCanvas, i: Int, last: Float) {
        val head = cHead[i]
        val len = cLen[i]
        val amp = cAmp[i]
        val s0 = max(0f, head - len)
        val s1 = min(last, head)
        if (s1 - s0 < 0.5f) return
        val glowT = 0.5f * (s0 + s1) / last
        val glow = mix(ArtInk.Teal, ArtInk.Gold, glowT)
        span(s0, s1)
        g.batchPolyline(glow, glowW, path, pathN, 0.45f * amp, add = true)
        for (k in 0 until TIERS) {
            val f0 = k.toFloat() / TIERS
            val f1 = (k + 1).toFloat() / TIERS
            val a0 = max(s0, head - len * (1f - f0))
            val a1 = min(s1, head - len * (1f - f1))
            if (a1 - a0 < 0.25f) continue
            span(a0, a1)
            g.batchPolyline(mix(ArtInk.Teal, ArtInk.GoldHi, 0.5f * (a0 + a1) / last), coreW * (0.75f + 0.25f * f1), path, pathN, min(0.9f, amp * (0.25f + 0.75f * f1.pow(1.4f))), add = true, buttCap = true)
        }
        if (head <= last) {
            at(head)
            val b = ((glowT / 0.5f) * (GLOW_BUCKETS - 1) + 0.5f).toInt().coerceIn(0, GLOW_BUCKETS - 1)
            frontBloom[b].draw(g, xy[0], xy[1], unit * (9f + 7f * amp), amp)
            ArtCommon.glowDotAdd(g, glow, mix(glow, white, 0.5f), xy[0], xy[1], 0.4f + 1.1f * unit, 1f + 3f * unit, amp)
        }
    }

    private fun shed(i: Int, last: Float) {
        val head = cHead[i]
        val speed = cSpeed[i]
        val back = speed * EMBER_LIFE
        val k0 = max(0, ceil((head - back) / SITE_STEP).toInt())
        val k1 = (min(head, last) / SITE_STEP).toInt()
        for (k in k0..k1) {
            val id = k * 2654435761L.toInt() xor cSalt[i]
            if (hash01(id) >= cShed[i]) continue
            val p = k * SITE_STEP
            if (p >= n) continue
            val age = (head - p) / speed
            val x = age / EMBER_LIFE
            if (x < 0f || x >= 1f) continue
            val h1 = hash01(id + 0x68e31da4)
            val h2 = hash01(id + 0xb5297a4d.toInt())
            val ex = pts[2 * p] + unit * 6f * x * noise1(age * 2.6f + 9f * h1, id)
            val ey = pts[2 * p + 1] - unit * (8f + 14f * h1) * x * (1.6f - 0.6f * x)
            val fade = 1f - x
            addSpark(ex, ey, unit * (0.7f + 0.8f * h2) * (1f - 0.4f * x), cAmp[i] * fade * sqrt(fade), if (p < 0.45f * last) 0 else 1)
        }
    }

    private fun exhale(i: Int) {
        val out = cHead[i] - (n - 1)
        if (out <= 0f) return
        var q = 0
        while (true) {
            val e = q * PLUME_STEP
            if (e > out) break
            val keep = 1f - smoothstep(0.7f * cLen[i], cLen[i], e)
            if (keep <= 0f) break
            val age = (out - e) / cSpeed[i]
            val x = age / PLUME_LIFE
            if (x < 1f) {
                val id = (q * 2654435761L.toInt() xor cSalt[i]) + 0x7f4a7c15
                val h1 = hash01(id)
                val h2 = hash01(id + 0x68e31da4)
                val h3 = hash01(id + 0xb5297a4d.toInt())
                val ang = mouth + (h1 - 0.5f) * 0.9f + 0.3f * x * noise1(age * 3f, id)
                val reach = plume * (20f + 28f * h2) * (0.6f + 0.4f * cAmp[i]) * x * (2f - x)
                val px = headX + cos(ang) * reach
                val py = headY + sin(ang) * reach - plume * (5f + 9f * h3) * x * x
                val fade = 1f - x
                addSpark(px, py, plume * (0.9f + 1f * h3) * (1f + 0.5f * x), cAmp[i] * keep * fade * sqrt(fade), 1)
            }
            q++
        }
    }

    private fun addSpark(x: Float, y: Float, r: Float, a: Float, hue: Int) {
        if (a <= 0.02f || spN >= MAX_SPARKS) return
        spX[spN] = x; spY[spN] = y; spR[spN] = r; spA[spN] = min(a, 1f); spHue[spN] = hue; spN++
    }

    private fun sparks(g: ArtCanvas) {
        if (spN == 0) return
        for (hue in 0 until 2) for (k in 0 until ALPHA_BINS) {
            val lo = k.toFloat() / ALPHA_BINS
            val hi = (k + 1).toFloat() / ALPHA_BINS
            for (pass in 0 until 2) {
                var c = 0
                for (i in 0 until spN) if (spHue[i] == hue && spA[i] > lo && spA[i] <= hi) {
                    dots[3 * c] = spX[i]; dots[3 * c + 1] = spY[i]; dots[3 * c + 2] = if (pass == 0) spR[i] * 2.4f else spR[i]; c++
                }
                g.batchDots(if (pass == 0) halo[hue] else core[hue], dots, c, if (pass == 0) 0.3f + 0.3f * hi else min(1f, 0.45f + 0.55f * hi), add = true)
            }
        }
    }

    companion object {
        const val CLOCK_SPEED = 700f
        const val PICK_SPEED = 4600f
        const val OFFSTAGE = 720.0
        const val PICK_LEN = 260f
        const val EMBER_LIFE = 0.5f
        const val PLUME_LIFE = 0.42f
        const val PLUME_STEP = 10f
        const val SITE_STEP = 16
        const val TIERS = 4
        const val ALPHA_BINS = 3
        const val MAX_SPARKS = 400
        const val GLOW_BUCKETS = 9
    }
}
