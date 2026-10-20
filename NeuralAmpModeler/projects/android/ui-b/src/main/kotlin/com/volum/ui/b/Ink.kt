package com.volum.ui.b

import androidx.compose.foundation.Canvas
import androidx.compose.runtime.Composable
import androidx.compose.runtime.State
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.draw.drawWithContent
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.TextUnit
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Telemetry
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.sin

/**
 * Every colour, type style, surface, selection mark and art piece candidate B draws
 * goes through this file, so merging `:design` (ticket 02) replaces these bodies and
 * nothing else. Until then they are neutral greys and the platform font on purpose:
 * no VoLum identity value may be copied into a candidate.
 */
internal object Ink {
    val ground = Color(0xFF0B0B0C)
    val panel = Color(0xFF151517)
    val raised = Color(0xFF212124)
    val pressed = Color(0xFF34343A)
    val line = Color(0xFF3C3C42)
    val text = Color(0xFFEAEAEC)
    val dim = Color(0xFF9A9AA0)
    val faint = Color(0xFF5A5A60)
    val hot = Color(0xFFFFFFFF)
    val alert = Color(0xFFD0584E)
    val scrim = Color(0xCC000000)
}

/** Stand-ins for Poiret One (display), Josefin Sans (label / value) and Michroma (tech). */
internal object Type {
    fun display(size: TextUnit, color: Color = Ink.text) =
        TextStyle(color = color, fontSize = size, fontWeight = FontWeight.Light, fontFamily = FontFamily.Default, letterSpacing = 0.4.sp)

    fun label(size: TextUnit = 12.sp, color: Color = Ink.dim) =
        TextStyle(color = color, fontSize = size, fontWeight = FontWeight.SemiBold, fontFamily = FontFamily.Default, letterSpacing = 1.1.sp)

    fun value(size: TextUnit = 14.sp, color: Color = Ink.text) =
        TextStyle(color = color, fontSize = size, fontWeight = FontWeight.Normal, fontFamily = FontFamily.Default)

    fun tech(size: TextUnit = 12.sp, color: Color = Ink.text) =
        TextStyle(color = color, fontSize = size, fontWeight = FontWeight.Medium, fontFamily = FontFamily.Monospace, letterSpacing = 1.5.sp)
}

/** The three desktop selection styles (`DrawVoLumSelection`): one per kind of mutually exclusive control. */
internal enum class Sel { AMBER_PICKER, BRASS, LIST_TEAL }

internal fun Modifier.selectionInk(on: Boolean, sel: Sel): Modifier = if (!on) this else drawWithContent {
    drawRect(Ink.hot.copy(alpha = 0.10f))
    drawContent()
    val w = 2.dp.toPx()
    when (sel) {
        Sel.AMBER_PICKER -> drawRect(Ink.hot, topLeft = Offset(0f, size.height - 3.dp.toPx()), size = Size(size.width, 3.dp.toPx()))
        Sel.BRASS -> drawRect(Ink.hot, style = Stroke(w), topLeft = Offset(w / 2, w / 2), size = Size(size.width - w, size.height - w))
        Sel.LIST_TEAL -> drawRect(Ink.hot, size = Size(3.dp.toPx(), size.height))
    }
}

/** Gold corner brackets of the desktop frames. */
internal fun Modifier.brackets(color: Color = Ink.faint): Modifier = drawBehind {
    val l = 14.dp.toPx()
    val w = 1.5.dp.toPx()
    val r = size.width
    val b = size.height
    listOf(
        Offset(0f, 0f) to Offset(l, 0f), Offset(0f, 0f) to Offset(0f, l),
        Offset(r, 0f) to Offset(r - l, 0f), Offset(r, 0f) to Offset(r, l),
        Offset(0f, b) to Offset(l, b), Offset(0f, b) to Offset(0f, b - l),
        Offset(r, b) to Offset(r - l, b), Offset(r, b) to Offset(r, b - l),
    ).forEach { (a, z) -> drawLine(color, a, z, w) }
}

/** PRE crosshatch, AMP circuit board and POST surfaces. */
internal enum class Surface { PRE, AMP, POST, PLAIN }

internal fun Modifier.surface(kind: Surface): Modifier = drawBehind {
    drawRect(if (kind == Surface.AMP) Ink.raised else Ink.panel)
}

/** Knob face: the shared `:design` knob replaces this drawing. */
internal fun DrawScope.knobFace(norm: Float, active: Boolean) {
    val stroke = 4.dp.toPx()
    val r = size.minDimension / 2 - stroke
    val c = Offset(size.width / 2, size.height / 2)
    val start = 135f
    val sweep = 270f
    val arcSize = Size(r * 2, r * 2)
    val topLeft = Offset(c.x - r, c.y - r)
    drawArc(Ink.line, start, sweep, false, topLeft, arcSize, style = Stroke(stroke, cap = StrokeCap.Round))
    drawArc(if (active) Ink.hot else Ink.dim, start, sweep * norm, false, topLeft, arcSize, style = Stroke(stroke, cap = StrokeCap.Round))
    drawCircle(Ink.raised, r - stroke * 1.5f, c)
    val a = Math.toRadians((start + sweep * norm).toDouble())
    val inner = r * 0.25f
    val outer = r - stroke * 2f
    drawLine(Ink.text, Offset(c.x + inner * cos(a).toFloat(), c.y + inner * sin(a).toFloat()),
        Offset(c.x + outer * cos(a).toFloat(), c.y + outer * sin(a).toFloat()), 3.dp.toPx(), StrokeCap.Round)
}

/** LED of a footswitch or stomp. */
internal fun DrawScope.led(on: Boolean, center: Offset, radius: Float) {
    if (on) drawCircle(Ink.hot.copy(alpha = 0.25f), radius * 2.2f, center)
    drawCircle(if (on) Ink.hot else Ink.faint, radius, center)
}

/**
 * Moving art of an amp (desktop `VoLumArtAnimator` + per-amp art): a neutral ring
 * study keyed by the amp's art case. [animate] off draws a still frame; telemetry is
 * read only in the draw phase, so the art never recomposes the UI.
 */
@Composable
internal fun StageArt(art: Int, tele: State<Telemetry>, animate: Boolean, modifier: Modifier = Modifier) {
    Canvas(modifier) {
        val t = if (animate) tele.value else null
        val energy = (t?.energy ?: 0.35).toFloat()
        val time = (t?.timeMs ?: 0L) / 1000f
        drawRect(Ink.panel)
        val c = Offset(size.width / 2, size.height * 0.55f)
        val base = size.minDimension * 0.18f
        val rings = 3 + art % 4
        for (i in 0 until rings) {
            val phase = time * (0.4f + 0.15f * i) + art
            val r = base * (1f + i * 0.45f) * (1f + 0.12f * energy * sin(phase).toFloat().coerceAtLeast(0f))
            drawCircle(Ink.dim.copy(alpha = 0.55f - i * 0.08f), r, c, style = Stroke((1.2f + energy * 1.5f).dp.toPx()))
        }
        val spokes = 6 + art
        for (k in 0 until spokes) {
            val a = (2 * PI * k / spokes + time * 0.2f).toFloat()
            val len = base * (0.6f + energy * 0.9f)
            drawLine(Ink.text.copy(alpha = 0.25f + energy * 0.5f), c, Offset(c.x + cos(a) * len, c.y + sin(a) * len), 1.dp.toPx())
        }
        drawLine(Ink.faint, Offset(0f, size.height * 0.82f), Offset(size.width, size.height * 0.82f), (1f + energy).dp.toPx())
    }
}

/** Pedal-card motif of a PRE / POST block. */
@Composable
internal fun PedalMotif(seed: Int, on: Boolean, modifier: Modifier = Modifier) {
    Canvas(modifier) {
        val col = if (on) Ink.text else Ink.faint
        val n = 3 + seed % 3
        for (i in 0 until n) {
            val y = size.height * (i + 1) / (n + 1)
            val amp = size.height * 0.08f * (1 + (seed + i) % 2)
            drawLine(col.copy(alpha = 0.6f), Offset(size.width * 0.15f, y - amp), Offset(size.width * 0.85f, y + amp), 1.2.dp.toPx())
        }
    }
}
