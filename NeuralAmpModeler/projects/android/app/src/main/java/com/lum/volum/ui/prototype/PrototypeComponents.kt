package com.lum.volum.ui.prototype

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.gestures.detectVerticalDragGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableLongStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.ProgressBarRangeInfo
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.onLongClick
import androidx.compose.ui.semantics.progressBarRangeInfo
import androidx.compose.ui.semantics.role
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.setProgress
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.cos
import kotlin.math.sin

@Composable
fun PrototypeButton(
    label: String,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    active: Boolean = false,
    primary: Boolean = false,
    danger: Boolean = false,
) {
    val fill = when {
        danger -> PrototypeTheme.red.copy(alpha = .16f)
        primary -> PrototypeTheme.amber
        active -> PrototypeTheme.teal.copy(alpha = .18f)
        else -> PrototypeTheme.panelHigh
    }
    val ink = when {
        primary -> PrototypeTheme.canvas
        danger -> PrototypeTheme.red
        active -> PrototypeTheme.teal
        else -> PrototypeTheme.text
    }
    val stroke = when {
        danger -> PrototypeTheme.red
        primary -> PrototypeTheme.amber
        active -> PrototypeTheme.teal
        else -> PrototypeTheme.line
    }
    Box(
        modifier
            .height(48.dp)
            .clip(RoundedCornerShape(9.dp))
            .background(fill)
            .border(1.dp, stroke, RoundedCornerShape(9.dp))
            .clickable(role = Role.Button, onClick = onClick)
            .semantics {
                role = Role.Button
                contentDescription = label
            }
            .padding(horizontal = 8.dp),
        contentAlignment = Alignment.Center,
    ) {
        Text(
            label.uppercase(),
            color = ink,
            fontFamily = PrototypeTheme.body,
            fontWeight = FontWeight.Bold,
            fontSize = 10.sp,
            letterSpacing = .7.sp,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
        )
    }
}

@OptIn(ExperimentalFoundationApi::class)
@Composable
fun HoldPowerButton(
    powered: Boolean,
    onLongPress: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Box(
        modifier
            .height(48.dp)
            .clip(RoundedCornerShape(9.dp))
            .background(if (powered) PrototypeTheme.teal.copy(.16f) else PrototypeTheme.panelHigh)
            .border(
                1.dp,
                if (powered) PrototypeTheme.teal else PrototypeTheme.line,
                RoundedCornerShape(9.dp),
            )
            .combinedClickable(onClick = {}, onLongClick = onLongPress)
            .semantics {
                role = Role.Button
                contentDescription = if (powered) "Power live. Hold to stop." else "Power stopped. Hold to start."
                onLongClick("Toggle power") {
                    onLongPress()
                    true
                }
            },
        contentAlignment = Alignment.Center,
    ) {
        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            Text(
                if (powered) "LIVE" else "OFF",
                color = if (powered) PrototypeTheme.teal else PrototypeTheme.muted,
                fontFamily = PrototypeTheme.display,
                fontSize = 9.sp,
            )
            Text("HOLD", color = PrototypeTheme.muted, fontFamily = PrototypeTheme.body, fontSize = 8.sp)
        }
    }
}

@Composable
fun LevelMeter(
    level: Float,
    modifier: Modifier = Modifier,
    vertical: Boolean = false,
    label: String = "Output",
) {
    Canvas(modifier.semantics { contentDescription = "$label level ${(level * 100).toInt()} percent" }) {
        drawRoundRect(PrototypeTheme.inset, cornerRadius = CornerRadius(5.dp.toPx()))
        val safe = level.coerceIn(0f, 1f)
        val top = when {
            safe > .88f -> PrototypeTheme.red
            safe > .7f -> PrototypeTheme.amber
            else -> PrototypeTheme.teal
        }
        if (vertical) {
            drawRoundRect(
                brush = Brush.verticalGradient(listOf(top, PrototypeTheme.teal)),
                topLeft = Offset(0f, size.height * (1f - safe)),
                size = androidx.compose.ui.geometry.Size(size.width, size.height * safe),
                cornerRadius = CornerRadius(5.dp.toPx()),
            )
        } else {
            drawRoundRect(
                brush = Brush.horizontalGradient(listOf(PrototypeTheme.teal, top)),
                size = androidx.compose.ui.geometry.Size(size.width * safe, size.height),
                cornerRadius = CornerRadius(5.dp.toPx()),
            )
        }
    }
}

@Composable
fun EndpointTile(
    label: String,
    level: Float,
    modifier: Modifier = Modifier,
) {
    Column(
        modifier
            .height(92.dp)
            .background(PrototypeTheme.inset, RoundedCornerShape(8.dp))
            .border(1.dp, PrototypeTheme.line, RoundedCornerShape(8.dp))
            .padding(6.dp),
        verticalArrangement = Arrangement.SpaceBetween,
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Text(
            label,
            color = PrototypeTheme.muted,
            fontFamily = PrototypeTheme.display,
            fontSize = 7.sp,
            maxLines = 1,
        )
        LevelMeter(level, Modifier.fillMaxWidth().height(44.dp), vertical = true, label = label)
    }
}

@Composable
fun BlockTile(
    block: RigBlock,
    enabled: Boolean,
    selected: Boolean,
    available: Boolean = true,
    onSelect: () -> Unit,
    onToggle: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val border = when {
        selected -> PrototypeTheme.amber
        enabled && available -> PrototypeTheme.teal
        else -> PrototypeTheme.line
    }
    val background = when {
        selected -> PrototypeTheme.amber.copy(alpha = .12f)
        else -> PrototypeTheme.inset
    }
    Box(
        modifier
            .height(92.dp)
            .clip(RoundedCornerShape(8.dp))
            .background(background)
            .border(1.dp, border, RoundedCornerShape(8.dp))
            .clickable(enabled = available, role = Role.Button, onClick = onSelect)
            .semantics {
                role = Role.Button
                contentDescription = "${block.title}, ${if (!available) "unavailable" else if (enabled) "on" else "off"}"
            },
    ) {
        Column(
            Modifier.fillMaxWidth().padding(top = 9.dp, start = 3.dp, end = 3.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
        ) {
            Text(
                block.shortLabel,
                color = if (available) PrototypeTheme.text else PrototypeTheme.muted.copy(alpha = .6f),
                fontFamily = PrototypeTheme.display,
                fontSize = 7.sp,
                textAlign = TextAlign.Center,
                maxLines = 1,
                overflow = TextOverflow.Clip,
            )
            Text(
                if (!available) "LOCKED" else if (enabled) "ON" else "OFF",
                color = when {
                    !available -> PrototypeTheme.muted.copy(alpha = .6f)
                    enabled -> PrototypeTheme.teal
                    else -> PrototypeTheme.muted
                },
                fontFamily = PrototypeTheme.body,
                fontWeight = FontWeight.Bold,
                fontSize = 8.sp,
            )
        }
        Box(
            Modifier
                .align(Alignment.BottomCenter)
                .size(48.dp)
                .clickable(enabled = available, role = Role.Switch, onClick = onToggle)
                .semantics {
                    role = Role.Switch
                    contentDescription = "${if (enabled) "Disable" else "Enable"} ${block.title}"
                },
            contentAlignment = Alignment.Center,
        ) {
            Canvas(Modifier.size(22.dp)) {
                drawCircle(
                    if (enabled && available) PrototypeTheme.teal.copy(.18f) else PrototypeTheme.panelHigh,
                )
                drawArc(
                    if (enabled && available) PrototypeTheme.teal else PrototypeTheme.muted,
                    startAngle = -55f,
                    sweepAngle = 290f,
                    useCenter = false,
                    style = Stroke(2.dp.toPx(), cap = StrokeCap.Round),
                )
                drawLine(
                    if (enabled && available) PrototypeTheme.teal else PrototypeTheme.muted,
                    Offset(size.width / 2, 2.dp.toPx()),
                    Offset(size.width / 2, size.height / 2),
                    2.dp.toPx(),
                    StrokeCap.Round,
                )
            }
        }
    }
}

@Composable
fun TouchKnob(
    parameter: RigParameter,
    value: Float,
    onBegin: () -> Unit,
    onChange: (Float) -> Unit,
    modifier: Modifier = Modifier,
    diameter: Dp = 62.dp,
) {
    val latestValue by rememberUpdatedState(value)
    val latestChange by rememberUpdatedState(onChange)
    var startedAt by remember { mutableLongStateOf(0L) }
    var fine by remember { mutableStateOf(false) }
    Column(
        modifier.semantics {
            contentDescription = "${parameter.label}, ${(value * 100).toInt()} percent. Drag vertically; hold then drag for fine adjustment."
            progressBarRangeInfo = ProgressBarRangeInfo(value, 0f..1f)
            setProgress {
                onBegin()
                latestChange(it.coerceIn(0f, 1f))
                true
            }
        },
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Box(
            Modifier
                .size(diameter)
                .pointerInput(parameter) {
                    detectVerticalDragGestures(
                        onDragStart = {
                            startedAt = System.currentTimeMillis()
                            onBegin()
                        },
                        onVerticalDrag = { change, amount ->
                            change.consume()
                            fine = System.currentTimeMillis() - startedAt > 450L
                            val scale = if (fine) 900f else 320f
                            latestChange((latestValue - amount / scale).coerceIn(0f, 1f))
                        },
                        onDragEnd = { fine = false },
                        onDragCancel = { fine = false },
                    )
                },
        ) {
            Canvas(Modifier.fillMaxWidth().fillMaxHeight()) {
                val stroke = 7.dp.toPx()
                drawArc(
                    PrototypeTheme.line,
                    140f,
                    260f,
                    false,
                    style = Stroke(stroke, cap = StrokeCap.Round),
                )
                drawArc(
                    if (fine) PrototypeTheme.blue else PrototypeTheme.amber,
                    140f,
                    260f * value,
                    false,
                    style = Stroke(stroke, cap = StrokeCap.Round),
                )
                val angle = Math.toRadians((140f + 260f * value).toDouble())
                val center = Offset(size.width / 2, size.height / 2)
                val radius = size.minDimension * .30f
                drawLine(
                    PrototypeTheme.text,
                    center,
                    center + Offset(cos(angle).toFloat() * radius, sin(angle).toFloat() * radius),
                    2.dp.toPx(),
                    StrokeCap.Round,
                )
                drawCircle(PrototypeTheme.panelHigh, size.minDimension * .23f, center)
                drawCircle(
                    if (fine) PrototypeTheme.blue else PrototypeTheme.text,
                    2.dp.toPx(),
                    center,
                )
            }
        }
        Text(
            parameter.label.uppercase(),
            color = PrototypeTheme.muted,
            fontFamily = PrototypeTheme.body,
            fontWeight = FontWeight.Bold,
            fontSize = 8.sp,
            maxLines = 1,
        )
        Text(
            if (fine) "FINE ${(value * 100).toInt()}" else "${(value * 100).toInt()}",
            color = if (fine) PrototypeTheme.blue else PrototypeTheme.text,
            fontFamily = PrototypeTheme.display,
            fontSize = 8.sp,
            maxLines = 1,
        )
    }
}

@Composable
fun ModeSelector(
    labels: List<String>,
    selected: Int,
    onSelect: (Int) -> Unit,
    modifier: Modifier = Modifier,
) {
    Row(modifier, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
        labels.forEachIndexed { index, label ->
            PrototypeButton(
                label = label,
                onClick = { onSelect(index) },
                modifier = Modifier.weight(1f),
                active = selected == index,
            )
        }
    }
}

@Composable
fun SectionHeading(
    section: RigSection,
    selected: Boolean,
    onSelect: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Row(
        modifier
            .height(48.dp)
            .clip(RoundedCornerShape(topStart = 8.dp, topEnd = 8.dp))
            .background(if (selected) PrototypeTheme.amber.copy(.12f) else PrototypeTheme.panel)
            .clickable(role = Role.Tab, onClick = onSelect)
            .semantics {
                role = Role.Tab
                contentDescription = "Open ${section.name} section"
            }
            .padding(horizontal = 8.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.SpaceBetween,
    ) {
        Text(
            section.name.uppercase(),
            color = if (selected) PrototypeTheme.amber else PrototypeTheme.muted,
            fontFamily = PrototypeTheme.display,
            fontSize = 8.sp,
            letterSpacing = 1.sp,
        )
        Text("SECTION", color = PrototypeTheme.muted, fontFamily = PrototypeTheme.body, fontSize = 7.sp)
    }
}

@Composable
fun HairlineConnector(modifier: Modifier = Modifier) {
    Box(modifier.height(2.dp).background(PrototypeTheme.lineBright))
}

@Composable
fun ValueBadge(label: String, value: String, modifier: Modifier = Modifier, accent: Color = PrototypeTheme.teal) {
    Row(
        modifier
            .height(48.dp)
            .background(PrototypeTheme.inset, RoundedCornerShape(8.dp))
            .border(1.dp, PrototypeTheme.line, RoundedCornerShape(8.dp))
            .padding(horizontal = 8.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(4.dp),
    ) {
        Text(label.uppercase(), color = PrototypeTheme.muted, fontFamily = PrototypeTheme.body, fontSize = 8.sp)
        Text(
            value,
            color = accent,
            fontFamily = PrototypeTheme.display,
            fontSize = 9.sp,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
        )
    }
}
