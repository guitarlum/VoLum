package com.volum.ui.a

import androidx.compose.foundation.ScrollState
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsPressedAsState
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.selection.selectable
import androidx.compose.foundation.verticalScroll
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.layout
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.TextUnit
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.Param
import com.volum.design.components.CrossButton
import com.volum.design.components.VoLumBanner
import com.volum.design.components.VoLumBannerKind
import com.volum.design.components.VoLumGlyph
import com.volum.design.components.VoLumIcon
import com.volum.design.components.VoLumKnob
import com.volum.design.components.VoLumLed
import com.volum.design.components.VoLumMeter
import com.volum.design.components.VoLumSelectionStyle
import com.volum.design.components.cornerBrackets
import com.volum.design.components.hairlineFrame
import com.volum.design.components.insetWell
import com.volum.design.components.panelDepth
import com.volum.design.components.selectionInk
import com.volum.design.components.volumScrollbar
import com.volum.design.components.volumSelection
import com.volum.design.theme.VFitText
import com.volum.design.theme.VText
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumType
import com.volum.design.theme.withOpacity
import com.volum.harness.featureId

/** Adds the harness feature ids, or nothing when [ids] is empty. One call per node: a second `featureId` on the same node is dropped. */
internal fun Modifier.ids(ids: List<String>): Modifier = if (ids.isEmpty()) this else featureId(*ids.distinct().toTypedArray())
internal fun Modifier.ids(vararg ids: String): Modifier = ids(ids.toList())
internal fun Modifier.desc(text: String): Modifier = semantics { contentDescription = text }

internal fun Modifier.swallowTaps(): Modifier = pointerInput(Unit) { detectTapGestures { } }

/** Section caption (`VoLumType.SectionHeader`): "MODE", "SOUNDS", "CAB NAMES". */
@Composable
internal fun Caption(text: String, modifier: Modifier = Modifier, color: Color = VoLumColors.GoldDim) =
    VText(text, modifier, style = VoLumType.label(10.sp, color))

/** Explanatory copy under a control, in the desktop's body face. */
@Composable
internal fun Hint(text: String, modifier: Modifier = Modifier, color: Color = VoLumColors.CreamDim, lines: Int = 2, align: TextAlign? = null) =
    VText(text, modifier, style = VoLumType.body(11.sp, color), maxLines = lines, textAlign = align)

/**
 * One option of a mutually exclusive group, drawn by the shared selection
 * ([volumSelection] + [selectionInk]): AmberPicker for mode-like pickers and tabs,
 * Brass for setting groups, ListTeal for list choices. The label shrinks to fit
 * instead of ellipsising.
 */
@Composable
internal fun Choice(
    label: String,
    selected: Boolean,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    style: VoLumSelectionStyle = VoLumSelectionStyle.Brass,
    enabled: Boolean = true,
    size: TextUnit = 12.sp,
    minWidth: Dp = 56.dp,
) {
    val source = remember { MutableInteractionSource() }
    val pressed by source.collectIsPressedAsState()
    Box(
        modifier
            .heightIn(min = 48.dp)
            .widthIn(min = minWidth)
            .selectable(selected, source, indication = null, enabled = enabled, role = Role.RadioButton, onClick = onClick)
            .alpha(if (enabled) 1f else 0.4f)
            .padding(vertical = 6.dp)
            .then(if (!selected) Modifier.hairlineFrame(if (pressed) VoLumColors.GoldDim else VoLumColors.BtnOffBorder, radius = 3.dp) else Modifier)
            .volumSelection(selected, pressed && enabled, style, inset = 0.dp)
            .padding(horizontal = 10.dp),
        contentAlignment = Alignment.Center,
    ) {
        val ink = if (selected) selectionInk(style, true) else if (pressed) VoLumColors.TextBright else VoLumColors.BtnOffText
        VFitText(label, style = VoLumType.label(size, ink), minSize = (size.value * 0.7f).sp, textAlign = TextAlign.Center)
    }
}

/** A 48 dp glyph button with the theme's press wash. */
@Composable
internal fun GlyphButton(
    glyph: VoLumGlyph,
    onClick: () -> Unit,
    desc: String,
    modifier: Modifier = Modifier,
    enabled: Boolean = true,
    tint: Color = VoLumColors.TextBright,
    size: Dp? = null,
) {
    Box(
        modifier.size(48.dp).clickable(enabled = enabled, role = Role.Button, onClickLabel = desc, onClick = onClick).desc(desc).alpha(if (enabled) 1f else 0.35f),
        contentAlignment = Alignment.Center,
    ) { VoLumIcon(glyph, color = tint, size = size) }
}

/** A glyph with its word ("Edit", "Delete", "Replace"): an icon never stands alone for an action that changes the library. */
@Composable
internal fun GlyphAction(
    glyph: VoLumGlyph,
    label: String,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    tint: Color = VoLumColors.Cream,
    vertical: Boolean = false,
) {
    val content: @Composable () -> Unit = {
        VoLumIcon(glyph, color = tint, size = 16.dp)
        VFitText(label, style = VoLumType.label(10.sp, tint), minSize = 7.sp, textAlign = TextAlign.Center)
    }
    val base = modifier.heightIn(min = 48.dp).widthIn(min = 48.dp).clickable(role = Role.Button, onClick = onClick).padding(horizontal = 8.dp)
    if (vertical) Column(base, horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(3.dp, Alignment.CenterVertically)) { content() }
    else Row(base, verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) { content() }
}

/** The knob of one param, in the desktop's units; MAIN gold, SUPPORT teal. */
@Composable
internal fun ParamKnob(c: Ctx, p: Param, label: String = p.label, support: Boolean = false, dial: Dp = 52.dp) {
    val v = c.s.param(p)
    VoLumKnob(
        value = v,
        onValueChange = { c.d(Action.SetParam(p, it)) },
        range = p.min..p.max,
        default = p.default,
        label = label,
        valueText = fmt(p, v),
        modifier = Modifier.ids(knobIds(p)),
        support = support,
        dialSize = dial,
        parseEntry = ::parseNumber,
    )
}

internal fun parseNumber(s: String): Double? = s.trim().replace(',', '.').trimEnd { !it.isDigit() }.trimStart('+').toDoubleOrNull()

/**
 * IN / OUT: the desktop's 18-segment ladder with a red clip lamp on top, lit
 * while the level is within 1 dB of full scale or the output limiter holds it.
 */
@Composable
internal fun MeterColumn(c: Ctx, out: Boolean, modifier: Modifier = Modifier, limited: Boolean = false) {
    val clip by remember(c.tel, out) { derivedStateOf { (if (out) c.tel.value.outDb else c.tel.value.inDb) > -1.0 } }
    Column(modifier.width(26.dp), horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(2.dp)) {
        VoLumLed(clip || (out && limited), color = VoLumColors.Danger)
        VoLumMeter({ if (out) c.outLevel() else c.inLevel() }, if (out) "OUT" else "IN", Modifier.weight(1f), out = out)
    }
}

/**
 * The modal panel every overlay sits on (`VoLumConfirmDialogControl` / settings
 * window chrome): panel depth, a gold-dim hairline, gold corner brackets, the
 * title in gold Josefin capitals, the drawn close cross. A banner raised while
 * the panel is open is shown inside it, above the content, pushing it down.
 * [fill] false sizes the panel to its content.
 */
@Composable
internal fun Sheet(
    title: String,
    closeIds: List<String>,
    c: Ctx,
    width: Dp = 880.dp,
    fill: Boolean = true,
    align: Alignment = Alignment.Center,
    actions: @Composable RowScope.() -> Unit = {},
    content: @Composable ColumnScope.() -> Unit,
) {
    Box(Modifier.fillMaxSize().padding(8.dp), contentAlignment = align) {
        Column(
            Modifier
                .width(width)
                .then(if (fill) Modifier.fillMaxHeight() else Modifier)
                .swallowTaps()
                .panelDepth(6.dp)
                .hairlineFrame(VoLumColors.GoldDim.withOpacity(0.55f), 1.dp, 6.dp)
                .cornerBrackets(VoLumColors.Corner, 16.dp, 6.dp)
                .padding(start = 18.dp, end = 8.dp, top = 2.dp, bottom = 10.dp),
        ) {
            Row(Modifier.fillMaxWidth().height(50.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                VFitText(title.uppercase(), Modifier.weight(1f), style = VoLumType.label(16.sp, VoLumColors.Gold), minSize = 11.sp)
                actions()
                CrossButton({ c.d(Action.Back) }, Modifier.ids(closeIds).desc("Close"))
            }
            Column(Modifier.padding(end = 10.dp)) {
                BannerSlot(c, Modifier.padding(bottom = 6.dp))
            }
            Column(Modifier.padding(end = 10.dp).then(if (fill) Modifier.weight(1f) else Modifier), content = content)
        }
    }
}

/** The error / info toast in the slot its surface reserves for it; nothing is drawn over content. */
@Composable
internal fun BannerSlot(c: Ctx, modifier: Modifier = Modifier) {
    val banner = c.s.ui.banner ?: return
    val error = banner.kind == com.volum.core.BannerKind.ERROR
    VoLumBanner(
        banner.message,
        if (error) VoLumBannerKind.ERROR else VoLumBannerKind.INFO,
        { c.d(Action.DismissBanner) },
        modifier.fillMaxWidth().then(if (error) Modifier.ids("banner.error") else Modifier),
        dismissModifier = Modifier.ids("banner.dismiss").desc("Dismiss"),
    )
}

/** A recessed note (empty catalogs, explanations): inset well, body copy. */
@Composable
internal fun Note(text: String, modifier: Modifier = Modifier, alert: Boolean = false, ids: List<String> = emptyList(), lines: Int = 3) {
    Box(modifier.ids(ids).insetWell(3.dp).then(if (alert) Modifier.hairlineFrame(VoLumColors.Danger.withOpacity(0.6f), radius = 3.dp) else Modifier).padding(horizontal = 12.dp, vertical = 10.dp)) {
        VText(text, style = VoLumType.body(12.sp, if (alert) VoLumColors.TextBright else VoLumColors.Cream), maxLines = lines)
    }
}

/**
 * Scrolling catalog in an inset well whose viewport is a whole number of [pitch]
 * rows, so no row is ever cut in half at the edge (a half row is a sub-48 dp
 * target). Rows must be multiples of [pitch] tall; [scrollToRow] lands the list
 * on a row so the current item is in view when the catalog opens.
 */
@Composable
internal fun RowList(
    pitch: Dp,
    modifier: Modifier = Modifier,
    scrollToRow: Int = 0,
    state: ScrollState = rememberScrollState(),
    content: @Composable ColumnScope.() -> Unit,
) {
    val px = with(LocalDensity.current) { pitch.roundToPx() }
    LaunchedEffect(scrollToRow, px) { state.scrollTo(scrollToRow.coerceAtLeast(0) * px) }
    Box(modifier) {
        Box(
            Modifier.layout { m, c ->
                val h = if (c.hasBoundedHeight) (c.maxHeight / px) * px else c.maxHeight
                val placeable = m.measure(c.copy(minHeight = 0, maxHeight = h))
                layout(placeable.width, placeable.height) { placeable.place(0, 0) }
            }.insetWell(3.dp).volumScrollbar(state),
        ) {
            Column(Modifier.verticalScroll(state).padding(end = 8.dp), content = content)
        }
    }
}

@Composable
internal fun Gap(w: Dp = 0.dp, h: Dp = 0.dp) = Spacer(Modifier.width(w).height(h))

@Composable
internal fun ColumnScope.Fill() = Spacer(Modifier.weight(1f))

@Composable
internal fun RowScope.Fill() = Spacer(Modifier.weight(1f))
