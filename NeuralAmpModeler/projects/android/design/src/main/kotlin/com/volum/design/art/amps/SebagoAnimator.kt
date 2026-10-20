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
import kotlin.math.cos
import kotlin.math.exp
import kotlin.math.floor
import kotlin.math.hypot
import kotlin.math.max
import kotlin.math.min
import kotlin.math.pow
import kotlin.math.sin
import kotlin.math.sqrt

/**
 * Sebago Texas Flood (`VoLumArtSebago.h`, class A): glitter walks the
 * Clifford orbit, rain sheets fall across the cloud with the level, and each
 * pick flares a star in the core whose light echo runs out through the filaments.
 */
class SebagoAnimator : AdditiveArtAnimator(NebulaHeroes::sebago) {
    private var cx = 0f
    private var cy = 0f
    private var rr = 1f
    private var xs = FloatArray(0)
    private var ys = FloatArray(0)
    private var fs = FloatArray(0)
    private var core = IntArray(0)
    private var echoPts = IntArray(0)

    private val litX = FloatArray(MAX_GLITTER + 2)
    private val litY = FloatArray(MAX_GLITTER + 2)
    private val litTier = IntArray(MAX_GLITTER + 2)
    private var litN = 0
    private val echoX = FloatArray(MAX_ECHO)
    private val echoY = FloatArray(MAX_ECHO)
    private val echoTier = IntArray(MAX_ECHO)
    private val echoWarm = BooleanArray(MAX_ECHO)
    private var echoN = 0
    private val dots = FloatArray(3 * (MAX_GLITTER + MAX_ECHO + 2))
    private val starX = FloatArray(ArtTiming.ONSET_SLOTS)
    private val starY = FloatArray(ArtTiming.ONSET_SLOTS)
    private val starFlash = FloatArray(ArtTiming.ONSET_SLOTS)
    private val starAge = FloatArray(ArtTiming.ONSET_SLOTS)
    private val white = argb(255, 240, 250, 252)
    private val starBloom = ArtCommon.MovingBloom(ArtInk.GoldHi, 0.5f)

    override fun prepareExtras(r: Box, d: Float) {
        cx = r.mw
        cy = r.mh
        rr = min(r.w, r.h)
        val a = -1.4; val b = 1.6; val c = 1.0; val dd = 0.75
        var x = 0.0
        var y = 0.0
        val s = rr * 0.22f
        val n = 9000
        val px = ArrayList<Float>(n); val py = ArrayList<Float>(n); val pf = ArrayList<Float>(n)
        for (i in 0 until n) {
            val nx = sin(a * y) + c * cos(a * x)
            val ny = sin(b * x) + dd * cos(b * y)
            x = nx
            y = ny
            if (i <= 120) continue
            val qx = cx + x.toFloat() * s
            val qy = cy - y.toFloat() * s
            if (qx < r.l || qx > r.r || qy < r.t || qy > r.b) continue
            px += qx; py += qy; pf += i.toFloat() / n
        }
        xs = px.toFloatArray(); ys = py.toFloatArray(); fs = pf.toFloatArray()
        val coreL = ArrayList<Int>()
        val ranked = ArrayList<Pair<Float, Int>>()
        for (k in xs.indices) {
            if (hypot(xs[k] - cx, ys[k] - cy) < 0.3f * rr) coreL += k
            val h = hash01(k * 2654435761L.toInt() xor 11)
            if (h < ECHO_KEEP) ranked += h to k
        }
        core = if (coreL.isEmpty()) IntArray(xs.size) { it } else coreL.toIntArray()
        echoPts = ranked.sortedBy { it.first }.map { it.second }.toIntArray()
    }

    override fun baseDim(m: ArtMotion): Float = 0.2f * m.wake

    override fun drawExtras(g: ArtCanvas, r: Box, m: ArtMotion) {
        if (xs.size < 2) return
        drawGlitter(g, m)
        drawEchoes(g, m)
    }

    private fun pointAt(j: Long, n: Long, period: Long): Int {
        var q = j % period
        if (q < 0) q += period
        return (if (q < n) q else period - q).toInt()
    }

    private fun drawGlitter(g: ArtCanvas, m: ArtMotion) {
        val amp = min(1f, m.wake * (0.3f + 0.7f * m.energy) + 0.35f * m.pick)
        if (amp <= 0f) return
        val n = xs.size.toLong()
        val period = 2 * (n - 1)
        val len = min(MAX_GLITTER.toFloat(), 60f + 240f * m.energy + 160f * m.pick)
        val u = m.clock * GLITTER_RATE
        val head = floor(u).toLong()
        val first = floor(u - len).toLong() + 1
        val depth = 0.5f * m.energy
        val sheetK = 6.2831853f / (0.9f * rr)
        val sheetPh = ((m.clock * SHEET_OMEGA) % 6.283185307179586).toFloat()
        litN = 0
        var j = head
        while (j >= first) {
            val x = (u - j).toFloat() / len
            val k = pointAt(j, n, period)
            var e = smoothstep(0f, 0.12f, x) * (1f - x).pow(1.4f)
            if (depth > 0f) {
                val s = (xs[k] - cx) * 0.33f + (ys[k] - cy) * 0.944f
                e *= 1f - depth * (0.5f - 0.5f * sin(sheetK * s - sheetPh))
            }
            if (e * amp >= 0.04f && litN < litX.size) {
                litX[litN] = xs[k]; litY[litN] = ys[k]; litTier[litN] = min(GLITTER_TIERS - 1, (e * GLITTER_TIERS).toInt()); litN++
            }
            j--
        }
        val tide = mix(ArtInk.Teal, ArtInk.Gold, fs[pointAt(head, n, period)])
        val coreCol = mix(tide, white, 0.4f)
        for (t in 0 until GLITTER_TIERS) {
            val wt = min(1f, amp * sqrt((t + 0.6f) / GLITTER_TIERS))
            if (t >= GLITTER_TIERS - 2) tier(g, litX, litY, litTier, null, litN, t, false, 2.4f + 0.8f * m.pick, tide, 0.5f * wt)
            tier(g, litX, litY, litTier, null, litN, t, false, 1.05f + 0.35f * m.pick, coreCol, wt)
        }
    }

    private fun drawEchoes(g: ArtCanvas, m: ArtMotion) {
        if (core.isEmpty()) return
        var nStars = 0
        echoN = 0
        val speed = 0.8f * rr
        val reach = 0.75f * rr
        for (s in 0 until ArtTiming.ONSET_SLOTS) {
            val age = m.onsetAge[s]
            if (age >= ArtTiming.MAX_EVENT_AGE) continue
            val fade = 1f - smoothstep(0.5f, ArtTiming.MAX_EVENT_AGE, age)
            if (fade <= 0f) continue
            val q = m.onsets - s
            val srcIdx = min(core.size - 1, (hash01(q * 747796405 + m.seed) * core.size).toInt())
            val src = core[srcIdx]
            val sx = xs[src]
            val sy = ys[src]
            val front = speed * age
            val gain = fade * (0.75f + 0.25f * m.energy)
            starX[nStars] = sx; starY[nStars] = sy; starFlash[nStars] = exp(-5f * age) * fade; starAge[nStars] = age; nStars++
            val inner = max(0f, front - 3.2f * SHELL_PX)
            for (k in echoPts) {
                if (echoN >= MAX_ECHO) break
                val dx = xs[k] - sx
                val dy = ys[k] - sy
                val d2 = dx * dx + dy * dy
                if (d2 >= front * front || d2 < inner * inner) continue
                val dist = sqrt(d2)
                val behind = front - dist
                val v = gain * smoothstep(0f, 4f, behind) * exp(-behind / SHELL_PX) * (1f - smoothstep(0.25f * rr, reach, dist))
                if (v < 0.05f) continue
                echoX[echoN] = xs[k]; echoY[echoN] = ys[k]; echoTier[echoN] = min(ECHO_TIERS - 1, (v * ECHO_TIERS).toInt()); echoWarm[echoN] = fs[k] >= 0.5f; echoN++
            }
        }
        val cool = mix(ArtInk.Teal, white, 0.35f)
        val warm = mix(ArtInk.Gold, ArtInk.GoldHi, 0.5f)
        for (t in 0 until ECHO_TIERS) {
            val wt = sqrt((t + 0.6f) / ECHO_TIERS)
            if (t > 0) {
                tier(g, echoX, echoY, echoTier, echoWarm, echoN, t, false, 2.6f, ArtInk.Teal, 0.5f * wt)
                tier(g, echoX, echoY, echoTier, echoWarm, echoN, t, true, 2.6f, ArtInk.Gold, 0.5f * wt)
            }
            tier(g, echoX, echoY, echoTier, echoWarm, echoN, t, false, 1.15f, cool, wt)
            tier(g, echoX, echoY, echoTier, echoWarm, echoN, t, true, 1.15f, warm, wt)
        }
        for (s in 0 until nStars) {
            if (starFlash[s] < 0.02f) continue
            starBloom.draw(g, starX[s], starY[s], rr * (0.08f + 0.2f * smoothstep(0f, 0.6f, starAge[s])), starFlash[s])
            ArtCommon.glowDotAdd(g, ArtInk.Gold, white, starX[s], starY[s], 1f + 1.4f * starFlash[s], 2.5f + 5.5f * starFlash[s], starFlash[s])
        }
    }

    private fun tier(g: ArtCanvas, lx: FloatArray, ly: FloatArray, lt: IntArray, lw: BooleanArray?, n: Int, tier: Int, warm: Boolean, radius: Float, c: Int, w: Float) {
        if (w <= 0f) return
        var k = 0
        for (i in 0 until n) {
            if (lt[i] != tier || (lw?.get(i) ?: false) != warm) continue
            dots[3 * k] = lx[i]; dots[3 * k + 1] = ly[i]; dots[3 * k + 2] = radius
            k++
        }
        g.batchDots(c, dots, k, min(w, 1f), add = true)
    }

    companion object {
        const val GLITTER_RATE = 700.0
        const val SHEET_OMEGA = 0.9
        const val MAX_GLITTER = 440
        const val GLITTER_TIERS = 4
        const val MAX_ECHO = 520
        const val ECHO_TIERS = 3
        const val ECHO_KEEP = 0.14f
        const val SHELL_PX = 14f
    }
}
