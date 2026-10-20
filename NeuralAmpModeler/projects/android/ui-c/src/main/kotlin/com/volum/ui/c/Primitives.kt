package com.volum.ui.c

import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsPressedAsState
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxScope
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.selection.selectable
import androidx.compose.foundation.selection.toggleable
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicText
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Shape
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import com.volum.core.Action
import com.volum.core.Telemetry
import com.volum.core.VoLumState
import com.volum.harness.featureId
import kotlinx.coroutines.flow.StateFlow

/** What every surface renders from and dispatches to. */
internal class Ui(val s: VoLumState, val d: (Action) -> Unit, val tel: StateFlow<Telemetry>)

internal fun Modifier.ids(ids: List<String>): Modifier = if (ids.isEmpty()) this else featureId(*ids.toTypedArray())
internal fun Modifier.ids(vararg ids: String): Modifier = if (ids.isEmpty()) this else featureId(*ids)

internal val Corner = RoundedCornerShape(10.dp)
internal val Pill = RoundedCornerShape(24.dp)

/** Press feedback shared by every tappable surface: a slight sink and a lift of the fill. */
@Composable
internal fun pressScale(source: MutableInteractionSource): Float {
    val pressed by source.collectIsPressedAsState()
    val s by animateFloatAsState(if (pressed) 0.95f else 1f, tween(if (pressed) 60 else 160), label = "press")
    return s
}

/** A button: [label] plus an optional second line. */
@Composable
internal fun Btn(
    label: String,
    ids: List<String>,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    sub: String? = null,
    enabled: Boolean = true,
    strong: Boolean = false,
    danger: Boolean = false,
    minWidth: Dp = 48.dp,
    height: Dp = Metrics.target,
    pad: Dp = 12.dp,
    cap: Float = 1.15f,
) {
    val src = remember { MutableInteractionSource() }
    val scale = pressScale(src)
    val pressed by src.collectIsPressedAsState()
    val fill = when {
        !enabled -> Look.ground
        strong -> if (pressed) Look.glow else Look.pick
        pressed -> Look.lineHi
        else -> Look.raised
    }
    val ink = when {
        !enabled -> Look.faint
        strong -> Look.pickInk
        danger -> Look.danger
        else -> Look.text
    }
    Box(
        modifier
            .heightIn(min = height)
            .widthIn(min = minWidth)
            .graphicsLayer { scaleX = scale; scaleY = scale }
            .clip(Corner)
            .background(fill)
            .border(1.dp, if (enabled) Look.line else Look.raised, Corner)
            .clickable(src, null, enabled = enabled, role = Role.Button, onClick = onClick)
            .ids(ids)
            .padding(horizontal = pad),
        contentAlignment = Alignment.Center,
    ) {
        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            BasicText(label, style = labelStyle(12f, ink, cap = cap), maxLines = 1, overflow = TextOverflow.Ellipsis)
            if (sub != null) BasicText(sub, style = labelStyle(9f, if (strong) Look.pickInk else Look.dim, bold = false, cap = cap), maxLines = 1, overflow = TextOverflow.Ellipsis)
        }
    }
}

/** A 48 dp button drawn as an icon, for chrome whose desktop counterpart is an icon too. */
@Composable
internal fun IconBtn(description: String, ids: List<String>, onClick: () -> Unit, active: Boolean = false, draw: androidx.compose.ui.graphics.drawscope.DrawScope.(Color) -> Unit) {
    val src = remember { MutableInteractionSource() }
    val scale = pressScale(src)
    Box(
        Modifier
            .size(Metrics.target)
            .graphicsLayer { scaleX = scale; scaleY = scale }
            .clip(Corner)
            .background(if (active) Look.pick else Look.raised)
            .border(1.dp, Look.line, Corner)
            .clickable(src, null, role = Role.Button, onClick = onClick)
            .semantics { contentDescription = description }
            .ids(ids)
            .drawBehind { draw(if (active) Look.pickInk else Look.text) },
    )
}

internal fun androidx.compose.ui.graphics.drawscope.DrawScope.gear(ink: Color) {
    val c = center
    val r = 7.dp.toPx()
    val w = 2.dp.toPx()
    drawCircle(ink, r, c, style = androidx.compose.ui.graphics.drawscope.Stroke(w))
    drawCircle(ink, 2.5.dp.toPx(), c)
    for (i in 0 until 8) {
        val a = Math.toRadians(i * 45.0)
        val dx = kotlin.math.cos(a).toFloat()
        val dy = kotlin.math.sin(a).toFloat()
        drawLine(ink, c + Offset(dx * r, dy * r), c + Offset(dx * (r + 3.5.dp.toPx()), dy * (r + 3.5.dp.toPx())), 3.dp.toPx())
    }
}

/** One option of a mutually exclusive group (`:design` VoLumSelection, AmberPicker style). */
@Composable
internal fun Choice(
    label: String,
    selected: Boolean,
    ids: List<String>,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    enabled: Boolean = true,
    sub: String? = null,
    minWidth: Dp = 48.dp,
) {
    val src = remember { MutableInteractionSource() }
    val scale = pressScale(src)
    val fill = when {
        selected -> Look.pick
        !enabled -> Look.ground
        else -> Look.raised
    }
    val ink = when {
        selected -> Look.pickInk
        !enabled -> Look.faint
        else -> Look.text
    }
    Box(
        modifier
            .height(Metrics.target)
            .widthIn(min = minWidth)
            .graphicsLayer { scaleX = scale; scaleY = scale }
            .clip(Corner)
            .background(fill)
            .border(1.dp, if (selected) Look.pick else Look.line, Corner)
            .selectable(selected, src, null, enabled = enabled, role = Role.RadioButton, onClick = onClick)
            .ids(ids)
            .padding(horizontal = 10.dp),
        contentAlignment = Alignment.Center,
    ) {
        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            BasicText(label, style = labelStyle(12f, ink), maxLines = 1, overflow = TextOverflow.Ellipsis)
            if (sub != null) BasicText(sub, style = labelStyle(9f, if (selected) Look.pickInk else Look.dim, bold = false), maxLines = 1, overflow = TextOverflow.Ellipsis)
        }
    }
}

/** An on/off switch with an LED, labelled. */
@Composable
internal fun Switch(
    label: String,
    on: Boolean,
    ids: List<String>,
    onToggle: () -> Unit,
    modifier: Modifier = Modifier,
    enabled: Boolean = true,
) {
    val src = remember { MutableInteractionSource() }
    val scale = pressScale(src)
    Row(
        modifier
            .height(Metrics.target)
            .widthIn(min = 48.dp)
            .graphicsLayer { scaleX = scale; scaleY = scale }
            .clip(Corner)
            .background(if (on) Look.glassHi else Look.raised)
            .border(1.dp, if (on) Look.lineHi else Look.line, Corner)
            .toggleable(on, src, null, enabled = enabled, role = Role.Switch) { onToggle() }
            .ids(ids)
            .padding(horizontal = 12.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        Led(on)
        BasicText(label, style = labelStyle(12f, if (enabled) Look.text else Look.faint), maxLines = 1)
    }
}

/** A 48 dp power button: the LED is the state. */
@Composable
internal fun Power(on: Boolean, ids: List<String>, onToggle: () -> Unit, description: String, modifier: Modifier = Modifier) {
    val src = remember { MutableInteractionSource() }
    val scale = pressScale(src)
    Box(
        modifier
            .size(Metrics.target)
            .graphicsLayer { scaleX = scale; scaleY = scale }
            .clip(CircleShape)
            .background(if (on) Look.glassHi else Look.raised)
            .border(1.dp, if (on) Look.glow else Look.line, CircleShape)
            .toggleable(on, src, null, role = Role.Switch) { onToggle() }
            .semantics { contentDescription = description }
            .ids(ids),
        contentAlignment = Alignment.Center,
    ) { Led(on, 12.dp) }
}

@Composable
internal fun Led(on: Boolean, size: Dp = 9.dp, color: Color = Look.glow) {
    Box(
        Modifier.size(size).drawBehind {
            if (on) drawCircle(color.copy(alpha = 0.35f), radius = this.size.minDimension)
            drawCircle(if (on) color else Look.faint, radius = this.size.minDimension / 2)
        },
    )
}

/** − value + for small integer settings; both buttons carry [ids]. */
@Composable
internal fun Stepper(value: String, ids: List<String>, onStep: (Int) -> Unit, modifier: Modifier = Modifier, label: String? = null, valueWidth: Dp = 64.dp) {
    Row(modifier, verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(4.dp)) {
        if (label != null) BasicText(label, style = labelStyle(10f), modifier = Modifier.padding(end = 4.dp))
        Btn("−", ids, { onStep(-1) })
        BasicText(value, style = techStyle(14f).copy(textAlign = TextAlign.Center), modifier = Modifier.width(valueWidth), maxLines = 1)
        Btn("+", ids, { onStep(1) })
    }
}

/** Small caps heading over a group. */
@Composable
internal fun Caption(text: String, modifier: Modifier = Modifier, color: Color = Look.dim) {
    BasicText(text.uppercase(), style = labelStyle(10f, color), modifier = modifier, maxLines = 1, overflow = TextOverflow.Ellipsis)
}

@Composable
internal fun Note(text: String, modifier: Modifier = Modifier, color: Color = Look.dim, maxLines: Int = 2) {
    BasicText(text, style = bodyStyle(12f, color, cap = 1.15f), modifier = modifier, maxLines = maxLines, overflow = TextOverflow.Ellipsis)
}

/** A captioned group of controls in one row; the caption sits to the left so a group costs one row of height. */
@Composable
internal fun Group(title: String, modifier: Modifier = Modifier, content: @Composable RowScope.() -> Unit) {
    Row(modifier, horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
        Caption(title, Modifier.padding(end = 2.dp))
        content()
    }
}

/** A captioned block of controls stacked under its caption, for settings panels. */
@Composable
internal fun Labeled(title: String, modifier: Modifier = Modifier, content: @Composable () -> Unit) {
    Column(modifier, verticalArrangement = Arrangement.spacedBy(4.dp)) {
        Caption(title)
        content()
    }
}

/** The glass card the floating blocks and dialogs share. */
@Composable
internal fun Glass(modifier: Modifier = Modifier, shape: Shape = Corner, border: Color = Look.line, fill: Color = Look.glass, content: @Composable BoxScope.() -> Unit) {
    Box(modifier.clip(shape).background(fill).border(1.dp, border, shape), content = content)
}

/** Gold-bracket stand-in: four corner ticks around a surface (`:design` gold corner brackets). */
internal fun Modifier.brackets(color: Color = Look.metal, len: Dp = 10.dp): Modifier = drawBehind {
    val l = len.toPx()
    val w = 1.5.dp.toPx()
    val r = size.width
    val b = size.height
    drawLine(color, Offset(0f, 0f), Offset(l, 0f), w); drawLine(color, Offset(0f, 0f), Offset(0f, l), w)
    drawLine(color, Offset(r, 0f), Offset(r - l, 0f), w); drawLine(color, Offset(r, 0f), Offset(r, l), w)
    drawLine(color, Offset(0f, b), Offset(l, b), w); drawLine(color, Offset(0f, b), Offset(0f, b - l), w)
    drawLine(color, Offset(r, b), Offset(r - l, b), w); drawLine(color, Offset(r, b), Offset(r, b - l), w)
}

@Composable
internal fun Gap(w: Dp = 0.dp, h: Dp = 0.dp) = Spacer(Modifier.width(w).height(h))

@Composable
internal fun RowScope.Fill() = Spacer(Modifier.weight(1f))

internal fun Modifier.fullWidth() = fillMaxWidth()
