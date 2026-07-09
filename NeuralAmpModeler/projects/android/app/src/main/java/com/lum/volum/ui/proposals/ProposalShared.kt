package com.lum.volum.ui.proposals

import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectHorizontalDragGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Slider
import androidx.compose.material3.SliderDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.role
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.cos
import kotlin.math.sin

@Composable
fun LabButton(
    text: String,
    palette: ProposalPalette,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    primary: Boolean = true,
) {
    val shape = RoundedCornerShape(if (primary) 14.dp else 10.dp)
    Box(
        modifier
            .height(52.dp)
            .clip(shape)
            .background(if (primary) palette.primary else palette.surfaceHigh)
            .border(1.dp, if (primary) palette.primary else palette.hairline, shape)
            .clickable(role = Role.Button, onClick = onClick)
            .semantics { contentDescription = text },
        contentAlignment = Alignment.Center,
    ) {
        Text(
            text.uppercase(),
            color = if (primary) palette.background else palette.text,
            fontFamily = palette.bodyFont,
            fontWeight = FontWeight.Bold,
            fontSize = 10.sp,
            letterSpacing = .8.sp,
            maxLines = 1,
        )
    }
}

@Composable
fun CompactAction(
    text: String,
    palette: ProposalPalette,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    active: Boolean = false,
) {
    val shape = RoundedCornerShape(9.dp)
    Box(
        modifier
            .height(48.dp)
            .clip(shape)
            .background(if (active) palette.primary.copy(alpha = .18f) else palette.surfaceHigh)
            .border(1.dp, if (active) palette.primary else palette.hairline, shape)
            .clickable(role = Role.Button, onClick = onClick)
            .padding(horizontal = 12.dp),
        contentAlignment = Alignment.Center,
    ) {
        Text(
            text.uppercase(),
            color = if (active) palette.primary else palette.text,
            fontFamily = palette.bodyFont,
            fontWeight = FontWeight.Bold,
            fontSize = 9.sp,
            letterSpacing = .7.sp,
            maxLines = 1,
        )
    }
}

@Composable
fun LiveStatus(
    state: DemoSnapshot,
    palette: ProposalPalette,
    modifier: Modifier = Modifier,
    compact: Boolean = false,
) {
    Row(
        modifier
            .semantics { contentDescription = "Live status, ${state.latencyMs} milliseconds, ${state.xruns} xruns" },
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(if (compact) 8.dp else 12.dp),
    ) {
        Box(
            Modifier
                .size(if (compact) 9.dp else 12.dp)
                .clip(CircleShape)
                .background(if (state.powered) palette.secondary else palette.muted),
        )
        Column {
            Text(
                if (state.powered) "LIVE" else "STANDBY",
                color = if (state.powered) palette.secondary else palette.muted,
                fontFamily = palette.displayFont,
                fontSize = if (compact) 9.sp else 11.sp,
                letterSpacing = 1.4.sp,
            )
            Text(
                "${state.latencyMs} ms  ·  ${state.xruns} xr",
                color = palette.muted,
                fontFamily = palette.bodyFont,
                fontSize = if (compact) 9.sp else 11.sp,
            )
        }
    }
}

@Composable
fun OutputMeter(
    peak: Float,
    palette: ProposalPalette,
    modifier: Modifier = Modifier,
    vertical: Boolean = false,
) {
    Canvas(modifier.semantics { contentDescription = "Output level ${(peak * 100).toInt()} percent" }) {
        drawRoundRect(palette.surfaceHigh, cornerRadius = CornerRadius(8f))
        val level = peak.coerceIn(0f, 1f)
        val color = if (level > .88f) palette.danger else if (level > .7f) palette.primary else palette.secondary
        if (vertical) {
            drawRoundRect(
                brush = Brush.verticalGradient(listOf(color, palette.secondary)),
                topLeft = Offset(0f, size.height * (1f - level)),
                size = Size(size.width, size.height * level),
                cornerRadius = CornerRadius(8f),
            )
        } else {
            drawRoundRect(
                brush = Brush.horizontalGradient(listOf(palette.secondary, color)),
                size = Size(size.width * level, size.height),
                cornerRadius = CornerRadius(8f),
            )
        }
    }
}

@Composable
fun FractalField(palette: ProposalPalette, modifier: Modifier = Modifier) {
    val transition = rememberInfiniteTransition(label = "fractal")
    val drift by transition.animateFloat(
        initialValue = 0f,
        targetValue = 1f,
        animationSpec = infiniteRepeatable(tween(8000), RepeatMode.Reverse),
        label = "drift",
    )
    Canvas(modifier) {
        drawRect(Brush.radialGradient(listOf(palette.secondary.copy(.16f), Color.Transparent)))
        repeat(9) { branch ->
            val path = Path()
            val x0 = size.width * (branch / 8f)
            path.moveTo(x0, size.height)
            repeat(7) { segment ->
                val y = size.height - segment * size.height / 6f
                val wave = sin((branch * 1.7f + segment + drift).toDouble()).toFloat()
                path.lineTo(x0 + wave * size.width * .12f, y)
            }
            drawPath(
                path,
                color = if (branch % 2 == 0) palette.primary.copy(.24f) else palette.secondary.copy(.2f),
                style = Stroke(width = if (branch % 3 == 0) 2.2f else 1.2f),
            )
        }
    }
}

@Composable
fun SafeFader(
    label: String,
    value: Float,
    palette: ProposalPalette,
    onValueChange: (Float) -> Unit,
    modifier: Modifier = Modifier,
) {
    Column(modifier.semantics { contentDescription = "$label ${(value * 100).toInt()} percent" }) {
        Text(
            "${label.uppercase()}  ·  ${(value * 100).toInt()}",
            color = palette.text,
            fontFamily = palette.bodyFont,
            fontWeight = FontWeight.Bold,
            fontSize = 9.sp,
            maxLines = 1,
        )
        Slider(
            value = value,
            onValueChange = onValueChange,
            colors = SliderDefaults.colors(
                thumbColor = palette.primary,
                activeTrackColor = palette.primary,
                inactiveTrackColor = palette.surfaceHigh,
            ),
        )
    }
}

@Composable
fun HorizontalScrub(
    label: String,
    value: Float,
    palette: ProposalPalette,
    onValueChange: (Float) -> Unit,
    modifier: Modifier = Modifier,
) {
    val latest by rememberUpdatedState(onValueChange)
    val current by rememberUpdatedState(value)
    val shape = RoundedCornerShape(12.dp)
    Row(
        modifier
            .height(64.dp)
            .clip(shape)
            .background(palette.surfaceHigh)
            .border(1.dp, palette.hairline, shape)
            .pointerInput(Unit) {
                detectHorizontalDragGestures { change, drag ->
                    change.consume()
                    latest((current + drag / 520f).coerceIn(0f, 1f))
                }
            }
            .padding(horizontal = 16.dp)
            .semantics { contentDescription = "$label, drag horizontally, ${(value * 100).toInt()} percent" },
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Column(Modifier.weight(1f)) {
            Text(label.uppercase(), color = palette.muted, fontFamily = palette.bodyFont, fontSize = 10.sp, letterSpacing = 1.2.sp)
            Text("${(value * 100).toInt()}%", color = palette.text, fontFamily = palette.displayFont, fontSize = 15.sp)
        }
        Box(Modifier.width(96.dp).height(5.dp).background(palette.background, RoundedCornerShape(3.dp))) {
            Box(Modifier.fillMaxWidth(value).fillMaxHeight().background(palette.primary, RoundedCornerShape(3.dp)))
        }
        Text("  ↔", color = palette.primary, fontSize = 18.sp)
    }
}

@Composable
fun FocusDial(
    label: String,
    value: Float,
    palette: ProposalPalette,
    onValueChange: (Float) -> Unit,
    modifier: Modifier = Modifier,
    diameter: Dp = 140.dp,
    showSteps: Boolean = true,
) {
    Column(modifier, horizontalAlignment = Alignment.CenterHorizontally) {
        Canvas(Modifier.size(diameter).semantics { contentDescription = "$label ${(value * 100).toInt()} percent" }) {
            val stroke = 13.dp.toPx()
            drawArc(palette.surfaceHigh, 140f, 260f, false, style = Stroke(stroke, cap = StrokeCap.Round))
            drawArc(palette.primary.copy(.24f), 140f, 260f * value, false, style = Stroke(stroke * 1.8f, cap = StrokeCap.Round))
            drawArc(palette.primary, 140f, 260f * value, false, style = Stroke(stroke, cap = StrokeCap.Round))
            val angle = Math.toRadians((140f + 260f * value).toDouble())
            val center = Offset(size.width / 2f, size.height / 2f)
            val r = size.minDimension * .35f
            drawLine(
                palette.primary,
                center,
                Offset(center.x + cos(angle).toFloat() * r, center.y + sin(angle).toFloat() * r),
                5.dp.toPx(),
                StrokeCap.Round,
            )
        }
        Text("${(value * 100).toInt()}", color = palette.text, fontFamily = palette.displayFont, fontSize = if (showSteps) 28.sp else 22.sp)
        Text(label.uppercase(), color = palette.muted, fontFamily = palette.bodyFont, fontSize = 12.sp, letterSpacing = 2.sp)
        if (showSteps) {
            Spacer(Modifier.height(10.dp))
            Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                CompactAction("−", palette, { onValueChange((value - .02f).coerceAtLeast(0f)) }, Modifier.width(58.dp))
                CompactAction("+", palette, { onValueChange((value + .02f).coerceAtMost(1f)) }, Modifier.width(58.dp))
            }
        }
    }
}

@Composable
fun EffectToggle(
    name: String,
    enabled: Boolean,
    value: String,
    palette: ProposalPalette,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val shape = RoundedCornerShape(14.dp)
    Column(
        modifier
            .height(104.dp)
            .clip(shape)
            .background(if (enabled) palette.primary.copy(.13f) else palette.surface)
            .border(1.dp, if (enabled) palette.primary else palette.hairline, shape)
            .clickable(role = Role.Switch, onClick = onClick)
            .padding(14.dp)
            .semantics {
                role = Role.Switch
                contentDescription = "$name ${if (enabled) "on" else "off"}, $value"
            },
        verticalArrangement = Arrangement.SpaceBetween,
    ) {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
            Text(name.uppercase(), color = if (enabled) palette.primary else palette.text, fontFamily = palette.displayFont, fontSize = 10.sp)
            Box(Modifier.size(9.dp).clip(CircleShape).background(if (enabled) palette.primary else palette.hairline))
        }
        Text(value, color = palette.muted, fontFamily = palette.bodyFont, fontSize = 12.sp)
    }
}

@Composable
fun ModeRow(
    labels: List<String>,
    selected: Int,
    palette: ProposalPalette,
    onSelect: (Int) -> Unit,
    modifier: Modifier = Modifier,
) {
    Row(modifier, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        labels.forEachIndexed { index, label ->
            CompactAction(label, palette, { onSelect(index) }, Modifier.weight(1f), active = index == selected)
        }
    }
}

@Composable
fun ScreenTitle(
    eyebrow: String,
    title: String,
    palette: ProposalPalette,
    modifier: Modifier = Modifier,
    alignEnd: Boolean = false,
) {
    Column(modifier, horizontalAlignment = if (alignEnd) Alignment.End else Alignment.Start) {
        Text(eyebrow.uppercase(), color = palette.primary, fontFamily = palette.bodyFont, fontSize = 10.sp, letterSpacing = 2.sp)
        Text(
            title,
            color = palette.text,
            fontFamily = palette.displayFont,
            fontSize = 22.sp,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
            textAlign = if (alignEnd) TextAlign.End else TextAlign.Start,
        )
    }
}
