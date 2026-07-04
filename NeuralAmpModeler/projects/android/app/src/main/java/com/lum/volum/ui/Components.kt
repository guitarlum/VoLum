package com.lum.volum.ui

import androidx.compose.animation.animateColorAsState
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.lum.volum.ui.theme.Josefin
import com.lum.volum.ui.theme.Michroma
import com.lum.volum.ui.theme.VoLumColors

/** A raised, hairline-bordered panel used to group controls. */
@Composable
fun Panel(modifier: Modifier = Modifier, content: @Composable () -> Unit) {
    Box(
        modifier = modifier
            .clip(RoundedCornerShape(18.dp))
            .background(VoLumColors.Panel)
            .border(1.dp, VoLumColors.Hairline, RoundedCornerShape(18.dp))
            .padding(16.dp)
    ) { content() }
}

/** Small Michroma section eyebrow with an accent tick. */
@Composable
fun SectionLabel(text: String, accent: Color = VoLumColors.Amber) {
    Row(verticalAlignment = Alignment.CenterVertically) {
        Box(Modifier.size(6.dp).clip(CircleShape).background(accent))
        Text(
            text = text.uppercase(),
            fontFamily = Michroma,
            fontSize = 12.sp,
            letterSpacing = 3.sp,
            color = VoLumColors.TextMid,
            modifier = Modifier.padding(start = 8.dp),
        )
    }
}

/** Pedal/section header with an integrated on/off pill. */
@Composable
fun PedalHeader(name: String, enabled: Boolean, accent: Color, onToggle: (Boolean) -> Unit) {
    Row(
        Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(name.uppercase(), fontFamily = Michroma, fontSize = 13.sp, letterSpacing = 2.sp,
            color = if (enabled) VoLumColors.TextHi else VoLumColors.TextMid)
        TogglePill(enabled, accent) { onToggle(!enabled) }
    }
}

/** A compact ON/OFF pill toggle with an animated glow. */
@Composable
fun TogglePill(on: Boolean, accent: Color, onClick: () -> Unit) {
    val bg by animateColorAsState(if (on) accent.copy(alpha = 0.16f) else VoLumColors.PanelHi, label = "pillBg")
    val fg by animateColorAsState(if (on) accent else VoLumColors.TextLow, label = "pillFg")
    Box(
        Modifier
            .clip(RoundedCornerShape(50))
            .background(bg)
            .border(1.dp, if (on) accent.copy(alpha = 0.6f) else VoLumColors.Hairline, RoundedCornerShape(50))
            .clickable { onClick() }
            .padding(horizontal = 14.dp, vertical = 6.dp)
    ) {
        Text(if (on) "ON" else "OFF", fontFamily = Josefin, fontWeight = FontWeight.SemiBold,
            fontSize = 12.sp, letterSpacing = 2.sp, color = fg)
    }
}

/** Big round power/transport button. Amber = ready, red glow = live. */
@Composable
fun PowerButton(running: Boolean, onClick: () -> Unit) {
    val ring by animateColorAsState(if (running) VoLumColors.Red else VoLumColors.Amber, label = "powerRing")
    Box(
        Modifier
            .size(84.dp)
            .clip(CircleShape)
            .background(VoLumColors.PanelHi)
            .border(2.dp, ring, CircleShape)
            .clickable { onClick() },
        contentAlignment = Alignment.Center,
    ) {
        Canvas(Modifier.size(34.dp)) {
            val stroke = size.minDimension * 0.11f
            // Power glyph: broken ring + top stem.
            drawArc(
                color = ring, startAngle = -60f, sweepAngle = 300f, useCenter = false,
                style = androidx.compose.ui.graphics.drawscope.Stroke(width = stroke,
                    cap = androidx.compose.ui.graphics.StrokeCap.Round),
            )
            drawLine(
                color = ring,
                start = androidx.compose.ui.geometry.Offset(size.width / 2f, size.height * 0.06f),
                end = androidx.compose.ui.geometry.Offset(size.width / 2f, size.height * 0.5f),
                strokeWidth = stroke, cap = androidx.compose.ui.graphics.StrokeCap.Round,
            )
        }
    }
}

/** Horizontal output meter: teal -> amber -> red as level rises. */
@Composable
fun LevelMeter(peak: Float, modifier: Modifier = Modifier) {
    val p by animateFloatAsState(peak.coerceIn(0f, 1.2f) / 1.2f, label = "meter")
    Canvas(modifier.height(8.dp)) {
        val r = size.height / 2f
        drawRoundRect(
            color = VoLumColors.Void,
            cornerRadius = androidx.compose.ui.geometry.CornerRadius(r, r),
        )
        if (p > 0f) {
            val color = when {
                p > 0.85f -> VoLumColors.Red
                p > 0.6f -> VoLumColors.Amber
                else -> VoLumColors.Teal
            }
            drawRoundRect(
                color = color,
                size = androidx.compose.ui.geometry.Size(size.width * p, size.height),
                cornerRadius = androidx.compose.ui.geometry.CornerRadius(r, r),
            )
        }
    }
}

/** A labeled dropdown selector styled as a pill. */
@Composable
fun Selector(label: String, items: List<String>, selectedIndex: Int, onSelect: (Int) -> Unit, modifier: Modifier = Modifier) {
    var open by remember { mutableStateOf(false) }
    Column(modifier) {
        Text(label.uppercase(), fontFamily = Josefin, fontWeight = FontWeight.SemiBold, fontSize = 10.sp,
            letterSpacing = 2.sp, color = VoLumColors.TextMid, modifier = Modifier.padding(bottom = 6.dp, start = 2.dp))
        Box {
            Row(
                Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(12.dp))
                    .background(VoLumColors.PanelHi)
                    .border(1.dp, VoLumColors.Hairline, RoundedCornerShape(12.dp))
                    .clickable { open = true }
                    .padding(horizontal = 14.dp, vertical = 12.dp),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text(items.getOrNull(selectedIndex) ?: "-", fontFamily = Josefin, fontSize = 14.sp,
                    color = VoLumColors.TextHi, maxLines = 1, overflow = TextOverflow.Ellipsis,
                    modifier = Modifier.padding(end = 8.dp))
                Text("v", fontFamily = Josefin, fontSize = 12.sp, color = VoLumColors.Amber)
            }
            DropdownMenu(expanded = open, onDismissRequest = { open = false },
                modifier = Modifier.background(VoLumColors.Panel)) {
                items.forEachIndexed { i, item ->
                    DropdownMenuItem(
                        text = { Text(item, fontFamily = Josefin, fontSize = 14.sp,
                            color = if (i == selectedIndex) VoLumColors.Amber else VoLumColors.TextHi) },
                        onClick = { onSelect(i); open = false },
                    )
                }
            }
        }
    }
}

/** A compact segmented control for small mode lists. */
@Composable
fun ModeSegments(items: List<String>, selected: Int, accent: Color, modifier: Modifier = Modifier, onSelect: (Int) -> Unit) {
    Row(
        modifier
            .clip(RoundedCornerShape(10.dp))
            .background(VoLumColors.Void)
            .padding(3.dp),
        horizontalArrangement = Arrangement.spacedBy(3.dp),
    ) {
        items.forEachIndexed { i, item ->
            val sel = i == selected
            Box(
                Modifier
                    .clip(RoundedCornerShape(8.dp))
                    .background(if (sel) accent.copy(alpha = 0.18f) else Color.Transparent)
                    .clickable { onSelect(i) }
                    .padding(horizontal = 12.dp, vertical = 7.dp),
            ) {
                Text(item, fontFamily = Josefin, fontWeight = FontWeight.SemiBold, fontSize = 12.sp,
                    color = if (sel) accent else VoLumColors.TextMid)
            }
        }
    }
}
