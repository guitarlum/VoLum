package com.volum.design.components

import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.drawWithCache
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.ClipOp
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.drawscope.clipRect
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.ic
import com.volum.design.theme.withOpacity
import kotlin.math.max

/** Which corners carry a bracket. */
object Corners {
    const val TL = 1
    const val TR = 2
    const val BL = 4
    const val BR = 8
    const val ALL = 15
    /** The hero skips top-right: the Dual Amp chip lives there. */
    const val NO_TR = TL or BL or BR
}

/** `DrawCornerAccent` in a DrawScope: an L of [size] at ([x], [y]), opening right/down unless flipped. */
fun DrawScope.cornerAccent(x: Float, y: Float, size: Float, flipH: Boolean, flipV: Boolean, color: Color, width: Float = 1.dp.toPx()) {
    val dx = if (flipH) -size else size
    val dy = if (flipV) -size else size
    drawLine(color, Offset(x, y), Offset(x + dx, y), width)
    drawLine(color, Offset(x, y), Offset(x, y + dy), width)
}

/**
 * Gold L-brackets in the corners (`DrawCornerAccent` x4): the window (22 / 6),
 * hero (16 / 6), lanes (12 / 6), pedal cards (8 / 4), quiet blocks (6 / 3).
 */
fun Modifier.cornerBrackets(
    color: Color = VoLumColors.Corner,
    size: Dp = 16.dp,
    inset: Dp = 6.dp,
    corners: Int = Corners.ALL,
    width: Dp = 1.dp,
): Modifier = drawWithCache {
    val s = size.toPx()
    val m = inset.toPx()
    val w = width.toPx()
    onDrawWithContent {
        drawContent()
        val r = this.size.width
        val b = this.size.height
        if (corners and Corners.TL != 0) cornerAccent(m, m, s, false, false, color, w)
        if (corners and Corners.TR != 0) cornerAccent(r - m, m, s, true, false, color, w)
        if (corners and Corners.BL != 0) cornerAccent(m, b - m, s, false, true, color, w)
        if (corners and Corners.BR != 0) cornerAccent(r - m, b - m, s, true, true, color, w)
    }
}

/** `DrawPanelDepth`: top-lit gradient, a faint rim light along the top and a shadow line along the bottom. */
fun Modifier.panelDepth(radius: Dp = 0.dp): Modifier = drawWithCache {
    val brush = Brush.verticalGradient(listOf(VoLumColors.PanelTop, VoLumColors.PanelBot))
    val cr = CornerRadius(radius.toPx())
    val inX = 1.5.dp.toPx()
    val px = 1.dp.toPx()
    val shadow = ic(64, 0, 0, 0)
    onDrawBehind {
        if (radius > 0.dp) drawRoundRect(brush, cornerRadius = cr) else drawRect(brush)
        drawLine(VoLumColors.RimLight, Offset(inX, px), Offset(size.width - inX, px), px)
        drawLine(shadow, Offset(inX, size.height - px), Offset(size.width - inX, size.height - px), px)
    }
}

/** `DrawInsetWell`: recessed well (inputs, list backgrounds, readouts, meters). */
fun Modifier.insetWell(radius: Dp = 3.dp): Modifier = drawWithCache {
    val cr = CornerRadius(radius.toPx())
    val shade = Brush.verticalGradient(listOf(VoLumColors.InnerShadow, Color.Transparent), startY = 0f, endY = size.height * 0.5f)
    val px = 1.dp.toPx()
    val lip = ic(20, 255, 248, 238)
    onDrawBehind {
        drawRoundRect(VoLumColors.WellDark, cornerRadius = cr)
        drawRoundRect(shade, size = Size(size.width, size.height * 0.5f), cornerRadius = cr)
        drawLine(lip, Offset(px, size.height - px), Offset(size.width - px, size.height - px), px)
    }
}

/** `FillVGradient` + `DrawVignette`: the top-lit canvas behind every VoLum screen. NanoVG's vignette is a two-stop ramp from the centre. */
fun Modifier.volumCanvas(vignette: Int = 72): Modifier = drawWithCache {
    val bg = Brush.verticalGradient(listOf(VoLumColors.CanvasTop, VoLumColors.CanvasBot))
    val rad = max(size.width, size.height) * 0.72f
    val vig = Brush.radialGradient(listOf(Color.Transparent, ic(vignette, 0, 0, 0)), center = Offset(size.width / 2, size.height / 2), radius = rad)
    onDrawBehind {
        drawRect(bg)
        drawRect(vig)
    }
}

/** `VoLumBackgroundControl`'s outer gold frame with 22 px window brackets. */
fun Modifier.windowFrame(): Modifier = drawWithCache {
    val stroke = Stroke(1.dp.toPx())
    onDrawWithContent {
        drawContent()
        drawRect(VoLumColors.Frame, style = stroke)
    }
}.cornerBrackets(VoLumColors.Corner, 22.dp, 6.dp)

/** A 1 px rectangle inside the bounds (`IGraphics::DrawRect`). */
fun Modifier.hairlineFrame(color: Color = VoLumColors.Frame, width: Dp = 1.dp, radius: Dp = 0.dp): Modifier = drawWithCache {
    val w = width.toPx()
    val cr = CornerRadius(radius.toPx())
    val stroke = Stroke(w)
    onDrawWithContent {
        drawContent()
        drawRoundRect(color, topLeft = Offset(w / 2, w / 2), size = Size(size.width - w, size.height - w), cornerRadius = cr, style = stroke)
    }
}

/**
 * The PRE strip's diagonal crosshatch (`_DrawStrip`, dormant PRE): teal hatch
 * both ways every 8 px at 0.7 px, clipped to the strip, on the hero well.
 */
fun Modifier.preCrosshatch(fill: Boolean = true): Modifier = drawWithCache {
    val step = 8.dp.toPx()
    val w = 0.7.dp.toPx()
    val hatch = ic(25, 100, 180, 200)
    val lines = ArrayList<Float>()
    var d = -size.height
    while (d < size.width) {
        lines += d
        d += step
    }
    val offs = lines.toFloatArray()
    onDrawBehind {
        if (fill) drawRect(VoLumColors.HeroBg)
        val h = size.height
        val r = size.width
        clipRect(0f, 0f, r, h, ClipOp.Intersect) {
            for (o in offs) {
                drawLine(hatch, Offset(o, 0f), Offset(o + h, h), w)
                drawLine(hatch, Offset(r - o, 0f), Offset(r - o - h, h), w)
            }
        }
    }
}

/**
 * The collapsed AMP strip's circuit-board grid (`_DrawAmpStrip`): teal grid
 * every 12 px at 0.6 px inset 4, with a node dot on every other crossing.
 */
fun Modifier.ampCircuitBoard(fill: Boolean = true): Modifier = drawWithCache {
    val step = 12.dp.toPx()
    val inset = 4.dp.toPx()
    val w = 0.6.dp.toPx()
    val dot = 1.5.dp.toPx()
    val grid = ic(28, 100, 180, 200)
    val node = ic(35, 120, 210, 220)
    val two = 2.dp.toPx()
    onDrawBehind {
        if (fill) drawRect(VoLumColors.HeroBg)
        val r = size.width
        val b = size.height
        var y = step
        while (y < b - 1f) {
            drawLine(grid, Offset(inset, y), Offset(r - inset, y), w)
            y += step
        }
        var x = step / 2
        while (x < r - 1f) {
            drawLine(grid, Offset(x, inset), Offset(x, b - inset), w)
            x += step
        }
        y = step
        while (y < b - 1f) {
            x = step / 2
            while (x < r - 1f) {
                if (x > two && x < r - two && y > two && y < b - two) drawCircle(node, dot, Offset(x, y))
                x += step * 2
            }
            y += step * 2
        }
    }
}

/** The gold divider with a diamond under a title (`VoLumAmpNameControl`, `VoLumSubRowTextControl`). */
fun Modifier.diamondRule(): Modifier = drawWithCache {
    val line = ic(51, 200, 162, 78)
    val half = 3.dp.toPx()
    val reach = 50.dp.toPx()
    val gap = 6.dp.toPx()
    val px = 1.dp.toPx()
    onDrawBehind {
        val cx = size.width / 2
        val cy = size.height / 2
        drawLine(line, Offset(cx - reach, cy), Offset(cx - gap, cy), px)
        drawLine(line, Offset(cx + gap, cy), Offset(cx + reach, cy), px)
        drawLine(VoLumColors.GoldDim, Offset(cx, cy - half), Offset(cx + half, cy), px)
        drawLine(VoLumColors.GoldDim, Offset(cx + half, cy), Offset(cx, cy + half), px)
        drawLine(VoLumColors.GoldDim, Offset(cx, cy + half), Offset(cx - half, cy), px)
        drawLine(VoLumColors.GoldDim, Offset(cx - half, cy), Offset(cx, cy - half), px)
    }
}

internal fun Color.a(alpha: Float) = withOpacity(alpha)
