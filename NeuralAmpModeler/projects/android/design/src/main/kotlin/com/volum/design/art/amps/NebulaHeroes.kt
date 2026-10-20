package com.volum.design.art.amps

import com.volum.design.art.ArtCanvas
import com.volum.design.art.ArtCommon
import com.volum.design.art.ArtInk
import com.volum.design.art.Box
import com.volum.design.art.argb
import com.volum.design.art.mix
import com.volum.design.art.withA
import kotlin.math.cos
import kotlin.math.max
import kotlin.math.min
import kotlin.math.sin

/**
 * The hero arts of the class-A (additive) amps, ported 1:1 from
 * `art/VoLumArt<Amp>.h`: Ampete One, H&K TriAmp, Lichtlaerm, Orange OD120 /
 * ORS100, Sebago. Static: they run once per layer build.
 */
object NebulaHeroes {
    private val kTeal = ArtInk.Teal
    private val kMid = ArtInk.Mid
    private val kGold = ArtInk.Gold
    private val kGoldHi = ArtInk.GoldHi

    /** The Heighway dragon's turn sequence after [iters] folds. */
    fun dragonTurns(iters: Int): IntArray {
        var turns = IntArray(0)
        repeat(iters) {
            val n2 = IntArray(turns.size * 2 + 1)
            turns.copyInto(n2)
            n2[turns.size] = 1
            for (j in turns.indices) n2[turns.size + 1 + j] = 1 - turns[turns.size - 1 - j]
            turns = n2
        }
        return turns
    }

    /** Ampete One's dragon path in art units: (points, count). */
    fun ampetePath(r: Box): FloatArray {
        val turns = dragonTurns(12)
        val n = turns.size + 1
        val raw = FloatArray(2 * n)
        var px = 0f
        var py = 0f
        var dir = 0
        val dxs = floatArrayOf(1f, 0f, -1f, 0f)
        val dys = floatArrayOf(0f, -1f, 0f, 1f)
        for (i in turns.indices) {
            px += dxs[dir]
            py += dys[dir]
            raw[2 * i + 2] = px
            raw[2 * i + 3] = py
            dir = (dir + if (turns[i] != 0) 1 else 3) % 4
        }
        var mnx = 1e9f; var mny = 1e9f; var mxx = -1e9f; var mxy = -1e9f
        for (i in 0 until n) {
            mnx = min(mnx, raw[2 * i]); mny = min(mny, raw[2 * i + 1]); mxx = max(mxx, raw[2 * i]); mxy = max(mxy, raw[2 * i + 1])
        }
        val extX = mxx - mnx
        val extY = mxy - mny
        val s = min((r.w * 0.60f) / (if (extX > 0f) extX else 1f), (r.h * 0.90f) / (if (extY > 0f) extY else 1f))
        val artH = extY * s
        val ox = r.l + r.w * 0.05f
        val oy = r.mh - artH * 0.5f
        val out = FloatArray(2 * n)
        for (i in 0 until n) {
            out[2 * i] = ox + (raw[2 * i] - mnx) * s
            out[2 * i + 1] = oy + (raw[2 * i + 1] - mny) * s
        }
        return out
    }

    fun ampeteOne(g: ArtCanvas, r: Box) {
        val rr = min(r.w, r.h)
        val p = ampetePath(r)
        val n = p.size / 2
        var mnx = 1e9f; var mny = 1e9f; var mxx = -1e9f; var mxy = -1e9f
        for (i in 0 until n) {
            mnx = min(mnx, p[2 * i]); mny = min(mny, p[2 * i + 1]); mxx = max(mxx, p[2 * i]); mxy = max(mxy, p[2 * i + 1])
        }
        val acx = (mnx + mxx) * 0.5f
        val acy = (mny + mxy) * 0.5f
        ArtCommon.bloom(g, acx, acy, rr * 0.6f, kTeal, 0.17f)
        ArtCommon.dust(g, 7, acx, acy, rr * 0.5f, 80)
        ArtCommon.gradStroke(g, p, n, kMid, kGoldHi, 2f, true)
        ArtCommon.glowDot(g, kGold, kGoldHi, p[2 * n - 2], p[2 * n - 1], 4.5f, 10f)
    }

    fun hkTriampKnot(r: Box, sc: Float): FloatArray {
        val p = FloatArray(2 * 721)
        for (i in 0..720) {
            val t = i * 6.28318f / 720f
            p[2 * i] = r.mw + sin(3f * t + 0.5f) * r.w * sc
            p[2 * i + 1] = r.mh + sin(4f * t) * r.h * sc * 1.06f
        }
        return p
    }

    fun hkTriamp(g: ArtCanvas, r: Box) {
        val rr = min(r.w, r.h)
        ArtCommon.bloom(g, r.mw, r.mh, rr * 0.55f, kTeal, 0.16f)
        ArtCommon.dust(g, 4, r.mw, r.mh, rr * 0.5f, 70)
        ArtCommon.gradStroke(g, hkTriampKnot(r, 0.34f), 721, argb(255, 60, 120, 150), kMid, 4f, false)
        ArtCommon.gradStroke(g, hkTriampKnot(r, 0.4f), 721, kTeal, kMid, 2.1f, true)
    }

    /** Lichtlaerm's anti-snowflake polyline (3 * 4^5 + 1 points). */
    fun lichtlaermPath(r: Box): FloatArray {
        val rr = min(r.w * 0.46f, r.h * 0.5f)
        var segs = ArrayList<FloatArray>()
        for (i in 0 until 3) {
            val a1 = (i * 120f - 90f) * 3.14159f / 180f
            val a2 = ((i + 1) * 120f - 90f) * 3.14159f / 180f
            segs += floatArrayOf(r.mw + rr * cos(a1), r.mh + rr * sin(a1), r.mw + rr * cos(a2), r.mh + rr * sin(a2))
        }
        repeat(5) {
            val next = ArrayList<FloatArray>(segs.size * 4)
            for (s in segs) {
                val dx = s[2] - s[0]
                val dy = s[3] - s[1]
                val ax = s[0] + dx / 3f
                val ay = s[1] + dy / 3f
                val bx = s[0] + dx * 2f / 3f
                val by = s[1] + dy * 2f / 3f
                val px2 = (s[0] + s[2]) / 2f - dy * 0.2887f
                val py2 = (s[1] + s[3]) / 2f + dx * 0.2887f
                next += floatArrayOf(s[0], s[1], ax, ay)
                next += floatArrayOf(ax, ay, px2, py2)
                next += floatArrayOf(px2, py2, bx, by)
                next += floatArrayOf(bx, by, s[2], s[3])
            }
            segs = next
        }
        val p = FloatArray(2 * (segs.size + 1))
        p[0] = segs[0][0]
        p[1] = segs[0][1]
        for (i in segs.indices) {
            p[2 * i + 2] = segs[i][2]
            p[2 * i + 3] = segs[i][3]
        }
        return p
    }

    fun lichtlaerm(g: ArtCanvas, r: Box) {
        val rr = min(r.w * 0.46f, r.h * 0.5f)
        ArtCommon.bloom(g, r.mw, r.mh, rr * 1.2f, kTeal, 0.15f)
        ArtCommon.dust(g, 7, r.mw, r.mh, rr * 1.1f, 60)
        val p = lichtlaermPath(r)
        ArtCommon.gradStroke(g, p, p.size / 2, kTeal, kGoldHi, 1.9f, true)
        ArtCommon.glowDot(g, kGold, kGoldHi, r.mw, r.mh, 3.5f, 10f)
    }

    fun orangeOd120(g: ArtCanvas, r: Box) {
        val rr = min(r.w, r.h)
        val pw = r.w * 0.82f
        val ph = r.h * 0.82f
        val pl = r.mw - pw / 2f
        val pt = r.mh - ph / 2f
        ArtCommon.bloom(g, r.mw, r.mh, rr * 0.55f, kTeal, 0.12f)
        ArtCommon.dust(g, 6, r.mw, r.mh, min(pw, ph) * 0.5f, 90)
        val step = 2f
        var px = 0f
        while (px < pw) {
            var py = 0f
            while (py < ph) {
                val cr = -0.745 + (px / pw - 0.5) * 0.01
                val ci = 0.186 + (py / ph - 0.5) * 0.01
                var zr = 0.0; var zi = 0.0; var it = 0
                while (zr * zr + zi * zi < 4.0 && it < 40) { val t = zr * zr - zi * zi + cr; zi = 2 * zr * zi + ci; zr = t; it++ }
                if (it in 3..39) {
                    val f = it / 40f
                    g.fillCircle(withA(mix(kTeal, kGold, f * 0.28f), 0.25f + 0.6f * f), pl + px, pt + py, step * 0.8f)
                }
                py += step
            }
            px += step
        }
    }

    fun orangeOrs100(g: ArtCanvas, r: Box) {
        val rr = min(r.w, r.h)
        val pw = r.w * 0.82f
        val ph = r.h * 0.82f
        val pl = r.mw - pw / 2f
        val pt = r.mh - ph / 2f
        ArtCommon.bloom(g, r.mw, r.mh, rr * 0.5f, kTeal, 0.12f)
        ArtCommon.dust(g, 2, r.mw, r.mh, min(pw, ph) * 0.5f, 70)
        val step = 2f
        var px = 0f
        while (px < pw) {
            var py = 0f
            while (py < ph) {
                var zr = (px / pw - 0.5) * 3.0
                var zi = (py / ph - 0.5) * 2.4
                var it = 0
                while (zr * zr + zi * zi < 4.0 && it < 32) { val t = zr * zr - zi * zi - 0.7; zi = 2 * zr * zi + 0.27015; zr = t; it++ }
                if (it in 3..31) {
                    val f = it / 32f
                    g.fillCircle(withA(mix(kTeal, kGold, f * 0.3f), 0.25f + 0.6f * f), pl + px, pt + py, step * 0.8f)
                }
                py += step
            }
            px += step
        }
    }

    fun sebago(g: ArtCanvas, r: Box) {
        val rr = min(r.w, r.h)
        ArtCommon.bloom(g, r.mw, r.mh, rr * 0.5f, kTeal, 0.16f)
        ArtCommon.dust(g, 5, r.mw, r.mh, rr * 0.5f, 70)
        val a = -1.4; val b = 1.6; val c = 1.0; val d = 0.75
        var x = 0.0
        var y = 0.0
        val s = rr * 0.22f
        val n = 9000
        for (i in 0 until n) {
            val nx = sin(a * y) + c * cos(a * x)
            val ny = sin(b * x) + d * cos(b * y)
            x = nx
            y = ny
            if (i <= 120) continue
            val px = r.mw + x.toFloat() * s
            val py = r.mh - y.toFloat() * s
            if (px < r.l || px > r.r || py < r.t || py > r.b) continue
            g.fillCircle(withA(mix(kTeal, kGold, i.toFloat() / n), 0.35f), px, py, 0.9f)
        }
    }
}
