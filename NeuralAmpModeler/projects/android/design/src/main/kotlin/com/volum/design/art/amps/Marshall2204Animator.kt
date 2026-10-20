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
import com.volum.design.art.hash01
import com.volum.design.art.mix
import com.volum.design.art.noise1
import com.volum.design.art.smoothstep
import com.volum.design.art.withA
import kotlin.math.cos
import kotlin.math.exp
import kotlin.math.floor
import kotlin.math.max
import kotlin.math.min
import kotlin.math.pow
import kotlin.math.sin
import kotlin.math.tan

/**
 * Marshall 2204 (`VoLumArtMarshall2204.h`, class B): the tree sways into the
 * wind with the level and a pick gusts it, fronts fan the coals through the
 * crown, and embers tear off the tips and blow downwind.
 */
class Marshall2204Animator : ArtAnimator() {
    private val full = ArtLayer()
    private val glow = ArtLayer()
    private val tree = ArtLayer()
    private val embers = ArtLayer()
    private val sway = Matrix()
    private var rr = 1f
    private var seenR = 0f
    private var seenT = 0f
    private var tipX = FloatArray(0)
    private var tipY = FloatArray(0)
    private var tipU = FloatArray(0)
    private var tipId = IntArray(0)
    private var spawnX = FloatArray(0)
    private var spawnY = FloatArray(0)
    private var litX = FloatArray(0)
    private var litY = FloatArray(0)
    private var litA = FloatArray(0)
    private var litN = 0
    private var dots = FloatArray(0)
    private var cores = FloatArray(0)
    private val bins = Array(HEAT_BINS) { FloatArray(3 * (EMBERS + ArtTiming.ONSET_SLOTS * BURST_SPARKS)) }
    private val binN = IntArray(HEAT_BINS)
    private val front = FloatArray(ArtTiming.ONSET_SLOTS)
    private val frontGain = FloatArray(ArtTiming.ONSET_SLOTS)
    private val groundBloom = ArtCommon.MovingBloom(ArtInk.Gold, 0.42f)
    private val coreCol = intArrayOf(ArtInk.GoldHi, mix(ArtInk.GoldHi, ArtInk.Teal, 0.45f), ArtInk.Teal)

    override fun prepare(r: Box, d: Float) {
        if (!full.matches(r, d)) full.build(r, d) { SceneHeroes.marshall2204(it, r) }
        if (!glow.matches(r, d)) glow.build(r, d) { SceneHeroes.marshall2204Glow(it, r) }
        if (!tree.matches(r, d)) tree.build(r, d) { SceneHeroes.marshall2204Tree(it, r) }
        if (!embers.matches(r, d)) embers.build(r, d) { SceneHeroes.marshall2204Embers(it, r) }
        listOf(full, glow, tree, embers).forEach { it.addBitmap() }
        findTips(r)
    }

    override fun drop() {
        full.reset(); glow.reset(); tree.reset(); embers.reset()
    }

    private fun pickHash(m: ArtMotion, slot: Int, salt: Int): Float = hash01(((m.onsets - slot) * 747796405 + m.seed) xor (salt * 2654435761L.toInt()))

    private fun windLean(m: ArtMotion): Float {
        val s = m.energy * (1.3f + 0.7f * sin(0.9 * m.clock).toFloat() + 0.3f * sin(2.3 * m.clock + 1.0).toFloat())
        var gust = 0f
        for (k in 0 until ArtTiming.ONSET_SLOTS) {
            val a = m.onsetAge[k]
            if (a >= ArtTiming.MAX_EVENT_AGE) continue
            val env = (1f - exp(-a / 0.07f)) * exp(-a / 0.45f) * (1f - a / ArtTiming.MAX_EVENT_AGE)
            gust += (0.75f + 0.5f * pickHash(m, k, 3)) * env * (1f + 0.3f * sin(11f * a))
        }
        gust = 2.6f * (1f - exp(-gust * 3.2f / 2.6f))
        return (s + gust).coerceIn(0f, MAX_LEAN)
    }

    private fun edgeFade(x: Float, y: Float) = smoothstep(seenR, seenR - 28f, x) * smoothstep(seenT, seenT + 14f, y)

    private fun findTips(r: Box) {
        rr = min(r.w, r.h)
        seenR = r.r
        seenT = r.t
        val tx = ArrayList<Float>(); val ty = ArrayList<Float>(); val tid = ArrayList<Int>()
        val sx = ArrayList<Float>(); val sy = ArrayList<Float>()
        var xLo = 1e9f
        var xHi = -1e9f
        var id = 0
        SceneHeroes.marshallGrow(r, 9) { _, _, ex, ey, d ->
            if (d >= 7) {
                id++
                xLo = min(xLo, ex)
                xHi = max(xHi, ex)
                if (!(ex < r.l + 4f || ex > r.r - 4f || ey < r.t + 4f || ey > r.b - 4f)) {
                    if (d >= 8 && hash01(id * 2654435761L.toInt() xor 61) < FAN_KEEP) { tx += ex; ty += ey; tid += id }
                    if (ex < r.r - 36f) { sx += ex; sy += ey }
                }
            }
        }
        val span = max(1f, xHi - xLo)
        tipX = tx.toFloatArray(); tipY = ty.toFloatArray(); tipId = tid.toIntArray()
        tipU = FloatArray(tipX.size) { (tipX[it] - xLo) / span }
        spawnX = sx.toFloatArray(); spawnY = sy.toFloatArray()
        litX = FloatArray(tipX.size); litY = FloatArray(tipX.size); litA = FloatArray(tipX.size)
        dots = FloatArray(3 * (tipX.size + EMBERS + ArtTiming.ONSET_SLOTS * BURST_SPARKS))
        cores = FloatArray(3 * tipX.size)
    }

    private fun spawnAt(h: Float) = min(spawnX.size - 1, (h * spawnX.size).toInt())

    override fun draw(g: ArtCanvas, r: Box, m: ArtMotion) {
        if (m.isRest) {
            g.drawLayerLit(full, m.bloom)
            return
        }
        val wake = m.wake
        val lean = windLean(m)
        val tS = tan(lean * 0.017453293f)
        val px = r.mw
        val py = r.t + r.h * 0.94f
        g.aboutPoint(sway, px, py, 0f, 1f, 1f, -lean)
        g.drawLayerLit(glow, m.bloom)
        val breath = 0.1f * m.energy * sin(0.45 * m.clock).toFloat()
        groundBloom.draw(g, px, r.b, rr * (0.8f + 0.1f * m.energy), min(1f, wake * (0.4f + 0.5f * m.energy + breath) + 0.35f * m.pick))
        g.drawLayerXform(tree, sway)
        g.drawLayerXform(tree, sway, m.bloom, additive = true)
        if (wake < 1f) {
            g.drawLayer(embers, 1f - wake)
            g.drawLayerAdd(embers, m.bloom * (1f - wake))
        }
        fanTips(m, tS, py)
        drawFan(g)
        blowEmbers(r, m, wake, tS, py)
        drawEmbers(g)
    }

    private fun fanTips(m: ArtMotion, tS: Float, py: Float) {
        litN = 0
        val wavePh = ((m.clock * 0.4) % 1.0).toFloat()
        val tw = (m.clock * 3.5).toFloat()
        for (k in 0 until ArtTiming.ONSET_SLOTS) {
            val a = m.onsetAge[k]
            val f = max(0f, 1f - a / ArtTiming.MAX_EVENT_AGE)
            front[k] = -0.15f + a / 0.5f
            frontGain[k] = if (a < ArtTiming.MAX_EVENT_AGE) f * f * (0.8f + 0.4f * pickHash(m, k, 5)) else 0f
        }
        val strike = m.onsets * 747796405
        for (i in tipX.indices) {
            val c = 0.5f + 0.5f * cos(6.2831853f * (1.3f * tipU[i] - wavePh))
            var a = 0.85f * m.energy * c * c * c * c
            for (k in 0 until ArtTiming.ONSET_SLOTS) if (frontGain[k] > 0f) {
                val d = (tipU[i] - front[k]) / 0.14f
                a += frontGain[k] * exp(-d * d)
            }
            a += 0.3f * m.pick * hash01(tipId[i] * 2654435761L.toInt() xor strike)
            a *= 0.6f + 0.8f * noise1(tw, tipId[i])
            if (a > 0.06f) { litX[litN] = tipX[i] + tS * (py - tipY[i]); litY[litN] = tipY[i]; litA[litN] = min(a, 1f); litN++ }
        }
    }

    private fun drawFan(g: ArtCanvas) {
        val halo = withA(ArtInk.Gold, 0.55f)
        for (k in 0 until FAN_TIERS) {
            val lo = k.toFloat() / FAN_TIERS
            val hi = (k + 1).toFloat() / FAN_TIERS
            var n = 0
            for (i in 0 until litN) if (litA[i] > lo && litA[i] <= hi) {
                val s = 0.6f + 0.4f * litA[i] / hi
                dots[3 * n] = litX[i]; dots[3 * n + 1] = litY[i]; dots[3 * n + 2] = 4.5f * s
                cores[3 * n] = litX[i]; cores[3 * n + 1] = litY[i]; cores[3 * n + 2] = 1.6f * s
                n++
            }
            if (n == 0) continue
            g.batchDots(halo, dots, n, hi, add = true)
            g.batchDots(ArtInk.GoldHi, cores, n, min(1f, 0.35f + 0.65f * hi), add = true)
        }
    }

    private fun push(x: Float, y: Float, s: Float, heat: Float) {
        if (s < 0.05f) return
        val b = if (heat < 0.3f) 0 else if (heat < 0.65f) 1 else 2
        val bin = bins[b]
        val o = 3 * binN[b]
        if (o + 2 >= bin.size) return
        bin[o] = x; bin[o + 1] = y; bin[o + 2] = s
        binN[b]++
    }

    private fun blowEmbers(r: Box, m: ArtMotion, wake: Float, tS: Float, py: Float) {
        binN.fill(0)
        if (spawnX.isEmpty()) return
        val reach = (r.w / 620f).coerceIn(0.45f, 1f)
        val density = 0.3f + 0.7f * m.energy
        val flickT = (m.clock * 7.0).toFloat()
        val driftX = (m.clock * 0.7).toFloat()
        val driftY = (m.clock * 0.9).toFloat()
        for (i in 0 until EMBERS) {
            val h0 = hash01(i * 2654435761L.toInt() xor 0x2204)
            val gate = smoothstep(0.85f * h0, 0.85f * h0 + 0.12f, density) * wake
            if (gate <= 0f) continue
            val cyc = m.clock / LIFE + hash01(i * 2654435761L.toInt() xor 0x51)
            val cycF = floor(cyc)
            val ph = (cyc - cycF).toFloat()
            val c = cycF.toLong().toInt()
            val si = spawnAt(hash01((i * 97 + c) * 2654435761L.toInt() xor 0x7a))
            val age = ph * LIFE
            val vx = (20f + 44f * m.energy + 14f * h0) * reach
            val vy = 8f + 12f * hash01(i * 2654435761L.toInt() xor 0x9d)
            val drift = smoothstep(0f, 0.8f, age)
            val x = spawnX[si] + tS * (py - spawnY[si]) + vx * age + 4f * m.energy * reach * age * age + 16f * drift * noise1(driftX, i + 40)
            val y = spawnY[si] - vy * age - 2.5f * age * age + 10f * drift * noise1(driftY, i + 90)
            val env = smoothstep(0f, 0.06f, ph) * (1f - ph).pow(1.3f)
            val flick = 0.75f + 0.5f * noise1(flickT, i + 7)
            push(x, y, gate * env * flick * edgeFade(x, y), ph)
        }
        for (k in 0 until ArtTiming.ONSET_SLOTS) {
            val a = m.onsetAge[k]
            if (a >= BURST_LIFE) continue
            val oi = spawnAt(pickHash(m, k, 11))
            val ox = spawnX[oi] + tS * (py - spawnY[oi])
            val left = 1f - a / BURST_LIFE
            val fade = left * left * smoothstep(0f, 0.03f, a)
            val drag = (1f - exp(-3f * a)) / 3f
            val wind = (18f + 30f * m.energy) * reach * a
            val pickId = m.onsets - k
            for (j in 0 until BURST_SPARKS) {
                val sid = (pickId * 8 + j) * 2654435761L.toInt()
                val ang = -1.25f + 1.45f * hash01(sid xor 0xb1)
                val speed = (55f + 70f * hash01(sid xor 0xc3)) * reach
                val x = ox + cos(ang) * speed * drag + wind
                val y = spawnY[oi] + sin(ang) * speed * drag - 8f * a + 14f * a * a * hash01(sid xor 0xd5)
                val flick = 0.75f + 0.5f * noise1(flickT * 1.3f, sid xor 0x3)
                push(x, y, 0.9f * fade * flick * edgeFade(x, y), a / BURST_LIFE)
            }
        }
    }

    private fun rings(g: ArtCanvas, b: Int, radius: Float, c: Int, w: Float) {
        val src = bins[b]
        val n = binN[b]
        for (i in 0 until n) {
            dots[3 * i] = src[3 * i]; dots[3 * i + 1] = src[3 * i + 1]; dots[3 * i + 2] = radius * src[3 * i + 2]
        }
        g.batchDots(c, dots, n, w, add = true)
    }

    private fun drawEmbers(g: ArtCanvas) {
        val halo = withA(ArtInk.Gold, 0.6f)
        val mid = withA(ArtInk.Gold, 0.8f)
        for (b in 0 until HEAT_BINS) {
            if (binN[b] == 0) continue
            val cool = b.toFloat()
            rings(g, b, 5.5f - cool, halo, 0.85f - 0.2f * cool)
            rings(g, b, 2.6f - 0.3f * cool, mid, 0.8f - 0.2f * cool)
            rings(g, b, 1.25f - 0.3f * cool, coreCol[b], 1f - 0.25f * cool)
        }
    }

    companion object {
        const val EMBERS = 40
        const val BURST_SPARKS = 7
        const val FAN_TIERS = 4
        const val HEAT_BINS = 3
        const val LIFE = 2.6f
        const val BURST_LIFE = 1.2f
        const val FAN_KEEP = 0.3f
        const val MAX_LEAN = 3.2f
    }
}
