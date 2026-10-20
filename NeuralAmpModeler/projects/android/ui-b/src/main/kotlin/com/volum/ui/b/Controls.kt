package com.volum.ui.b

import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectHorizontalDragGestures
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
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.selection.toggleable
import androidx.compose.foundation.text.BasicText
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.SideEffect
import androidx.compose.runtime.Stable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateMapOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.draw.drawWithCache
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.hapticfeedback.HapticFeedbackType
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.LocalHapticFeedback
import androidx.compose.ui.semantics.ProgressBarRangeInfo
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.progressBarRangeInfo
import androidx.compose.ui.semantics.selected
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.setProgress
import androidx.compose.ui.semantics.stateDescription
import androidx.compose.ui.text.TextMeasurer
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.TextUnit
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.isSpecified
import androidx.compose.ui.unit.sp
import com.volum.core.Param
import com.volum.design.components.VoLumKnob
import com.volum.design.components.VoLumSelectionStyle
import com.volum.design.components.VoLumSwitchTrack
import com.volum.design.components.VoLumTextField
import com.volum.design.components.cornerBrackets
import com.volum.design.components.hairlineFrame
import com.volum.design.components.panelDepth
import com.volum.design.components.selectionInk
import com.volum.design.components.volumSelection
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumMotion
import com.volum.design.theme.VoLumType
import com.volum.design.theme.withOpacity
import com.volum.harness.featureId
import kotlin.math.abs
import kotlin.math.roundToInt

/** The three selection dialects of `:design`, by their desktop names. */
internal typealias Sel = VoLumSelectionStyle

internal fun Modifier.ids(ids: Array<out String>): Modifier = if (ids.isEmpty()) this else featureId(*ids)

/** Keeps touches from falling through an opaque layer to the surface underneath. */
internal fun Modifier.swallowTouches(): Modifier = pointerInput(Unit) {
    awaitPointerEventScope { while (true) awaitPointerEvent().changes.forEach { it.consume() } }
}

/**
 * Labels that sit side by side (a row of knobs, the header tools, a grid of tiles) and so
 * must read at one size: every [FitText] in the group reports the scale it needs to fit its
 * own box, and all of them draw at the smallest.
 */
@Stable
internal class FitGroup {
    private val scales = mutableStateMapOf<Any, Float>()
    val scale: Float get() = scales.values.minOrNull() ?: 1f
    fun report(key: Any, scale: Float) { if (scales[key] != scale) scales[key] = scale }
    fun drop(key: Any) { scales.remove(key) }
}

@Composable
internal fun rememberFitGroup(): FitGroup = remember { FitGroup() }

private fun TextStyle.scaled(k: Float): TextStyle = if (k >= 1f) this else copy(
    fontSize = if (fontSize.isSpecified) fontSize * k else (12f * k).sp,
    letterSpacing = if (letterSpacing.isSpecified) letterSpacing * k else letterSpacing,
)

/** The largest scale (at most 1) at which [text] fits [maxW] x [maxH] px on one line. */
private fun fitScale(m: TextMeasurer, text: String, style: TextStyle, maxW: Int, maxH: Int): Float {
    fun size(k: Float) = m.measure(text, style.scaled(k), maxLines = 1, softWrap = false).size
    val full = size(1f)
    if (full.width <= maxW && full.height <= maxH) return 1f
    var k = minOf(maxW.toFloat() / full.width.coerceAtLeast(1), maxH.toFloat() / full.height.coerceAtLeast(1), 1f)
    var tries = 0
    while (tries < 14 && size(k).let { it.width > maxW || it.height > maxH }) { k *= 0.96f; tries++ }
    return k
}

/**
 * One line that shrinks until it fits its box in width and height, so no label wraps,
 * clips or ellipsises at font scale 1.3. With a [group] it reads at the group's size.
 */
@Composable
internal fun FitText(text: String, style: TextStyle, modifier: Modifier = Modifier, align: TextAlign = TextAlign.Center, fitHeight: Boolean = true, group: FitGroup? = null) {
    val measurer = rememberTextMeasurer()
    BoxWithConstraints(modifier, contentAlignment = when (align) {
        TextAlign.Start, TextAlign.Left -> Alignment.CenterStart
        TextAlign.End, TextAlign.Right -> Alignment.CenterEnd
        else -> Alignment.Center
    }) {
        val maxW = if (constraints.hasBoundedWidth) constraints.maxWidth else Int.MAX_VALUE
        val maxH = if (fitHeight && constraints.hasBoundedHeight) constraints.maxHeight else Int.MAX_VALUE
        val own = remember(text, style, maxW, maxH) { fitScale(measurer, text, style, maxW, maxH) }
        val k = if (group == null) own else {
            val key = remember { Any() }
            DisposableEffect(group, key) { onDispose { group.drop(key) } }
            SideEffect { group.report(key, own) }
            minOf(own, group.scale)
        }
        BasicText(text, style = style.scaled(k).copy(textAlign = align), maxLines = 1, softWrap = false, overflow = TextOverflow.Clip)
    }
}

/** Height of one line of [style] at the current font scale. */
@Composable
internal fun rememberLineHeight(style: TextStyle): Dp {
    val measurer = rememberTextMeasurer()
    val density = LocalDensity.current
    return remember(style, density) { with(density) { measurer.measure(" ", style, maxLines = 1).size.height.toDp() } }
}

/** Helper copy the player has to read from the mic stand: Josefin SemiBold at 12 sp in the medium text ink; the thin cut read faint. */
internal fun hint(color: Color = VoLumColors.TextMed, size: TextUnit = 12.sp): TextStyle = VoLumType.label(size, color)

@Composable
internal fun Caption(text: String, modifier: Modifier = Modifier, color: Color = VoLumColors.CreamDim) {
    FitText(text, VoLumType.label(10.sp, color), modifier.height(18.dp), align = TextAlign.Start)
}

/** How a [Pad] reads at rest: a quiet brass-framed button, a destructive one, or a frameless surface. */
internal enum class Tone { FRAMED, DANGER, PRIMARY, BARE }

/**
 * The pressable surface every tap control of B is built on. Active and pressed looks
 * come from `:design`'s [volumSelection] in the dialect [style]; [content] gets the ink
 * that goes with the state. One `featureId` call on the clickable node.
 */
@Composable
internal fun Pad(
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    ids: Array<out String> = emptyArray(),
    selected: Boolean? = null,
    enabled: Boolean = true,
    role: Role = Role.Button,
    style: Sel = Sel.Brass,
    tone: Tone = Tone.FRAMED,
    description: String? = null,
    haptic: Boolean = false,
    content: @Composable BoxScope.(ink: Color) -> Unit,
) {
    val source = remember { MutableInteractionSource() }
    val pressed by source.collectIsPressedAsState()
    val scale by animateFloatAsState(if (pressed && haptic) 0.97f else 1f, tween(if (pressed) VoLumMotion.PressMs else VoLumMotion.PressMs * 2), label = "press")
    val haptics = LocalHapticFeedback.current
    val active = selected == true
    val ink = when {
        !enabled -> VoLumColors.CreamDim
        active -> selectionInk(style, true)
        tone == Tone.DANGER -> VoLumColors.Danger
        tone == Tone.PRIMARY -> VoLumColors.Gold
        pressed -> VoLumColors.TextBright
        else -> VoLumColors.BtnOffText
    }
    Box(
        modifier
            .defaultMinSize(48.dp, 48.dp)
            .graphicsLayer { scaleX = scale; scaleY = scale }
            .alpha(if (enabled) 1f else 0.45f)
            .clickable(source, null, enabled = enabled, role = role) {
                if (haptic) haptics.performHapticFeedback(HapticFeedbackType.LongPress)
                onClick()
            }
            .drawWithCache {
                val cr = CornerRadius(3.dp.toPx())
                val px = 1.dp.toPx()
                val stroke = Stroke(if (pressed) 1.35f * px else px)
                val border = when (tone) {
                    Tone.DANGER -> VoLumColors.Danger.withOpacity(if (pressed) 0.9f else 0.5f)
                    Tone.PRIMARY -> if (pressed) VoLumColors.Gold else VoLumColors.GoldDim.withOpacity(0.8f)
                    else -> if (pressed) VoLumColors.GoldDim else VoLumColors.BtnOffBorder
                }
                onDrawBehind {
                    if (active || tone == Tone.BARE) return@onDrawBehind
                    drawRoundRect(if (tone == Tone.DANGER) VoLumColors.DangerFill.withOpacity(0.12f) else VoLumColors.BtnOffBg, cornerRadius = cr)
                    drawRoundRect(border, Offset(px / 2, px / 2), Size(size.width - px, size.height - px), cr, style = stroke)
                }
            }
            .volumSelection(active, pressed && enabled, style, radius = 3.dp, inset = 0.dp)
            .semantics {
                if (selected != null) this.selected = selected
                if (description != null) contentDescription = description
            }
            .ids(ids),
        contentAlignment = Alignment.Center,
    ) { content(ink) }
}

@Composable
internal fun TextPad(
    label: String,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    ids: Array<out String> = emptyArray(),
    selected: Boolean? = null,
    enabled: Boolean = true,
    style: Sel = Sel.Brass,
    tone: Tone = Tone.FRAMED,
    size: TextUnit = 12.sp,
    role: Role = Role.Button,
    group: FitGroup? = null,
) {
    Pad(onClick, modifier, ids, selected, enabled, role, style, tone) { ink ->
        FitText(label, VoLumType.label(size, ink), Modifier.fillMaxSize().padding(horizontal = 10.dp, vertical = 4.dp), group = group)
    }
}

/** Mutually exclusive options in one of the shared selection dialects, every option label at one size. */
@Composable
internal fun Choice(
    options: List<String>,
    selected: Int,
    onSelect: (Int) -> Unit,
    modifier: Modifier = Modifier,
    ids: (Int) -> Array<out String> = { emptyArray() },
    enabled: (Int) -> Boolean = { true },
    style: Sel = Sel.AmberPicker,
    size: TextUnit = 12.sp,
) {
    val group = rememberFitGroup()
    Row(modifier, horizontalArrangement = Arrangement.spacedBy(4.dp)) {
        options.forEachIndexed { i, o ->
            TextPad(o, { onSelect(i) }, Modifier.weight(1f).fillMaxSize(), ids(i), i == selected, enabled(i), style, size = size, role = Role.RadioButton, group = group)
        }
    }
}

/** Each arrow glyph of a [Stepper] sits this far in from its edge; the rest of the width is the value's. */
internal val StepperGlyphLane = 26.dp

/** The width a [Stepper] [total] wide gives its value: all but the two glyph lanes, whatever the arrow targets measure. */
internal fun stepperValueWidth(total: Dp): Dp = (total - StepperGlyphLane * 2).coerceAtLeast(0.dp)

/**
 * `<  value  >` stepper with the desktop's gold Josefin arrows; each arrow is its own
 * [arrow]-wide target with its own ids. The glyphs sit at the outer edges and the value
 * is drawn over the inner part of both targets, so a narrow stepper (the amp CHANNEL)
 * still reads "< 3 >" with 48 dp targets.
 */
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
    valueStyle: TextStyle = VoLumType.Stepper,
    description: String = "",
    arrow: Dp = 48.dp,
) {
    BoxWithConstraints(modifier) {
        Row(Modifier.fillMaxSize()) {
            Pad(onPrev, Modifier.width(arrow).fillMaxHeight(), prevIds, enabled = prevEnabled, tone = Tone.BARE, description = "previous $description") { ink ->
                BasicText("<", Modifier.align(Alignment.CenterStart).padding(start = 10.dp), VoLumType.label(17.sp, if (ink == VoLumColors.CreamDim) ink else VoLumColors.Gold))
            }
            Box(Modifier.weight(1f))
            Pad(onNext, Modifier.width(arrow).fillMaxHeight(), nextIds, enabled = nextEnabled, tone = Tone.BARE, description = "next $description") { ink ->
                BasicText(">", Modifier.align(Alignment.CenterEnd).padding(end = 10.dp), VoLumType.label(17.sp, if (ink == VoLumColors.CreamDim) ink else VoLumColors.Gold))
            }
        }
        FitText(value, valueStyle, Modifier.align(Alignment.Center).width(stepperValueWidth(maxWidth)).fillMaxHeight())
    }
}

/** On / off row: the `:design` slide track and a label that fits; the whole row is one toggle target. */
@Composable
internal fun Switch(label: String, on: Boolean, onClick: () -> Unit, modifier: Modifier = Modifier, ids: Array<out String> = emptyArray(), enabled: Boolean = true, group: FitGroup? = null) {
    val source = remember { MutableInteractionSource() }
    val pressed by source.collectIsPressedAsState()
    Row(
        modifier
            .defaultMinSize(minHeight = 48.dp)
            .alpha(if (enabled) 1f else 0.4f)
            .toggleable(on, source, indication = null, enabled = enabled, role = Role.Switch) { onClick() }
            .ids(ids)
            .padding(horizontal = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(7.dp),
    ) {
        VoLumSwitchTrack(on, pressed = pressed)
        FitText(label, VoLumType.label(12.sp, if (on) VoLumColors.TextBright else VoLumColors.Cream), Modifier.weight(1f), align = TextAlign.Start, group = group)
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

internal fun parseNumber(text: String): Double? = Regex("-?\\d+(?:[.,]\\d+)?").find(text)?.value?.replace(',', '.')?.toDoubleOrNull()

/**
 * A rig parameter on the shared `:design` dial: drag anywhere on the cell, hold
 * still for fine adjust, double tap resets; SetProgress / SetText / Reset for the
 * harness and accessibility land on the same node as [ids]. The caption is drawn
 * over the dial's own (empty) label line as fit text, at the size of its [group]
 * (one size per knob row, never "OUTP…"); the dial takes what height is left.
 */
@Composable
internal fun Knob(
    p: Param,
    value: Double,
    onSet: (Double) -> Unit,
    modifier: Modifier = Modifier,
    ids: Array<out String> = emptyArray(),
    label: String = p.label,
    support: Boolean = false,
    group: FitGroup? = null,
) {
    val line = rememberLineHeight(VoLumType.KnobLabel)
    BoxWithConstraints(modifier) {
        val dial = if (constraints.hasBoundedHeight) (maxHeight - line * 2 - 9.dp).coerceIn(30.dp, 46.dp) else 46.dp
        VoLumKnob(
            value = value,
            onValueChange = onSet,
            range = p.min..p.max,
            default = p.default,
            label = "",
            valueText = formatValue(value, p.min, p.max, p.unit),
            modifier = Modifier.fillMaxSize().semantics { contentDescription = label }.ids(ids),
            support = support,
            dialSize = dial,
            parseEntry = { t -> parseNumber(t) },
        )
        FitText(label, VoLumType.KnobLabel, Modifier.align(Alignment.TopCenter).fillMaxWidth().height(line).padding(horizontal = 2.dp), group = group)
    }
}

/** Horizontal slider in the desktop's metronome-volume look: drag or tap along the track. */
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
            .semantics(mergeDescendants = true) {
                contentDescription = label
                stateDescription = text
                progressBarRangeInfo = ProgressBarRangeInfo(value.toFloat(), min.toFloat()..max.toFloat())
                setProgress { t -> set(t.toDouble().coerceIn(min, max)); true }
            }
            .ids(ids)
            .padding(horizontal = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        FitText(label, VoLumType.label(12.sp, VoLumColors.TextBright), Modifier.width(84.dp), align = TextAlign.Start)
        Box(
            Modifier.weight(1f).fillMaxHeight()
                .pointerInput(min, max) {
                    detectTapGestures { pos -> set(min + (pos.x / size.width).coerceIn(0f, 1f) * (max - min)) }
                }
                .pointerInput(min, max) {
                    detectHorizontalDragGestures(
                        onDragStart = { dragging = true }, onDragEnd = { dragging = false }, onDragCancel = { dragging = false },
                    ) { change, _ ->
                        change.consume()
                        set(min + (change.position.x / size.width).coerceIn(0f, 1f) * (max - min))
                    }
                }
                .drawBehind {
                    val y = size.height / 2
                    val r = (if (dragging) 9 else 7).dp.toPx()
                    drawLine(VoLumColors.Frame, Offset(0f, y), Offset(size.width, y), 4.dp.toPx(), StrokeCap.Round)
                    drawLine(VoLumColors.KnobGold, Offset(0f, y), Offset(size.width * norm, y), 4.dp.toPx(), StrokeCap.Round)
                    drawCircle(if (dragging) VoLumColors.Gold else VoLumColors.Cream, r, Offset(size.width * norm, y))
                },
        )
        FitText(text, VoLumType.label(12.sp, VoLumColors.TextBright), Modifier.width(80.dp), align = TextAlign.End)
    }
}

/** Single-line entry on the `:design` name field; [number] opens the numeric keyboard. */
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
    VoLumTextField(
        value, onChange, modifier.ids(ids), placeholder,
        KeyboardOptions(keyboardType = if (number) KeyboardType.Decimal else KeyboardType.Text, imeAction = ImeAction.Done),
        KeyboardActions(onDone = { onDone() }),
    )
}

@Composable
internal fun RowScope.Gap(w: Dp = 8.dp) = Box(Modifier.width(w))

/** A framed group inside a panel: panel depth, brass hairline, 8 dp card brackets, a section caption. */
@Composable
internal fun Card(modifier: Modifier = Modifier, caption: String? = null, accent: Color = VoLumColors.Corner, content: @Composable ColumnScope.() -> Unit) {
    Column(
        modifier.panelDepth(4.dp).hairlineFrame(VoLumColors.Frame, radius = 4.dp).cornerBrackets(accent.withOpacity(0.8f), 8.dp, 4.dp).padding(12.dp),
        verticalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        if (caption != null) FitText(caption, VoLumType.SectionHeader, Modifier.fillMaxWidth().height(16.dp), align = TextAlign.Start)
        content()
    }
}

/** A status line that is not a control: a lamp and the words, no pad behind it. */
@Composable
internal fun Status(text: String, modifier: Modifier = Modifier, color: Color = VoLumColors.Amber) {
    Row(modifier, verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        com.volum.design.components.VoLumLed(true, color = color)
        FitText(text, VoLumType.label(12.sp, color), Modifier.weight(1f), align = TextAlign.Start)
    }
}
