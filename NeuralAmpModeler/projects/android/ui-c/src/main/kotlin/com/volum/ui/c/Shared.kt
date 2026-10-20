package com.volum.ui.c

import androidx.compose.foundation.ScrollState
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.remember
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.layout.layout
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.unit.Density
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.floor
import com.volum.core.Action
import com.volum.core.Amp
import com.volum.core.BannerKind
import com.volum.core.Cab
import com.volum.core.Param
import com.volum.core.VoLumState
import com.volum.design.art.ArtRef
import com.volum.design.components.CrossButton
import com.volum.design.components.VoLumBanner
import com.volum.design.components.VoLumBannerKind
import com.volum.design.components.VoLumListRow
import com.volum.design.components.VoLumSelectionStyle
import com.volum.design.components.cornerBrackets
import com.volum.design.components.hairlineFrame
import com.volum.design.components.panelDepth
import com.volum.design.components.volumScrollbar
import com.volum.design.theme.VFitText
import com.volum.design.theme.VText
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumType
import com.volum.design.theme.withOpacity

internal fun captureName(ui: Ui, p: Param): String {
    val idx = ui.s.param(p).toInt()
    val pedal = ui.s.library.pedals.firstOrNull { it.index == idx } ?: return "Empty"
    return if (pedal.custom) pedal.name else Captures.fullName(pedal.index, pedal.name)
}

internal fun cabLabel(cab: Cab, amp: Amp?): String = when {
    cab == Cab.CUSTOM_IR -> "Custom IR"
    amp?.custom == true -> amp.cabNames.getOrNull(cab.ordinal) ?: cab.label
    else -> cab.label
}

/** The bundled PRE captures as the desktop capture menu groups and names them. */
internal object Captures {
    val groups = listOf("Klon" to listOf(6, 3), "TS / Boost" to listOf(7, 8, 4), "Distortion" to listOf(5), "Fuzz" to listOf(9, 1, 2))
    private val names = mapOf(1 to "PettyJohn Nuke", 2 to "JHS Bender", 3 to "PettyJohn Myth", 4 to "PettyJohn Mash", 5 to "Revival Drive", 6 to "Klon", 7 to "Halcyon TS", 8 to "Halcyon TS +Gain", 9 to "Fatbee")
    fun fullName(index: Int, fallback: String) = if (index == 0) "Empty" else names[index] ?: fallback
}

internal fun artOf(amp: Amp?): ArtRef = amp?.let { ArtRef.of(it) } ?: ArtRef.factory(13)

/** C's glass over the art, one name per role, all from the `:design` palette. */
internal object Glass {
    /** Tabs, tiles and the rail floating on the art. */
    val Float = VoLumColors.HeroBg.withOpacity(0.8f)
    /** The title strip and paddles under the PLAY sound name. */
    val Strip = VoLumColors.WellDark.withOpacity(0.8f)
    /** The edit readout over the art. */
    val Readout = VoLumColors.WellDark.withOpacity(0.84f)
    /** The tuner's veil over the art. */
    val Veil = VoLumColors.Scrim.withOpacity(0.45f)
}

/**
 * The largest factor (at most 1) at which every text of [entries] fits its room
 * at [style], so a whole list shares one size instead of fitting row by row.
 */
@Composable
internal fun sharedFit(entries: List<Pair<String, Dp>>, style: TextStyle): Float {
    val measurer = rememberTextMeasurer()
    val density = LocalDensity.current
    return remember(entries, style, density) {
        entries.minOfOrNull { (text, room) ->
            val needed = measurer.measure(text, style, softWrap = false, maxLines = 1).size.width
            val avail = with(density) { room.toPx() }
            if (needed <= avail || needed == 0) 1f else avail / needed
        }?.let { floor(it * 50f) / 50f }?.coerceIn(0.5f, 1f) ?: 1f
    }
}

/**
 * Lists built from `VoLumListRow`: the font scale drops by [sharedFit] so the
 * longest name fits at the row's 13 sp and every row shows the same size.
 */
@Composable
internal fun SharedFitRows(entries: List<Pair<String, Dp>>, content: @Composable () -> Unit) {
    val k = sharedFit(entries, VoLumType.label(13.sp))
    val d = LocalDensity.current
    if (k >= 1f) content() else CompositionLocalProvider(LocalDensity provides Density(d.density, d.fontScale * k)) { content() }
}

/** The room a `VoLumListRow` of [row] width leaves its title. */
internal fun rowTitleRoom(row: Dp, art: Boolean, trailing: Dp = 0.dp): Dp =
    row - 18.dp - (if (art) 30.dp else 0.dp) - (if (trailing > 0.dp) trailing + 8.dp else 0.dp) - 2.dp

/** One cell of [columns] in a snapped list [width] wide (its scrollbar gutter included). */
internal fun cellWidth(width: Dp, columns: Int): Dp = (width - 8.dp - 6.dp * (columns - 1)) / columns

/**
 * The chrome of every panel: panel depth, a gold hairline and gold corner brackets
 * set inside the padding, so they never touch text. The title is Poiret One as the
 * desktop's overlay titles are. A banner, when there is one, gets its own row
 * under the header instead of landing on top of the content.
 */
@Composable
internal fun PanelFrame(
    ui: Ui,
    title: String,
    closeId: String?,
    onClose: () -> Unit,
    modifier: Modifier = Modifier,
    crumb: String? = null,
    actions: @Composable RowScope.() -> Unit = {},
    lead: @Composable RowScope.() -> Unit = {},
    content: @Composable ColumnScope.() -> Unit,
) {
    val floor = LocalPanelFloor.current
    Column(
        modifier
            .panelDepth(6.dp)
            .hairlineFrame(VoLumColors.Frame, radius = 6.dp)
            .cornerBrackets(VoLumColors.Corner, 10.dp, 5.dp)
            .padding(start = 18.dp, end = 12.dp, top = 6.dp, bottom = 12.dp)
            .cleanFloor(floor, 18.dp),
        verticalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        Row(Modifier.fillMaxWidth().height(Metrics.target), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            if (crumb != null) VText(crumb.uppercase() + "  >", style = VoLumType.label(10.sp, VoLumColors.TealDim))
            lead()
            if (title.isNotEmpty()) VFitText(title, Modifier.weight(1f, fill = false), style = VoLumType.display(22.sp, VoLumColors.Gold), minSize = 14.sp)
            Fill()
            actions()
            if (closeId != null) CrossButton(onClose, Modifier.ids(closeId.ifEmpty { "nav.back" }), VoLumColors.TextDim)
        }
        PanelBanner(ui)
        content()
    }
}

/**
 * Where a panel may end, given its outer height in px: the hanging placement
 * provides it so a panel never stops across a row of controls under it.
 */
internal val LocalPanelFloor = staticCompositionLocalOf<((Int) -> Int)?> { null }

/** Stretches the panel (content kept at the top) to the height [floor] picks; [chrome] is its vertical padding. */
private fun Modifier.cleanFloor(floor: ((Int) -> Int)?, chrome: Dp): Modifier = if (floor == null) this else layout { m, c ->
    val p = m.measure(c)
    val pad = chrome.roundToPx()
    val h = (floor(p.height + pad) - pad).coerceIn(p.height, c.maxHeight.coerceAtLeast(p.height))
    layout(p.width, h) { p.place(0, 0) }
}

/** Errors and notices in a reserved row, never over a control. */
@Composable
internal fun PanelBanner(ui: Ui, modifier: Modifier = Modifier) {
    val b = ui.s.ui.banner
    val error = b?.kind == BannerKind.ERROR
    VoLumBanner(
        b?.message,
        if (error || b == null) VoLumBannerKind.ERROR else VoLumBannerKind.INFO,
        { ui.d(Action.DismissBanner) },
        modifier.fillMaxWidth().then(if (error) Modifier.ids("banner.error") else Modifier),
        dismissModifier = Modifier.ids("banner.dismiss"),
    )
}

/** The floating banner of the bare views: it sits on the art, where no control is. */
@Composable
internal fun FloatingBanner(ui: Ui, modifier: Modifier) {
    Box(modifier.widthIn(max = 780.dp)) { PanelBanner(ui) }
}

/** A 48 dp list row in the ListTeal style (or AmberPicker for preset rows). */
@Composable
internal fun ListRow(
    label: String,
    ids: List<String>,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    sub: String? = null,
    selected: Boolean = false,
    enabled: Boolean = true,
    art: ArtRef? = null,
    style: VoLumSelectionStyle = VoLumSelectionStyle.ListTeal,
    trailing: (@Composable RowScope.() -> Unit)? = null,
) {
    VoLumListRow(label, selected, onClick, modifier.ids(ids), art = art, subtitle = sub, style = style, enabled = enabled, trailing = trailing)
}

/**
 * A scrolling list of 48 dp rows sized to its [rows], but never taller than the room
 * it is given, cut to whole rows so no row is sliced into a sub-48 dp target. The
 * gold `:design` scrollbar shows when it scrolls.
 */
@Composable
internal fun SnapList(rows: Int, modifier: Modifier, state: ScrollState = rememberScrollState(), inner: Modifier = Modifier, content: @Composable ColumnScope.() -> Unit) {
    BoxWithConstraints(modifier) {
        val density = LocalDensity.current
        val pitch = rowPitchPx()
        val gap = with(density) { RowGap.roundToPx() }
        val fit = if (constraints.hasBoundedHeight) ((constraints.maxHeight + gap) / pitch).coerceAtLeast(1) else rows
        if (rows <= fit) {
            Column(Modifier.fillMaxWidth().then(inner), verticalArrangement = Arrangement.spacedBy(RowGap), content = content)
        } else {
            val height = with(density) { (fit * pitch - gap + 2).toDp() }
            Column(
                Modifier.fillMaxWidth().height(height).volumScrollbar(state).verticalScroll(state).then(inner).padding(end = 8.dp),
                verticalArrangement = Arrangement.spacedBy(RowGap),
                content = content,
            )
        }
    }
}

internal val RowGap = 4.dp

/** One list row plus its gap, in whole pixels as the layout places them. */
@Composable
internal fun rowPitchPx(): Int = with(LocalDensity.current) { Metrics.target.roundToPx() + RowGap.roundToPx() }

/** Rows of [columns] equal cells. */
@Composable
internal fun <T> Grid(items: List<T>, columns: Int, modifier: Modifier = Modifier, cell: @Composable (T, Modifier) -> Unit) {
    Column(modifier, verticalArrangement = Arrangement.spacedBy(RowGap)) {
        items.chunked(columns.coerceAtLeast(1)).forEach { row ->
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                row.forEach { cell(it, Modifier.weight(1f)) }
                repeat(columns - row.size) { Box(Modifier.weight(1f)) }
            }
        }
    }
}

internal fun gridRows(n: Int, columns: Int) = (n + columns - 1) / columns

/** A row-high group label with a gold rule, so a list keeps its row rhythm. */
@Composable
internal fun GroupHeader(text: String, modifier: Modifier = Modifier) {
    Box(
        modifier.fillMaxWidth().height(Metrics.target).drawBehind {
            drawLine(VoLumColors.Divider, Offset(0f, size.height - 1f), Offset(size.width, size.height - 1f), 1f)
        }.padding(horizontal = 6.dp),
        contentAlignment = Alignment.BottomStart,
    ) {
        VText(text.uppercase(), Modifier.padding(bottom = 8.dp), style = VoLumType.label(10.sp, VoLumColors.GoldDim))
    }
}

/** The state-of-the-rig sentence the views share: channel and cab of MAIN, or of SUPPORT. */
internal fun chCab(s: VoLumState, support: Boolean = false): String {
    if (support) {
        val lane = s.rig.support
        val amp = s.library.amp(lane.ampId) ?: return "Empty lane"
        return "CH ${lane.channel + 1}  ·  " + if (lane.cab == Cab.CUSTOM_IR) (s.library.ir(lane.irId)?.name ?: "Custom IR") else cabLabel(lane.cab, amp)
    }
    return "CH ${s.rig.channel + 1}  ·  " + if (s.rig.cab == Cab.CUSTOM_IR) (s.library.ir(s.rig.irId)?.name ?: "Custom IR") else cabLabel(s.rig.cab, s.amp)
}

@Composable
internal fun Scroll(modifier: Modifier, content: @Composable ColumnScope.() -> Unit) {
    val st = rememberScrollState()
    Column(modifier.volumScrollbar(st).verticalScroll(st), verticalArrangement = Arrangement.spacedBy(8.dp), content = content)
}

@Composable
internal fun Spaced(modifier: Modifier = Modifier, gap: Int = 8, content: @Composable RowScope.() -> Unit) {
    Row(modifier, verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(gap.dp), content = content)
}

@Composable
internal fun Stack(modifier: Modifier = Modifier, gap: Int = 6, content: @Composable ColumnScope.() -> Unit) {
    Column(modifier, verticalArrangement = Arrangement.spacedBy(gap.dp), content = content)
}

internal fun Modifier.capped(max: androidx.compose.ui.unit.Dp) = heightIn(max = max)
