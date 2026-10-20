package com.volum.design.art

import android.graphics.Canvas
import android.graphics.LinearGradient
import android.graphics.Matrix
import android.graphics.Paint
import android.graphics.Path
import android.graphics.PorterDuff
import android.graphics.PorterDuffXfermode
import android.graphics.RadialGradient
import android.graphics.RectF
import android.graphics.Shader
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.max
import kotlin.math.sin
import kotlin.math.sqrt

/**
 * The desktop `IGraphics` vocabulary the arts are written in, over an Android
 * [Canvas] in art units (1 desktop px = 1 dp; the caller scales the canvas by
 * density). Every [Paint], [Path] and scratch buffer is owned here and reused,
 * so live (per-frame) drawing allocates nothing.
 *
 * Two NanoVG behaviours the desktop look depends on are reproduced exactly:
 * - a gradient uses only its first and last stop; a radial gradient's inner
 *   radius is `R * firstStop.offset` and its outer radius is `R`
 *   (`IGraphicsNanoVG.cpp` NanoVGPaint);
 * - `EBlend::Add` is `glBlendFunc(SRC_ALPHA, DST_ALPHA)` over premultiplied
 *   colour, so a source of alpha `a` (paint alpha x blend weight) adds
 *   `rgb * a^2`. [add] and the `add*` gradients bake the square in.
 */
class ArtCanvas {
    lateinit var c: Canvas
        private set

    private val stroke = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.STROKE; strokeCap = Paint.Cap.BUTT; strokeJoin = Paint.Join.MITER }
    private val round = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.STROKE; strokeCap = Paint.Cap.ROUND; strokeJoin = Paint.Join.ROUND }
    private val fill = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.FILL }
    private val bitmapPaint = Paint(Paint.FILTER_BITMAP_FLAG)
    private val addMode = PorterDuffXfermode(PorterDuff.Mode.ADD)
    private val rect = RectF()
    private val tmpMatrix = Matrix()

    private val out = Path()
    private var pts = FloatArray(256)
    private var nPts = 0
    private var subStart = 0
    private var open = false

    fun begin(canvas: Canvas): ArtCanvas {
        c = canvas
        clearPath()
        return this
    }

    /** NanoVG blend weight on a colour: alpha x [w]; additive squares it. */
    private fun Paint.ink(col: Int, w: Float, add: Boolean) {
        val a = (alphaOf(col) / 255f) * w
        val eff = if (add) a * a else a
        color = (col and 0xFFFFFF) or ((eff * 255f + 0.5f).toInt().coerceIn(0, 255) shl 24)
        xfermode = if (add) addMode else null
        shader = null
    }

    private fun Paint.shaded(s: Shader, w: Float, add: Boolean) {
        color = -0x1000000 // opaque black: the shader supplies the colour
        alpha = ((if (add) w * w else w) * 255f + 0.5f).toInt().coerceIn(0, 255)
        xfermode = if (add) addMode else null
        shader = s
    }

    // ---- lines and shapes ---------------------------------------------------

    /** `IGraphics::DrawLine` (butt caps). */
    fun line(col: Int, x1: Float, y1: Float, x2: Float, y2: Float, width: Float = 1f, w: Float = 1f, add: Boolean = false) {
        if (alphaOf(col) == 0 || w <= 0f) return
        stroke.ink(col, w, add)
        stroke.strokeWidth = width
        c.drawLine(x1, y1, x2, y2, stroke)
    }

    fun fillCircle(col: Int, cx: Float, cy: Float, r: Float, w: Float = 1f, add: Boolean = false) {
        if (r <= 0f || alphaOf(col) == 0 || w <= 0f) return
        fill.ink(col, w, add)
        c.drawCircle(cx, cy, r, fill)
    }

    fun drawCircle(col: Int, cx: Float, cy: Float, r: Float, width: Float = 1f, w: Float = 1f, add: Boolean = false) {
        if (r <= 0f || alphaOf(col) == 0 || w <= 0f) return
        stroke.ink(col, w, add)
        stroke.strokeWidth = width
        c.drawCircle(cx, cy, r, stroke)
    }

    fun fillRect(col: Int, l: Float, t: Float, r: Float, b: Float, w: Float = 1f, add: Boolean = false) {
        if (alphaOf(col) == 0 || w <= 0f) return
        fill.ink(col, w, add)
        c.drawRect(l, t, r, b, fill)
    }

    /** `IGraphics::DrawRect`: a 1 px stroke centred on the edge. */
    fun drawRect(col: Int, l: Float, t: Float, r: Float, b: Float, width: Float = 1f, w: Float = 1f) {
        if (alphaOf(col) == 0) return
        stroke.ink(col, w, false)
        stroke.strokeWidth = width
        c.drawRect(l, t, r, b, stroke)
    }

    fun fillRoundRect(col: Int, l: Float, t: Float, r: Float, b: Float, rad: Float, w: Float = 1f) {
        if (alphaOf(col) == 0) return
        fill.ink(col, w, false)
        rect.set(l, t, r, b)
        c.drawRoundRect(rect, rad, rad, fill)
    }

    fun drawRoundRect(col: Int, l: Float, t: Float, r: Float, b: Float, rad: Float, width: Float = 1f, w: Float = 1f) {
        if (alphaOf(col) == 0) return
        stroke.ink(col, w, false)
        stroke.strokeWidth = width
        rect.set(l, t, r, b)
        c.drawRoundRect(rect, rad, rad, stroke)
    }

    fun fillEllipse(col: Int, l: Float, t: Float, r: Float, b: Float, w: Float = 1f, add: Boolean = false) {
        if (alphaOf(col) == 0 || w <= 0f) return
        fill.ink(col, w, add)
        rect.set(l, t, r, b)
        c.drawOval(rect, fill)
    }

    /** `IGraphics::FillEllipse(col, cx, cy, rx, ry)`. */
    fun fillEllipseC(col: Int, cx: Float, cy: Float, rx: Float, ry: Float, w: Float = 1f) = fillEllipse(col, cx - rx, cy - ry, cx + rx, cy + ry, w)

    /** `IGraphics::DrawArc`: degrees, 0 at 12 o'clock, clockwise. */
    fun drawArc(col: Int, cx: Float, cy: Float, r: Float, a1: Float, a2: Float, width: Float = 1f, w: Float = 1f) {
        if (alphaOf(col) == 0 || a2 <= a1) return
        stroke.ink(col, w, false)
        stroke.strokeWidth = width
        rect.set(cx - r, cy - r, cx + r, cy + r)
        c.drawArc(rect, a1 - 90f, a2 - a1, false, stroke)
    }

    // ---- the IGraphics current path -------------------------------------------

    fun clearPath() {
        out.rewind()
        nPts = 0
        subStart = 0
        open = false
    }

    private fun push(x: Float, y: Float) {
        if (nPts * 2 + 2 > pts.size) pts = pts.copyOf(pts.size * 2)
        pts[nPts * 2] = x
        pts[nPts * 2 + 1] = y
        nPts++
    }

    fun moveTo(x: Float, y: Float) {
        flushSub(closed = false)
        subStart = nPts
        push(x, y)
        open = true
    }

    fun lineTo(x: Float, y: Float) {
        if (!open) moveTo(x, y) else push(x, y)
    }

    fun cubicTo(x1: Float, y1: Float, x2: Float, y2: Float, x3: Float, y3: Float, steps: Int = 16) {
        if (!open) return
        val x0 = pts[(nPts - 1) * 2]
        val y0 = pts[(nPts - 1) * 2 + 1]
        for (i in 1..steps) {
            val t = i.toFloat() / steps
            val s = 1f - t
            push(
                s * s * s * x0 + 3f * s * s * t * x1 + 3f * s * t * t * x2 + t * t * t * x3,
                s * s * s * y0 + 3f * s * s * t * y1 + 3f * s * t * t * y2 + t * t * t * y3,
            )
        }
    }

    fun closePath() = flushSub(closed = true)

    /** `IGraphics::FillConvexPolygon`. */
    fun fillPoly(col: Int, xs: FloatArray, ys: FloatArray, n: Int) {
        if (n < 3) return
        moveTo(xs[0], ys[0])
        for (i in 1 until n) lineTo(xs[i], ys[i])
        closePath()
        fillPath(col)
    }

    /** `IGraphics::PathTriangle`. */
    fun pathTriangle(x1: Float, y1: Float, x2: Float, y2: Float, x3: Float, y3: Float) {
        moveTo(x1, y1)
        lineTo(x2, y2)
        lineTo(x3, y3)
        closePath()
    }

    /**
     * `IGraphics::PathCircle`: one solid subpath. NanoVG forces every subpath
     * solid, so overlapping parts of one fill union instead of punching holes;
     * every subpath here is added clockwise to match under the non-zero rule.
     */
    fun pathCircle(cx: Float, cy: Float, r: Float) {
        flushSub(closed = false)
        out.addCircle(cx, cy, r, Path.Direction.CW)
    }

    fun pathRect(l: Float, t: Float, r: Float, b: Float) {
        flushSub(closed = false)
        out.addRect(l, t, r, b, Path.Direction.CW)
    }

    fun pathEllipse(cx: Float, cy: Float, rx: Float, ry: Float) {
        flushSub(closed = false)
        rect.set(cx - rx, cy - ry, cx + rx, cy + ry)
        out.addOval(rect, Path.Direction.CW)
    }

    private fun flushSub(closed: Boolean) {
        if (!open) return
        open = false
        val n = nPts - subStart
        if (n < 2) {
            nPts = subStart
            return
        }
        var area = 0f
        if (closed) {
            for (i in 0 until n) {
                val a = subStart + i
                val b = subStart + (i + 1) % n
                area += pts[a * 2] * pts[b * 2 + 1] - pts[b * 2] * pts[a * 2 + 1]
            }
        }
        val reverse = closed && area < 0f
        for (k in 0 until n) {
            val i = subStart + if (reverse) n - 1 - k else k
            if (k == 0) out.moveTo(pts[i * 2], pts[i * 2 + 1]) else out.lineTo(pts[i * 2], pts[i * 2 + 1])
        }
        if (closed) out.close()
        nPts = subStart
    }

    fun fillPath(col: Int, w: Float = 1f, add: Boolean = false) {
        flushSub(closed = true)
        if (alphaOf(col) != 0 && w > 0f) {
            fill.ink(col, w, add)
            c.drawPath(out, fill)
        }
        clearPath()
    }

    fun fillPath(s: Shader, w: Float = 1f, add: Boolean = false) {
        flushSub(closed = true)
        if (w > 0f) {
            fill.shaded(s, w, add)
            c.drawPath(out, fill)
        }
        clearPath()
    }

    /** [roundJoin] is `RoundStroke()`; with [buttCap] too, the tier stroke whose segments meet end to end. */
    fun strokePath(col: Int, width: Float, w: Float = 1f, add: Boolean = false, roundJoin: Boolean = false, buttCap: Boolean = false) {
        flushSub(closed = false)
        if (alphaOf(col) != 0 && w > 0f) {
            val p = if (roundJoin) round else stroke
            p.ink(col, w, add)
            p.strokeWidth = width
            if (roundJoin && buttCap) p.strokeCap = Paint.Cap.BUTT
            c.drawPath(out, p)
            round.strokeCap = Paint.Cap.ROUND
        }
        clearPath()
    }

    fun strokePath(s: Shader, width: Float, w: Float = 1f, add: Boolean = false) {
        flushSub(closed = false)
        if (w > 0f) {
            stroke.shaded(s, w, add)
            stroke.strokeWidth = width
            c.drawPath(out, stroke)
        }
        clearPath()
    }

    // ---- batched live primitives (art/VoLumArtAnimator.h cookbook) ------------

    /** `BatchLines`: [n] segments `x0, y0, x1, y1, ...`, one round-capped stroke. */
    fun batchLines(col: Int, width: Float, xyxy: FloatArray, n: Int, w: Float = 1f, add: Boolean = false, buttCap: Boolean = false) {
        if (n <= 0 || alphaOf(col) == 0 || w <= 0f) return
        out.rewind()
        for (i in 0 until n) {
            out.moveTo(xyxy[4 * i], xyxy[4 * i + 1])
            out.lineTo(xyxy[4 * i + 2], xyxy[4 * i + 3])
        }
        round.ink(col, w, add)
        round.strokeWidth = width
        if (buttCap) round.strokeCap = Paint.Cap.BUTT
        c.drawPath(out, round)
        round.strokeCap = Paint.Cap.ROUND
        out.rewind()
    }

    /** `BatchPolyline`: one open round-joined polyline through [n] points. */
    fun batchPolyline(col: Int, width: Float, xy: FloatArray, n: Int, w: Float = 1f, add: Boolean = false, buttCap: Boolean = false) {
        if (n < 2 || alphaOf(col) == 0 || w <= 0f) return
        out.rewind()
        out.moveTo(xy[0], xy[1])
        for (i in 1 until n) out.lineTo(xy[2 * i], xy[2 * i + 1])
        round.ink(col, w, add)
        round.strokeWidth = width
        if (buttCap) round.strokeCap = Paint.Cap.BUTT
        c.drawPath(out, round)
        round.strokeCap = Paint.Cap.ROUND
        out.rewind()
    }

    /** `BatchDots`: [n] filled circles `x, y, r, ...` in one fill. */
    fun batchDots(col: Int, xyr: FloatArray, n: Int, w: Float = 1f, add: Boolean = false) {
        if (n <= 0 || alphaOf(col) == 0 || w <= 0f) return
        out.rewind()
        for (i in 0 until n) out.addCircle(xyr[3 * i], xyr[3 * i + 1], xyr[3 * i + 2], Path.Direction.CW)
        fill.ink(col, w, add)
        c.drawPath(out, fill)
        out.rewind()
    }

    // ---- shaders ----------------------------------------------------------------

    fun fillShader(s: Shader, l: Float, t: Float, r: Float, b: Float, w: Float = 1f, add: Boolean = false) {
        if (w <= 0f) return
        fill.shaded(s, w, add)
        c.drawRect(l, t, r, b, fill)
    }

    fun fillCircleShader(s: Shader, cx: Float, cy: Float, r: Float, w: Float = 1f, add: Boolean = false) {
        if (w <= 0f || r <= 0f) return
        fill.shaded(s, w, add)
        c.drawCircle(cx, cy, r, fill)
    }

    fun strokeCircleShader(s: Shader, cx: Float, cy: Float, r: Float, width: Float, w: Float = 1f, add: Boolean = false) {
        if (w <= 0f || r <= 0f) return
        stroke.shaded(s, w, add)
        stroke.strokeWidth = width
        c.drawCircle(cx, cy, r, stroke)
    }

    // ---- layers -------------------------------------------------------------

    fun drawLayer(layer: ArtLayer, w: Float = 1f) {
        val bmp = layer.bitmap ?: return
        if (w <= 0f) return
        bitmapPaint.alpha = (w.coerceAtMost(1f) * 255f + 0.5f).toInt()
        bitmapPaint.xfermode = null
        rect.set(layer.box.l, layer.box.t, layer.box.l + layer.logicalW, layer.box.t + layer.logicalH)
        c.drawBitmap(bmp, null, rect, bitmapPaint)
    }

    /** `ArtLayer::DrawAdd`: the layer at blend weight [w] under `EBlend::Add`. */
    fun drawLayerAdd(layer: ArtLayer, w: Float) {
        if (w <= 0f) return
        val bmp = layer.addBitmap() ?: return
        bitmapPaint.alpha = ((w * w).coerceAtMost(1f) * 255f + 0.5f).toInt()
        bitmapPaint.xfermode = addMode
        rect.set(layer.box.l, layer.box.t, layer.box.l + layer.logicalW, layer.box.t + layer.logicalH)
        c.drawBitmap(bmp, null, rect, bitmapPaint)
        bitmapPaint.xfermode = null
    }

    /** `ArtLayer::DrawLit`: the exact blit plus PLAY's bloom. */
    fun drawLayerLit(layer: ArtLayer, bloom: Float) {
        drawLayer(layer)
        drawLayerAdd(layer, bloom)
    }

    /** `ArtLayer::DrawXform`: the layer under [m] (art units). */
    fun drawLayerXform(layer: ArtLayer, m: Matrix, w: Float = 1f, additive: Boolean = false) {
        if (w <= 0f) return
        if (m.isIdentity && !additive) return drawLayer(layer, w)
        c.save()
        c.concat(m)
        if (additive) drawLayerAdd(layer, w) else drawLayer(layer, w)
        c.restore()
    }

    /** `volumart::AboutPoint`: rotate (deg), x-skew (deg), scale about (px, py), into [into]. */
    fun aboutPoint(into: Matrix, px: Float, py: Float, rotDeg: Float, sx: Float = 1f, sy: Float = 1f, skewXDeg: Float = 0f): Matrix {
        into.reset()
        into.preTranslate(px, py)
        if (rotDeg != 0f) into.preRotate(rotDeg)
        if (skewXDeg != 0f) into.preSkew(kotlin.math.tan(skewXDeg * PI.toFloat() / 180f), 0f)
        if (sx != 1f || sy != 1f) into.preScale(sx, sy)
        into.preTranslate(-px, -py)
        return into
    }

    fun save() = c.save()
    fun restore() = c.restore()
    fun clip(l: Float, t: Float, r: Float, b: Float) {
        c.clipRect(l, t, r, b)
    }

    fun translate(dx: Float, dy: Float) = c.translate(dx, dy)

    companion object {
        /** NanoVG two-stop linear gradient from (x0, y0) to (x1, y1). */
        fun linear(x0: Float, y0: Float, x1: Float, y1: Float, c0: Int, c1: Int): Shader = LinearGradient(x0, y0, x1, y1, c0, c1, Shader.TileMode.CLAMP)

        /** NanoVG radial gradient: [c0] inside `inner * R`, [c1] at `R`. */
        fun radial(cx: Float, cy: Float, r: Float, c0: Int, c1: Int, inner: Float = 0f): Shader {
            val f = inner.coerceIn(0f, 0.999f)
            return RadialGradient(cx, cy, max(r, 0.01f), intArrayOf(c0, c0, c1), floatArrayOf(0f, f, 1f), Shader.TileMode.CLAMP)
        }

        /**
         * The same gradients for `EBlend::Add`: NanoVG interpolates premultiplied
         * colour linearly, then the blend multiplies by alpha again, so the
         * light added falls off with alpha squared. Paint alpha then carries w^2.
         *
         * At most three colours: Skia draws up to two intervals analytically,
         * more go through a texture per shader instance, and live art re-places
         * its shaders every frame (that flooded the emulator's host renderer).
         * The square is sampled at the midpoint; with an inner radius the ramp
         * from it outward stays linear.
         */
        fun addLinear(x0: Float, y0: Float, x1: Float, y1: Float, c0: Int, c1: Int): Shader =
            LinearGradient(x0, y0, x1, y1, intArrayOf(squared(c0, c1, 0f), squared(c0, c1, 0.5f), squared(c0, c1, 1f)), floatArrayOf(0f, 0.5f, 1f), Shader.TileMode.CLAMP)

        fun addRadial(cx: Float, cy: Float, r: Float, c0: Int, c1: Int, inner: Float = 0f): Shader {
            val f = inner.coerceIn(0f, 0.999f)
            val cols = if (f > 0f) intArrayOf(squared(c0, c1, 0f), squared(c0, c1, 0f), squared(c0, c1, 1f))
            else intArrayOf(squared(c0, c1, 0f), squared(c0, c1, 0.5f), squared(c0, c1, 1f))
            val pos = if (f > 0f) floatArrayOf(0f, f, 1f) else floatArrayOf(0f, 0.5f, 1f)
            return RadialGradient(cx, cy, max(r, 0.01f), cols, pos, Shader.TileMode.CLAMP)
        }

        /** Unpremultiplied colour whose premultiplied value is NanoVG's `premul(t) * alpha(t)`. */
        private fun squared(c0: Int, c1: Int, t: Float): Int {
            val a0 = alphaOf(c0) / 255f
            val a1 = alphaOf(c1) / 255f
            val a = a0 + (a1 - a0) * t
            if (a <= 1e-4f) return 0
            fun ch(x0: Int, x1: Int) = ((x0 * a0 + (x1 * a1 - x0 * a0) * t) / a).toInt().coerceIn(0, 255)
            return argb((a * a * 255f + 0.5f).toInt(), ch(redOf(c0), redOf(c1)), ch(greenOf(c0), greenOf(c1)), ch(blueOf(c0), blueOf(c1)))
        }

        const val TAU = (2.0 * PI).toFloat()

        fun cosf(a: Float) = cos(a)
        fun sinf(a: Float) = sin(a)
        fun sqrtf(a: Float) = sqrt(a)
    }
}
