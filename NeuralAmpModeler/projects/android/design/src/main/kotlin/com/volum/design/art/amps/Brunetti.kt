package com.volum.design.art.amps

import android.graphics.Matrix
import android.graphics.Shader
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
import com.volum.design.art.withA
import kotlin.math.exp
import kotlin.math.floor
import kotlin.math.min
import kotlin.math.sin
import kotlin.math.tanh

/** Brunetti XL 2 (`VoLumArtBrunetti.h`): "Verdant Fern", the three Barnsley fern passes over a ground bloom. */
object Brunetti {
    class Fern(val n: Int, val fh: Float, val fsc: Float, val bx: Float, val by: Float, val base: Int, val tip: Int, val rr: Float, val a: Float, val gold: Boolean)

    const val BANDS = 4
    val BAND_EDGE = floatArrayOf(0f, 0.34f, 0.58f, 0.79f, 1f)

    /** Pass 0 = back, 1 = front, 2 = front gold-tipped. */
    fun fernAt(r: Box, pass: Int): Fern {
        val w = r.w
        val h = r.h
        return when (pass) {
            0 -> Fern(6000, (h * 0.78f) / 10.5f, h * 0.188f, r.l + w * 0.55f, r.t + h * 0.9f, argb(255, 45, 90, 110), ArtInk.Mid, 0.9f, 0.32f, false)
            1 -> Fern(11000, (h * 0.86f) / 10.5f, h * 0.207f, r.l + w * 0.5f, r.t + h * 0.94f, argb(255, 55, 120, 140), ArtInk.Teal, 1.1f, 0.7f, false)
            else -> Fern(9000, (h * 0.86f) / 10.5f, h * 0.207f, r.l + w * 0.5f, r.t + h * 0.94f, argb(255, 55, 120, 140), ArtInk.Teal, 1.15f, 0.55f, true)
        }
    }

    fun bandOf(py: Float): Int {
        val s = py / 10f
        var k = 0
        while (k < BANDS - 1 && s >= BAND_EDGE[k + 1]) k++
        return k
    }

    /** The pass's points in the hero's order (same LCG, maps and cull): visit(i, py, sx, sy). */
    inline fun points(cull: Box, f: Fern, visit: (Int, Float, Float, Float) -> Unit) {
        var px = 0f
        var py = 0f
        var s = 42
        for (i in 0 until f.n) {
            s = s * 1103515245 + 12345
            val rv = Integer.remainderUnsigned(s, 1000) / 1000f
            val nx: Float
            val ny: Float
            if (rv < 0.01f) { nx = 0f; ny = 0.16f * py }
            else if (rv < 0.86f) { nx = 0.85f * px + 0.04f * py; ny = -0.04f * px + 0.85f * py + 1.6f }
            else if (rv < 0.93f) { nx = 0.2f * px - 0.26f * py; ny = 0.23f * px + 0.22f * py + 1.6f }
            else { nx = -0.15f * px + 0.28f * py; ny = 0.26f * px + 0.24f * py + 0.44f }
            px = nx
            py = ny
            if (i == 0) continue
            val sx = f.bx + px * f.fsc
            val sy = f.by - py * f.fh
            if (sx < cull.l - 4f || sx > cull.r + 4f || sy < cull.t - 4f || sy > cull.b + 4f) continue
            visit(i, py, sx, sy)
        }
    }

    /** One pass's points in one bend band ([band] < 0: all), coloured as the hero colours them. */
    fun drawBand(g: ArtCanvas, cull: Box, f: Fern, band: Int) {
        points(cull, f) { _, py, sx, sy ->
            if (band < 0 || bandOf(py) == band) {
                var c = mix(f.base, f.tip, min(1f, py / 9f))
                if (f.gold && py > 7.6f) c = mix(c, ArtInk.GoldHi, (py - 7.6f) / 2.4f)
                g.fillCircle(withA(c, f.a), sx, sy, f.rr)
            }
        }
    }

    fun groundBloom(g: ArtCanvas, r: Box) = ArtCommon.bloom(g, r.mw, r.t + r.h * 0.86f, min(r.w, r.h) * 0.8f, ArtInk.Teal, 0.13f)

    fun hero(g: ArtCanvas, r: Box) {
        groundBloom(g, r)
        for (pass in 0 until 3) drawBand(g, r, fernAt(r, pass), -1)
    }

    fun animator(): ArtAnimator = Animator()

    /**
     * Class C: gust fronts sweep the frond left to right and it bends as one
     * loaded cantilever (four height bands, chords that meet without a tear),
     * a pick gusts it and springs it back, leaflets catch the light as a front
     * passes, and spores drift off the gold tips.
     */
    private class Animator : ArtAnimator() {
        private val rest = ArtLayer()
        private val back = ArtLayer()
        private val bands = Array(BANDS) { ArtLayer() }
        private val xf = Matrix()
        private val xfv = FloatArray(9)
        private val a = FloatArray(BANDS)
        private val b = FloatArray(BANDS)
        private var backA = 0f
        private var backB = 0f
        private var frontBy = 0f
        private var frontH = 1f
        private var backBy = 0f
        private var backH = 1f
        private var frontX = 0f
        private var backX = 0f
        private var frondLeft = 0f
        private var rl = 0f
        private var rh = 1f
        private var leafX = FloatArray(0); private var leafY = FloatArray(0); private var leafBand = IntArray(0); private var leafId = IntArray(0); private var leafGold = BooleanArray(0)
        private var tipX = FloatArray(0); private var tipY = FloatArray(0); private var tipBand = IntArray(0)
        private val buckets = Array(4) { FloatArray(0) }
        private val bucketN = IntArray(4)
        private val front = FloatArray(ArtTiming.ONSET_SLOTS)
        private val frontW = FloatArray(ArtTiming.ONSET_SLOTS)
        private var bloomShader: Shader? = null
        private var bloomAddShader: Shader? = null
        private var gx = 0f
        private var gy = 0f
        private var gR = 0f
        private val leafHot = argb(255, 200, 245, 240)
        private val spore = mix(ArtInk.Teal, ArtInk.GoldHi, 0.7f)
        private val sporeHot = argb(255, 255, 244, 200)

        override fun prepare(r: Box, d: Float) {
            if (!rest.matches(r, d)) rest.build(r, d) { hero(it, r) }
            if (!back.matches(r, d)) back.build(r, d) { drawBand(it, r, fernAt(r, 0), -1) }
            for (k in 0 until BANDS) if (!bands[k].matches(r, d)) bands[k].build(r, d) {
                drawBand(it, r, fernAt(r, 1), k)
                drawBand(it, r, fernAt(r, 2), k)
            }
            rest.addBitmap(); back.addBitmap(); bands.forEach { it.addBitmap() }
            rl = r.l
            rh = r.h
            gx = r.mw
            gy = r.t + r.h * 0.86f
            gR = min(r.w, r.h) * 0.8f
            bloomShader = ArtCanvas.radial(gx, gy, gR, withA(ArtInk.Teal, 0.13f), withA(ArtInk.Teal, 0f))
            bloomAddShader = ArtCommon.bloomAddShader(gx, gy, gR, ArtInk.Teal, 0.13f)
            findLeaves(r)
        }

        override fun drop() {
            rest.reset(); back.reset(); bands.forEach { it.reset() }
        }

        private fun findLeaves(r: Box) {
            val bk = fernAt(r, 0)
            val fr = fernAt(r, 1)
            val gd = fernAt(r, 2)
            frontBy = fr.by; frontH = 10f * fr.fh; backBy = bk.by; backH = 10f * bk.fh
            frontX = fr.bx + 1.2f * fr.fsc
            backX = bk.bx + 1.2f * bk.fsc
            frondLeft = fr.bx - 2.3f * fr.fsc
            val lx = ArrayList<Float>(); val ly = ArrayList<Float>(); val lb = ArrayList<Int>(); val li = ArrayList<Int>(); val lg = ArrayList<Boolean>()
            points(r, fr) { i, py, sx, sy ->
                if (hash01(i * 2654435761L.toInt() xor 0x1eaf) < LEAF_SHARE) { lx += sx; ly += sy; lb += bandOf(py); li += i; lg += py > 7.6f }
            }
            leafX = lx.toFloatArray(); leafY = ly.toFloatArray(); leafBand = lb.toIntArray(); leafId = li.toIntArray(); leafGold = lg.toBooleanArray()
            val tx = ArrayList<Float>(); val ty = ArrayList<Float>(); val tb = ArrayList<Int>()
            points(r, gd) { _, py, sx, sy -> if (py > 8.4f) { tx += sx; ty += sy; tb += bandOf(py) } }
            val n = tx.size
            val keep = min(n, MAX_TIPS)
            tipX = FloatArray(keep) { tx[it * n / keep] }; tipY = FloatArray(keep) { ty[it * n / keep] }; tipBand = IntArray(keep) { tb[it * n / keep] }
            for (k in 0 until 4) buckets[k] = FloatArray(3 * (leafX.size + AMBIENT_SPORES + ArtTiming.ONSET_SLOTS * PUFF_SPORES))
        }

        private fun wind(phase: Double): Float {
            val turn = phase / TAU
            val n = floor(turn)
            val q = ((turn - n) / FRONT_SHARE).toFloat()
            if (q >= 1f) return 0f
            val bb = sin(3.14159265f * q)
            val strength = 0.45f + 0.55f * hash01(n.toLong().toInt() * 2654435761L.toInt() xor 0x5eed)
            return strength * bb * bb
        }

        private fun gustSpring(age: Float): Float = if (age <= 0f) 0f else exp(-3f * age) * sin(7.5f * age) * (1f - smoothstep(1.1f, 1.5f, age))

        private fun variation(m: ArtMotion, slot: Int) = 0.7f + 0.3f * hash01((m.onsets - slot) * 747796405 + m.seed)

        private fun chordAt(s: Float, bend: Float, flutter: Float): Float {
            val s2 = s * s
            return bend * s2 * (6f - 4f * s + s2) / 3f + flutter * s2 * s2
        }

        private fun lean(x: Float, drift: Double): Float {
            val ph = drift - (x - rl) / (WIND_REACH * rh)
            return wind(ph) - 0.4f * wind(ph - 1.6)
        }

        override fun draw(g: ArtCanvas, r: Box, m: ArtMotion) {
            if (m.isRest) {
                g.drawLayerLit(rest, m.bloom)
                return
            }
            val h = r.h
            val e = m.energy
            val wake = m.wake
            val tf = m.clock.toFloat()
            val t = m.clock
            val drift = WIND_RATE * t + 0.8 * sin(0.23 * t)
            var gust = 0f
            var gustBack = 0f
            for (k in 0 until ArtTiming.ONSET_SLOTS) if (m.onsetAge[k] < ArtTiming.MAX_EVENT_AGE) {
                val v = variation(m, k)
                gust += v * gustSpring(m.onsetAge[k])
                gustBack += v * gustSpring(m.onsetAge[k] - 0.08f)
            }
            val sway = (0.008 * sin(1.7 * t + 0.4) + 0.006 * sin(2.9 * t + 2.1)).toFloat()
            val swayBack = (0.007 * sin(1.4 * t + 2.6) + 0.005 * sin(2.5 * t + 0.3)).toFloat()
            val bend = h * (e * (0.012f + 0.045f * lean(frontX, drift) + sway) + 0.1f * tanh(0.9f * gust))
            val flutter = h * e * 0.012f * noise1(tf * 3.1f, 23)
            val bendBack = 0.6f * h * (e * (0.012f + 0.045f * lean(backX, drift) + swayBack) + 0.1f * tanh(0.9f * gustBack))
            for (k in 0 until BANDS) {
                val s0 = BAND_EDGE[k]
                val s1 = BAND_EDGE[k + 1]
                val d0 = chordAt(s0, bend, flutter)
                val kk = (chordAt(s1, bend, flutter) - d0) / ((s1 - s0) * frontH)
                b[k] = -kk
                a[k] = d0 + kk * (frontBy - s0 * frontH)
            }
            run {
                val d0 = chordAt(0f, bendBack, 0f)
                val kk = (chordAt(1f, bendBack, 0f) - d0) / backH
                backB = -kk
                backA = d0 + kk * backBy
            }
            bloomShader?.let { g.fillCircleShader(it, gx, gy, gR) }
            bloomAddShader?.let { ArtCommon.bloomAdd(g, it, gx, gy, gR, m.bloom) }
            bent(g, back, backA, backB, m.bloom)
            for (k in 0 until BANDS) bent(g, bands[k], a[k], b[k], m.bloom)
            sheen(g, h, m, drift, wake)
            spores(g, h, m, wake)
        }

        private fun bent(g: ArtCanvas, layer: ArtLayer, aa: Float, bb: Float, bloom: Float) {
            xfv[0] = 1f; xfv[1] = bb; xfv[2] = aa; xfv[3] = 0f; xfv[4] = 1f; xfv[5] = 0f; xfv[6] = 0f; xfv[7] = 0f; xfv[8] = 1f
            xf.setValues(xfv)
            g.drawLayerXform(layer, xf)
            if (bloom > 0f) g.drawLayerXform(layer, xf, bloom, additive = true)
        }

        private fun glints(g: ArtCanvas, k: Int, halo: Int, core: Int, coreR: Float, haloW: Float, coreW: Float) {
            val n = bucketN[k]
            if (n <= 0) return
            val xyr = buckets[k]
            g.batchDots(halo, xyr, n, haloW, add = true)
            for (i in 0 until n) xyr[3 * i + 2] = coreR
            g.batchDots(core, xyr, n, coreW, add = true)
        }

        private fun emit(k: Int, x: Float, y: Float, rad: Float) {
            val buf = buckets[k]
            val o = 3 * bucketN[k]
            if (o + 2 >= buf.size) return
            buf[o] = x; buf[o + 1] = y; buf[o + 2] = rad
            bucketN[k]++
        }

        private fun sheen(g: ArtCanvas, h: Float, m: ArtMotion, drift: Double, wake: Float) {
            val e = m.energy
            val tf = m.clock.toFloat()
            for (k in 0 until ArtTiming.ONSET_SLOTS) {
                val age = m.onsetAge[k]
                front[k] = frondLeft - 0.15f * h + age * 3f * h
                frontW[k] = if (age < ArtTiming.MAX_EVENT_AGE) variation(m, k) * (1f - smoothstep(0.45f, 1.3f, age)) else 0f
            }
            val tipGlow = 0.16f * e * wake + 0.35f * m.pick
            val invSigma = 1f / (0.13f * h)
            bucketN.fill(0)
            for (i in leafX.indices) {
                val flick = 0.5f + noise1(tf * 2.4f, leafId[i])
                val amb = e * wind(drift - (leafX[i] - rl) / (WIND_REACH * h))
                var lit = wake * amb * amb * (0.35f + 0.65f * flick)
                for (k in 0 until ArtTiming.ONSET_SLOTS) if (frontW[k] > 0f) {
                    val dd = (leafX[i] - front[k]) * invSigma
                    if (dd > -2.5f && dd < 2.5f) lit += frontW[k] * exp(-dd * dd) * (0.55f + 0.45f * flick)
                }
                if (leafGold[i]) lit += tipGlow * (0.4f + 0.6f * flick)
                if (lit < 0.06f) continue
                emit((if (leafGold[i]) 2 else 0) + (if (lit > 0.5f) 1 else 0), leafX[i] + a[leafBand[i]] + b[leafBand[i]] * leafY[i], leafY[i], 2.4f)
            }
            glints(g, 0, ArtInk.Teal, leafHot, 1f, 0.28f, 0.4f)
            glints(g, 1, ArtInk.Teal, leafHot, 1f, 0.45f, 0.65f)
            glints(g, 2, ArtInk.Gold, ArtInk.GoldHi, 1f, 0.28f, 0.4f)
            glints(g, 3, ArtInk.Gold, ArtInk.GoldHi, 1f, 0.45f, 0.65f)
        }

        private fun tipAt(hv: Float) = min(tipX.size - 1, (hv * tipX.size).toInt())

        private fun spores(g: ArtCanvas, h: Float, m: ArtMotion, wake: Float) {
            if (tipX.isEmpty()) return
            val e = m.energy
            val tf = m.clock.toFloat()
            bucketN[0] = 0
            bucketN[1] = 0
            val live = 16f + 24f * e
            for (i in 0 until AMBIENT_SPORES) {
                val vis = (live - i).coerceIn(0f, 1f)
                if (vis <= 0f) break
                val key = i * 2654435761L.toInt()
                val cyc = m.clock / SPORE_LIFE + hash01(key xor 0x51)
                val cn = floor(cyc)
                val u = (cyc - cn).toFloat()
                val tp = tipAt(hash01(cn.toLong().toInt() * 747796405 xor key))
                val dx = a[tipBand[tp]] + b[tipBand[tp]] * tipY[tp]
                val x = tipX[tp] + dx * (1f - u) + h * (0.06f * u * u + 0.02f * u * noise1(tf * 0.9f, 300 + i))
                val y = tipY[tp] - h * (0.075f * u + 0.012f * u * noise1(tf * 0.7f, 400 + i))
                val env = smoothstep(0f, 0.12f, u) * (1f - smoothstep(0.5f, 1f, u))
                val al = vis * wake * (0.45f + 0.55f * e) * env * (0.75f + 0.5f * noise1(tf * 3.3f, 500 + i))
                if (al > 0.02f) emit(if (al > 0.5f) 1 else 0, x, y, 2.5f)
            }
            for (k in 0 until ArtTiming.ONSET_SLOTS) {
                val age = m.onsetAge[k]
                if (age >= ArtTiming.MAX_EVENT_AGE) continue
                val pick = m.onsets - k
                val f = 1f - exp(-2.8f * age)
                val fade = variation(m, k) * smoothstep(0f, 0.04f, age) * (1f - smoothstep(0.35f, 1.45f, age))
                for (q in 0 until PUFF_SPORES) {
                    val key = (pick * PUFF_SPORES + q) * 2654435761L.toInt()
                    val tp = tipAt(hash01(key xor 0x77))
                    val dx = a[tipBand[tp]] + b[tipBand[tp]] * tipY[tp]
                    val x = tipX[tp] + dx * (1f - f) + h * f * (0.05f + 0.12f * hash01(key xor 0x99))
                    val y = tipY[tp] - h * (f * (0.03f + 0.08f * hash01(key xor 0xab)) + 0.02f * age)
                    val al = fade * (0.8f + 0.4f * noise1(tf * 4f, 600 + q))
                    if (al > 0.02f) emit(if (al > 0.5f) 1 else 0, x, y, 2.5f)
                }
            }
            glints(g, 0, spore, sporeHot, 0.9f, 0.35f, 0.55f)
            glints(g, 1, spore, sporeHot, 0.9f, 0.6f, 0.95f)
        }

        companion object {
            const val TAU = 6.283185307179586
            const val WIND_RATE = 1.15
            const val WIND_REACH = 0.5f
            const val FRONT_SHARE = 0.4
            const val LEAF_SHARE = 0.045f
            const val MAX_TIPS = 64
            const val AMBIENT_SPORES = 40
            const val PUFF_SPORES = 8
            const val SPORE_LIFE = 3.2
        }
    }
}
