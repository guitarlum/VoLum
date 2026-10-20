package com.volum.ui.a

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.ScrollState
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectHorizontalDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.defaultMinSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.layout
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.semantics.CustomAccessibilityAction
import androidx.compose.ui.semantics.ProgressBarRangeInfo
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.customActions
import androidx.compose.ui.semantics.progressBarRangeInfo
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.setProgress
import androidx.compose.ui.semantics.setText
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.abs

internal fun Modifier.defaultMinSizeBoth(size: Dp): Modifier = defaultMinSize(size, size)
internal fun Modifier.semanticsDesc(desc: String): Modifier = semantics { contentDescription = desc }

/**
 * Rotary knob: vertical drag over 160 dp sweeps the whole range; sliding the finger
 * sideways while dragging divides the speed (fine adjust); double-tap opens exact
 * entry in place of the readout. Semantics: range + SetProgress, SetText and a Reset
 * action, which the harness and TalkBack both use.
 */
@Composable
internal fun Knob(
    label: String,
    value: Double,
    min: Double,
    max: Double,
    display: String,
    onSet: (Double) -> Unit,
    onReset: () -> Unit,
    modifier: Modifier = Modifier,
    ids: List<String> = emptyList(),
) {
    var editing by remember { mutableStateOf(false) }
    var active by remember { mutableStateOf(false) }
    val current by rememberUpdatedState(value)
    val set by rememberUpdatedState(onSet)
    val norm = if (max > min) ((value - min) / (max - min)).toFloat() else 0f
    Column(
        modifier
            .width(84.dp)
            .ids(ids)
            .semantics(mergeDescendants = true) {
                contentDescription = label
                progressBarRangeInfo = ProgressBarRangeInfo(value.toFloat(), min.toFloat()..max.toFloat())
                setProgress { v -> set(v.toDouble().coerceIn(min, max)); true }
                setText { t -> parseNumber(t.text)?.let { set(it.coerceIn(min, max)); true } ?: false }
                customActions = listOf(CustomAccessibilityAction("Reset") { onReset(); true })
            }
            .pointerInput(min, max) { detectTapGestures(onDoubleTap = { editing = true }) }
            .pointerInput(min, max) {
                var start = 0f
                var travel = 0f
                var sideways = 0f
                detectDragGestures(
                    onDragStart = { active = true; start = if (max > min) ((current - min) / (max - min)).toFloat() else 0f; travel = 0f; sideways = 0f },
                    onDragEnd = { active = false },
                    onDragCancel = { active = false },
                ) { change, drag ->
                    change.consume()
                    sideways += drag.x
                    val fine = 1f + abs(sideways) / 60.dp.toPx()
                    travel += -drag.y / fine
                    val n = (start + travel / 160.dp.toPx()).coerceIn(0f, 1f)
                    set(min + n * (max - min))
                }
            },
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        T(label, Type.label(11.sp, if (active) Ink.selected else Ink.dim), align = TextAlign.Center)
        Canvas(Modifier.padding(vertical = 4.dp).size(52.dp)) {
            val stroke = 3.dp.toPx()
            val inset = stroke
            val arc = Size(size.width - inset * 2, size.height - inset * 2)
            drawArc(Ink.line, 135f, 270f, false, Offset(inset, inset), arc, style = Stroke(stroke, cap = StrokeCap.Round))
            drawArc(if (active) Ink.selected else Ink.text, 135f, 270f * norm, false, Offset(inset, inset), arc, style = Stroke(stroke, cap = StrokeCap.Round))
            val a = Math.toRadians(135.0 + 270.0 * norm)
            val c = Offset(size.width / 2, size.height / 2)
            val r = size.minDimension / 2 - inset * 3
            drawLine(Ink.text, c, c + Offset((r * Math.cos(a)).toFloat(), (r * Math.sin(a)).toFloat()), stroke * 0.8f, StrokeCap.Round)
        }
        if (editing) {
            ExactEntry(display, onDone = { t -> parseNumber(t)?.let { onSet(it.coerceIn(min, max)) }; editing = false })
        } else {
            T(display, Type.value(12.sp, if (active) Ink.selected else Ink.text), align = TextAlign.Center)
        }
    }
}

@Composable
private fun ExactEntry(initial: String, onDone: (String) -> Unit) {
    var text by remember { mutableStateOf(initial.takeWhile { it.isDigit() || it == '-' || it == '.' || it == '+' }) }
    val focus = remember { FocusRequester() }
    LaunchedEffect(Unit) { focus.requestFocus() }
    BasicTextField(
        text,
        { text = it },
        Modifier.width(72.dp).border(1.dp, Ink.selected, Corner).padding(horizontal = 4.dp, vertical = 2.dp).focusRequester(focus),
        textStyle = Type.value(12.sp).copy(textAlign = TextAlign.Center),
        singleLine = true,
        cursorBrush = SolidColor(Ink.selected),
        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal, imeAction = ImeAction.Done),
        keyboardActions = KeyboardActions(onDone = { onDone(text) }),
    )
}

internal fun parseNumber(s: String): Double? = s.trim().replace(',', '.').trimEnd { !it.isDigit() }.trimStart('+').toDoubleOrNull()

/** Horizontal slider for metronome and IR settings; same semantics contract as [Knob]. */
@Composable
internal fun Slider(
    label: String,
    value: Double,
    min: Double,
    max: Double,
    display: String,
    onSet: (Double) -> Unit,
    modifier: Modifier = Modifier,
    ids: List<String> = emptyList(),
) {
    val current by rememberUpdatedState(value)
    val set by rememberUpdatedState(onSet)
    var width by remember { mutableFloatStateOf(1f) }
    val norm = if (max > min) ((value - min) / (max - min)).toFloat() else 0f
    Column(
        modifier
            .height(56.dp)
            .ids(ids)
            .semantics(mergeDescendants = true) {
                contentDescription = label
                progressBarRangeInfo = ProgressBarRangeInfo(value.toFloat(), min.toFloat()..max.toFloat())
                setProgress { v -> set(v.toDouble().coerceIn(min, max)); true }
            }
            .layout { m, c -> val p = m.measure(c); width = p.width.toFloat().coerceAtLeast(1f); layout(p.width, p.height) { p.place(0, 0) } }
            .pointerInput(min, max) {
                detectTapGestures { o -> set(min + (o.x / width).coerceIn(0f, 1f) * (max - min)) }
            }
            .pointerInput(min, max) {
                var start = 0f
                var travel = 0f
                detectHorizontalDragGestures(onDragStart = { start = if (max > min) ((current - min) / (max - min)).toFloat() else 0f; travel = 0f }) { change, dx ->
                    change.consume()
                    travel += dx
                    set(min + (start + travel / width).coerceIn(0f, 1f) * (max - min))
                }
            },
        verticalArrangement = Arrangement.Center,
    ) {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
            T(label, Type.label(11.sp))
            T(display, Type.value(12.sp))
        }
        Canvas(Modifier.fillMaxWidth().height(20.dp)) {
            val y = size.height / 2
            drawLine(Ink.line, Offset(0f, y), Offset(size.width, y), 3.dp.toPx(), StrokeCap.Round)
            drawLine(Ink.text, Offset(0f, y), Offset(size.width * norm, y), 3.dp.toPx(), StrokeCap.Round)
            drawCircle(Ink.selected, 7.dp.toPx(), Offset(size.width * norm, y))
        }
    }
}

/** ‹ value › stepper; both arrows carry [ids] because either one changes the value. */
@Composable
internal fun Stepper(
    label: String?,
    text: String,
    onPrev: () -> Unit,
    onNext: () -> Unit,
    modifier: Modifier = Modifier,
    prevIds: List<String> = emptyList(),
    nextIds: List<String> = emptyList(),
    enabled: Boolean = true,
    valueWidth: Dp = 56.dp,
    valueSize: androidx.compose.ui.unit.TextUnit = 15.sp,
) {
    Row(modifier, verticalAlignment = Alignment.CenterVertically) {
        if (label != null) T(label, Type.label(11.sp), Modifier.padding(end = 6.dp))
        IconTap(Glyph.PREV, onPrev, ids = prevIds, enabled = enabled, desc = "${label ?: text} down")
        T(text, Type.value(valueSize, if (enabled) Ink.text else Ink.faint), Modifier.width(valueWidth), align = TextAlign.Center)
        IconTap(Glyph.NEXT, onNext, ids = nextIds, enabled = enabled, desc = "${label ?: text} up")
    }
}

/** Vertical level meter; reads [level] in the draw phase so telemetry never recomposes the surface. */
@Composable
internal fun Meter(label: String, level: () -> Float, modifier: Modifier = Modifier, alert: Boolean = false) {
    Column(modifier.width(18.dp), horizontalAlignment = Alignment.CenterHorizontally) {
        Canvas(Modifier.weight(1f).width(6.dp)) {
            drawRect(Ink.raised)
            val l = level().coerceIn(0f, 1f)
            drawRect(if (alert) Ink.alert else Ink.text, Offset(0f, size.height * (1 - l)), Size(size.width, size.height * l))
        }
        T(label, Type.label(fixedSp(8.dp), if (alert) Ink.alert else Ink.faint, 0.sp), Modifier.padding(top = 2.dp))
    }
}

/**
 * Scrolling catalog whose viewport is a whole number of [pitch] rows, so no row is
 * ever cut in half at the edge (a half row is a sub-48 dp target). Rows must be
 * multiples of [pitch] tall; [scrollToPx] lands the list on a row boundary.
 */
@Composable
internal fun RowList(
    pitch: Dp,
    modifier: Modifier = Modifier,
    state: ScrollState = rememberScrollState(),
    scrollToRow: Int = 0,
    content: @Composable ColumnScope.() -> Unit,
) {
    val px = with(LocalDensity.current) { pitch.roundToPx() }
    LaunchedEffect(scrollToRow, px) { state.scrollTo(scrollToRow * px) }
    Box(modifier) {
        Box(
            Modifier.layout { m, c ->
                val h = if (c.hasBoundedHeight) (c.maxHeight / px) * px else c.maxHeight
                val placeable = m.measure(c.copy(minHeight = 0, maxHeight = h))
                layout(placeable.width, placeable.height) { placeable.place(0, 0) }
            },
        ) {
            Column(Modifier.verticalScroll(state), content = content)
        }
    }
}

/** One catalog row, exactly [pitch] tall times [rows]. */
@Composable
internal fun ListRow(
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    ids: List<String> = emptyList(),
    selected: Boolean = false,
    pick: Pick = Pick.TEAL,
    pitch: Dp = 48.dp,
    content: @Composable () -> Unit,
) {
    Tap(onClick, modifier.fillMaxWidth().height(pitch), ids, selected, pick) {
        Box(Modifier.fillMaxWidth(), contentAlignment = Alignment.CenterStart) { content() }
    }
}

@Composable
internal fun SectionLabel(text: String, modifier: Modifier = Modifier, ids: List<String> = emptyList()) =
    T(text, Type.label(11.sp, Ink.faint), modifier.ids(ids).padding(vertical = 4.dp))

@Composable
internal fun Notice(text: String, modifier: Modifier = Modifier, alert: Boolean = false, ids: List<String> = emptyList()) {
    Box(modifier.ids(ids).background(Ink.raised, Corner).border(1.dp, if (alert) Ink.alert else Ink.line, Corner).padding(10.dp)) {
        T(text, Type.value(12.sp, if (alert) Ink.alert else Ink.text), maxLines = 3)
    }
}
