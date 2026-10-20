package com.volum.ui.c

import androidx.compose.foundation.layout.Box
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.Stroke
import com.volum.core.Telemetry
import kotlinx.coroutines.flow.StateFlow

/**
 * The stage the whole instrument floats over. Until `:design` delivers the hero and
 * per-amp art (ticket 02) this slot draws a neutral, energy-driven field so layout,
 * metering and motion can be judged; it is not VoLum's art and is replaced wholesale.
 */
@Composable
internal fun StageArt(tel: StateFlow<Telemetry>, art: Int, animate: Boolean, modifier: Modifier) {
    val t by tel.collectAsState()
    val cx = 0.5f + ((art % 5) - 2) * 0.05f
    Box(
        modifier.drawBehind {
            val e = t.energy.toFloat()
            val time = if (animate) t.timeMs / 1000f else 0f
            drawRect(Brush.verticalGradient(0f to Look.ground, 0.62f to Color(0xFF1A1A1D), 1f to Look.ground))
            val c = Offset(size.width * cx, size.height * 0.56f)
            val r = size.height * (0.32f + 0.22f * e)
            drawCircle(Brush.radialGradient(listOf(Look.glow.copy(alpha = 0.10f + 0.30f * e), Color.Transparent), c, r * 1.8f), r * 1.8f, c)
            for (i in 1..6) {
                val phase = ((time * 0.12f + i / 6f) % 1f)
                val rr = r * (0.4f + phase * 1.6f)
                drawCircle(Look.glow.copy(alpha = (0.18f * e + 0.04f) * (1f - phase)), rr, c, style = Stroke(1.2f))
            }
            if (t.pickAgeMs in 0..140) {
                val k = 1f - t.pickAgeMs / 140f
                drawCircle(Look.glow.copy(alpha = 0.25f * k * e), r * (1.05f + 0.25f * (1f - k)), c, style = Stroke(3f))
            }
            drawLine(Look.glow.copy(alpha = 0.15f + 0.35f * e), Offset(0f, size.height * 0.62f), Offset(size.width, size.height * 0.62f), 1.5f)
        },
    )
}
