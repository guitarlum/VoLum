package com.volum.design.art.amps

import android.graphics.Matrix
import com.volum.design.art.ArtAnimator
import com.volum.design.art.ArtCanvas
import com.volum.design.art.ArtCommon
import com.volum.design.art.ArtInk
import com.volum.design.art.ArtLayer
import com.volum.design.art.ArtMotion
import com.volum.design.art.ArtTiming
import com.volum.design.art.Box
import com.volum.design.art.argb
import com.volum.design.art.hash01
import com.volum.design.art.mix
import com.volum.design.art.noise1
import com.volum.design.art.smoothstep
import kotlin.math.cos
import kotlin.math.exp
import kotlin.math.max
import kotlin.math.min
import kotlin.math.pow
import kotlin.math.sin
import kotlin.math.sqrt

/**
 * Fryette Deliverance (`VoLumArtFryette.h`, class B): the spiral arms turn
 * about the core in three radial rings (the inner ring winding tighter with
 * the level), disc stars light as an arm passes them, knots twinkle, and each
 * pick bursts a nova on an arm.
 */
class FryetteAnimator : ArtAnimator() {
    private val under = ArtLayer()
    private val bins = Array(BINS) { ArtLayer() }
    private val rest = ArtLayer()
    private val spin = Matrix()
    private val binDeg = FloatArray(BINS)
    private val binRad = FloatArray(BINS)
    private var cx = 0f
    private var cy = 0f
    private var rr = 1f
    private var maxR = 1f

    private val dRr = FloatArray(DISC_STARS)
    private val dTheta0 = FloatArray(DISC_STARS)
    private val dArm = FloatArray(DISC_STARS)
    private val dSize = FloatArray(DISC_STARS)
    private val dOmega = DoubleArray(DISC_STARS)
    private val dBin = IntArray(DISC_STARS)
    private val dId = IntArray(DISC_STARS)
    private val kTheta = FloatArray(KNOTS)
    private val kRr = FloatArray(KNOTS)
    private val kFreq = FloatArray(KNOTS)
    private val kPhase = FloatArray(KNOTS)
    private val kSize = FloatArray(KNOTS)
    private val kBin = IntArray(KNOTS)
    private val litX = FloatArray(DISC_STARS + KNOTS)
    private val litY = FloatArray(DISC_STARS + KNOTS)
    private val litR = FloatArray(DISC_STARS + KNOTS)
    private val litA = FloatArray(DISC_STARS + KNOTS)
    private var litN = 0
    private val dots = FloatArray(3 * (DISC_STARS + KNOTS))
    private val seg = FloatArray(8)
    private val starWhite = argb(255, 236, 244, 255)
    private val tierCol = intArrayOf(mix(ArtInk.Mid, ArtInk.Blue, 0.5f), ArtInk.Blue, mix(ArtInk.Blue, argb(255, 236, 244, 255), 0.55f))
    private val coreBloom = ArtCommon.MovingBloom(ArtInk.Gold, 0.4f)
    private val novaBlue = ArtCommon.MovingBloom(ArtInk.Blue, 0.5f)
    private val novaGold = ArtCommon.MovingBloom(ArtInk.GoldHi, 0.5f)

    override fun prepare(r: Box, d: Float) {
        if (!under.matches(r, d)) under.build(r, d) { SceneHeroes.fryetteBloom(it, r) }
        for (k in 0 until BINS) if (!bins[k].matches(r, d)) bins[k].build(r, d) { SceneHeroes.fryetteArms(it, r, k) }
        if (!rest.matches(r, d)) rest.build(r, d) { SceneHeroes.fryette(it, r) }
        under.addBitmap()
        bins.forEach { it.addBitmap() }
        rest.addBitmap()
        cx = r.mw
        cy = r.mh
        rr = min(r.w, r.h)
        placeStars()
    }

    override fun drop() {
        under.reset()
        bins.forEach { it.reset() }
        rest.reset()
    }

    private fun placeStars() {
        maxR = rr * 0.46f
        val pi = 3.14159f
        for (i in 0 until DISC_STARS) {
            val id = i * 2654435761L.toInt()
            val rho = 0.12f + 0.84f * sqrt(hash01(id xor 0x51))
            val tArm = rho.pow(1f / 0.7f)
            dRr[i] = maxR * rho
            dTheta0[i] = 2f * pi * hash01(id xor 0x52)
            dArm[i] = tArm * 4.2f * pi
            dSize[i] = 0.6f + 0.5f * hash01(id xor 0x53)
            dOmega[i] = PATTERN_RAD_PER_SECOND * 0.85 / (rho.toDouble() + 0.25)
            dBin[i] = min(2, (tArm * 3f).toInt())
            dId[i] = i + 101
        }
        for (j in 0 until KNOTS) {
            val id = j * 2654435761L.toInt()
            val u = 0.16f + 0.8f * hash01(id xor 0x61)
            kTheta[j] = (j and 1) * pi + u * 4.2f * pi + 0.05f * (hash01(id xor 0x62) - 0.5f)
            kRr[j] = maxR * u.pow(0.7f) * (1f + 0.08f * (hash01(id xor 0x63) - 0.5f))
            kFreq[j] = 1.3f + 1.1f * hash01(id xor 0x64)
            kPhase[j] = 2f * pi * hash01(id xor 0x65)
            kSize[j] = 1f + 0.5f * (1f - u)
            kBin[j] = min(2, (u * 3f).toInt())
        }
    }

    override fun draw(g: ArtCanvas, r: Box, m: ArtMotion) {
        if (m.isRest) {
            g.drawLayerLit(rest, m.bloom)
            return
        }
        val wake = m.wake
        val pattern = -((m.clock * PATTERN_DEG_PER_SECOND) % 360.0).toFloat()
        for (k in 0 until BINS) {
            binDeg[k] = pattern + WIND_DEG[k] * m.energy
            binRad[k] = binDeg[k] * (3.14159265f / 180f)
        }
        g.drawLayerLit(under, m.bloom)
        if (wake < 1f) for (b in bins) g.drawLayer(b, 1f - wake)
        if (wake > 0f) for (k in 0 until BINS) {
            g.aboutPoint(spin, cx, cy, binDeg[k])
            g.drawLayerXform(bins[k], spin, wake)
            val lift = min(1f, m.bloom + BIN_LIFT[k] * m.energy + 0.2f * m.pick) * wake
            if (lift > 0.02f) g.drawLayerXform(bins[k], spin, lift, additive = true)
        }
        if (wake > 0f) {
            lightStars(m, wake)
            drawStars(g)
        }
        novae(g, m, wake)
        SceneHeroes.fryetteCore(g, r)
        val breathe = 0.5f + 0.5f * sin(((m.clock * 1.3) % TWO_PI).toFloat())
        val glow = min(1f, wake * (0.3f + 0.4f * m.energy + 0.15f * m.energy * breathe) + 0.5f * m.pick)
        coreBloom.draw(g, cx, cy, rr * (0.08f + 0.05f * m.energy + 0.06f * m.pick), glow)
        ArtCommon.glowDotAdd(g, ArtInk.Gold, ArtInk.GoldHi, cx, cy, 4f + 3f * m.pick, 8f + 6f * m.energy, 0.8f * glow)
    }

    private fun lightStars(m: ArtMotion, wake: Float) {
        litN = 0
        val tf = m.clock.toFloat()
        val disc = wake * (0.35f + 0.65f * m.energy)
        for (i in 0 until DISC_STARS) {
            val ang = dTheta0[i] - ((m.clock * dOmega[i]) % TWO_PI).toFloat()
            val d = ang - (dArm[i] + binRad[dBin[i]])
            val crest = (0.5f + 0.5f * cos(2f * d)).pow(20f)
            val a = disc * (0.22f + 0.78f * crest) * (0.8f + 0.4f * noise1(tf * 0.9f, dId[i]))
            if (a > 0.02f) { litX[litN] = cx + dRr[i] * cos(ang); litY[litN] = cy + dRr[i] * sin(ang); litR[litN] = dSize[i] + 0.5f * crest; litA[litN] = min(a, 1f); litN++ }
        }
        val knotAmp = wake * (0.3f + 0.7f * m.energy)
        for (j in 0 until KNOTS) {
            val sw = sin(((m.clock * kFreq[j]) % TWO_PI).toFloat() + kPhase[j])
            val a = if (sw > 0f) knotAmp * sw * sw * sw * sw else 0f
            if (a <= 0.02f) continue
            val ang = kTheta[j] + binRad[kBin[j]]
            litX[litN] = cx + kRr[j] * cos(ang); litY[litN] = cy + kRr[j] * sin(ang); litR[litN] = kSize[j]; litA[litN] = min(a, 1f); litN++
        }
    }

    private fun drawStars(g: ArtCanvas) {
        for (k in 0 until TIERS) {
            val lo = k.toFloat() / TIERS
            val hi = (k + 1).toFloat() / TIERS
            for (pass in (if (k == 0) 1 else 0) until 2) {
                var n = 0
                for (i in 0 until litN) if (litA[i] > lo && litA[i] <= hi) {
                    dots[3 * n] = litX[i]; dots[3 * n + 1] = litY[i]; dots[3 * n + 2] = if (pass == 0) 2.8f * litR[i] else litR[i]; n++
                }
                g.batchDots(tierCol[k], dots, n, if (pass == 0) 0.45f * hi else 0.9f * hi, add = true)
            }
        }
    }

    private fun novaEnvelope(age: Float): Float {
        if (age < 0f || age >= ArtTiming.MAX_EVENT_AGE) return 0f
        val rise = min(1f, age / 0.05f)
        return rise * exp(-3.2f * max(0f, age - 0.05f)) * (1f - smoothstep(0.9f, 1.45f, age))
    }

    private fun nova(g: ArtCanvas, x: Float, y: Float, gold: Boolean, env: Float, w: Float) {
        if (w <= 0.01f) return
        val tint = if (gold) ArtInk.GoldHi else ArtInk.Blue
        (if (gold) novaGold else novaBlue).draw(g, x, y, 8f + 22f * env, w)
        val len = 5f + 19f * env
        val s = 0.45f * len
        seg[0] = x - len; seg[1] = y; seg[2] = x + len; seg[3] = y; seg[4] = x; seg[5] = y - len; seg[6] = x; seg[7] = y + len
        g.batchLines(tint, 0.8f, seg, 2, min(1f, 0.55f * w), add = true)
        seg[0] = x - s; seg[1] = y; seg[2] = x + s; seg[3] = y; seg[4] = x; seg[5] = y - s; seg[6] = x; seg[7] = y + s
        g.batchLines(starWhite, 1.3f, seg, 2, min(1f, 0.9f * w), add = true)
        ArtCommon.glowDotAdd(g, tint, starWhite, x, y, 1.1f + 1.4f * env, 2.5f + 5f * env, w)
    }

    private fun novae(g: ArtCanvas, m: ArtMotion, wake: Float) {
        val pi = 3.14159f
        for (k in 0 until ArtTiming.ONSET_SLOTS) {
            val env = novaEnvelope(m.onsetAge[k])
            if (env <= 0.004f || m.onsets < k + 1) continue
            val pickId = m.onsets - k
            val h1 = hash01(pickId * 747796405 + m.seed)
            val h2 = hash01(pickId * 2654435761L.toInt() xor 0x71)
            val h3 = hash01(pickId * 2654435761L.toInt() xor 0x72)
            val h4 = hash01(pickId * 2654435761L.toInt() xor 0x73)
            val u = 0.28f + 0.62f * h1
            val theta = (if (h2 < 0.5f) 0f else pi) + u * 4.2f * pi
            val rad = maxR * u.pow(0.7f) * (1f + 0.08f * (h3 - 0.5f))
            val gold = h4 > 0.8f
            val ang = theta + binRad[min(2, (u * 3f).toInt())]
            nova(g, cx + rad * cos(ang), cy + rad * sin(ang), gold, env, env * wake)
            nova(g, cx + rad * cos(theta), cy + rad * sin(theta), gold, env, env * (1f - wake))
        }
    }

    companion object {
        const val BINS = 3
        const val TIERS = 3
        const val DISC_STARS = 64
        const val KNOTS = 18
        const val TWO_PI = 6.283185307179586
        const val PATTERN_DEG_PER_SECOND = 9.0
        const val PATTERN_RAD_PER_SECOND = PATTERN_DEG_PER_SECOND * 3.141592653589793 / 180.0
        val WIND_DEG = floatArrayOf(-5f, -1.5f, 1.5f)
        val BIN_LIFT = floatArrayOf(0.5f, 0.36f, 0.24f)
    }
}
