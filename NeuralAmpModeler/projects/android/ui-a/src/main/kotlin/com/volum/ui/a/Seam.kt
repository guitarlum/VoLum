package com.volum.ui.a

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsPressedAsState
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicText
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.TextUnit
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.harness.featureId

/**
 * The only place candidate A touches look-and-feel. Until `feature/android-design`
 * is merged these are deliberately neutral greys on the platform default face, not
 * VoLum values; each entry is the hook a `:design` component replaces (see the
 * Requests section of reports/03-a-r1.md). Nothing else in `:ui-a` names a colour,
 * a font or draws selection or art.
 */
internal object Ink {
    val bg = Color(0xFF0E0E0E)
    val panel = Color(0xFF181818)
    val raised = Color(0xFF222222)
    val line = Color(0xFF3A3A3A)
    val text = Color(0xFFE8E8E8)
    val dim = Color(0xFF9A9A9A)
    val faint = Color(0xFF5E5E5E)
    val selected = Color(0xFFFFFFFF)
    val alert = Color(0xFFC85A5A)
    val scrim = Color(0xCC000000)
}

internal object Type {
    fun display(size: TextUnit = 22.sp, color: Color = Ink.text) = TextStyle(color = color, fontSize = size, fontWeight = FontWeight.Light)
    fun label(size: TextUnit = 12.sp, color: Color = Ink.dim) = TextStyle(color = color, fontSize = size, fontWeight = FontWeight.SemiBold, letterSpacing = 0.8.sp)
    fun value(size: TextUnit = 13.sp, color: Color = Ink.text) = TextStyle(color = color, fontSize = size)
}

internal val Corner = RoundedCornerShape(4.dp)

/** Adds the harness feature ids, or nothing when [ids] is empty. */
internal fun Modifier.ids(ids: List<String>): Modifier = if (ids.isEmpty()) this else featureId(*ids.toTypedArray())
internal fun Modifier.ids(vararg ids: String): Modifier = ids(ids.toList())

@Composable
internal fun T(text: String, style: TextStyle, modifier: Modifier = Modifier, align: TextAlign = TextAlign.Start, maxLines: Int = 1) =
    BasicText(text, modifier, style.copy(textAlign = align), overflow = TextOverflow.Ellipsis, maxLines = maxLines, softWrap = maxLines > 1)

/** Panel surface; [framed] is where `:design` draws the gold corner brackets, [texture] the PRE crosshatch / AMP circuit board. */
enum class Texture { PLAIN, PRE, AMP, POST }

@Composable
internal fun Frame(modifier: Modifier = Modifier, texture: Texture = Texture.PLAIN, framed: Boolean = false, content: @Composable BoxScope.() -> Unit) {
    Box(
        modifier
            .background(if (texture == Texture.PLAIN) Ink.panel else Ink.raised, Corner)
            .border(1.dp, if (framed) Ink.dim else Ink.line, Corner),
        content = content,
    )
}

/** Selection styles of the desktop `DrawVoLumSelection`; `:design` supplies the real AmberPicker / Brass / ListTeal drawing. */
enum class Pick { AMBER, BRASS, TEAL }

/**
 * A pressable surface with press feedback and the selection drawing for [selected].
 * Every tappable control in candidate A goes through this, so press states and the
 * selection language change in one place.
 */
@Composable
internal fun Tap(
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    ids: List<String> = emptyList(),
    selected: Boolean = false,
    pick: Pick = Pick.AMBER,
    enabled: Boolean = true,
    minSize: Dp = 48.dp,
    content: @Composable BoxScope.() -> Unit,
) {
    val source = remember { MutableInteractionSource() }
    val pressed by source.collectIsPressedAsState()
    val fill = when {
        pressed -> Ink.line
        selected && pick == Pick.AMBER -> Ink.raised
        else -> Color.Transparent
    }
    val edge = when {
        selected -> Ink.selected
        else -> Ink.line
    }
    Box(
        modifier
            .ids(ids)
            .clickable(source, indication = null, enabled = enabled, onClick = onClick)
            .background(fill, Corner)
            .border(if (selected) 1.5.dp else 1.dp, edge, Corner)
            .alpha(if (enabled) 1f else 0.4f)
            .defaultMinSizeBoth(minSize)
            .padding(horizontal = 6.dp),
        contentAlignment = Alignment.Center,
        content = content,
    )
}

/** Text button / selectable chip. */
@Composable
internal fun Chip(
    text: String,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    ids: List<String> = emptyList(),
    selected: Boolean = false,
    pick: Pick = Pick.AMBER,
    enabled: Boolean = true,
    size: TextUnit = 12.sp,
) = Tap(onClick, modifier, ids, selected, pick, enabled) {
    T(text, Type.label(size, if (selected) Ink.selected else Ink.text), align = TextAlign.Center)
}

/** On/off switch with its label; `:design` replaces the track drawing. */
@Composable
internal fun Switch(label: String, on: Boolean, onClick: () -> Unit, modifier: Modifier = Modifier, ids: List<String> = emptyList()) =
    Tap(onClick, modifier, ids, selected = on, pick = Pick.BRASS) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Led(on, Modifier.size(12.dp))
            Spacer(Modifier.width(6.dp))
            T(label, Type.label(12.sp, if (on) Ink.selected else Ink.dim))
        }
    }

@Composable
internal fun Led(on: Boolean, modifier: Modifier = Modifier) {
    Canvas(modifier.defaultMinSizeBoth(8.dp)) {
        drawCircle(if (on) Ink.selected else Ink.faint, radius = size.minDimension / 2f)
    }
}

/** Neutral line glyphs standing in for the desktop header icons until `:design` ships them. */
enum class Glyph { TUNER, METRONOME, GEAR, PREV, NEXT, CLOSE, EDIT, DOWN, PLUS }

@Composable
internal fun GlyphIcon(glyph: Glyph, modifier: Modifier = Modifier, color: Color = Ink.text) {
    Canvas(modifier.defaultMinSizeBoth(20.dp)) {
        val w = size.width
        val h = size.height
        val stroke = Stroke(width = 1.8.dp.toPx(), cap = StrokeCap.Round)
        fun line(x0: Float, y0: Float, x1: Float, y1: Float) = drawLine(color, Offset(x0 * w, y0 * h), Offset(x1 * w, y1 * h), stroke.width, StrokeCap.Round)
        when (glyph) {
            Glyph.TUNER -> {
                line(0.35f, 0.15f, 0.35f, 0.55f); line(0.65f, 0.15f, 0.65f, 0.55f)
                drawArc(color, 0f, 180f, false, Offset(0.35f * w, 0.35f * h), androidx.compose.ui.geometry.Size(0.3f * w, 0.4f * h), style = stroke)
                line(0.5f, 0.75f, 0.5f, 0.95f)
            }
            Glyph.METRONOME -> {
                line(0.3f, 0.9f, 0.45f, 0.1f); line(0.7f, 0.9f, 0.55f, 0.1f); line(0.45f, 0.1f, 0.55f, 0.1f)
                line(0.3f, 0.9f, 0.7f, 0.9f); line(0.5f, 0.7f, 0.75f, 0.25f)
            }
            Glyph.GEAR -> {
                drawCircle(color, 0.22f * w, style = stroke)
                for (i in 0 until 8) {
                    val a = Math.toRadians(i * 45.0)
                    val c = Offset(w / 2, h / 2)
                    drawLine(color, c + Offset((0.3f * w * Math.cos(a)).toFloat(), (0.3f * h * Math.sin(a)).toFloat()),
                        c + Offset((0.44f * w * Math.cos(a)).toFloat(), (0.44f * h * Math.sin(a)).toFloat()), stroke.width, StrokeCap.Round)
                }
            }
            Glyph.PREV -> { line(0.62f, 0.2f, 0.38f, 0.5f); line(0.38f, 0.5f, 0.62f, 0.8f) }
            Glyph.NEXT -> { line(0.38f, 0.2f, 0.62f, 0.5f); line(0.62f, 0.5f, 0.38f, 0.8f) }
            Glyph.CLOSE -> { line(0.25f, 0.25f, 0.75f, 0.75f); line(0.75f, 0.25f, 0.25f, 0.75f) }
            Glyph.EDIT -> { line(0.3f, 0.3f, 0.7f, 0.3f); line(0.7f, 0.3f, 0.7f, 0.7f); line(0.3f, 0.7f, 0.7f, 0.3f) }
            Glyph.DOWN -> { line(0.5f, 0.2f, 0.5f, 0.8f); line(0.3f, 0.6f, 0.5f, 0.8f); line(0.7f, 0.6f, 0.5f, 0.8f) }
            Glyph.PLUS -> { line(0.5f, 0.2f, 0.5f, 0.8f); line(0.2f, 0.5f, 0.8f, 0.5f) }
        }
    }
}

@Composable
internal fun IconTap(glyph: Glyph, onClick: () -> Unit, modifier: Modifier = Modifier, ids: List<String> = emptyList(), enabled: Boolean = true, desc: String) =
    Tap(onClick, modifier.semanticsDesc(desc), ids, enabled = enabled) { GlyphIcon(glyph, Modifier.padding(12.dp)) }

/**
 * Stand-in for the `:design` art (sunset hero, per-amp art, pedal-card motifs). It
 * only proves the surface reads [energy] per frame without recomposing; it draws a
 * neutral level ring, not VoLum art.
 */
@Composable
internal fun ArtSlot(label: String, energy: () -> Float, modifier: Modifier = Modifier, dim: Boolean = false) {
    Box(modifier.background(Ink.panel, Corner).border(1.dp, Ink.line, Corner), contentAlignment = Alignment.Center) {
        Canvas(Modifier.fillMaxSize()) {
            val e = energy()
            val r = size.minDimension * (0.22f + 0.12f * e)
            drawCircle(if (dim) Ink.faint else Ink.dim, r, style = Stroke(1.5.dp.toPx()))
            drawLine(Ink.line, Offset(0f, size.height * 0.7f), Offset(size.width, size.height * 0.7f), 1.dp.toPx())
        }
        T(label, Type.label(10.sp, Ink.faint), Modifier.align(Alignment.BottomCenter).padding(6.dp))
    }
}
