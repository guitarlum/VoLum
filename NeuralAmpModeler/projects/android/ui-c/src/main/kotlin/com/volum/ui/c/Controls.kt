package com.volum.ui.c

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectHorizontalDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicText
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.semantics.CustomAccessibilityAction
import androidx.compose.ui.semantics.ProgressBarRangeInfo
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.customActions
import androidx.compose.ui.semantics.progressBarRangeInfo
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.setProgress
import androidx.compose.ui.semantics.setText
import androidx.compose.ui.semantics.stateDescription
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import com.volum.core.Action
import com.volum.core.Param
import com.volum.core.Telemetry
import kotlinx.coroutines.flow.StateFlow
import kotlin.math.abs
import kotlin.math.cos
import kotlin.math.roundToInt
import kotlin.math.sin

/** How a param reads on its knob, and how typed text maps back to it. */
internal object Fmt {
    private fun isPercent(p: Param) = p.min == 0.0 && p.max <= 1.0 && p.unit.isEmpty()

    fun of(p: Param, v: Double): String = when {
        p == Param.OUTPUT && v <= p.min -> "−∞"
        p == Param.SUPPORT_OUTPUT && v <= p.min -> "−∞"
        p == Param.MAIN_PAN || p == Param.SUPPORT_PAN -> pan(v)
        p.unit == "dB" -> "%+.1f dB".format(v)
        p.unit == "ms" -> if (v < 10) "%.1f ms".format(v) else "%.0f ms".format(v)
        p.unit == "Hz" -> if (v >= 1000) "%.2f kHz".format(v / 1000) else if (p.max <= 20) "%.1f Hz".format(v) else "%.0f Hz".format(v)
        p.unit == "s" -> "%.1f s".format(v)
        p.unit == "st" -> "%+d st".format(v.roundToInt())
        isPercent(p) -> "%.0f%%".format(v * 100)
        else -> "%.1f".format(v)
    }

    fun pan(v: Double): String = when {
        abs(v) < 0.02 -> "C"
        v < 0 -> "L %.0f".format(-v * 100)
        else -> "R %.0f".format(v * 100)
    }

    fun parse(p: Param, text: String): Double? {
        val n = text.replace(',', '.').filter { it.isDigit() || it == '.' || it == '-' || it == '+' }.toDoubleOrNull() ?: return null
        return when {
            isPercent(p) && n > 1.0 -> n / 100.0
            p == Param.MAIN_PAN || p == Param.SUPPORT_PAN -> if (abs(n) > 1.0) n / 100.0 else n
            else -> n
        }
    }
}

/** A knob bound to one rig param. */
@Composable
internal fun ParamKnob(ui: Ui, p: Param, ids: List<String>, label: String = p.label, glow: Float = 0f) {
    Knob(
        label = label,
        value = ui.s.param(p),
        min = p.min,
        max = p.max,
        ids = ids,
        format = { Fmt.of(p, it) },
        parse = { Fmt.parse(p, it) },
        onSet = { ui.d(Action.SetParam(p, it)) },
        onReset = { ui.d(Action.ResetParam(p)) },
        glow = glow,
    )
}

/**
 * Vertical drag sets the value (full range over 220 dp); sliding the finger more than
 * 64 dp sideways while dragging slows it to a quarter for fine adjust. Double tap
 * resets, long press opens exact entry. A value bubble rides above the dial while it
 * moves so the finger never hides the number being set.
 */
@Composable
internal fun Knob(
    label: String,
    value: Double,
    min: Double,
    max: Double,
    ids: List<String>,
    format: (Double) -> String,
    parse: (String) -> Double?,
    onSet: (Double) -> Unit,
    onReset: () -> Unit,
    modifier: Modifier = Modifier,
    glow: Float = 0f,
    dial: Dp = 56.dp,
) {
    var dragging by remember { mutableStateOf(false) }
    var fine by remember { mutableStateOf(false) }
    var editing by remember { mutableStateOf(false) }
    val current by rememberUpdatedState(value)
    val set by rememberUpdatedState(onSet)
    val reset by rememberUpdatedState(onReset)
    val span = (max - min).takeIf { it > 0 } ?: 1.0
    val norm = ((value - min) / span).toFloat().coerceIn(0f, 1f)
    val text = format(value)
    Column(modifier.width(84.dp), horizontalAlignment = Alignment.CenterHorizontally) {
        Box(Modifier.height(18.dp).fillMaxWidth(), contentAlignment = Alignment.Center) {
            if (dragging) {
                Box(Modifier.clip(RoundedCornerShape(9.dp)).background(Look.pick).padding(horizontal = 8.dp)) {
                    BasicText(if (fine) "$text ·" else text, style = techStyle(11f, Look.pickInk), maxLines = 1)
                }
            } else {
                BasicText(label, style = labelStyle(10f), maxLines = 1)
            }
        }
        Box(
            Modifier
                .size(dial)
                .pointerInput(min, max) {
                    var n = 0f
                    var dx = 0f
                    detectDragGestures(
                        onDragStart = { n = ((current - min) / span).toFloat().coerceIn(0f, 1f); dx = 0f; dragging = true; fine = false },
                        onDragEnd = { dragging = false },
                        onDragCancel = { dragging = false },
                    ) { change, amount ->
                        change.consume()
                        dx += amount.x
                        fine = abs(dx) > 64.dp.toPx()
                        n = (n - amount.y / 220.dp.toPx() * (if (fine) 0.25f else 1f)).coerceIn(0f, 1f)
                        set(min + n * span)
                    }
                }
                .pointerInput(Unit) { detectTapGestures(onDoubleTap = { reset() }, onLongPress = { editing = true }) }
                .semantics {
                    contentDescription = label
                    stateDescription = text
                    progressBarRangeInfo = ProgressBarRangeInfo(value.toFloat(), min.toFloat()..max.toFloat())
                    setProgress { v -> set(v.toDouble().coerceIn(min, max)); true }
                    setText { t -> parse(t.text)?.let { set(it.coerceIn(min, max)); true } ?: false }
                    customActions = listOf(CustomAccessibilityAction("Reset") { reset(); true })
                }
                .ids(ids)
                .drawBehind { drawDial(norm, glow, dragging) },
        )
        if (editing) {
            ExactEntry(value, format, parse, onDone = { v -> v?.let { set(it.coerceIn(min, max)) }; editing = false })
        } else {
            BasicText(text, style = techStyle(11f, if (dragging) Look.pick else Look.text), maxLines = 1, modifier = Modifier.height(18.dp))
        }
    }
}

private fun androidx.compose.ui.graphics.drawscope.DrawScope.drawDial(norm: Float, glow: Float, active: Boolean) {
    val stroke = 4.dp.toPx()
    val inset = stroke
    val arcSize = Size(size.width - inset * 2, size.height - inset * 2)
    val tl = Offset(inset, inset)
    drawCircle(Look.raised, radius = size.minDimension / 2 - stroke * 1.6f)
    drawCircle(Look.line, radius = size.minDimension / 2 - stroke * 1.6f, style = Stroke(1.dp.toPx()))
    drawArc(Look.line, 135f, 270f, false, tl, arcSize, style = Stroke(stroke, cap = StrokeCap.Round))
    val ink = if (active) Look.pick else Look.glow.copy(alpha = 0.7f + 0.3f * glow)
    drawArc(ink, 135f, 270f * norm, false, tl, arcSize, style = Stroke(stroke, cap = StrokeCap.Round))
    val a = Math.toRadians(135.0 + 270.0 * norm)
    val c = center
    val r = size.minDimension / 2 - stroke * 2.6f
    drawLine(Look.text, c + Offset((cos(a) * r * 0.35f).toFloat(), (sin(a) * r * 0.35f).toFloat()), c + Offset((cos(a) * r).toFloat(), (sin(a) * r).toFloat()), 2.5.dp.toPx(), cap = StrokeCap.Round)
}

@Composable
private fun ExactEntry(value: Double, format: (Double) -> String, parse: (String) -> Double?, onDone: (Double?) -> Unit) {
    var text by remember { mutableStateOf(format(value).filter { it.isDigit() || it == '.' || it == '-' || it == '+' }) }
    val focus = remember { FocusRequester() }
    LaunchedEffect(Unit) { focus.requestFocus() }
    BasicTextField(
        value = text,
        onValueChange = { text = it },
        singleLine = true,
        textStyle = techStyle(12f, Look.pickInk).copy(textAlign = TextAlign.Center),
        cursorBrush = SolidColor(Look.pickInk),
        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal, imeAction = ImeAction.Done),
        keyboardActions = KeyboardActions(onDone = { onDone(parse(text)) }),
        modifier = Modifier.width(72.dp).height(22.dp).clip(RoundedCornerShape(6.dp)).background(Look.pick).focusRequester(focus),
    )
}

/** A horizontal fader: drag or tap the track; same semantics as a knob. */
@Composable
internal fun Fader(
    label: String,
    value: Double,
    min: Double,
    max: Double,
    ids: List<String>,
    format: (Double) -> String,
    onSet: (Double) -> Unit,
    modifier: Modifier = Modifier,
) {
    val set by rememberUpdatedState(onSet)
    val span = (max - min).takeIf { it > 0 } ?: 1.0
    val norm = ((value - min) / span).toFloat().coerceIn(0f, 1f)
    Column(modifier) {
        androidx.compose.foundation.layout.Row(Modifier.fillMaxWidth()) {
            BasicText(label.uppercase(), style = labelStyle(10f), maxLines = 1)
            androidx.compose.foundation.layout.Spacer(Modifier.weight(1f))
            BasicText(format(value), style = techStyle(12f), maxLines = 1)
        }
        Box(
            Modifier
                .fillMaxWidth()
                .height(Metrics.target)
                .pointerInput(min, max) {
                    detectTapGestures { o -> set(min + (o.x / size.width).coerceIn(0f, 1f) * span) }
                }
                .pointerInput(min, max) {
                    detectHorizontalDragGestures { change, _ ->
                        change.consume()
                        set(min + (change.position.x / size.width).coerceIn(0f, 1f) * span)
                    }
                }
                .semantics {
                    contentDescription = label
                    stateDescription = format(value)
                    progressBarRangeInfo = ProgressBarRangeInfo(value.toFloat(), min.toFloat()..max.toFloat())
                    setProgress { v -> set(v.toDouble().coerceIn(min, max)); true }
                }
                .ids(ids)
                .drawBehind {
                    val y = size.height / 2
                    val h = 6.dp.toPx()
                    drawRoundRect(Look.raised, Offset(0f, y - h / 2), Size(size.width, h), androidx.compose.ui.geometry.CornerRadius(h / 2))
                    drawRoundRect(Look.glow, Offset(0f, y - h / 2), Size(size.width * norm, h), androidx.compose.ui.geometry.CornerRadius(h / 2))
                    drawCircle(Look.pick, 11.dp.toPx(), Offset(size.width * norm, y))
                    drawCircle(Look.ground, 4.dp.toPx(), Offset(size.width * norm, y))
                },
        )
    }
}

/** Text entry box. [onChange] receives every edit; the field keeps its own text so partial input survives. */
@Composable
internal fun Field(
    value: String,
    ids: List<String>,
    onChange: (String) -> Unit,
    modifier: Modifier = Modifier,
    numeric: Boolean = false,
    onDone: () -> Unit = {},
    hint: String = "",
    autofocus: Boolean = false,
    key: Any? = Unit,
) {
    var text by remember(key) { mutableStateOf(value) }
    val focus = remember { FocusRequester() }
    if (autofocus) LaunchedEffect(Unit) { runCatching { focus.requestFocus() } }
    BasicTextField(
        value = text,
        onValueChange = { text = it; onChange(it) },
        singleLine = true,
        textStyle = bodyStyle(15f),
        cursorBrush = SolidColor(Look.pick),
        keyboardOptions = KeyboardOptions(keyboardType = if (numeric) KeyboardType.Decimal else KeyboardType.Text, imeAction = ImeAction.Done),
        keyboardActions = KeyboardActions(onDone = { onDone() }),
        modifier = modifier
            .height(Metrics.target)
            .clip(Corner)
            .background(Look.ground)
            .border(1.dp, Look.lineHi, Corner)
            .focusRequester(focus)
            .ids(ids),
        decorationBox = { inner ->
            Box(Modifier.padding(horizontal = 12.dp), contentAlignment = Alignment.CenterStart) {
                if (text.isEmpty() && hint.isNotEmpty()) BasicText(hint, style = bodyStyle(14f, Look.faint))
                inner()
            }
        },
    )
}

/** A segmented level ladder fed from the telemetry flow; reads happen in the draw phase only. */
@Composable
internal fun Meter(tel: StateFlow<Telemetry>, modifier: Modifier, vertical: Boolean = true, pick: (Telemetry) -> Double) {
    val t by tel.collectAsState()
    Box(
        modifier.drawBehind {
            val level = t.meter(pick(t)).toFloat()
            val segments = 14
            val gap = 2.dp.toPx()
            for (i in 0 until segments) {
                val f = (i + 1f) / segments
                val lit = level >= f - 0.5f / segments
                val c = when {
                    f > 0.93f -> Look.danger
                    f > 0.8f -> Look.warn
                    else -> Look.glow
                }
                val color = if (lit) c else Look.raised
                if (vertical) {
                    val h = (size.height - gap * (segments - 1)) / segments
                    drawRect(color, Offset(0f, size.height - (i + 1) * h - i * gap), Size(size.width, h))
                } else {
                    val w = (size.width - gap * (segments - 1)) / segments
                    drawRect(color, Offset(i * (w + gap), 0f), Size(w, size.height))
                }
            }
        },
    )
}

/** A lamp lit by a telemetry predicate. */
@Composable
internal fun Lamp(tel: StateFlow<Telemetry>, color: Color, modifier: Modifier = Modifier, size: Dp = 8.dp, lit: (Telemetry) -> Boolean) {
    val t by tel.collectAsState()
    Box(
        modifier.size(size).drawBehind {
            val on = lit(t)
            if (on) drawCircle(color.copy(alpha = 0.3f), radius = this.size.minDimension)
            drawCircle(if (on) color else Look.faint, radius = this.size.minDimension / 2)
        },
    )
}

@Composable
internal fun Frame(modifier: Modifier = Modifier, content: @Composable () -> Unit) {
    Box(modifier.border(1.dp, Look.line, Corner).padding(8.dp)) { content() }
}
