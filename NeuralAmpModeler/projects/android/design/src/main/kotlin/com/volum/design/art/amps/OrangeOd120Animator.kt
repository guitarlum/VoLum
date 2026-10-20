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
import kotlin.math.ceil
import kotlin.math.exp
import kotlin.math.floor
import kotlin.math.max
import kotlin.math.min
import kotlin.math.pow
import kotlin.math.sin

/**
 * Orange OD120 (`VoLumArtOrangeOd120.h`, class A): a roaming glow warms the
 * Mandelbrot field, embers smoulder and crawl along the rims of the dark
 * bulbs, and each pick runs a ripple out through the escape-time contours.
 */
class OrangeOd120Animator : AdditiveArtAnimator(NebulaHeroes::orangeOd120) {
    private var cx = 0f
    private var cy = 0f
    private var pw = 0f
    private var ph = 0f
    private var rr = 1f
    private var coastX = FloatArray(0)
    private var coastY = FloatArray(0)
    private var coastId = IntArray(0)
    private var contX = FloatArray(0)
    private var contY = FloatArray(0)
    private var contGrain = FloatArray(0)
    private val levelStart = IntArray(MAX_ITER + 1)

    private val emberX = FloatArray(MAX_EMBERS_LIT)
    private val emberY = FloatArray(MAX_EMBERS_LIT)
    private val emberA = FloatArray(MAX_EMBERS_LIT)
    private var emberN = 0
    private val rippleX = FloatArray(MAX_RIPPLE_LIT)
    private val rippleY = FloatArray(MAX_RIPPLE_LIT)
    private val rippleA = FloatArray(MAX_RIPPLE_LIT)
    private var rippleN = 0
    private val dots = FloatArray(3 * max(MAX_EMBERS_LIT, MAX_RIPPLE_LIT))
    private val roam = ArtCommon.MovingBloom(mix(ArtInk.Teal, ArtInk.GoldHi, 0.12f), 0.6f)
    private val emberHalo = mix(ArtInk.Teal, ArtInk.GoldHi, 0.22f)
    private val emberCore = argb(255, 226, 248, 230)
    private val rippleCore = argb(255, 205, 246, 250)

    override fun prepareExtras(r: Box, d: Float) {
        cx = r.mw
        cy = r.mh
        pw = r.w * 0.82f
        ph = r.h * 0.82f
        rr = min(r.w, r.h)
        val pl = cx - pw / 2f
        val pt = cy - ph / 2f
        val step = 2f
        val xs = ArrayList<Float>()
        val ys = ArrayList<Float>()
        var p = 0f
        while (p < pw) { xs += p; p += 2f * step }
        p = 0f
        while (p < ph) { ys += p; p += 2f * step }
        val nx = xs.size
        val ny = ys.size
        val esc = IntArray(nx * ny)
        for (i in 0 until nx) for (j in 0 until ny) {
            val cr = -0.745 + (xs[i] / pw - 0.5) * 0.01
            val ci = 0.186 + (ys[j] / ph - 0.5) * 0.01
            var zr = 0.0; var zi = 0.0; var it = 0
            while (zr * zr + zi * zi < 4.0 && it < MAX_ITER) { val t = zr * zr - zi * zi + cr; zi = 2 * zr * zi + ci; zr = t; it++ }
            esc[i * ny + j] = it
        }
        class E(val x: Float, val y: Float, val id: Int)
        class C(val x: Float, val y: Float, val grain: Float, val level: Int, val id: Int)
        val coast = ArrayList<E>()
        val contour = ArrayList<C>()
        val di = intArrayOf(-1, 1, 0, 0)
        val dj = intArrayOf(0, 0, -1, 1)
        for (i in 0 until nx) for (j in 0 until ny) {
            val v = esc[i * ny + j]
            if (v >= MAX_ITER || v <= 2) continue
            var isCoast = false
            var isContour = false
            for (k in 0 until 4) {
                val a = i + di[k]
                val b = j + dj[k]
                if (a < 0 || a >= nx || b < 0 || b >= ny) continue
                val n = esc[a * ny + b]
                isCoast = isCoast || n >= MAX_ITER
                isContour = isContour || n < v
            }
            val id = i * ny + j + 1
            val x = pl + xs[i]
            val y = pt + ys[j]
            if (isCoast) coast += E(x, y, id)
            if (isContour && v >= MIN_LEVEL) contour += C(x, y, hash01(id * 2654435761L.toInt() xor 23), v, id)
        }
        fun order(id: Int) = hash01(id * 2654435761L.toInt() xor 9)
        val c2 = coast.sortedBy { order(it.id) }.take(MAX_COAST)
        val k2 = contour.sortedBy { order(it.id) }.take(MAX_CONTOUR).sortedBy { it.level }
        coastX = FloatArray(c2.size) { c2[it].x }; coastY = FloatArray(c2.size) { c2[it].y }; coastId = IntArray(c2.size) { c2[it].id }
        contX = FloatArray(k2.size) { k2[it].x }; contY = FloatArray(k2.size) { k2[it].y }; contGrain = FloatArray(k2.size) { k2[it].grain }
        var n = 0
        for (lv in 0..MAX_ITER) {
            while (n < k2.size && k2[n].level < lv) n++
            levelStart[lv] = n
        }
    }

    override fun baseDim(m: ArtMotion): Float = 0.14f * m.wake

    override fun drawExtras(g: ArtCanvas, r: Box, m: ArtMotion) {
        val wake = m.wake
        val t = m.clock.toFloat()
        val reach = 0.7f + 0.3f * m.energy
        val hx = cx + pw * 0.3f * reach * sin(0.29 * m.clock + 1.1).toFloat()
        val hy = cy + ph * 0.28f * reach * sin(0.41 * m.clock + 0.4).toFloat()
        roam.draw(g, hx, hy, rr * (0.34f + 0.06f * m.energy), wake * (0.35f + 0.4f * m.energy))
        g.drawLayerAdd(full, 0.3f * m.pick)
        smoulder(m, wake, t, hx, hy)
        ripple(m)
        dotsTiers(g, emberX, emberY, emberA, emberN, emberHalo, emberCore, 3.4f, 1.25f)
        dotsTiers(g, rippleX, rippleY, rippleA, rippleN, ArtInk.Teal, rippleCore, 2.8f, 0.95f)
    }

    private fun smoulder(m: ArtMotion, wake: Float, t: Float, hx: Float, hy: Float) {
        emberN = 0
        if (wake <= 0f && m.pick <= 0f) return
        val gain = wake * (0.5f + 0.5f * m.energy)
        val thr = 0.62f - 0.36f * m.energy
        val sigma = rr * 0.24f
        val inv = 1f / (2f * sigma * sigma)
        val flare = m.onsets * 747796405 + m.seed
        for (i in coastX.indices) {
            if (emberN >= MAX_EMBERS_LIT) return
            val x = coastX[i]
            val y = coastY[i]
            val dx = x - hx
            val dy = y - hy
            val roamW = exp(-(dx * dx + dy * dy) * inv)
            val crawl = (0.5f + 0.9f * (noise1(0.019f * x + 0.011f * y - 0.31f * t, 9) + noise1(0.008f * x - 0.017f * y + 0.23f * t, 21))).coerceIn(0f, 1f)
            val heat = 0.55f * roamW + 0.5f * crawl
            val glow = gain * smoothstep(thr, 1f, heat) * (0.75f + 0.5f * noise1(t * 3.3f, coastId[i]))
            val spark = hash01(coastId[i] * 2654435761L.toInt() xor flare)
            val a = glow + 0.8f * m.pick * spark * spark * spark
            if (a > 0.03f) { emberX[emberN] = x; emberY[emberN] = y; emberA[emberN] = min(a, 1f); emberN++ }
        }
    }

    private fun ripple(m: ArtMotion) {
        rippleN = 0
        for (k in 0 until ArtTiming.ONSET_SLOTS) {
            val age = m.onsetAge[k]
            if (age >= RIPPLE_LIFE) continue
            val u = max(0f, age) / RIPPLE_LIFE
            val front = FRONT_START - FRONT_TRAVEL * u.pow(0.4f)
            val pickId = m.onsets - k
            val s = (0.7f + 0.3f * hash01(pickId * 747796405 + m.seed)) * (1f - u).pow(1.3f)
            val lo = max(MIN_LEVEL, floor(front - AHEAD).toInt())
            val hi = min(MAX_ITER - 1, ceil(front + BEHIND).toInt())
            for (lv in lo..hi) {
                val d = lv - front
                val wgt = if (d < 0f) 1f + d / AHEAD else 1f - d / BEHIND
                if (wgt <= 0f) continue
                val a = s * wgt * (if (d < 0f) 1f else wgt)
                if (a < 0.03f) continue
                for (n in levelStart[lv] until levelStart[lv + 1]) {
                    if (rippleN >= MAX_RIPPLE_LIT) return
                    rippleX[rippleN] = contX[n]; rippleY[rippleN] = contY[n]; rippleA[rippleN] = min(1f, a * (0.85f + 0.3f * contGrain[n])); rippleN++
                }
            }
        }
    }

    private fun dotsTiers(g: ArtCanvas, lx: FloatArray, ly: FloatArray, la: FloatArray, n: Int, halo: Int, core: Int, haloR: Float, coreR: Float) {
        if (n == 0) return
        var k = 0
        for (i in 0 until n) if (la[i] > 0.5f) { dots[3 * k] = lx[i]; dots[3 * k + 1] = ly[i]; dots[3 * k + 2] = haloR * 2f; k++ }
        g.batchDots(halo, dots, k, 0.4f, add = true)
        for (t in 0 until TIERS) {
            val lo = t.toFloat() / TIERS
            val hi = (t + 1).toFloat() / TIERS
            for (pass in 0 until 2) {
                k = 0
                for (i in 0 until n) if (la[i] > lo && la[i] <= hi) { dots[3 * k] = lx[i]; dots[3 * k + 1] = ly[i]; dots[3 * k + 2] = if (pass == 0) haloR else coreR; k++ }
                g.batchDots(if (pass == 0) halo else core, dots, k, if (pass == 0) 0.3f + 0.35f * hi else min(1f, 0.45f + 0.55f * hi), add = true)
            }
        }
    }

    companion object {
        const val MAX_ITER = 40
        const val MIN_LEVEL = 18
        const val MAX_COAST = 640
        const val MAX_CONTOUR = 1800
        const val MAX_EMBERS_LIT = 240
        const val MAX_RIPPLE_LIT = 300
        const val TIERS = 3
        const val RIPPLE_LIFE = 1.3f
        const val FRONT_START = 39.5f
        const val FRONT_TRAVEL = 22f
        const val AHEAD = 1.2f
        const val BEHIND = 3.5f
    }
}
