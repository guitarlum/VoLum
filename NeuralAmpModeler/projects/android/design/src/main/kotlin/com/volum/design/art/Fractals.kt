package com.volum.design.art

import android.graphics.Matrix
import kotlin.math.abs
import kotlin.math.cos
import kotlin.math.min
import kotlin.math.pow
import kotlin.math.sin
import kotlin.math.sqrt

/**
 * `DrawSidebarMiniFractal` and `DrawCustomAmpArt` of `VoLumFractalArt.h`: the
 * 22 px amp-list thumbnails (one per fractal case) and the six custom-amp art
 * styles (hero, thumbnail and builder swatch alike).
 */
object Fractals {
    private const val PI = 3.14159f

    fun sidebarMini(g: ArtCanvas, r: Box, idx: Int, bright: Int = argb(200, 120, 210, 220), dim: Int = argb(100, 100, 180, 200)) {
        val cx = r.mw
        val cy = r.mh
        val sz = r.w * 0.38f
        when (idx % 15) {
            0 -> {
                var turns = IntArray(0)
                repeat(6) {
                    val n2 = IntArray(turns.size * 2 + 1)
                    turns.copyInto(n2)
                    n2[turns.size] = 1
                    for (j in turns.indices) n2[turns.size + 1 + j] = 1 - turns[turns.size - 1 - j]
                    turns = n2
                }
                val step = 1.8f
                var px = cx - 3f
                var py = cy + 2f
                var dir = 0
                val dx = floatArrayOf(step, 0f, -step, 0f)
                val dy = floatArrayOf(0f, -step, 0f, step)
                for (t in turns) {
                    val nx = px + dx[dir]
                    val ny = py + dy[dir]
                    g.line(bright, px, py, nx, ny, 1f)
                    px = nx
                    py = ny
                    dir = (dir + if (t != 0) 1 else 3) % 4
                }
            }
            1 -> {
                val mn = r.minSide
                val sep = mn * 0.24f
                val ew = mn * 0.17f
                val eh = ew * 0.5f
                fun eye(exc: Float) {
                    val xs = ArrayList<Float>()
                    val ys = ArrayList<Float>()
                    fun q(ax: Float, ay: Float, bx: Float, by: Float, dx3: Float, dy3: Float) {
                        for (i in 0..10) {
                            val t = i / 10f
                            val mt = 1f - t
                            xs += exc + mt * mt * ax + 2f * mt * t * bx + t * t * dx3
                            ys += cy + mt * mt * ay + 2f * mt * t * by + t * t * dy3
                        }
                    }
                    q(-ew, 0f, 0f, -eh, ew, 0f)
                    q(ew, 0f, 0f, eh, -ew, 0f)
                    for (i in 1 until xs.size) g.line(bright, xs[i - 1], ys[i - 1], xs[i], ys[i], 1f)
                    g.line(bright, xs.last(), ys.last(), xs.first(), ys.first(), 1f)
                    g.fillEllipse(bright, exc - ew * 0.13f, cy - eh * 0.92f, exc + ew * 0.13f, cy + eh * 0.92f)
                }
                eye(cx - sep)
                eye(cx + sep)
            }
            2 -> {
                var px2 = 0f
                var py2 = 0f
                var rng = 42
                for (i in 0 until 300) {
                    rng = rng * 1103515245 + 12345
                    val rv = (Integer.remainderUnsigned(rng, 1000)) / 1000f
                    val nx: Float
                    val ny: Float
                    if (rv < 0.01f) { nx = 0f; ny = 0.16f * py2 }
                    else if (rv < 0.86f) { nx = 0.85f * px2 + 0.04f * py2; ny = -0.04f * px2 + 0.85f * py2 + 1.6f }
                    else if (rv < 0.93f) { nx = 0.2f * px2 - 0.26f * py2; ny = 0.23f * px2 + 0.22f * py2 + 1.6f }
                    else { nx = -0.15f * px2 + 0.28f * py2; ny = 0.26f * px2 + 0.24f * py2 + 0.44f }
                    px2 = nx
                    py2 = ny
                    val sx = cx + px2 * 3f
                    val sy = r.b - 2f - py2 * 1.8f
                    if (sx > r.l && sx < r.r && sy > r.t && sy < r.b) g.fillRect(bright, sx, sy, sx + 1f, sy + 1f)
                }
            }
            3 -> {
                val maxR = r.minSide * 0.42f
                for (arm in 0 until 2) {
                    val off = arm * PI
                    for (i in 0 until 60) {
                        val t = i / 60f
                        val th = off + t * 3.2f * PI
                        val rr = maxR * t.pow(0.7f)
                        val px = cx + rr * cos(th)
                        val py = cy + rr * sin(th)
                        if (px > r.l && px < r.r && py > r.t && py < r.b) g.fillRect(if (t < 0.5f) bright else dim, px, py, px + 1f, py + 1f)
                    }
                }
                g.fillCircle(bright, cx, cy, 1.6f)
            }
            4 -> for (j in 0 until 30) {
                val t1 = j * 6.28f / 30f
                val t2 = (j + 1) * 6.28f / 30f
                g.line(bright, cx + sin(3 * t1) * sz, cy + sin(4 * t1) * sz * 0.8f, cx + sin(3 * t2) * sz, cy + sin(4 * t2) * sz * 0.8f, 1f)
            }
            5 -> {
                val rr = r.minSide * 0.46f
                var segs = ArrayList<FloatArray>()
                for (i in 0 until 3) {
                    val a1 = (i * 120f - 90f) * PI / 180f
                    val a2 = ((i + 1) * 120f - 90f) * PI / 180f
                    segs += floatArrayOf(cx + rr * cos(a1), cy + rr * sin(a1), cx + rr * cos(a2), cy + rr * sin(a2))
                }
                repeat(3) {
                    val n2 = ArrayList<FloatArray>(segs.size * 4)
                    for (s in segs) {
                        val dx2 = s[2] - s[0]
                        val dy2 = s[3] - s[1]
                        val ax = s[0] + dx2 / 3
                        val ay = s[1] + dy2 / 3
                        val bx = s[0] + dx2 * 2 / 3
                        val by = s[1] + dy2 * 2 / 3
                        val px2 = (s[0] + s[2]) / 2 - dy2 * 0.2887f
                        val py2 = (s[1] + s[3]) / 2 + dx2 * 0.2887f
                        n2 += floatArrayOf(s[0], s[1], ax, ay)
                        n2 += floatArrayOf(ax, ay, px2, py2)
                        n2 += floatArrayOf(px2, py2, bx, by)
                        n2 += floatArrayOf(bx, by, s[2], s[3])
                    }
                    segs = n2
                }
                for (s in segs) g.line(bright, s[0], s[1], s[2], s[3], 1f)
            }
            6 -> {
                val stk = ArrayDeque<FloatArray>()
                stk.addLast(floatArrayOf(cx, r.b - 2f, -90f, sz * 1.2f, 0f))
                while (stk.isNotEmpty()) {
                    val b = stk.removeLast()
                    if (b[4] > 4 || b[3] < 1.5f) continue
                    val rad = b[2] * PI / 180f
                    val ex = b[0] + b[3] * cos(rad)
                    val ey = b[1] + b[3] * sin(rad)
                    g.line(if (b[4] < 2) bright else dim, b[0], b[1], ex, ey, 1f)
                    stk.addLast(floatArrayOf(ex, ey, b[2] - 28f, b[3] * 0.65f, b[4] + 1))
                    stk.addLast(floatArrayOf(ex, ey, b[2] + 28f, b[3] * 0.65f, b[4] + 1))
                }
            }
            7 -> {
                val s = r.minSide * 0.4f
                val ccy = cy + r.h * 0.03f
                val ax = cx
                val ay = ccy - s
                val bx = cx - s * 0.866f
                val by = ccy + s * 0.5f
                val dx2 = cx + s * 0.866f
                val dy2 = ccy + s * 0.5f
                val mABx = (ax + bx) / 2f
                val mABy = (ay + by) / 2f
                val mACx = (ax + dx2) / 2f
                val mACy = (ay + dy2) / 2f
                val mBCx = (bx + dx2) / 2f
                val mBCy = (by + dy2) / 2f
                fun tri(x1: Float, y1: Float, x2: Float, y2: Float, x3: Float, y3: Float) {
                    g.line(bright, x1, y1, x2, y2, 1f)
                    g.line(bright, x2, y2, x3, y3, 1f)
                    g.line(bright, x3, y3, x1, y1, 1f)
                }
                tri(ax, ay, mABx, mABy, mACx, mACy)
                tri(mABx, mABy, bx, by, mBCx, mBCy)
                tri(mACx, mACy, mBCx, mBCy, dx2, dy2)
            }
            8 -> {
                var segs = arrayListOf(floatArrayOf(cx - sz, cy + sz * 0.3f, cx + sz, cy + sz * 0.3f))
                repeat(6) {
                    val n2 = ArrayList<FloatArray>(segs.size * 2)
                    for (s in segs) {
                        val mx = (s[0] + s[2]) / 2 + (s[3] - s[1]) / 2
                        val my = (s[1] + s[3]) / 2 - (s[2] - s[0]) / 2
                        n2 += floatArrayOf(s[0], s[1], mx, my)
                        n2 += floatArrayOf(mx, my, s[2], s[3])
                    }
                    segs = n2
                }
                for (s in segs) g.line(bright, s[0], s[1], s[2], s[3], 0.8f)
            }
            9 -> escape(g, r) { px, py ->
                val cr = -0.745 + ((px - r.l) / r.w - 0.5) * 0.008
                val ci = 0.186 + ((py - r.t) / r.h - 0.5) * 0.008
                var zr = 0.0; var zi = 0.0; var it = 0
                while (zr * zr + zi * zi < 4 && it < 30) { val t = zr * zr - zi * zi + cr; zi = 2 * zr * zi + ci; zr = t; it++ }
                if (it in 4..29) argb(it * 8, 80 + it * 3, 180 + it * 2, 220) else 0
            }
            10 -> escape(g, r) { px, py ->
                var zr = ((px - r.l) / r.w - 0.5) * 3.0
                var zi = ((py - r.t) / r.h - 0.5) * 2.4
                var it = 0
                while (zr * zr + zi * zi < 4 && it < 25) { val t = zr * zr - zi * zi - 0.7; zi = 2 * zr * zi + 0.27015; zr = t; it++ }
                if (it in 3..24) argb(it * 10, 70 + it * 4, 160 + it * 4, 220) else 0
            }
            11 -> {
                val a = -1.4; val b = 1.6; val c = 1.0; val d = 0.75
                var x = 0.0
                var y = 0.0
                val scale = r.minSide * 0.21f
                for (i in 0 until 2200) {
                    val nx = sin(a * y) + c * cos(a * x)
                    val ny = sin(b * x) + d * cos(b * y)
                    x = nx
                    y = ny
                    if (i < 120) continue
                    val px = cx + x.toFloat() * scale
                    val py = cy - y.toFloat() * scale
                    if (px < r.l || px > r.r || py < r.t || py > r.b) continue
                    g.fillRect(argb(70 + i % 90, 90 + i % 80, 175 + i % 70, 220), px, py, px + 1f, py + 1f)
                }
            }
            12 -> escape(g, r) { px, py ->
                val cr = -1.75 + ((px - r.l) / r.w) * 0.15
                val ci = -0.08 + ((py - r.t) / r.h) * 0.12
                var zr = 0.0; var zi = 0.0; var it = 0
                while (zr * zr + zi * zi < 4 && it < 30) { val t = zr * zr - zi * zi + cr; zi = abs(2 * zr * zi) + ci; zr = t; it++ }
                if (it in 3..29) argb(it * 8, 100 + it * 3, 170 + it * 2, 230) else 0
            }
            13 -> {
                val horizon = r.t + r.h * 0.60f
                val rr = r.minSide * 0.24f
                val cyS = horizon - rr * 1.15f
                g.fillCircle(argb(235, 10, 12, 16), cx, cyS, rr)
                for (i in 0 until 10) {
                    val a = i / 10f * 6.28318f
                    g.line(bright, cx + cos(a) * rr * 1.05f, cyS + sin(a) * rr * 1.05f, cx + cos(a) * rr * 1.4f, cyS + sin(a) * rr * 1.4f, 1f)
                }
                g.drawCircle(bright, cx, cyS, rr, 1.5f)
                g.line(dim, cx, cyS + rr, cx, horizon, 1f)
                g.line(bright, r.l, horizon, r.r, horizon, 1f)
            }
            else -> {
                fun bolt(x0: Float, y0: Float, x1: Float, y1: Float, flip: Boolean) {
                    val steps = 5
                    var px = x0
                    var py = y0
                    val fs = if (flip) -1f else 1f
                    for (i in 1..steps) {
                        val t = i.toFloat() / steps
                        var nx = x0 + (x1 - x0) * t + (if (i and 1 != 0) -1f else 1f) * sz * 0.22f * fs
                        var ny = y0 + (y1 - y0) * t
                        if (i == steps) { nx = x1; ny = y1 }
                        g.line(if (i < 3) bright else dim, px, py, nx, ny, 1.2f)
                        if (i in 2..4) {
                            val bx = nx + fs * sz * 0.38f
                            val by = ny + (if (i and 1 != 0) -1f else 1f) * sz * 0.18f
                            if (bx > r.l + 1f && bx < r.r - 1f && by > r.t + 1f && by < r.b - 1f) g.line(dim, nx, ny, bx, by, 0.8f)
                        }
                        px = nx
                        py = ny
                    }
                }
                bolt(cx - sz * 0.45f, r.b - 2f, cx + sz * 0.12f, cy + sz * 0.05f, false)
                bolt(cx + sz * 0.45f, r.t + 2f, cx - sz * 0.10f, cy - sz * 0.04f, true)
            }
        }
    }

    /** The 2 px escape-time grid of the Mandelbrot / Julia / Burning Ship minis. */
    private inline fun escape(g: ArtCanvas, r: Box, colour: (Float, Float) -> Int) {
        val step = 2f
        var px = r.l + 1
        while (px < r.r - 1) {
            var py = r.t + 1
            while (py < r.b - 1) {
                val c = colour(px, py)
                if (c != 0) g.fillRect(c, px, py, px + step - 0.5f, py + step - 0.5f)
                py += step
            }
            px += step
        }
    }

    /** `DrawCustomAmpArt`: six styles, [artId] taken mod 6. */
    fun customAmpArt(g: ArtCanvas, rect: Box, artId: Int, bright: Int = ArtInk.CustomArtBright, dim: Int = ArtInk.CustomArtDim) {
        val cx = rect.mw
        val cy = rect.mh
        val scale = rect.minSide
        val big = scale > 40f
        val tk = if (big) 2f else 1f
        val rr0 = scale * 0.42f
        val p = 3.14159265f
        when (((artId % 6) + 6) % 6) {
            0 -> {
                val rings = if (big) 6 else 3
                for (k in 1..rings) {
                    val rr = rr0 * k / rings
                    var px = 0f
                    var py = 0f
                    for (i in 0..6) {
                        val a = (i * 60f - 90f) * p / 180f
                        val nx = cx + rr * cos(a)
                        val ny = cy + rr * sin(a)
                        if (i > 0) g.line(if (k % 2 != 0) bright else dim, px, py, nx, ny, tk)
                        px = nx
                        py = ny
                    }
                }
            }
            1 -> {
                val spokes = if (big) 36 else 16
                for (i in 0 until spokes) {
                    val a = i * 6.28318531f / spokes
                    val r0 = rr0 * 0.16f
                    val r1 = rr0 * if (i % 3 == 0) 1f else 0.68f
                    g.line(if (i % 2 != 0) bright else dim, cx + r0 * cos(a), cy + r0 * sin(a), cx + r1 * cos(a), cy + r1 * sin(a), tk)
                }
                g.drawCircle(dim, cx, cy, rr0 * 0.16f, tk)
            }
            2 -> {
                val n = if (big) 460 else 90
                val golden = 2.39996323f
                val dotR = if (big) 1.8f else 1f
                for (i in 0 until n) {
                    val rr = rr0 * sqrt(i.toFloat() / n)
                    val a = i * golden
                    g.fillCircle(if (i and 3 != 0) bright else dim, cx + rr * cos(a), cy + rr * sin(a), dotR)
                }
            }
            3 -> {
                val layers = if (big) 9 else 5
                for (k in 0 until layers) {
                    val rr = rr0 * (1f - k.toFloat() / (layers + 1))
                    val rot = k * 14f * p / 180f
                    var px = 0f
                    var py = 0f
                    for (i in 0..4) {
                        val a = rot + (i * 90f + 45f) * p / 180f
                        val nx = cx + rr * cos(a)
                        val ny = cy + rr * sin(a)
                        if (i > 0) g.line(if (k % 2 != 0) bright else dim, px, py, nx, ny, tk)
                        px = nx
                        py = ny
                    }
                }
            }
            4 -> {
                val rBig = rr0
                val rSmall = rr0 * 0.4f
                val d = rr0 * 0.5f
                val kk = (rBig - rSmall) / rSmall
                val steps = if (big) 260 else 110
                val turns = if (big) 6.2832f * 5f else 6.2832f * 3f
                var px = 0f
                var py = 0f
                for (i in 0..steps) {
                    val t = turns * i / steps
                    val nx = cx + (rBig - rSmall) * cos(t) + d * cos(kk * t)
                    val ny = cy + (rBig - rSmall) * sin(t) - d * sin(kk * t)
                    if (i > 0) g.line(if ((i / 14) % 2 != 0) bright else dim, px, py, nx, ny, tk * 0.85f)
                    px = nx
                    py = ny
                }
            }
            else -> {
                val layers = if (big) 4 else 2
                for (k in 1..layers) {
                    val rr = rr0 * k / layers
                    for (tri in 0 until 2) {
                        val baseRot = (if (tri == 0) -90f else 30f) * p / 180f
                        var px = 0f
                        var py = 0f
                        for (i in 0..3) {
                            val a = baseRot + (i * 120f) * p / 180f
                            val nx = cx + rr * cos(a)
                            val ny = cy + rr * sin(a)
                            if (i > 0) g.line(if (tri != 0) dim else bright, px, py, nx, ny, tk)
                            px = nx
                            py = ny
                        }
                    }
                }
            }
        }
    }

    /**
     * `art/VoLumArtCustom.h` (class C): while sounding the art turns slowly
     * (neighbouring styles in opposite directions) and breathes with the level;
     * a pick flashes it.
     */
    class CustomArtAnimator(style: Int) : ArtAnimator() {
        private val style = ((style % 6) + 6) % 6
        private val dir = if (this.style % 2 == 0) 1f else -1f
        private val full = ArtLayer()
        private val m = Matrix()

        override fun prepare(r: Box, d: Float) {
            if (!full.matches(r, d)) full.build(r, d) { customAmpArt(it, r, style) }
            full.addBitmap()
        }

        override fun draw(g: ArtCanvas, r: Box, mo: ArtMotion) {
            val wake = mo.wake
            if (mo.isRest || wake <= 0f) {
                g.drawLayerLit(full, mo.bloom)
                return
            }
            val t = mo.clock.toFloat()
            val turn = dir * 6f * t
            val s = 1f + 0.012f * mo.energy * sin(1.6f * t) + 0.03f * mo.pick
            g.aboutPoint(m, r.mw, r.mh, turn, s, s)
            g.drawLayer(full, 1f - wake)
            g.drawLayerXform(full, m, wake)
            g.drawLayerXform(full, m, min(1f, mo.bloom + 0.25f * mo.pick), additive = true)
        }

        override fun drop() = full.reset()
    }
}
