package com.volum.design.art.amps

import com.volum.design.art.AdditiveArtAnimator
import com.volum.design.art.ArtCanvas
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
import com.volum.design.art.withA
import kotlin.math.cos
import kotlin.math.min
import kotlin.math.sqrt

/**
 * Orange ORS100 (`VoLumArtOrangeOrs100.h`, class A): a crest flows inward
 * through the Julia contours (four quarter-phased band layers, so the wave
 * travels without the nebula pulsing), picks surge it and flash the
 * filament edge, and glints light where the crest reaches them.
 */
class OrangeOrs100Animator : AdditiveArtAnimator(NebulaHeroes::orangeOrs100) {
    private val bands = Array(BAND_LAYERS) { ArtLayer() }
    private val levelXY = Array(LEVELS) { FloatArray(0) }
    private var glX = FloatArray(0)
    private var glY = FloatArray(0)
    private var glLevel = FloatArray(0)
    private var glId = IntArray(0)
    private var litX = FloatArray(0)
    private var litY = FloatArray(0)
    private var litA = FloatArray(0)
    private var litN = 0
    private var dots = FloatArray(0)
    private val surge = FloatArray(ArtTiming.ONSET_SLOTS)
    private val surgePhase = FloatArray(ArtTiming.ONSET_SLOTS)
    private val halo = mix(ArtInk.Teal, ArtInk.GoldHi, 0.45f)
    private val hot = argb(255, 255, 244, 210)
    private var sampledFor: Box? = null

    override fun prepareExtras(r: Box, d: Float) {
        if (sampledFor?.let { it.w == r.w && it.h == r.h && it.l == r.l && it.t == r.t } != true) {
            sample(r)
            sampledFor = r
        }
        for (k in 0 until BAND_LAYERS) if (!bands[k].matches(r, d)) {
            bands[k].build(r, d) { paintBand(it, k) }
            bands[k].addBitmap()
        }
    }

    override fun drop() {
        super.drop()
        bands.forEach { it.reset() }
    }

    private fun sample(r: Box) {
        val pw = r.w * 0.82f
        val ph = r.h * 0.82f
        val pl = r.mw - pw / 2f
        val pt = r.mh - ph / 2f
        val lists = Array(LEVELS) { ArrayList<Float>() }
        var px = 0f
        while (px < pw) {
            var py = 0f
            while (py < ph) {
                var zr = (px / pw - 0.5) * 3.0
                var zi = (py / ph - 0.5) * 2.4
                var it = 0
                while (zr * zr + zi * zi < 4.0 && it < 32) { val t = zr * zr - zi * zi - 0.7; zi = 2 * zr * zi + 0.27015; zr = t; it++ }
                if (it in 3..31) { lists[it] += pl + px; lists[it] += pt + py }
                py += 2f
            }
            px += 2f
        }
        for (i in 0 until LEVELS) levelXY[i] = lists[i].toFloatArray()
        class Cand(val score: Float, val x: Float, val y: Float, val level: Float, val id: Int)
        val cands = ArrayList<Cand>()
        for (level in EDGE_LEVEL until LEVELS) {
            val xy = levelXY[level]
            var i = 0
            while (i + 1 < xy.size) {
                val id = level * 65536 + i / 2
                cands += Cand(level + 6f * hash01(id * 2654435761L.toInt() xor 10), xy[i], xy[i + 1], level.toFloat(), id)
                i += 2
            }
        }
        val top = if (cands.size > MAX_GLINTS) cands.sortedByDescending { it.score }.take(MAX_GLINTS) else cands
        glX = FloatArray(top.size) { top[it].x }; glY = FloatArray(top.size) { top[it].y }
        glLevel = FloatArray(top.size) { top[it].level }; glId = IntArray(top.size) { top[it].id }
        litX = FloatArray(top.size); litY = FloatArray(top.size); litA = FloatArray(top.size)
        dots = FloatArray(3 * top.size)
    }

    private fun fillLevel(g: ArtCanvas, level: Int, c: Int) {
        val xy = levelXY[level]
        if (xy.size < 2) return
        g.clearPath()
        var i = 0
        while (i + 1 < xy.size) {
            g.pathCircle(xy[i], xy[i + 1], 2f * 0.8f)
            i += 2
        }
        g.fillPath(c)
    }

    private fun paintBand(g: ArtCanvas, k: Int) {
        val flowInk = argb(255, 90, 196, 212)
        for (level in FIRST_LEVEL until LEVELS) {
            if (k == EDGE_BAND) {
                if (level >= EDGE_LEVEL) {
                    val deep = (level - EDGE_LEVEL).toFloat() / (LEVELS - 1 - EDGE_LEVEL)
                    fillLevel(g, level, withA(mix(ArtInk.Teal, ArtInk.Gold, 0.3f + 0.4f * deep), 0.85f))
                }
            } else if (level % CYCLE_BANDS == k) {
                fillLevel(g, level, withA(mix(flowInk, ArtInk.Gold, 0.45f * level / LEVELS.toFloat()), 0.8f))
            }
        }
    }

    override fun baseDim(m: ArtMotion): Float = 0.22f * m.wake

    override fun drawExtras(g: ArtCanvas, r: Box, m: ArtMotion) {
        val wake = m.wake
        val cycle = ((m.clock * CYCLE_RAD_PER_SECOND) % 6.283185307179586).toFloat()
        val flow = wake * (0.36f + 0.3f * m.energy)
        for (j in 0 until ArtTiming.ONSET_SLOTS) {
            val age = m.onsetAge[j]
            val v = hash01((m.onsets - j) * 747796405 + m.seed)
            surge[j] = pickFade(age) * (0.45f + 0.2f * v)
            surgePhase[j] = if (pickFade(age) > 0f) age * SURGE_RAD_PER_SECOND * (0.85f + 0.3f * v) else 0f
        }
        for (k in 0 until CYCLE_BANDS) {
            val s = flow * crest(cycle - QUARTER * k)
            var e2 = s * s
            for (j in 0 until ArtTiming.ONSET_SLOTS) {
                val v = surge[j] * crest(surgePhase[j] - QUARTER * k)
                e2 += v * v
            }
            g.drawLayerAdd(bands[k], min(1f, sqrt(e2)))
        }
        val fresh = pickFade(m.onsetAge[0])
        val glow = wake * (0.22f + 0.28f * m.energy)
        val strike = 0.8f * m.pick
        val after = 0.5f * fresh
        g.drawLayerAdd(bands[EDGE_BAND], min(1f, sqrt(glow * glow + strike * strike + after * after)))
        drawGlints(g, m, cycle, flow, fresh)
    }

    private fun crest(phase: Float) = 0.5f + 0.5f * cos(phase)

    private fun pickFade(age: Float): Float {
        if (!(age < ArtTiming.MAX_EVENT_AGE)) return 0f
        val left = 1f - age / ArtTiming.MAX_EVENT_AGE
        return smoothstep(0f, 0.06f, age) * left * left
    }

    private fun drawGlints(g: ArtCanvas, m: ArtMotion, cycle: Float, flow: Float, fresh: Float) {
        val pickSalt = m.onsets * 747796405 + m.seed
        val flare = 0.9f * m.pick + 0.7f * fresh
        val t = m.clock.toFloat()
        litN = 0
        for (i in glX.indices) {
            val s = crest(cycle - QUARTER * glLevel[i])
            var a = 1.3f * flow * s * s * (0.6f + 0.8f * noise1(t * 2.5f, glId[i]))
            if (flare > 0f && hash01(glId[i] * 2654435761L.toInt() xor pickSalt) < 0.55f) a += flare
            if (a > 0.03f) { litX[litN] = glX[i]; litY[litN] = glY[i]; litA[litN] = min(a, 1f); litN++ }
        }
        if (litN == 0) return
        for (k in 0 until TIERS) {
            val lo = k.toFloat() / TIERS
            val hi = (k + 1).toFloat() / TIERS
            for (pass in 0 until 2) {
                var n = 0
                for (i in 0 until litN) if (litA[i] > lo && litA[i] <= hi) { dots[3 * n] = litX[i]; dots[3 * n + 1] = litY[i]; dots[3 * n + 2] = if (pass == 0) 3f else 1.1f; n++ }
                g.batchDots(if (pass == 0) halo else hot, dots, n, if (pass == 0) 0.3f + 0.35f * hi else min(1f, 0.45f + 0.5f * hi), add = true)
            }
        }
    }

    companion object {
        const val LEVELS = 32
        const val FIRST_LEVEL = 3
        const val CYCLE_BANDS = 4
        const val EDGE_BAND = CYCLE_BANDS
        const val BAND_LAYERS = CYCLE_BANDS + 1
        const val EDGE_LEVEL = 12
        const val MAX_GLINTS = 140
        const val TIERS = 3
        const val CYCLE_RAD_PER_SECOND = 6.283185307179586 / 3.2
        const val SURGE_RAD_PER_SECOND = 6.2831853f * 2f
        const val QUARTER = 1.5707963f
    }
}
