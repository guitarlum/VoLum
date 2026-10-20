package com.volum.design.art

import android.graphics.Shader
import kotlin.math.cos
import kotlin.math.min
import kotlin.math.sin
import kotlin.math.sqrt

/**
 * The atmosphere helpers of `art/VoLumArtCommon.h` (bloom, dust, glow line/dot,
 * gradient stroke). These allocate shaders, so they belong to static art only
 * (layer builds); live drawing uses the `*Add` forms with prepared shaders.
 */
object ArtCommon {
    /** `Bloom`: NanoVG keeps only the first and last stop, so peak at the centre falls linearly to clear at [r]. */
    fun bloom(g: ArtCanvas, cx: Float, cy: Float, r: Float, c: Int, peak: Float) {
        if (r <= 0f || peak <= 0f) return
        val a0 = min(255, (peak * 255f).toInt())
        g.pathCircle(cx, cy, r)
        g.fillPath(ArtCanvas.radial(cx, cy, r, argb(a0, redOf(c), greenOf(c), blueOf(c)), argb(0, redOf(c), greenOf(c), blueOf(c))))
    }

    /** The shader [bloomAdd] draws with; build it where the geometry is fixed. */
    fun bloomAddShader(cx: Float, cy: Float, r: Float, c: Int, peak: Float): Shader =
        ArtCanvas.addRadial(cx, cy, r, withA(c, peak), withA(c, 0f))

    /**
     * A `BloomAdd` whose centre and radius change every frame: one unit-radius
     * shader built up front, placed by its local matrix, so a moving light
     * allocates nothing.
     */
    class MovingBloom(c: Int, peak: Float) {
        private val shader: Shader = ArtCanvas.addRadial(0f, 0f, 1f, withA(c, peak), withA(c, 0f))
        private val m = android.graphics.Matrix()

        fun draw(g: ArtCanvas, x: Float, y: Float, r: Float, w: Float) {
            if (r <= 0f || w <= 0f) return
            m.setScale(r, r)
            m.postTranslate(x, y)
            shader.setLocalMatrix(m)
            g.fillCircleShader(shader, x, y, r, min(w, 1f), add = true)
        }
    }

    /** `BloomAdd`: light that shines, never shades, at blend weight [w]. */
    fun bloomAdd(g: ArtCanvas, s: Shader, cx: Float, cy: Float, r: Float, w: Float) {
        if (r <= 0f || w <= 0f) return
        g.fillCircleShader(s, cx, cy, r, min(w, 1f), add = true)
    }

    fun dust(g: ArtCanvas, seed: Int, cx: Float, cy: Float, r: Float, n: Int) {
        val f = Frand(seed)
        for (i in 0 until n) {
            val a = f.next() * 6.28318f
            val rr = r * sqrt(f.next())
            val px = cx + rr * cos(a)
            val py = cy + rr * sin(a)
            val al = ((0.08f + 0.28f * f.next()) * 255f).toInt()
            val c = if (f.next() > 0.75f) ArtInk.Teal else ArtInk.Mid
            g.fillCircle(argb(al, redOf(c), greenOf(c), blueOf(c)), px, py, f.next() * 1.3f + 0.3f)
        }
    }

    fun glowLine(g: ArtCanvas, glowCol: Int, core: Int, x1: Float, y1: Float, x2: Float, y2: Float, w: Float, glowW: Float) {
        g.line(withA(glowCol, (alphaOf(glowCol) / 255f) * 0.32f), x1, y1, x2, y2, w + glowW)
        g.line(core, x1, y1, x2, y2, w)
    }

    fun glowDot(g: ArtCanvas, glowCol: Int, core: Int, cx: Float, cy: Float, r: Float, glowR: Float) {
        g.fillCircle(withA(glowCol, 0.26f), cx, cy, r + glowR)
        g.fillCircle(withA(glowCol, 0.45f), cx, cy, r + glowR * 0.5f)
        g.fillCircle(core, cx, cy, r)
    }

    /** `GlowDotAdd`: GlowDot's look added at weight [w]. */
    fun glowDotAdd(g: ArtCanvas, glowCol: Int, core: Int, x: Float, y: Float, r: Float, glowR: Float, w: Float) {
        if (w <= 0f) return
        val k = min(w, 1f)
        g.fillCircle(withA(glowCol, 0.26f), x, y, r + glowR, k, add = true)
        g.fillCircle(withA(glowCol, 0.45f), x, y, r + glowR * 0.5f, k, add = true)
        g.fillCircle(core, x, y, r, k, add = true)
    }

    /** `GradStroke`: polyline [xy] (n points) from [c0] to [c1] at alpha 0.85, optional faint teal underlay. */
    fun gradStroke(g: ArtCanvas, xy: FloatArray, n: Int, c0: Int, c1: Int, lw: Float, glowOn: Boolean) {
        if (glowOn) for (i in 1 until n) g.line(withA(ArtInk.Teal, 0.22f), xy[2 * i - 2], xy[2 * i - 1], xy[2 * i], xy[2 * i + 1], lw + 4f)
        for (i in 1 until n) {
            val t = i.toFloat() / n
            g.line(withA(mix(c0, c1, t), 0.85f), xy[2 * i - 2], xy[2 * i - 1], xy[2 * i], xy[2 * i + 1], lw)
        }
    }

    /** `DrawCornerAccent`: L-shaped gold bracket. */
    fun cornerAccent(g: ArtCanvas, x: Float, y: Float, size: Float, flipH: Boolean, flipV: Boolean, col: Int, width: Float = 1f) {
        val dx = if (flipH) -size else size
        val dy = if (flipV) -size else size
        g.line(col, x, y, x + dx, y, width)
        g.line(col, x, y, x, y + dy, width)
    }

    fun diamond(g: ArtCanvas, cx: Float, cy: Float, half: Float, col: Int) {
        g.line(col, cx, cy - half, cx + half, cy)
        g.line(col, cx + half, cy, cx, cy + half)
        g.line(col, cx, cy + half, cx - half, cy)
        g.line(col, cx - half, cy, cx, cy - half)
    }
}
