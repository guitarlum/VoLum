package com.volum.ui.b

import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.snap
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsPressedAsState
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxScope
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.defaultMinSize
import androidx.compose.foundation.layout.fillMaxSize
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
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.hapticfeedback.HapticFeedbackType
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalHapticFeedback
import androidx.compose.ui.semantics.CustomAccessibilityAction
import androidx.compose.ui.semantics.ProgressBarRangeInfo
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.customActions
import androidx.compose.ui.semantics.progressBarRangeInfo
import androidx.compose.ui.semantics.selected
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.setProgress
import androidx.compose.ui.semantics.setText
import androidx.compose.ui.semantics.stateDescription
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.isSpecified
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Popup
import androidx.compose.ui.window.PopupProperties
import com.volum.harness.featureId
import kotlin.math.abs
import kotlin.math.roundToInt

internal fun Modifier.ids(ids: Array<out String>): Modifier = if (ids.isEmpty()) this else featureId(*ids)

/** Keeps touches from falling through an opaque layer to the surface underneath. */
internal fun Modifier.swallowTouches(): Modifier = pointerInput(Unit) {
    awaitPointerEventScope { while (true) awaitPointerEvent().changes.forEach { it.consume() } }
}

/** One line of text that shrinks until it fits its width, so labels never wrap or clip at large font scales. */
@Composable
internal fun FitText(text: String, style: TextStyle, modifier: Modifier = Modifier, align: TextAlign = TextAlign.Center, fitHeight: Boolean = false) {
    val measurer = rememberTextMeasurer()
    BoxWithConstraints(modifier, contentAlignment = when (align) {
        TextAlign.Start, TextAlign.Left -> Alignment.CenterStart
        TextAlign.End, TextAlign.Right -> Alignment.CenterEnd
        else -> Alignment.Center
    }) {
        val maxW = if (constraints.hasBoundedWidth) constraints.maxWidth else Int.MAX_VALUE
        val maxH = if (fitHeight && constraints.hasBoundedHeight) constraints.maxHeight else Int.MAX_VALUE
        val fitted = remember(text, style, maxW, maxH) {
            var s = style
            var tries = 0
            while (tries < 14 && measurer.measure(text, s, maxLines = 1, softWrap = false).size.let { it.width > maxW || it.height > maxH }) {
                s = s.copy(
                    fontSize = if (s.fontSize.isSpecified) s.fontSize * 0.9f else 12.sp,
                    letterSpacing = if (s.letterSpacing.isSpecified) s.letterSpacing * 0.8f else s.letterSpacing,
                )
                tries++
            }
            s
        }
        BasicText(text, style = fitted.copy(textAlign = align), maxLines = 1, softWrap = false, overflow = TextOverflow.Clip)
    }
}

/** The pressable surface every tap control is built on: press scale, selection ink, haptic, featureId on the clickable node. */
@Composable
internal fun Pad(
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    ids: Array<out String> = emptyArray(),
    selected: Boolean? = null,
    enabled: Boolean = true,
    role: Role = Role.Button,
    sel: Sel = Sel.BRASS,
    fill: Color = Ink.raised,
    description: String? = null,
    haptic: Boolean = false,
    content: @Composable BoxScope.() -> Unit,
) {
    val source = remember { MutableInteractionSource() }
    val pressed by source.collectIsPressedAsState()
    val scale by animateFloatAsState(if (pressed) 0.95f else 1f, tween(if (pressed) 60 else 140), label = "press")
    val haptics = LocalHapticFeedback.current
    Box(
        modifier
            .defaultMinSize(48.dp, 48.dp)
            .graphicsLayer { scaleX = scale; scaleY = scale }
            .clip(RoundedCornerShape(6.dp))
            .background(if (pressed) Ink.pressed else fill)
            .selectionInk(selected == true, sel)
            .alpha(if (enabled) 1f else 0.38f)
            .clickable(source, null, enabled = enabled, role = role) {
                if (haptic) haptics.performHapticFeedback(HapticFeedbackType.LongPress)
                onClick()
            }
            .semantics {
                if (selected != null) this.selected = selected
                if (description != null) contentDescription = description
            }
            .ids(ids),
        contentAlignment = Alignment.Center,
        content = content,
    )
}

@Composable
internal fun TextPad(
    label: String,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    ids: Array<out String> = emptyArray(),
    selected: Boolean? = null,
    enabled: Boolean = true,
    sel: Sel = Sel.BRASS,
    style: TextStyle = Type.label(13.sp, Ink.text),
    role: Role = Role.Button,
    fill: Color = Ink.raised,
) {
    Pad(onClick, modifier, ids, selected, enabled, role, sel, fill) {
        FitText(label, style, Modifier.fillMaxWidth().padding(horizontal = 8.dp))
    }
}

@Composable
internal fun Caption(text: String, modifier: Modifier = Modifier, color: Color = Ink.dim) {
    FitText(text, Type.label(11.sp, color), modifier.height(18.dp), align = TextAlign.Start)
}

/** Mutually exclusive options drawn with one of the shared selection styles. */
@Composable
internal fun Choice(
    options: List<String>,
    selected: Int,
    onSelect: (Int) -> Unit,
    modifier: Modifier = Modifier,
    ids: (Int) -> Array<out String> = { emptyArray() },
    enabled: (Int) -> Boolean = { true },
    sel: Sel = Sel.AMBER_PICKER,
    weights: List<Float>? = null,
) {
    Row(modifier, horizontalArrangement = Arrangement.spacedBy(4.dp)) {
        options.forEachIndexed { i, o ->
            TextPad(o, { onSelect(i) }, Modifier.weight(weights?.get(i) ?: 1f).fillMaxSize(), ids(i), i == selected, enabled(i), sel, role = Role.Tab)
        }
    }
}

/** ‹ value › stepper; both arrows carry their own ids. */
@Composable
internal fun Stepper(
    value: String,
    onPrev: () -> Unit,
    onNext: () -> Unit,
    modifier: Modifier = Modifier,
    prevIds: Array<out String> = emptyArray(),
    nextIds: Array<out String> = emptyArray(),
    prevEnabled: Boolean = true,
    nextEnabled: Boolean = true,
    valueStyle: TextStyle = Type.tech(16.sp),
    description: String = "",
) {
    Row(modifier, verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(4.dp)) {
        Pad(onPrev, Modifier.width(48.dp).fillMaxSize(), prevIds, enabled = prevEnabled, description = "previous $description") {
            BasicText("‹", style = Type.display(26.sp))
        }
        FitText(value, valueStyle, Modifier.weight(1f))
        Pad(onNext, Modifier.width(48.dp).fillMaxSize(), nextIds, enabled = nextEnabled, description = "next $description") {
            BasicText("›", style = Type.display(26.sp))
        }
    }
}

/** On / off switch with an LED. */
@Composable
internal fun Switch(label: String, on: Boolean, onClick: () -> Unit, modifier: Modifier = Modifier, ids: Array<out String> = emptyArray(), enabled: Boolean = true) {
    Pad(onClick, modifier, ids, selected = on, enabled = enabled, sel = Sel.BRASS, role = Role.Switch, haptic = true) {
        Row(Modifier.fillMaxSize().padding(horizontal = 10.dp), verticalAlignment = Alignment.CenterVertically) {
            Canvas(Modifier.size(12.dp)) { led(on, Offset(size.width / 2, size.height / 2), size.minDimension / 3) }
            FitText(label, Type.label(12.sp, if (on) Ink.text else Ink.dim), Modifier.weight(1f).padding(start = 8.dp), align = TextAlign.Start)
        }
    }
}

internal fun formatValue(v: Double, min: Double, max: Double, unit: String): String = when {
    unit == "dB" -> String.format("%.1f dB", v)
    unit == "Hz" -> if (v >= 1000) String.format("%.2f kHz", v / 1000) else if (max <= 20.0) String.format("%.1f Hz", v) else "${v.roundToInt()} Hz"
    unit == "ms" -> if (max <= 30.0) String.format("%.1f ms", v) else "${v.roundToInt()} ms"
    unit == "s" -> String.format("%.1f s", v)
    unit == "st" -> (if (v > 0) "+" else "") + "${v.roundToInt()} st"
    unit == "dBu" -> String.format("%.1f dBu", v)
    min == -1.0 && max == 1.0 -> when {
        abs(v) < 0.01 -> "C"
        v < 0 -> "L${(abs(v) * 100).roundToInt()}"
        else -> "R${(v * 100).roundToInt()}"
    }
    min == 0.0 && max <= 1.0 -> "${(v / max * 100).roundToInt()}%"
    else -> String.format("%.1f", v)
}

private fun parseNumber(text: String): Double? = Regex("-?\\d+(?:[.,]\\d+)?").find(text)?.value?.replace(',', '.')?.toDoubleOrNull()

/**
 * Fingertip knob: vertical drag over the whole cell (slide sideways for fine adjust),
 * double tap resets, long press opens exact entry. Exposes SetProgress, SetText and a
 * Reset action for the harness and accessibility.
 */
@Composable
internal fun Knob(
    label: String,
    value: Double,
    min: Double,
    max: Double,
    default: Double,
    unit: String,
    onSet: (Double) -> Unit,
    onReset: () -> Unit,
    modifier: Modifier = Modifier,
    ids: Array<out String> = emptyArray(),
    dial: Dp = 60.dp,
) {
    val norm = if (max > min) ((value - min) / (max - min)).toFloat().coerceIn(0f, 1f) else 0f
    var dragging by remember { mutableStateOf(false) }
    var exact by remember { mutableStateOf(false) }
    val shown by animateFloatAsState(norm, if (dragging) snap() else tween(140), label = "knob")
    val set by rememberUpdatedState(onSet)
    val reset by rememberUpdatedState(onReset)
    val current by rememberUpdatedState(norm)
    var accum by remember { mutableFloatStateOf(0f) }
    var sideways by remember { mutableFloatStateOf(0f) }
    var start by remember { mutableFloatStateOf(0f) }
    val text = formatValue(value, min, max, unit)
    Column(
        modifier
            .defaultMinSize(64.dp, 64.dp)
            .clip(RoundedCornerShape(8.dp))
            .background(if (dragging) Ink.raised else Color.Transparent)
            .pointerInput(min, max) {
                detectTapGestures(onDoubleTap = { reset() }, onLongPress = { exact = true })
            }
            .pointerInput(min, max) {
                detectDragGestures(
                    onDragStart = { dragging = true; accum = 0f; sideways = 0f; start = current },
                    onDragEnd = { dragging = false },
                    onDragCancel = { dragging = false },
                ) { change, drag ->
                    change.consume()
                    sideways += drag.x
                    val fine = if (abs(sideways) > 56.dp.toPx()) 0.15f else 1f
                    accum += -drag.y / 200.dp.toPx() * fine
                    val n = (start + accum).coerceIn(0f, 1f)
                    set(min + n * (max - min))
                }
            }
            .semantics(mergeDescendants = true) {
                contentDescription = label
                stateDescription = text
                progressBarRangeInfo = ProgressBarRangeInfo(value.toFloat(), min.toFloat()..max.toFloat())
                setProgress { t -> set(t.toDouble().coerceIn(min, max)); true }
                setText { t -> parseNumber(t.text)?.let { set(it.coerceIn(min, max)); true } ?: false }
                customActions = listOf(CustomAccessibilityAction("Reset") { reset(); true })
            }
            .ids(ids)
            .padding(vertical = 4.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        FitText(label, Type.label(11.sp, if (dragging) Ink.text else Ink.dim), Modifier.fillMaxWidth().height(18.dp).padding(horizontal = 2.dp))
        Canvas(Modifier.size(dial)) { knobFace(shown, dragging || value != default) }
        FitText(text, Type.value(13.sp, if (dragging) Ink.hot else Ink.text), Modifier.fillMaxWidth().height(20.dp).padding(horizontal = 2.dp))
    }
    if (exact) ExactEntry(label, text, unit, onDone = { v -> v?.let { set(it.coerceIn(min, max)) }; exact = false })
}

@Composable
private fun ExactEntry(label: String, current: String, unit: String, onDone: (Double?) -> Unit) {
    var text by remember { mutableStateOf(parseNumber(current)?.let { if (it == it.roundToInt().toDouble()) it.roundToInt().toString() else it.toString() } ?: "") }
    Popup(alignment = Alignment.Center, onDismissRequest = { onDone(null) }, properties = PopupProperties(focusable = true)) {
        Column(Modifier.width(280.dp).clip(RoundedCornerShape(10.dp)).background(Ink.panel).brackets().padding(12.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Caption("$label ${if (unit.isNotEmpty()) "($unit)" else ""}")
            Field(text, { text = it }, Modifier.fillMaxWidth().height(56.dp), number = true, onDone = { onDone(parseNumber(text)) })
            Row(Modifier.fillMaxWidth().height(48.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                TextPad("CANCEL", { onDone(null) }, Modifier.weight(1f).fillMaxSize())
                TextPad("SET", { onDone(parseNumber(text)) }, Modifier.weight(1f).fillMaxSize(), selected = true)
            }
        }
    }
}

/** Horizontal fader: drag or tap along the track. */
@Composable
internal fun Fader(
    label: String,
    value: Double,
    min: Double,
    max: Double,
    format: (Double) -> String,
    onSet: (Double) -> Unit,
    modifier: Modifier = Modifier,
    ids: Array<out String> = emptyArray(),
) {
    val norm = if (max > min) ((value - min) / (max - min)).toFloat().coerceIn(0f, 1f) else 0f
    val set by rememberUpdatedState(onSet)
    var dragging by remember { mutableStateOf(false) }
    val text = format(value)
    Row(
        modifier
            .defaultMinSize(minHeight = 56.dp)
            .clip(RoundedCornerShape(8.dp))
            .background(Ink.raised)
            .semantics(mergeDescendants = true) {
                contentDescription = label
                stateDescription = text
                progressBarRangeInfo = ProgressBarRangeInfo(value.toFloat(), min.toFloat()..max.toFloat())
                setProgress { t -> set(t.toDouble().coerceIn(min, max)); true }
            }
            .ids(ids)
            .padding(horizontal = 12.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        FitText(label, Type.label(12.sp), Modifier.width(96.dp), align = TextAlign.Start)
        Canvas(
            Modifier.weight(1f).fillMaxSize()
                .pointerInput(min, max) {
                    detectTapGestures { p -> set(min + (p.x / size.width).coerceIn(0f, 1f) * (max - min)) }
                }
                .pointerInput(min, max) {
                    detectDragGestures(onDragStart = { dragging = true }, onDragEnd = { dragging = false }, onDragCancel = { dragging = false }) { change, _ ->
                        change.consume()
                        set(min + (change.position.x / size.width).coerceIn(0f, 1f) * (max - min))
                    }
                },
        ) {
            val y = size.height / 2
            drawLine(Ink.line, Offset(0f, y), Offset(size.width, y), 4.dp.toPx())
            drawLine(Ink.text, Offset(0f, y), Offset(size.width * norm, y), 4.dp.toPx())
            drawCircle(if (dragging) Ink.hot else Ink.text, if (dragging) 13.dp.toPx() else 10.dp.toPx(), Offset(size.width * norm, y))
        }
        FitText(text, Type.value(14.sp), Modifier.width(88.dp), align = TextAlign.End)
    }
}

/** IN / OUT ladder; reads telemetry in the draw phase only. */
@Composable
internal fun Meter(input: Boolean, modifier: Modifier = Modifier) {
    val tele = LocalStage.current.tele
    Canvas(modifier) {
        val t = tele.value
        val level = t.meter(if (input) t.inDb else t.outDb).toFloat()
        drawRect(Ink.panel)
        val segments = 14
        val gap = 2.dp.toPx()
        val h = (size.height - gap * (segments - 1)) / segments
        for (i in 0 until segments) {
            val lit = (i + 1f) / segments <= level + 0.001f
            val top = size.height - (i + 1) * h - i * gap
            val c = when {
                !lit -> Ink.raised
                i >= segments - 2 -> Ink.alert
                else -> Ink.dim
            }
            drawRect(c, Offset(0f, top), Size(size.width, h))
        }
    }
}

/** Single-line text entry; [number] opens the numeric keyboard. */
@Composable
internal fun Field(
    value: String,
    onChange: (String) -> Unit,
    modifier: Modifier = Modifier,
    ids: Array<out String> = emptyArray(),
    number: Boolean = false,
    placeholder: String = "",
    onDone: () -> Unit = {},
) {
    BasicTextField(
        value = value,
        onValueChange = onChange,
        modifier = modifier.defaultMinSize(minHeight = 48.dp).clip(RoundedCornerShape(6.dp)).background(Ink.raised).ids(ids),
        singleLine = true,
        textStyle = Type.value(16.sp),
        cursorBrush = SolidColor(Ink.hot),
        keyboardOptions = KeyboardOptions(keyboardType = if (number) KeyboardType.Decimal else KeyboardType.Text, imeAction = ImeAction.Done),
        keyboardActions = KeyboardActions(onDone = { onDone() }),
        decorationBox = { inner ->
            Box(Modifier.fillMaxSize().padding(horizontal = 12.dp), contentAlignment = Alignment.CenterStart) {
                if (value.isEmpty()) BasicText(placeholder, style = Type.value(16.sp, Ink.faint))
                inner()
            }
        },
    )
}

@Composable
internal fun RowScope.Gap(w: Dp = 8.dp) = Box(Modifier.width(w))

@Composable
internal fun Card(modifier: Modifier = Modifier, caption: String? = null, content: @Composable ColumnScope.() -> Unit) {
    Column(modifier.clip(RoundedCornerShape(8.dp)).background(Ink.panel).padding(10.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
        if (caption != null) Caption(caption, Modifier.fillMaxWidth())
        content()
    }
}
