package com.volum.design.components

import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsPressedAsState
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.selection.selectable
import androidx.compose.foundation.selection.selectableGroup
import androidx.compose.foundation.selection.toggleable
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.draw.drawWithCache
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.design.theme.VFitText
import com.volum.design.theme.VText
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumMotion
import com.volum.design.theme.VoLumType
import com.volum.design.theme.ic
import com.volum.design.theme.withOpacity

/**
 * The AMP-row slide switch (`NAMSwitchControl`: NOISE GATE, EQ, PING-PONG,
 * TEMPO SYNC): a rounded track, gold when on, with the dark handle sliding to
 * the side. The whole row (track + [label]) is one 48 dp toggle target.
 */
@Composable
fun VoLumToggle(
    checked: Boolean,
    onCheckedChange: (Boolean) -> Unit,
    modifier: Modifier = Modifier,
    label: String? = null,
    enabled: Boolean = true,
) {
    val source = remember { MutableInteractionSource() }
    val pressed by source.collectIsPressedAsState()
    Row(
        modifier
            .heightIn(min = 48.dp)
            .alpha(if (enabled) 1f else 0.4f)
            .toggleable(checked, source, indication = null, enabled = enabled, role = Role.Switch, onValueChange = onCheckedChange)
            .padding(horizontal = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        VoLumSwitchTrack(checked, pressed = pressed)
        if (label != null) VText(label, style = VoLumType.label(13.sp))
    }
}

internal fun lerp(a: Color, b: Color, t: Float) = Color(a.red + (b.red - a.red) * t, a.green + (b.green - a.green) * t, a.blue + (b.blue - a.blue) * t, a.alpha + (b.alpha - a.alpha) * t)

/** `_DrawMiniPill`: the 22x14 on/off pill of the quiet PRE/POST slots. Draw-only; the row owns the touch target. */
@Composable
fun MiniPill(on: Boolean, modifier: Modifier = Modifier, dimmed: Boolean = false) {
    val pos by animateFloatAsState(if (on) 1f else 0f, tween(VoLumMotion.ToggleMs), label = "pill")
    Spacer(modifier.size(22.dp, 14.dp).drawWithCache {
        val radius = size.height / 2f
        val cr = CornerRadius(radius)
        val px = 1.dp.toPx()
        val stroke = Stroke(px)
        val knobR = radius - 1.5f * px
        onDrawBehind {
            drawRoundRect(ic(255, 8, 10, 14), cornerRadius = cr)
            val track = if (pos > 0.5f) VoLumColors.Gold.withOpacity(if (dimmed) 0.18f else 0.35f) else VoLumColors.Frame
            drawRoundRect(track, Offset(px / 2, px / 2), Size(size.width - px, size.height - px), cr, style = stroke)
            val cx = radius + (size.width - 2 * radius) * pos
            drawCircle(lerp(VoLumColors.TextDim, VoLumColors.Gold, pos), knobR, Offset(cx, size.height / 2f))
        }
    })
}

/**
 * The vertical pedal power switch beside the IN meter on PRE / POST
 * (`VoLumPowerSwitchControl`): power glyph over a slim track, gold knob up when on.
 */
@Composable
fun VoLumPowerSwitch(on: Boolean, onToggle: (Boolean) -> Unit, modifier: Modifier = Modifier, enabled: Boolean = true) {
    val source = remember { MutableInteractionSource() }
    val pressed by source.collectIsPressedAsState()
    val pos by animateFloatAsState(if (on) 1f else 0f, tween(VoLumMotion.ToggleMs), label = "power")
    Spacer(
        modifier
            .size(48.dp, 72.dp)
            .alpha(if (enabled) 1f else 0.4f)
            .toggleable(on, source, indication = null, enabled = enabled, role = Role.Switch, onValueChange = onToggle)
            .drawWithCache {
                val px = 1.dp.toPx()
                val cx = size.width / 2f
                val trackT = 24 * px
                val trackB = size.height - 4 * px
                val tr = CornerRadius(6 * px)
                val glyph = Stroke(1.6f * px, cap = StrokeCap.Round)
                val thin = Stroke(px)
                onDrawBehind {
                    val ink = lerp(VoLumColors.TextDim, VoLumColors.Gold, pos)
                    drawArc(ink, -60f, 300f, false, Offset(cx - 6 * px, 4 * px), Size(12 * px, 12 * px), style = glyph)
                    drawLine(ink, Offset(cx, 2.5f * px), Offset(cx, 9.5f * px), 1.6f * px, StrokeCap.Round)
                    drawRoundRect(ic(255, 8, 10, 14), Offset(cx - 6 * px, trackT), Size(12 * px, trackB - trackT), tr)
                    drawRoundRect(lerp(VoLumColors.Frame, VoLumColors.Gold.withOpacity(0.35f), pos), Offset(cx - 6 * px, trackT), Size(12 * px, trackB - trackT), tr, style = thin)
                    val cy = trackB - 8 * px - (trackB - trackT - 16 * px) * pos
                    drawCircle(ink, 8 * px, Offset(cx, cy))
                    if (pressed) drawCircle(VoLumColors.TextBright, 10 * px, Offset(cx, cy), style = thin)
                }
            },
    )
}

/**
 * `VoLumModePickerControl`: a vertical list of modes (DIGITAL / ANALOG /
 * REVERSE) behind a hairline, the active one in solid amber.
 */
@Composable
fun VoLumModePicker(options: List<String>, selected: Int, onSelect: (Int) -> Unit, modifier: Modifier = Modifier, itemModifier: (Int) -> Modifier = { Modifier }) {
    Column(
        modifier
            .selectableGroup()
            .drawBehind { drawLine(VoLumColors.Frame, Offset(0f, 0f), Offset(0f, size.height), 1.dp.toPx()) }
            .padding(start = 12.dp),
    ) {
        options.forEachIndexed { i, label ->
            val source = remember { MutableInteractionSource() }
            val pressed by source.collectIsPressedAsState()
            Box(
                itemModifier(i)
                    .widthIn(min = 96.dp)
                    .heightIn(min = 48.dp)
                    .selectable(i == selected, source, indication = null, role = Role.RadioButton) { onSelect(i) }
                    .volumSelection(i == selected, pressed, VoLumSelectionStyle.AmberPicker, radius = 0.dp, inset = 1.dp)
                    .padding(start = 6.dp, end = 10.dp),
                contentAlignment = Alignment.CenterStart,
            ) {
                VText(label, style = VoLumType.label(11.sp), color = selectionInk(VoLumSelectionStyle.AmberPicker, i == selected))
            }
        }
    }
}

/** `VoLumSubModePillControl`: horizontal pill of sub-modes (Dark / Shimmer / Bloom), amber on the active one. */
@Composable
fun VoLumSubModePill(options: List<String>, selected: Int, onSelect: (Int) -> Unit, modifier: Modifier = Modifier, itemModifier: (Int) -> Modifier = { Modifier }) {
    Row(
        modifier
            .heightIn(min = 48.dp)
            .selectableGroup()
            .drawWithCache {
                val cr = CornerRadius(4.dp.toPx())
                val px = 1.dp.toPx()
                val stroke = Stroke(px)
                onDrawBehind {
                    drawRoundRect(ic(160, 14, 16, 22), cornerRadius = cr)
                    drawRoundRect(VoLumColors.Frame, Offset(px / 2, px / 2), Size(size.width - px, size.height - px), cr, style = stroke)
                }
            },
    ) {
        options.forEachIndexed { i, label ->
            val source = remember { MutableInteractionSource() }
            val pressed by source.collectIsPressedAsState()
            Box(
                itemModifier(i)
                    .weight(1f)
                    .heightIn(min = 48.dp)
                    .selectable(i == selected, source, indication = null, role = Role.RadioButton) { onSelect(i) }
                    .volumSelection(i == selected, pressed, VoLumSelectionStyle.AmberPicker, radius = 3.dp, inset = 1.5.dp)
                    .drawBehind { if (i > 0) drawLine(ic(96, 200, 162, 78), Offset(0f, 3.dp.toPx()), Offset(0f, size.height - 3.dp.toPx()), 1.dp.toPx()) }
                    .padding(horizontal = 6.dp),
                contentAlignment = Alignment.Center,
            ) {
                VFitText(label, style = VoLumType.label(9.5.sp, selectionInk(VoLumSelectionStyle.AmberPicker, i == selected)), minSize = 7.sp, textAlign = TextAlign.Center)
            }
        }
    }
}

/**
 * The cab row (`VoLumSpeakerRowControl`): mutually exclusive buttons in the
 * Brass dialect with the soft glow under the active one. [enabled] false is
 * the greyed empty slot ("--" / disabled Custom IR).
 */
@Composable
fun VoLumBrassButton(
    label: String,
    selected: Boolean,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    enabled: Boolean = true,
    minWidth: Dp = 52.dp,
    leading: (@Composable () -> Unit)? = null,
) {
    val source = remember { MutableInteractionSource() }
    val pressed by source.collectIsPressedAsState()
    Row(
        modifier
            .heightIn(min = 48.dp)
            .widthIn(min = minWidth)
            .selectable(selected, source, indication = null, enabled = enabled, role = Role.RadioButton, onClick = onClick)
            .padding(vertical = 10.dp)
            .drawWithCache {
                val cr = CornerRadius(3.dp.toPx())
                val px = 1.dp.toPx()
                val thin = Stroke(px)
                val bold = Stroke(1.35f * px)
                onDrawBehind {
                    if (!selected) {
                        drawRoundRect(VoLumColors.BtnOffBg, cornerRadius = cr)
                        drawRoundRect(if (enabled) (if (pressed) VoLumColors.GoldDim else VoLumColors.BtnOffBorder) else ic(50, 200, 162, 70), Offset(px / 2, px / 2), Size(size.width - px, size.height - px), cr, style = if (pressed) bold else thin)
                    }
                }
            }
            .volumSelection(selected, pressed && enabled, VoLumSelectionStyle.Brass, radius = 3.dp, inset = 0.dp, glow = true)
            .padding(horizontal = 12.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(6.dp, Alignment.CenterHorizontally),
    ) {
        leading?.invoke()
        val ink = when {
            !enabled -> ic(70, 235, 220, 200)
            selected -> selectionInk(VoLumSelectionStyle.Brass, true)
            pressed -> VoLumColors.TextBright
            else -> VoLumColors.BtnOffText
        }
        VText(label, style = VoLumType.label(13.sp), color = ink)
    }
}

/**
 * The Art Deco channel stepper (`VoLumChannelStepControl`): `<  3  >` with thin
 * gold lines, the number in gold. Each arrow is its own 48 dp target.
 */
@Composable
fun VoLumChannelStepper(
    text: String,
    onPrev: () -> Unit,
    onNext: () -> Unit,
    modifier: Modifier = Modifier,
    prevModifier: Modifier = Modifier,
    nextModifier: Modifier = Modifier,
) {
    Row(modifier.height(48.dp), verticalAlignment = Alignment.CenterVertically) {
        StepArrow("<", onPrev, prevModifier, leftLine = true)
        VText(text, Modifier.widthIn(min = 28.dp), style = VoLumType.Stepper, textAlign = TextAlign.Center)
        StepArrow(">", onNext, nextModifier, leftLine = false)
    }
}

@Composable
private fun StepArrow(glyph: String, onClick: () -> Unit, modifier: Modifier, leftLine: Boolean) {
    val source = remember { MutableInteractionSource() }
    val pressed by source.collectIsPressedAsState()
    Box(
        modifier
            .size(48.dp)
            .clickable(source, indication = null, role = Role.Button, onClick = onClick)
            .drawBehind {
                val y = size.height / 2
                val line = ic(50, 200, 162, 78)
                if (leftLine) drawLine(line, Offset(0f, y), Offset(size.width * 0.3f, y), 1.dp.toPx())
                else drawLine(line, Offset(size.width * 0.7f, y), Offset(size.width, y), 1.dp.toPx())
            },
        contentAlignment = Alignment.Center,
    ) {
        VText(glyph, style = VoLumType.body(17.sp, if (pressed) VoLumColors.Gold else VoLumColors.TextBright))
    }
}

/**
 * `DrawVoLumSegmentSwitch`: recessed two-state brass pill (FULL | LITE, All | One).
 * Each half is its own radio target ([segmentModifier] false = [first], true =
 * [second], for feature ids) with the brass press wash under the finger. The
 * labels shrink 11 -> 8 sp before they would clip.
 */
@Composable
fun VoLumSegmentSwitch(
    first: String,
    second: String,
    secondActive: Boolean,
    onSelect: (Boolean) -> Unit,
    modifier: Modifier = Modifier,
    segmentModifier: (second: Boolean) -> Modifier = { Modifier },
) = SegmentSwitch(first, second, secondActive, onSelect, modifier, segmentModifier, previewPressed = null)

/** [VoLumSegmentSwitch] with a half held down for the gallery ([previewPressed] true = the second half). */
@Composable
internal fun SegmentSwitch(
    first: String,
    second: String,
    secondActive: Boolean,
    onSelect: (Boolean) -> Unit,
    modifier: Modifier,
    segmentModifier: (second: Boolean) -> Modifier,
    previewPressed: Boolean?,
) {
    val pos by animateFloatAsState(if (secondActive) 1f else 0f, tween(VoLumMotion.ToggleMs), label = "segment")
    Row(
        modifier
            .height(48.dp)
            .padding(vertical = 10.dp)
            .selectableGroup()
            .drawWithCache {
                val cr = CornerRadius(size.height / 2)
                val px = 1.dp.toPx()
                val activeStroke = Stroke(1.25f * px)
                val trackStroke = Stroke(px)
                onDrawBehind {
                    drawRoundRect(ic(255, 9, 9, 14), cornerRadius = cr)
                    val half = size.width / 2
                    val x = half * pos
                    drawRoundRect(VoLumColors.Gold.withOpacity(0.30f), Offset(x, 0f), Size(half, size.height), cr)
                    drawRoundRect(VoLumColors.Gold, Offset(x + 0.6f * px, 0.6f * px), Size(half - 1.2f * px, size.height - 1.2f * px), cr, style = activeStroke)
                    drawRoundRect(VoLumColors.Frame, Offset(px / 2, px / 2), Size(size.width - px, size.height - px), cr, style = trackStroke)
                }
            },
    ) {
        listOf(first to false, second to true).forEach { (label, isSecond) ->
            val source = remember { MutableInteractionSource() }
            val held by source.collectIsPressedAsState()
            val pressed = held || previewPressed == isSecond
            val active = secondActive == isSecond
            Box(
                segmentModifier(isSecond).weight(1f).fillMaxWidth().heightIn(min = 28.dp)
                    .selectable(active, source, indication = null, role = Role.RadioButton) { onSelect(isSecond) }
                    .drawWithCache {
                        val cr = CornerRadius(size.height / 2)
                        val inset = 2.dp.toPx()
                        onDrawBehind { if (pressed) drawRoundRect(VoLumColors.SelBg, Offset(inset, inset), Size(size.width - 2 * inset, size.height - 2 * inset), cr) }
                    },
                contentAlignment = Alignment.Center,
            ) {
                val ink = when {
                    active -> VoLumColors.Gold
                    pressed -> VoLumColors.TextBright
                    else -> VoLumColors.CreamDim
                }
                VFitText(label, Modifier.padding(horizontal = 10.dp), style = VoLumType.label(11.sp, ink), minSize = 8.sp, textAlign = TextAlign.Center)
            }
        }
    }
}

/**
 * The IN / OUT ladder (`VoLumPlaySurface::DrawMeter`): 18 brass segments in an
 * inset well, the caption under it. [level] is 0..1 and read in the draw
 * phase only; pass a frame-ticked source (see [com.volum.design.art.rememberArtFrames]).
 * [horizontal] lays the same ladder left to right with the caption before it,
 * for strips that have width to spare and no height.
 */
@Composable
fun VoLumMeter(level: () -> Float, label: String, modifier: Modifier = Modifier, out: Boolean = false, frame: () -> Long = { 0L }, horizontal: Boolean = false) {
    if (horizontal) {
        Row(modifier, verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            VText(label, style = VoLumType.label(8.sp, VoLumColors.CreamDim))
            Spacer(Modifier.weight(1f).height(12.dp).insetWell(1.dp).meterLadder(level, out, frame, horizontal = true))
        }
        return
    }
    Column(modifier, horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(4.dp)) {
        Spacer(Modifier.weight(1f).width(12.dp).insetWell(1.dp).meterLadder(level, out, frame, horizontal = false))
        VText(label, style = VoLumType.label(8.sp, VoLumColors.CreamDim))
    }
}

private fun Modifier.meterLadder(level: () -> Float, out: Boolean, frame: () -> Long, horizontal: Boolean): Modifier = drawWithCache {
    val segments = 18
    val px = 1.dp.toPx()
    val span = (if (horizontal) size.width else size.height) - 4 * px
    val seg = span / segments
    val lit = if (out) VoLumColors.Gold else VoLumColors.GoldDim
    val unlit = lit.withOpacity(0.10f)
    val cell = if (horizontal) Size(seg - px, size.height - 4 * px) else Size(size.width - 4 * px, seg - px)
    onDrawBehind {
        frame()
        val v = level().coerceIn(0f, 1f)
        for (i in 0 until segments) {
            if (horizontal) {
                val frac = (i + 1).toFloat() / segments
                drawRect(if (frac <= v) lit else unlit, Offset(2 * px + i * seg, 2 * px), cell)
            } else {
                val frac = (segments - i).toFloat() / segments
                drawRect(if (frac <= v) lit else unlit, Offset(2 * px, 2 * px + i * seg), cell)
            }
        }
    }
}
