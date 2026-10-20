package com.volum.design.art.amps

import com.volum.design.art.ArtCanvas
import com.volum.design.art.ArtCommon
import com.volum.design.art.ArtInk
import com.volum.design.art.Box
import com.volum.design.art.Frand
import com.volum.design.art.argb
import com.volum.design.art.mix
import com.volum.design.art.withA
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.floor
import kotlin.math.max
import kotlin.math.min
import kotlin.math.pow
import kotlin.math.sin
import kotlin.math.sqrt

/**
 * The hero arts of the class-B / class-C amps, ported 1:1 from
 * `art/VoLumArt<Amp>.h` (static parts). The Dual-lane fit
 * (`VoLumArtLaneFit.h`) is the identity at mono and BUILD sizes and is not
 * applied here.
 */
object SceneHeroes {
    private val kTeal = ArtInk.Teal
    private val kMid = ArtInk.Mid
    private val kDim = ArtInk.Dim
    private val kGold = ArtInk.Gold
    private val kGoldHi = ArtInk.GoldHi

    // ---- Bad Cat Mini Cat: "Eyes in the Dark" -------------------------------------

    fun badCatSky(g: ArtCanvas, r: Box) {
        val rr = min(r.w, r.h)
        ArtCommon.bloom(g, r.mw, r.mh, rr * 0.75f, kTeal, 0.10f)
        val f = Frand(4)
        for (i in 0 until 44) {
            val px = r.l + f.next() * r.w
            val py = r.t + f.next() * r.h
            val al = ((0.06f + 0.14f * f.next()) * 255f).toInt()
            g.fillCircle(argb(al, 120, 210, 220), px, py, f.next() * 1f + 0.3f)
        }
    }

    /** The almond of one eye: upper lid then lower lid, 2 x 17 points. */
    fun badCatAlmond(exc: Float, cy: Float, ew: Float, eh: Float, xs: FloatArray, ys: FloatArray) {
        var k = 0
        fun q(ax: Float, ay: Float, bx: Float, by: Float, dx3: Float, dy3: Float) {
            for (i in 0..16) {
                val t = i / 16f
                val mt = 1f - t
                xs[k] = exc + mt * mt * ax + 2f * mt * t * bx + t * t * dx3
                ys[k] = cy + mt * mt * ay + 2f * mt * t * by + t * t * dy3
                k++
            }
        }
        q(-ew, 0f, 0f, -eh, ew, 0f)
        q(ew, 0f, 0f, eh, -ew, 0f)
    }

    fun badCatEye(g: ArtCanvas, r: Box, exc: Float, pupil: Boolean) {
        val rr = min(r.w, r.h)
        val ew = rr * 0.17f
        val eh = ew * 0.5f
        val xs = FloatArray(34)
        val ys = FloatArray(34)
        badCatAlmond(exc, r.mh, ew, eh, xs, ys)
        g.fillPoly(withA(kTeal, 0.30f), xs, ys, 34)
        for (i in 1 until 34) g.line(withA(kTeal, 0.85f), xs[i - 1], ys[i - 1], xs[i], ys[i], 2f)
        g.line(withA(kTeal, 0.85f), xs[33], ys[33], xs[0], ys[0], 2f)
        if (pupil) {
            val pw = ew * 0.13f
            val ph = eh * 0.92f
            g.fillEllipse(withA(kGold, 0.35f), exc - pw * 2.4f, r.mh - ph * 1.15f, exc + pw * 2.4f, r.mh + ph * 1.15f)
            g.fillEllipse(withA(kGold, 0.95f), exc - pw, r.mh - ph, exc + pw, r.mh + ph)
        }
    }

    fun badCatEyes(g: ArtCanvas, r: Box, pupils: Boolean) {
        val sep = min(r.w, r.h) * 0.24f
        badCatEye(g, r, r.mw - sep, pupils)
        badCatEye(g, r, r.mw + sep, pupils)
    }

    fun badCat(g: ArtCanvas, r: Box) {
        badCatSky(g, r)
        badCatEyes(g, r, true)
    }

    // ---- Brunetti XL 2: "Verdant Fern" ---------------------------------------------

    fun brunetti(g: ArtCanvas, r: Box) {
        val w = r.w
        val h = r.h
        ArtCommon.bloom(g, r.mw, r.t + h * 0.86f, min(w, h) * 0.8f, kTeal, 0.13f)
        fun fern(n: Int, fh: Float, fsc: Float, bx: Float, by: Float, base: Int, tip: Int, rr: Float, a: Float, gold: Boolean) {
            var px = 0f
            var py = 0f
            var s = 42
            for (i in 0 until n) {
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
                val sx = bx + px * fsc
                val sy = by - py * fh
                if (sx < r.l - 4f || sx > r.r + 4f || sy < r.t - 4f || sy > r.b + 4f) continue
                var c = mix(base, tip, min(1f, py / 9f))
                if (gold && py > 7.6f) c = mix(c, kGoldHi, (py - 7.6f) / 2.4f)
                g.fillCircle(withA(c, a), sx, sy, rr)
            }
        }
        fern(6000, (h * 0.78f) / 10.5f, h * 0.188f, r.l + w * 0.55f, r.t + h * 0.9f, argb(255, 45, 90, 110), kMid, 0.9f, 0.32f, false)
        fern(11000, (h * 0.86f) / 10.5f, h * 0.207f, r.l + w * 0.5f, r.t + h * 0.94f, argb(255, 55, 120, 140), kTeal, 1.1f, 0.7f, false)
        fern(9000, (h * 0.86f) / 10.5f, h * 0.207f, r.l + w * 0.5f, r.t + h * 0.94f, argb(255, 55, 120, 140), kTeal, 1.15f, 0.55f, true)
    }

    // ---- Fryette Deliverance 120: "Spiral Galaxy" -----------------------------------

    fun fryette(g: ArtCanvas, r: Box) {
        val maxR = min(r.w, r.h) * 0.46f
        for (b in 6 downTo 1) g.fillCircle(argb(6, 120, 210, 220), r.mw, r.mh, maxR * b / 6f)
        val f = Frand(5)
        for (arm in 0 until 2) {
            val off = arm * 3.14159f
            for (i in 0 until 560) {
                val t = i / 560f
                val theta = off + t * 4.2f * 3.14159f
                val rr = maxR * t.pow(0.7f)
                val px = r.mw + rr * cos(theta) + (f.next() - 0.5f) * rr * 0.12f
                val py = r.mh + rr * sin(theta) + (f.next() - 0.5f) * rr * 0.12f
                if (px < r.l || px > r.r || py < r.t || py > r.b) continue
                val al = (255f * (0.2f + 0.6f * (1f - t))).toInt()
                g.fillCircle(if (t < 0.5f) argb(al, 120, 210, 220) else argb(al, 100, 180, 200), px, py, if (t < 0.2f) 1.4f else 1f)
            }
        }
        g.fillCircle(argb(50, 200, 165, 87), r.mw, r.mh, 10f)
        g.fillCircle(argb(90, 200, 165, 87), r.mw, r.mh, 6f)
        g.fillCircle(argb(235, 252, 222, 145), r.mw, r.mh, 4f)
    }

    // ---- Marshall JMP 2203: "Depth Triforce" ----------------------------------------

    fun jmpTriOutline(g: ArtCanvas, ccx: Float, ccy: Float, s: Float, col: Int, thick: Float, glow: Boolean, glowW: Float = 5f) {
        val ax = ccx; val ay = ccy - s
        val bx = ccx - s * 0.866f; val by = ccy + s * 0.5f
        val cx = ccx + s * 0.866f; val cy = ccy + s * 0.5f
        val abx = (ax + bx) / 2f; val aby = (ay + by) / 2f
        val acx = (ax + cx) / 2f; val acy = (ay + cy) / 2f
        val bcx = (bx + cx) / 2f; val bcy = (by + cy) / 2f
        val tris = arrayOf(
            floatArrayOf(ax, ay, abx, aby, acx, acy),
            floatArrayOf(abx, aby, bx, by, bcx, bcy),
            floatArrayOf(acx, acy, bcx, bcy, cx, cy),
        )
        for (t in tris) {
            if (glow) {
                ArtCommon.glowLine(g, kGold, col, t[0], t[1], t[2], t[3], thick, glowW)
                ArtCommon.glowLine(g, kGold, col, t[2], t[3], t[4], t[5], thick, glowW)
                ArtCommon.glowLine(g, kGold, col, t[4], t[5], t[0], t[1], thick, glowW)
            } else {
                g.line(col, t[0], t[1], t[2], t[3], thick)
                g.line(col, t[2], t[3], t[4], t[5], thick)
                g.line(col, t[4], t[5], t[0], t[1], thick)
            }
        }
    }

    fun jmp2203(g: ArtCanvas, r: Box) {
        val w = r.w
        val h = r.h
        val cy2 = r.mh + h * 0.02f
        val rr = min(w, h)
        ArtCommon.bloom(g, r.mw, cy2, rr * 0.5f, kGold, 0.10f)
        val baseS = min(w * 0.34f, h * 0.42f)
        val k = 6
        for (i in k - 1 downTo 0) {
            val dt = i.toFloat() / (k - 1)
            val s = baseS * (1f - dt * 0.14f)
            val off = dt * rr * 0.07f
            jmpTriOutline(g, r.mw, cy2 - off, s, withA(mix(kGold, kTeal, dt), 0.85f - dt * 0.5f), if (i == 0) 2.2f else 1.2f, i == 0)
        }
    }

    // ---- Marshall JVM: "Levy Nebula" --------------------------------------------------

    fun jvmLevy(r: Box): FloatArray {
        var segs = arrayListOf(floatArrayOf(-1f, 0f, 1f, 0f))
        repeat(11) {
            val next = ArrayList<FloatArray>(segs.size * 2)
            for (s in segs) {
                val mx = (s[0] + s[2]) / 2f + (s[3] - s[1]) / 2f
                val my = (s[1] + s[3]) / 2f - (s[2] - s[0]) / 2f
                next += floatArrayOf(s[0], s[1], mx, my)
                next += floatArrayOf(mx, my, s[2], s[3])
            }
            segs = next
        }
        val n = segs.size + 1
        val raw = FloatArray(2 * n)
        raw[0] = segs[0][0]; raw[1] = segs[0][1]
        for (i in segs.indices) { raw[2 * i + 2] = segs[i][2]; raw[2 * i + 3] = segs[i][3] }
        var mnx = 1e9f; var mny = 1e9f; var mxx = -1e9f; var mxy = -1e9f
        for (i in 0 until n) { mnx = min(mnx, raw[2 * i]); mny = min(mny, raw[2 * i + 1]); mxx = max(mxx, raw[2 * i]); mxy = max(mxy, raw[2 * i + 1]) }
        val bcx = (mnx + mxx) / 2f
        val bcy = (mny + mxy) / 2f
        val ext = max(mxx - mnx, mxy - mny)
        val s = min(r.w, r.h) * 1.25f / (if (ext > 0f) ext else 1f)
        for (i in 0 until n) {
            raw[2 * i] = r.mw + (raw[2 * i] - bcx) * s
            raw[2 * i + 1] = r.mh + (raw[2 * i + 1] - bcy) * s
        }
        return raw
    }

    fun jvm210(g: ArtCanvas, r: Box) {
        val rr = min(r.w, r.h)
        ArtCommon.bloom(g, r.mw, r.mh, rr * 0.55f, kTeal, 0.16f)
        ArtCommon.dust(g, 8, r.mw, r.mh, rr * 0.5f, 70)
        val p = jvmLevy(r)
        ArtCommon.gradStroke(g, p, p.size / 2, kTeal, kGoldHi, 1.6f, true)
    }

    // ---- Marshall 2204: "Windswept Glow" ----------------------------------------------

    /** Branches depth first, leaned 12 degrees into the gust: (x, y, ex, ey, depth) per branch. */
    inline fun marshallGrow(r: Box, maxD: Int, cb: (Float, Float, Float, Float, Int) -> Unit) {
        val rr = min(r.w, r.h)
        val lean = 12f
        val stk = ArrayDeque<FloatArray>()
        stk.addLast(floatArrayOf(r.mw, r.t + r.h * 0.94f, -90f, rr * 0.34f, 0f))
        while (stk.isNotEmpty()) {
            val b = stk.removeLast()
            val d = b[4].toInt()
            if (d > maxD || b[3] < 2.5f) continue
            val rad = b[2] * 3.14159f / 180f
            val ex = b[0] + b[3] * cos(rad)
            val ey = b[1] + b[3] * sin(rad)
            cb(b[0], b[1], ex, ey, d)
            val sp = 22f + d * 4f
            stk.addLast(floatArrayOf(ex, ey, b[2] - sp + lean, b[3] * 0.67f, (d + 1).toFloat()))
            stk.addLast(floatArrayOf(ex, ey, b[2] + sp + lean, b[3] * 0.67f, (d + 1).toFloat()))
        }
    }

    fun marshall2204(g: ArtCanvas, r: Box) {
        val rr = min(r.w, r.h)
        ArtCommon.bloom(g, r.mw, r.b, rr * 0.8f, kGold, 0.12f)
        ArtCommon.bloom(g, r.mw, r.mh, rr * 0.6f, kTeal, 0.06f)
        marshallGrow(r, 9) { x, y, ex, ey, d ->
            val c = withA(mix(kDim, kTeal, max(0f, 1f - d / 6f)), 0.9f)
            if (d < 3) ArtCommon.glowLine(g, kTeal, c, x, y, ex, ey, 2.6f, 4f) else g.line(c, x, y, ex, ey, 1f)
        }
        marshallGrow(r, 9) { _, _, ex, ey, d ->
            if (d >= 7) ArtCommon.glowDot(g, kGold, if (d % 2 != 0) kGoldHi else kTeal, ex, ey, 1.7f, 4f)
        }
        val f = Frand(9)
        for (i in 0 until 26) {
            val px = r.l + r.w * (0.55f + 0.4f * f.next())
            val py = r.t + r.h * (0.1f + 0.5f * f.next())
            // Two Frand calls in one desktop argument list: MSVC (the reference build) evaluates the radius first.
            val radius = f.next() * 1.6f + 0.5f
            val core = if (f.next() > 0.5f) kGoldHi else kTeal
            ArtCommon.glowDot(g, kGold, core, px, py, radius, 4f)
        }
    }

    // ---- Soldano SLO100: "Beacon Sweep" -------------------------------------------------

    fun soldanoSea(g: ArtCanvas, r: Box) {
        val w = r.w
        val h = r.h
        val rr = min(w, h)
        ArtCommon.bloom(g, r.mw, r.t + h * 0.44f, rr * 0.5f, kTeal, 0.08f)
        val step = 2f
        val pw = w * 0.82f
        val ph = h * 0.82f
        val pl = r.mw - pw / 2f
        val pt = r.mh - ph / 2f
        val ink = argb(255, 10, 20, 28)
        var px = 0f
        while (px < pw) {
            var py = 0f
            while (py < ph) {
                val cr = -1.75 + (px / pw) * 0.15
                val ci = -0.08 + (py / ph) * 0.12
                var zr = 0.0; var zi = 0.0; var it = 0
                while (zr * zr + zi * zi < 4.0 && it < 64) { val t = zr * zr - zi * zi + cr; zi = kotlin.math.abs(2.0 * zr * zi) + ci; zr = t; it++ }
                if (it in 4..63) {
                    val f = it / 64f
                    val fc = f * f
                    g.fillCircle(withA(mix(ink, kTeal, fc), 0.32f + 0.52f * fc), pl + px, pt + py, step * 0.8f)
                }
                py += step
            }
            px += step
        }
    }

    fun soldanoCone(g: ArtCanvas, bx: Float, by: Float, ang: Float, spread: Float, len: Float, a: Float) {
        val ex = bx + cos(ang) * len
        val ey = by + sin(ang) * len
        g.pathTriangle(bx, by, bx + cos(ang - spread) * len, by + sin(ang - spread) * len, bx + cos(ang + spread) * len, by + sin(ang + spread) * len)
        g.fillPath(ArtCanvas.linear(bx, by, ex, ey, withA(kGold, a), argb(0, 200, 165, 87)))
    }

    fun soldanoBeams(g: ArtCanvas, r: Box, fade: Float = 1f, lantern: Boolean = true) {
        val bx = r.mw
        val by = r.t + r.h * 0.44f
        val rr = min(r.w, r.h)
        val pi = 3.14159f
        soldanoCone(g, bx, by, pi * 0.14f, 0.055f, rr * 1.0f, 0.085f * fade)
        soldanoCone(g, bx, by, pi * 0.14f + pi, 0.055f, rr * 0.9f, 0.038f * fade)
        soldanoCone(g, bx, by, pi * 0.86f, 0.05f, rr * 0.85f, 0.048f * fade)
        if (lantern) ArtCommon.glowDot(g, kGold, kGoldHi, bx, by, 2.2f, 6f)
    }

    fun soldano(g: ArtCanvas, r: Box) {
        soldanoSea(g, r)
        soldanoBeams(g, r)
    }

    // ---- Diezel Herbert Mk1: "Lichtenberg Glow" -------------------------------------------

    /** One discharge segment: x0, y0, x1, y1, tier (0 bright, 1 mid, 2 dim). */
    fun diezelDischarge(r: Box): FloatArray {
        val cx = r.mw
        val w = r.w
        val h = r.h
        val px = FloatArray(1500)
        val py = FloatArray(1500)
        var n = 0
        px[n] = cx; py[n] = r.b - 16f; n++
        px[n] = cx; py[n] = r.t + 16f; n++
        val cell = 6f
        val grid = HashSet<Long>()
        fun key(x: Float, y: Float): Long = (floor(x / cell).toLong() shl 32) xor floor(y / cell).toLong()
        grid += key(px[0], py[0])
        grid += key(px[1], py[1])
        val attractors = 360
        val tx = FloatArray(attractors)
        val ty = FloatArray(attractors)
        val f = Frand(0xBEEF)
        for (i in 0 until attractors) {
            tx[i] = cx + (f.next() - 0.5f) * w * 0.72f
            ty[i] = r.t + 18f + f.next() * (h - 36f)
        }
        val out = ArrayList<Float>(1400 * 5)
        for (i in 0 until 1400) {
            val ti = (f.next() * attractors).toInt() % attractors
            val trials = min(48, n)
            var nearest = 0
            var best = 1e30f
            for (k in 0 until trials) {
                val idx = (f.next() * n).toInt() % n
                val dx = px[idx] - tx[ti]
                val dy = py[idx] - ty[ti]
                val d = dx * dx + dy * dy
                if (d < best) { best = d; nearest = idx }
            }
            val dx = tx[ti] - px[nearest]
            val dy = ty[ti] - py[nearest]
            val dist = sqrt(dx * dx + dy * dy)
            if (dist < 0.001f) continue
            val ang = atan2(dy, dx) + (f.next() - 0.5f) * 0.6f
            val len = 9f + f.next() * 5f
            val nx = px[nearest] + cos(ang) * len
            val ny = py[nearest] + sin(ang) * len
            if (nx < r.l + 14f || nx > r.r - 14f || ny < r.t + 12f || ny > r.b - 12f) continue
            if (!grid.add(key(nx, ny))) continue
            val tier = if (i < 90) 0f else if (i < 380) 1f else 2f
            out += px[nearest]; out += py[nearest]; out += nx; out += ny; out += tier
            if (n < px.size) { px[n] = nx; py[n] = ny; n++ }
        }
        return out.toFloatArray()
    }

    fun diezelHerbert(g: ArtCanvas, r: Box) {
        val segs = diezelDischarge(r)
        ArtCommon.bloom(g, r.mw, r.mh, min(r.w, r.h) * 0.55f, kTeal, 0.16f)
        var i = 0
        while (i + 4 < segs.size) {
            val tier = segs[i + 4].toInt()
            val col = when (tier) { 0 -> ArtInk.HeroBright; 1 -> ArtInk.HeroMid; else -> ArtInk.HeroDim }
            val th = when (tier) { 0 -> 2.4f; 1 -> 1.5f; else -> 1f }
            g.line(col, segs[i], segs[i + 1], segs[i + 2], segs[i + 3], th)
            i += 5
        }
        ArtCommon.glowDot(g, kTeal, kTeal, r.mw, r.b - 16f, 2.6f, 5f)
        ArtCommon.glowDot(g, kTeal, kTeal, r.mw, r.t + 16f, 2.6f, 5f)
        g.fillCircle(withA(kGoldHi, 0.85f), r.mw, r.b - 16f, 1.2f)
        g.fillCircle(withA(kGoldHi, 0.85f), r.mw, r.t + 16f, 1.2f)
    }
}
