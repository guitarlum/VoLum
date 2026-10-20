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
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.drawWithContent
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.ceil
import kotlin.math.floor
import kotlin.math.max
import kotlin.math.min

/**
 * Scrolling catalog whose row height divides the viewport exactly: the rows on
 * screen are always whole, so no list control is ever half clipped at the edge
 * (a clipped row would be a sub-48 dp touch target).
 */
@Composable
internal fun <T> FitGrid(
    items: List<T>,
    columns: Int,
    minRow: Dp,
    modifier: Modifier = Modifier,
    gap: Dp = 8.dp,
    maxRow: Dp = minRow * 1.4f,
    empty: String? = null,
    cell: @Composable (T) -> Unit,
) {
    BoxWithConstraints(modifier) {
        if (items.isEmpty() && empty != null) {
            Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                FitText(empty, Type.value(14.sp, Ink.dim), Modifier.fillMaxWidth().padding(16.dp))
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
        Column(
            Modifier.fillMaxSize()
                .verticalScroll(scroll)
                .padding(end = if (scrolls) 8.dp else 0.dp),
            verticalArrangement = Arrangement.spacedBy(gap),
        ) {
            items.chunked(columns).forEach { row ->
                Row(Modifier.fillMaxWidth().height(rowH), horizontalArrangement = Arrangement.spacedBy(gap)) {
                    row.forEach { Box(Modifier.weight(1f).fillMaxHeight()) { cell(it) } }
                    repeat(columns - row.size) { Box(Modifier.weight(1f)) }
                }
            }
        }
        if (scrolls) {
            Box(Modifier.fillMaxSize().drawWithContent {
                val total = scroll.maxValue + size.height
                val thumb = size.height * size.height / total
                val top = (size.height - thumb) * scroll.value / scroll.maxValue.coerceAtLeast(1)
                drawRect(Ink.line, Offset(size.width - 3.dp.toPx(), 0f), Size(3.dp.toPx(), size.height))
                drawRect(Ink.text, Offset(size.width - 3.dp.toPx(), top), Size(3.dp.toPx(), thumb))
            })
        }
    }
}

@Composable
internal fun SectionTitle(text: String, modifier: Modifier = Modifier) {
    FitText(text, Type.label(12.sp, Ink.text), modifier.height(24.dp), align = TextAlign.Start)
}
