package com.volum.ui.b

import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsPressedAsState
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.compositionLocalOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.drawWithCache
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.PathEffect
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.design.components.VoLumGlyph
import com.volum.design.components.VoLumIcon
import com.volum.design.components.volumScrollbar
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumType
import com.volum.design.theme.withOpacity
import kotlin.math.ceil
import kotlin.math.max
import kotlin.math.min

/** How many rows a catalog shows whole and how tall each is, in whole pixels. */
internal data class GridRows(val visible: Int, val rowPx: Int)

/**
 * Rows of at least [minRowPx] that fill [heightPx] exactly with [gapPx] between them; a
 * list that fits uses at most [maxRowPx]. Counted in whole pixels, the size the rows are
 * laid out at, so the last visible row's frame always lands inside the viewport.
 */
internal fun gridRows(heightPx: Int, gapPx: Int, minRowPx: Int, maxRowPx: Int, rows: Int): GridRows {
    val visible = max(1, (heightPx + gapPx) / (minRowPx + gapPx))
    val exact = (heightPx - gapPx * (visible - 1)) / visible
    return GridRows(visible, if (rows <= visible) min(exact, maxRowPx) else exact)
}

/** The fit group a [FitGrid] gives its cells, so every name in one grid reads at one size; null outside a grid. */
internal val LocalGridGroup = compositionLocalOf<FitGroup?> { null }

/**
 * Scrolling catalog whose row height divides the viewport exactly: the rows on
 * screen are always whole, so no list control is ever half clipped at the edge
 * (a clipped row would be a sub-48 dp touch target). Draws the `:design` scrollbar.
 * [reveal] is the item that must be on screen when the catalog opens (the current one).
 */
@Composable
internal fun <T> FitGrid(
    items: List<T>,
    columns: Int,
    minRow: Dp,
    modifier: Modifier = Modifier,
    gap: Dp = 6.dp,
    maxRow: Dp = minRow * 1.4f,
    empty: String? = null,
    reveal: Int? = null,
    cell: @Composable (T) -> Unit,
) {
    BoxWithConstraints(modifier) {
        if (items.isEmpty() && empty != null) {
            Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                FitText(empty, hint(size = 13.sp), Modifier.fillMaxWidth().height(24.dp).padding(horizontal = 16.dp))
            }
            return@BoxWithConstraints
        }
        val density = LocalDensity.current
        val rows = ceil(items.size / columns.toFloat()).toInt()
        val gapPx = with(density) { gap.roundToPx() }
        val fit = with(density) { gridRows(constraints.maxHeight, gapPx, minRow.roundToPx(), maxRow.roundToPx(), rows) }
        val rowH = with(density) { fit.rowPx.toDp() }
        val scroll = rememberScrollState()
        val scrolls = rows > fit.visible
        val bar = if (scrolls) 10.dp else 0.dp
        if (scrolls && reveal != null && reveal >= 0) {
            val firstRow = (reveal / columns - fit.visible + 1).coerceAtLeast(0)
            LaunchedEffect(Unit) { scroll.scrollTo((fit.rowPx + gapPx) * firstRow) }
        }
        CompositionLocalProvider(LocalGridGroup provides rememberFitGroup()) {
            Column(
                Modifier.fillMaxSize()
                    .then(if (scrolls) Modifier.volumScrollbar(scroll) else Modifier)
                    .verticalScroll(scroll)
                    .padding(end = bar),
                verticalArrangement = Arrangement.spacedBy(gap),
            ) {
                items.chunked(columns).forEach { row ->
                    Row(Modifier.fillMaxWidth().height(rowH), horizontalArrangement = Arrangement.spacedBy(gap)) {
                        row.forEach { Box(Modifier.weight(1f).fillMaxHeight()) { cell(it) } }
                        repeat(columns - row.size) { Box(Modifier.weight(1f)) }
                    }
                }
            }
        }
    }
}

/** The desktop's dashed "+ add" cell outline, gold and filled while pressed. */
internal fun Modifier.dashedAddFrame(pressed: Boolean): Modifier = drawWithCache {
    val px = 1.dp.toPx()
    val dash = Stroke(1.2f * px, pathEffect = PathEffect.dashPathEffect(floatArrayOf(5 * px, 4 * px)))
    val cr = CornerRadius(4.dp.toPx())
    onDrawBehind {
        if (pressed) drawRoundRect(VoLumColors.SelBg, cornerRadius = cr)
        drawRoundRect(if (pressed) VoLumColors.Gold else VoLumColors.GoldDim.withOpacity(0.55f), Offset(px, px), Size(size.width - 2 * px, size.height - 2 * px), cr, style = dash)
    }
}

/** A dashed add cell at the end of a catalog: the glyph and what it adds; [ids] go on its one clickable. */
@Composable
internal fun AddCell(label: String, ids: Array<out String>, onClick: () -> Unit, modifier: Modifier = Modifier) {
    val source = remember { MutableInteractionSource() }
    val pressed by source.collectIsPressedAsState()
    Row(
        modifier.clickable(source, null, role = Role.Button, onClick = onClick).dashedAddFrame(pressed).ids(ids).padding(horizontal = 14.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(8.dp, Alignment.CenterHorizontally),
    ) {
        VoLumIcon(VoLumGlyph.Plus, color = VoLumColors.Gold, size = 14.dp)
        FitText(label, VoLumType.label(12.sp, VoLumColors.Gold), Modifier.weight(1f, fill = false).height(18.dp))
    }
}

@Composable
internal fun SectionTitle(text: String, modifier: Modifier = Modifier) {
    FitText(text, VoLumType.SectionHeader, modifier.height(18.dp), align = TextAlign.Start)
}
