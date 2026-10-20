package com.volum.design.components

import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.size
import androidx.compose.runtime.Composable
import androidx.compose.runtime.State
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.drawWithCache
import androidx.compose.ui.geometry.Rect
import androidx.compose.ui.geometry.RoundRect
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.StrokeJoin
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Fill
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import com.volum.design.theme.VoLumColors
import kotlin.math.max
import kotlin.math.min
import kotlin.math.pow

/**
 * Every drawn glyph of the desktop chrome, one geometry each:
 *
 * - [Tuner], [Metronome], [Gear]: `resources/img/Tuner.svg`, `Metronome.svg`,
 *   `Gear.svg` (500-unit viewBox), drawn into the 26 dp header ink band.
 * - [PlayRing] / [BuildFaders]: `VoLumModeToggleControl::DrawPlayGlyph` /
 *   `DrawBuildGlyph` (the stomp ring is "go to PLAY", the faders "go to BUILD").
 * - [Lock] / [Unlock], [Store], [ChevronRight]: the PRE / POST block header
 *   (`VoLumTriptychControl::_DrawLockIcon`, `_DrawStoreToAmpIcon`, `_DrawChevron`).
 * - [Plus], [Edit], [Delete]: the amp list (`DrawPlusGlyph`, `DrawPenGlyph`, `DrawBinGlyph`).
 * - [Close]: `DrawCrossGlyph`. [IrBars]: the Custom IR cab button's impulse glyph.
 * - [ChevronDown], [Prev], [Next]: the block chevron turned and at 1.4x, for
 *   menus and steppers that are not the Josefin `<` `>` of the preset bar.
 *
 * At its native size ([nativeW] x [nativeH] dp) a glyph is the desktop's at
 * 1 px = 1 dp; a larger box scales it uniformly, strokes included.
 */
enum class VoLumGlyph(val nativeW: Float, val nativeH: Float) {
    Tuner(26f, 26f), Metronome(26f, 26f), Gear(26f, 26f),
    PlayRing(26f, 26f), BuildFaders(26f, 26f),
    Lock(16f, 16f), Unlock(16f, 16f), Store(16f, 16f),
    ChevronRight(8f, 10f), ChevronDown(10f, 8f), Prev(12f, 14f), Next(12f, 14f),
    Plus(16f, 16f), Edit(20f, 20f), Delete(20f, 20f), Close(28f, 28f), IrBars(13f, 16f),
}

/**
 * A VoLum glyph at its native size (or [size]), tinted [color]. Colour and
 * alpha are read in the draw phase, so a press tint does not rebuild the paths.
 */
@Composable
fun VoLumIcon(glyph: VoLumGlyph, modifier: Modifier = Modifier, color: Color = VoLumColors.TextBright, alpha: Float = 1f, size: Dp? = null) {
    val ink = rememberUpdatedState(color)
    val a = rememberUpdatedState(alpha)
    val face = remember(glyph) { Modifier.glyphFace(glyph, ink, a) }
    Spacer(modifier.size(size ?: glyph.nativeW.dp, size ?: glyph.nativeH.dp).then(face))
}

/**
 * Draw-only glyph behind the content, centred and fitted to the node, for a
 * caller that keeps its own clickable and feature id on the node.
 */
fun Modifier.volumGlyph(glyph: VoLumGlyph, color: () -> Color, alpha: () -> Float = { 1f }): Modifier = drawWithCache {
    val geo = GlyphGeometry(glyph, size.width, size.height, density)
    onDrawBehind { geo.draw(this, color(), alpha()) }
}

private fun Modifier.glyphFace(glyph: VoLumGlyph, ink: State<Color>, alpha: State<Float>): Modifier = drawWithCache {
    val geo = GlyphGeometry(glyph, size.width, size.height, density)
    onDrawBehind { geo.draw(this, ink.value, alpha.value) }
}

/** One glyph's paths for one node size, built once in `drawWithCache`. */
internal class GlyphGeometry(glyph: VoLumGlyph, w: Float, h: Float, density: Float) {
    private class Part(val path: Path, val style: androidx.compose.ui.graphics.drawscope.DrawStyle, val alpha: Float)

    private val parts = ArrayList<Part>(8)
    private val k: Float
    private val ox: Float
    private val oy: Float

    init {
        val svg = glyph == VoLumGlyph.Tuner || glyph == VoLumGlyph.Metronome || glyph == VoLumGlyph.Gear
        val unitsW = if (svg) 500f else glyph.nativeW
        val unitsH = if (svg) 500f else glyph.nativeH
        val fit = min(w / (glyph.nativeW * density), h / (glyph.nativeH * density))
        k = density * fit * glyph.nativeW / unitsW
        ox = (w - unitsW * k) / 2f
        oy = (h - unitsH * k) / 2f
        build(glyph)
    }

    fun draw(scope: DrawScope, color: Color, alpha: Float) {
        for (i in parts.indices) {
            val p = parts[i]
            scope.drawPath(p.path, color, alpha * p.alpha, p.style)
        }
    }

    private fun x(u: Float) = ox + u * k
    private fun y(u: Float) = oy + u * k
    private fun stroke(width: Float, cap: StrokeCap = StrokeCap.Butt, join: StrokeJoin = StrokeJoin.Miter) = Stroke(width * k, cap = cap, join = join)
    private fun add(style: androidx.compose.ui.graphics.drawscope.DrawStyle, alpha: Float = 1f, build: Path.() -> Unit) {
        parts += Part(Path().apply(build), style, alpha)
    }
    private fun Path.m(px: Float, py: Float) = moveTo(x(px), y(py))
    private fun Path.l(px: Float, py: Float) = lineTo(x(px), y(py))
    private fun Path.q(cx: Float, cy: Float, px: Float, py: Float) = quadraticTo(x(cx), y(cy), x(px), y(py))
    private fun Path.c(c1x: Float, c1y: Float, c2x: Float, c2y: Float, px: Float, py: Float) = cubicTo(x(c1x), y(c1y), x(c2x), y(c2y), x(px), y(py))
    private fun Path.circle(cx: Float, cy: Float, r: Float) = addOval(Rect(x(cx - r), y(cy - r), x(cx + r), y(cy + r)))
    private fun line(x0: Float, y0: Float, x1: Float, y1: Float, width: Float, cap: StrokeCap = StrokeCap.Butt, alpha: Float = 1f) =
        add(stroke(width, cap), alpha) { m(x0, y0); l(x1, y1) }

    private fun build(glyph: VoLumGlyph) {
        val round = StrokeCap.Round
        when (glyph) {
            VoLumGlyph.Gear -> {
                add(stroke(40f)) { circle(250f, 250f, 70f) }
                add(stroke(40f)) {
                    m(250f, 446f); l(211f, 446f); q(200f, 337f, 100f, 382f); l(61f, 314f); q(150f, 250f, 61f, 186f)
                    l(100f, 118f); q(200f, 163f, 211f, 54f); l(289f, 54f); q(300f, 163f, 400f, 118f); l(439f, 186f)
                    q(350f, 250f, 439f, 314f); l(400f, 382f); q(300f, 337f, 289f, 446f); l(250f, 446f)
                }
            }
            VoLumGlyph.Metronome -> {
                add(stroke(30f, join = StrokeJoin.Round)) { m(160f, 440f); l(200f, 80f); l(300f, 80f); l(340f, 440f); close() }
                line(250f, 360f, 310f, 90f, 22f, round)
                add(Fill) { circle(290f, 170f, 24f) }
                line(140f, 440f, 360f, 440f, 30f, round)
            }
            VoLumGlyph.Tuner -> {
                add(stroke(34f, round)) { arcTo(Rect(x(110f), y(150f), x(390f), y(430f)), 180f, 180f, true) }
                line(250f, 290f, 250f, 150f, 24f, round)
                add(Fill) { circle(250f, 290f, 28f) }
                line(165f, 250f, 135f, 230f, 20f, round, 0.72f)
                line(335f, 250f, 365f, 230f, 20f, round, 0.72f)
                line(155f, 355f, 345f, 355f, 28f, round)
                line(210f, 405f, 290f, 405f, 28f, round)
            }
            VoLumGlyph.PlayRing -> {
                add(stroke(1.8f)) { circle(13f, 13f, 10.2f) }
                add(Fill) { circle(13f, 13f, 3.1f) }
            }
            VoLumGlyph.BuildFaders -> {
                val xs = floatArrayOf(-7.4f, 0f, 7.4f)
                val capY = floatArrayOf(-3.0f, 4.2f, -5.6f)
                for (i in 0 until 3) {
                    val fx = 13f + xs[i]
                    line(fx, 13f - 9.4f, fx, 13f + 9.4f, 1.6f)
                    line(fx - 3f, 13f + capY[i], fx + 3f, 13f + capY[i], 2.6f)
                }
            }
            VoLumGlyph.Lock, VoLumGlyph.Unlock -> lock(glyph == VoLumGlyph.Lock)
            VoLumGlyph.Store -> add(stroke(2.25f, round, StrokeJoin.Round)) {
                val cx = 8f
                val cy = 8f
                m(cx, cy - 16f * 0.30f); l(cx, cy + 16f * 0.28f)
                m(cx - 16f * 0.23f, cy - 16f * 0.02f); l(cx, cy + 16f * 0.28f); l(cx + 16f * 0.23f, cy - 16f * 0.02f)
            }
            VoLumGlyph.ChevronRight -> add(stroke(1.2f)) { m(2.25f, 1.5f); l(5.75f, 5f); l(2.25f, 8.5f) }
            VoLumGlyph.ChevronDown -> add(stroke(1.2f)) { m(1.5f, 2.25f); l(5f, 5.75f); l(8.5f, 2.25f) }
            VoLumGlyph.Prev -> add(stroke(1.6f, round, StrokeJoin.Round)) { m(8.5f, 2f); l(3.5f, 7f); l(8.5f, 12f) }
            VoLumGlyph.Next -> add(stroke(1.6f, round, StrokeJoin.Round)) { m(3.5f, 2f); l(8.5f, 7f); l(3.5f, 12f) }
            VoLumGlyph.Plus -> {
                line(2f, 8f, 14f, 8f, 1.6f)
                line(8f, 2f, 8f, 14f, 1.6f)
            }
            VoLumGlyph.Edit -> {
                val cx = 10f
                val cy = 10f
                line(cx - 5f, cy + 4f, cx + 3f, cy - 4f, 1.4f)
                line(cx - 2f, cy + 6f, cx + 5f, cy - 1f, 1.4f)
                line(cx + 3f, cy - 4f, cx + 5f, cy - 1f, 1.4f)
                line(cx - 5f, cy + 4f, cx - 6f, cy + 7f, 1.4f)
                line(cx - 2f, cy + 6f, cx - 6f, cy + 7f, 1.4f)
            }
            VoLumGlyph.Delete -> {
                add(stroke(1.3f)) { addRect(Rect(x(5f), y(7f), x(15f), y(16f))) }
                line(3f, 7f, 17f, 7f, 1.3f)
                line(8f, 4f, 12f, 4f, 1.3f)
            }
            VoLumGlyph.Close -> {
                val half = 28f * 0.26f
                line(14f - half, 14f - half, 14f + half, 14f + half, 1.4f, round)
                line(14f - half, 14f + half, 14f + half, 14f - half, 1.4f, round)
            }
            VoLumGlyph.IrBars -> {
                val w = 13f
                val base = 16f
                line(0f, base, w, base, 1f, alpha = 0.5f)
                for (i in 0 until 5) {
                    val bx = w * i / 5f
                    line(bx, base, bx, base - 16f * 0.55f.pow(i), if (i == 0) 1.7f else 1f)
                }
            }
        }
    }

    private fun lock(locked: Boolean) {
        val r = 16f
        val cx = 8f
        val cy = 8f
        val bodyW = r * 0.58f
        val bodyH = r * 0.38f
        val width = max(2.6f, r * 0.13f)
        val rise = r * 0.34f
        val bodyTop = cy - (bodyH + rise) * 0.5f + rise
        val bodyL = cx - bodyW * 0.5f
        val bodyR = cx + bodyW * 0.5f
        val corner = min(2f, bodyW * 0.12f) * k
        add(Fill) { addRoundRect(RoundRect(x(bodyL), y(bodyTop), x(bodyR), y(bodyTop + bodyH), CornerRadius(corner))) }
        add(stroke(width, StrokeCap.Round, StrokeJoin.Round)) {
            if (locked) {
                val left = bodyL + bodyW * 0.18f
                val right = bodyR - bodyW * 0.18f
                val ctrl = bodyTop - rise * 1.12f
                m(left, bodyTop + 0.4f); l(left, bodyTop - rise * 0.40f)
                c(left, ctrl, right, ctrl, right, bodyTop - rise * 0.40f)
                l(right, bodyTop + 0.4f)
            } else {
                val hinge = bodyR - bodyW * 0.18f
                val top = bodyTop - rise * 1.16f
                val looseX = bodyL + bodyW * 0.10f
                m(hinge, bodyTop + 0.4f); l(hinge, bodyTop - rise * 0.45f)
                c(hinge - bodyW * 0.04f, top, looseX + bodyW * 0.34f, top, looseX, bodyTop - rise * 0.54f)
            }
        }
    }
}
