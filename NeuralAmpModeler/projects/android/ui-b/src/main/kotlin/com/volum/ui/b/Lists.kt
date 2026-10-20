package com.volum.ui.b

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
import androidx.compose.runtime.compositionLocalOf
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.design.components.volumScrollbar
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumType
import kotlin.math.ceil
import kotlin.math.floor
import kotlin.math.max
import kotlin.math.min

/** The texts of a grid that should all read at one size, the style they start from and the room a cell leaves them. */
internal data class SharedText(val texts: List<String>, val style: TextStyle, val inset: Dp)

/** The one size [FitGrid] picked for its [SharedText], or null outside a grid. */
internal val LocalGridText = compositionLocalOf<TextStyle?> { null }

/**
 * Scrolling catalog whose row height divides the viewport exactly: the rows on
 * screen are always whole, so no list control is ever half clipped at the edge
 * (a clipped row would be a sub-48 dp touch target). Draws the `:design` scrollbar.
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
    shared: SharedText? = null,
    cell: @Composable (T) -> Unit,
) {
    BoxWithConstraints(modifier) {
        if (items.isEmpty() && empty != null) {
            Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                FitText(empty, VoLumType.body(13.sp, VoLumColors.CreamDim), Modifier.fillMaxWidth().padding(16.dp))
            }
            return@BoxWithConstraints
        }
        val h = maxHeight
        val rows = ceil(items.size / columns.toFloat()).toInt()
        val visible = max(1, floor(((h + gap) / (minRow + gap))).toInt())
        val exact = (h - gap * (visible - 1)) / visible
        val rowH = if (rows <= visible) min(exact.value, maxRow.value).dp else exact
        val scroll = rememberScrollState()
        val scrolls = rows > visible
        val bar = if (scrolls) 10.dp else 0.dp
        val cellW = (maxWidth - bar - gap * (columns - 1)) / columns
        val gridText = shared?.let { rememberSharedStyle(it.texts, it.style, cellW - it.inset) }
        CompositionLocalProvider(LocalGridText provides gridText) {
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

@Composable
internal fun SectionTitle(text: String, modifier: Modifier = Modifier) {
    FitText(text, VoLumType.SectionHeader, modifier.height(18.dp), align = TextAlign.Start)
}
