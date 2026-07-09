package com.lum.volum.ui.proposals

import androidx.compose.foundation.Canvas
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.Stroke
import kotlin.math.cos
import kotlin.math.sin

@Composable
fun ConceptPreludeBackdrop(
    spec: ProposalSpec,
    palette: ProposalPalette,
    modifier: Modifier = Modifier,
) {
    if (spec.currentVibe) {
        FractalField(palette, modifier)
        return
    }
    Canvas(modifier) {
        when (spec.id) {
            ProposalId.ThumbDeck -> {
                repeat(4) { index ->
                    val inset = 22f + index * 22f
                    drawRoundRect(
                        color = if (index % 2 == 0) palette.primary.copy(.08f) else palette.secondary.copy(.06f),
                        topLeft = Offset(inset, inset + 60f),
                        size = Size(size.width - inset * 2f, size.height - inset * 1.6f),
                        cornerRadius = CornerRadius(42f),
                        style = Stroke(2f),
                    )
                }
            }
            ProposalId.BlackoutLive -> {
                repeat(7) { index ->
                    val y = size.height * index / 6f
                    drawLine(palette.hairline.copy(.35f), Offset(0f, y), Offset(size.width, y), 1f)
                }
                drawCircle(
                    brush = Brush.radialGradient(listOf(palette.secondary.copy(.11f), Color.Transparent)),
                    radius = size.minDimension * .72f,
                    center = center,
                )
            }
            ProposalId.FlightRack -> {
                repeat(5) { index ->
                    val top = 30f + index * size.height / 5.5f
                    val path = Path().apply {
                        moveTo(20f, top)
                        lineTo(size.width - 58f, top)
                        lineTo(size.width - 26f, top + 28f)
                        lineTo(20f, top + 28f)
                        close()
                    }
                    drawPath(path, if (index % 2 == 0) palette.primary.copy(.05f) else palette.surface.copy(.2f))
                    drawPath(path, palette.hairline.copy(.45f), style = Stroke(1.5f))
                }
            }
            ProposalId.OrbitCockpit -> {
                val r = size.minDimension * .17f
                repeat(4) { ring ->
                    drawCircle(palette.hairline.copy(.55f), r * (ring + 1), center, style = Stroke(if (ring == 1) 3f else 1.5f))
                }
                repeat(6) { sector ->
                    val angle = Math.toRadians((sector * 60 - 90).toDouble())
                    val point = Offset(
                        center.x + cos(angle).toFloat() * r * 3.6f,
                        center.y + sin(angle).toFloat() * r * 3.6f,
                    )
                    drawLine(
                        if (sector % 2 == 0) palette.primary.copy(.28f) else palette.secondary.copy(.22f),
                        center,
                        point,
                        2f,
                    )
                }
            }
            else -> Unit
        }
    }
}
