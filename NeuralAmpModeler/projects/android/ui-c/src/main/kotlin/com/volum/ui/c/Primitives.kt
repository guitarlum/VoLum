package com.volum.ui.c

import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsPressedAsState
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxScope
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.selection.toggleable
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LongState
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.PointerEventPass
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.TextUnit
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.Telemetry
import com.volum.core.VoLumState
import com.volum.design.components.VoLumBrassButton
import com.volum.design.components.VoLumButton
import com.volum.design.components.VoLumGlyph
import com.volum.design.components.VoLumLed
import com.volum.design.components.VoLumMeter
import com.volum.design.components.VoLumSelectionStyle
import com.volum.design.components.VoLumSubModePill
import com.volum.design.components.VoLumSwitchTrack
import com.volum.design.components.VoLumToggle
import com.volum.design.components.volumGlyph
import com.volum.design.components.volumSelection
import com.volum.design.theme.VFitText
import com.volum.design.theme.VText
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumType
import com.volum.design.theme.withOpacity
import com.volum.harness.featureId
import kotlinx.coroutines.flow.StateFlow

/** What every surface renders from and dispatches to. */
internal class Ui(val s: VoLumState, val d: (Action) -> Unit, val tel: StateFlow<Telemetry>)

internal fun Modifier.ids(ids: List<String>): Modifier = if (ids.isEmpty()) this else featureId(*ids.toTypedArray())
internal fun Modifier.ids(vararg ids: String): Modifier = if (ids.isEmpty()) this else featureId(*ids)

/** Layout of the instrument on a 914 x 411 dp landscape phone. */
internal object Metrics {
    val top = 48.dp
    /** The BUILD sheet: one control row plus the knob row, a little under half the screen. */
    val sheet = 188.dp
    /** A sheet folded down to the row a picker was opened from. */
    val tray = 72.dp
    /** A picker folded down to its header while a deeper panel is open from it. */
    val pickerHead = 60.dp
    val rail = 270.dp
    val target = 48.dp
}

/** A modal or panel action: the `:design` button, one line, never ellipsised by width it was not given. */
@Composable
internal fun Btn(label: String, ids: List<String>, onClick: () -> Unit, modifier: Modifier = Modifier, enabled: Boolean = true, primary: Boolean = false, danger: Boolean = false) {
    VoLumButton(label, onClick, modifier.ids(ids), primary = primary || danger, destructive = danger, enabled = enabled)
}

/** One option of a Brass group (cab row, time signatures, segmented settings). */
@Composable
internal fun Brass(label: String, selected: Boolean, ids: List<String>, onClick: () -> Unit, modifier: Modifier = Modifier, enabled: Boolean = true, minWidth: Dp = 52.dp) {
    VoLumBrassButton(label, selected, onClick, modifier.ids(ids), enabled = enabled, minWidth = minWidth)
}

/** A mode picker in the AmberPicker style, one feature id per option. */
@Composable
internal fun Pills(options: List<String>, selected: Int, idFor: (Int) -> String?, onSelect: (Int) -> Unit, modifier: Modifier = Modifier) {
    VoLumSubModePill(options, selected, onSelect, modifier, itemModifier = { i -> idFor(i)?.let { Modifier.ids(it) } ?: Modifier })
}

@Composable
internal fun Toggle(label: String, on: Boolean, ids: List<String>, onToggle: () -> Unit, modifier: Modifier = Modifier, enabled: Boolean = true) {
    VoLumToggle(on, { onToggle() }, modifier.ids(ids), label = label, enabled = enabled)
}

/** A `:design` switch with its caption above it, for a row that has no room for the label beside it. */
@Composable
internal fun CompactToggle(label: String, on: Boolean, ids: List<String>, onToggle: () -> Unit, modifier: Modifier = Modifier) {
    val src = remember { MutableInteractionSource() }
    val pressed by src.collectIsPressedAsState()
    Column(
        modifier.size(72.dp, Metrics.target).toggleable(on, src, null, role = Role.Switch, onValueChange = { onToggle() }).ids(ids),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.spacedBy(4.dp, Alignment.CenterVertically),
    ) {
        VFitText(label.uppercase(), style = VoLumType.label(9.sp, if (on) VoLumColors.Gold else VoLumColors.CreamDim), minSize = 7.sp)
        VoLumSwitchTrack(on, pressed = pressed)
    }
}

/** Small caps heading over or beside a group. */
@Composable
internal fun Caption(text: String, modifier: Modifier = Modifier, color: Color = VoLumColors.GoldDim) {
    VFitText(text.uppercase(), modifier, style = VoLumType.label(10.sp, color), minSize = 7.sp)
}

/**
 * Helper and hint copy: the `:design` body face in its SemiBold cut, one step
 * brighter than the body default. Josefin Regular at 11-12 sp anti-aliases to grey
 * on raisin-black and is lost at arm's length.
 */
internal fun helper(size: TextUnit = 12.sp, color: Color = VoLumColors.TextMed) = VoLumType.body(size, color).copy(fontWeight = FontWeight.SemiBold)

@Composable
internal fun Note(text: String, modifier: Modifier = Modifier, color: Color = VoLumColors.TextMed, maxLines: Int = 3, size: TextUnit = 12.sp) {
    VText(text, modifier, style = helper(size, color), maxLines = maxLines, overflow = TextOverflow.Ellipsis)
}

/** A single line that shrinks to fit instead of ellipsising (names, dense labels). */
@Composable
internal fun Fit(text: String, modifier: Modifier = Modifier, size: TextUnit = 13.sp, color: Color = VoLumColors.TextBright, min: TextUnit = (size.value * 0.7f).sp) {
    VFitText(text, modifier, style = VoLumType.label(size, color), minSize = min)
}

@Composable
internal fun Gap(w: Dp = 0.dp, h: Dp = 0.dp) = Spacer(Modifier.width(w).height(h))

@Composable
internal fun RowScope.Fill() = Spacer(Modifier.weight(1f))

/** A 48 dp target drawn as a `:design` glyph with the brass press wash. */
@Composable
internal fun GlyphBtn(glyph: VoLumGlyph, description: String, ids: List<String>, onClick: () -> Unit, modifier: Modifier = Modifier, color: Color = VoLumColors.TextBright, glyphSize: Dp = 20.dp) {
    val src = remember { MutableInteractionSource() }
    val pressed by src.collectIsPressedAsState()
    Box(
        modifier
            .size(Metrics.target)
            .clickable(src, null, role = Role.Button, onClick = onClick)
            .semantics { contentDescription = description }
            .ids(ids)
            .volumSelection(false, pressed, VoLumSelectionStyle.Brass, radius = 4.dp, inset = 4.dp),
        contentAlignment = Alignment.Center,
    ) {
        val ink by rememberUpdatedState(if (pressed) VoLumColors.Gold else color)
        Spacer(Modifier.size(glyphSize).volumGlyph(glyph, { ink }))
    }
}

/** While [on], the subtree takes no touch: what has stepped back behind a panel cannot be hit blind. */
internal fun Modifier.inert(on: Boolean): Modifier = if (!on) this else pointerInput(Unit) {
    awaitPointerEventScope {
        while (true) awaitPointerEvent(PointerEventPass.Initial).changes.forEach { it.consume() }
    }
}

/** A tappable container whose children draw its face; the press wash is the Brass hover. */
@Composable
internal fun Tappable(onClick: () -> Unit, modifier: Modifier = Modifier, ids: List<String> = emptyList(), role: Role = Role.Button, enabled: Boolean = true, content: @Composable BoxScope.(pressed: Boolean) -> Unit) {
    val src = remember { MutableInteractionSource() }
    val pressed by src.collectIsPressedAsState()
    Box(modifier.clickable(src, null, enabled = enabled, role = role, onClick = onClick).ids(ids)) { content(pressed) }
}

/**
 * A soft gold halo that breathes with the signal while [on]: telemetry is read in
 * the draw phase on the art frame clock, so it never recomposes the surface.
 */
internal fun Modifier.signalGlow(tel: StateFlow<Telemetry>, frames: LongState, on: Boolean, selected: Boolean = false, radius: Dp = 6.dp): Modifier = drawBehind {
    frames.longValue
    val e = if (on) tel.value.energy.toFloat() else 0f
    val pad = 3.dp.toPx() + 3.dp.toPx() * e
    val cr = CornerRadius(radius.toPx() + pad)
    val base = if (selected) 0.22f else 0f
    val a = (base + 0.42f * e).coerceAtMost(0.7f)
    if (a > 0f) drawRoundRect((if (selected) VoLumColors.Amber else VoLumColors.Gold).withOpacity(a * 0.5f), Offset(-pad, -pad), Size(size.width + 2 * pad, size.height + 2 * pad), cr)
}

/** A `:design` lamp lit by a telemetry predicate; recomposes only when it flips. */
@Composable
internal fun Lamp(tel: StateFlow<Telemetry>, color: Color, modifier: Modifier = Modifier, lit: (Telemetry) -> Boolean) {
    val t = tel.collectAsState()
    val test by rememberUpdatedState(lit)
    val on by remember { derivedStateOf { test(t.value) } }
    VoLumLed(on, modifier, color)
}

@Composable
internal fun InMeter(ui: Ui, frames: LongState, modifier: Modifier, horizontal: Boolean = false) {
    VoLumMeter({ ui.tel.value.meter(ui.tel.value.inDb).toFloat() }, "IN", modifier, frame = { frames.longValue }, horizontal = horizontal)
}

@Composable
internal fun OutMeter(ui: Ui, frames: LongState, modifier: Modifier, right: Boolean = false, horizontal: Boolean = false) {
    VoLumMeter({ ui.tel.value.let { it.meter(if (right) it.outRightDb else it.outDb) }.toFloat() }, if (right) "R" else "OUT", modifier, out = true, frame = { frames.longValue }, horizontal = horizontal)
}
