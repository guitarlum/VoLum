package com.lum.volum.ui

import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.text.BasicText
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import com.lum.volum.ui.theme.Josefin
import com.lum.volum.ui.theme.Michroma
import com.lum.volum.ui.theme.VoLumColors
import kotlin.math.cos
import kotlin.math.sin

/**
 * A rotary knob rendered on a Canvas: a dim 270-degree track, a glowing value
 * arc in [accent], and a pointer. Vertical drag (up = increase) sets the value.
 * Values are normalized 0..1; the caller supplies the human-readable [display].
 */
@Composable
fun Knob(
    label: String,
    value: Float,
    display: String,
    onValueChange: (Float) -> Unit,
    modifier: Modifier = Modifier,
    accent: Color = VoLumColors.Amber,
    enabled: Boolean = true,
    diameter: Int = 74,
) {
    val cb by rememberUpdatedState(onValueChange)
    val v = value.coerceIn(0f, 1f)
    val startDeg = 135f
    val sweep = 270f
    val trackColor = if (enabled) VoLumColors.AmberDim.copy(alpha = 0.55f) else VoLumColors.Hairline
    val liveAccent = if (enabled) accent else VoLumColors.TextLow

    Column(horizontalAlignment = Alignment.CenterHorizontally, modifier = modifier) {
        Canvas(
            modifier = Modifier
                .size(diameter.dp)
                .pointerInput(enabled) {
                    if (!enabled) return@pointerInput
                    detectDragGestures { change, drag ->
                        change.consume()
                        val next = (value - drag.y / 320f).coerceIn(0f, 1f)
                        cb(next)
                    }
                }
        ) {
            val stroke = size.minDimension * 0.10f
            val inset = stroke / 2f + size.minDimension * 0.06f
            val arcSize = Size(size.width - inset * 2, size.height - inset * 2)
            val topLeft = Offset(inset, inset)

            // Dim full track.
            drawArc(
                color = trackColor,
                startAngle = startDeg,
                sweepAngle = sweep,
                useCenter = false,
                topLeft = topLeft,
                size = arcSize,
                style = Stroke(width = stroke, cap = StrokeCap.Round),
            )
            // Glowing value arc (soft under-glow + crisp arc).
            if (v > 0f) {
                drawArc(
                    color = liveAccent.copy(alpha = 0.22f),
                    startAngle = startDeg,
                    sweepAngle = sweep * v,
                    useCenter = false,
                    topLeft = topLeft,
                    size = arcSize,
                    style = Stroke(width = stroke * 2.1f, cap = StrokeCap.Round),
                )
                drawArc(
                    color = liveAccent,
                    startAngle = startDeg,
                    sweepAngle = sweep * v,
                    useCenter = false,
                    topLeft = topLeft,
                    size = arcSize,
                    style = Stroke(width = stroke, cap = StrokeCap.Round),
                )
            }

            // Center cap with a subtle radial so it reads like a metal knob.
            val c = Offset(size.width / 2f, size.height / 2f)
            val capR = size.minDimension * 0.30f
            drawCircle(
                brush = Brush.radialGradient(
                    colors = listOf(VoLumColors.PanelHi, VoLumColors.Void),
                    center = Offset(c.x - capR * 0.3f, c.y - capR * 0.3f),
                    radius = capR * 1.6f,
                ),
                radius = capR,
                center = c,
            )
            // Pointer.
            val ang = Math.toRadians((startDeg + sweep * v).toDouble())
            val r0 = capR * 0.35f
            val r1 = capR * 0.95f
            drawLine(
                color = liveAccent,
                start = Offset(c.x + (r0 * cos(ang)).toFloat(), c.y + (r0 * sin(ang)).toFloat()),
                end = Offset(c.x + (r1 * cos(ang)).toFloat(), c.y + (r1 * sin(ang)).toFloat()),
                strokeWidth = stroke * 0.85f,
                cap = StrokeCap.Round,
            )
        }
        Spacer(Modifier.height(6.dp))
        BasicText(
            text = display,
            style = TextStyle(fontFamily = Michroma, fontSize = 11.sp, color = if (enabled) VoLumColors.TextHi else VoLumColors.TextLow, textAlign = TextAlign.Center),
        )
        Spacer(Modifier.height(2.dp))
        BasicText(
            text = label.uppercase(),
            style = TextStyle(fontFamily = Josefin, fontWeight = FontWeight.SemiBold, fontSize = 10.sp, letterSpacing = 1.5.sp, color = VoLumColors.TextMid, textAlign = TextAlign.Center),
        )
    }
}
