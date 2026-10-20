package com.volum.design.components

import androidx.compose.foundation.ScrollState
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsPressedAsState
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.lazy.LazyListState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.drawWithCache
import androidx.compose.ui.draw.drawWithContent
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.design.art.AmpThumbnail
import com.volum.design.art.ArtRef
import com.volum.design.theme.VFitText
import com.volum.design.theme.VText
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumType
import com.volum.design.theme.ic

/**
 * A list row with a thumbnail (`VoLumAmpListControl` rows, list menus): the
 * ListTeal selection with the gold edge bar on the selected row, a framed 22 dp
 * art thumbnail, the name auto-shrinking 13 -> 10.5 sp before it clips.
 */
@Composable
fun VoLumListRow(
    title: String,
    selected: Boolean,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    art: ArtRef? = null,
    subtitle: String? = null,
    style: VoLumSelectionStyle = VoLumSelectionStyle.ListTeal,
    enabled: Boolean = true,
    trailing: (@Composable RowScope.() -> Unit)? = null,
) {
    val source = remember { MutableInteractionSource() }
    val pressed by source.collectIsPressedAsState()
    Row(
        modifier
            .heightIn(min = 48.dp)
            .clickable(source, indication = null, enabled = enabled, role = Role.Button, onClick = onClick)
            .padding(horizontal = 2.dp, vertical = 0.5.dp)
            .volumSelection(selected, pressed, style, radius = 4.dp, inset = 0.dp)
            .drawWithCache {
                val bar = 2.5.dp.toPx()
                val inset = 3.dp.toPx()
                onDrawBehind { if (selected && style == VoLumSelectionStyle.ListTeal) drawRect(VoLumColors.Gold, Offset(0f, inset), Size(bar, size.height - 2 * inset)) }
            }
            .padding(start = 8.dp, end = 6.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        if (art != null) AmpThumbnail(art, Modifier.size(22.dp), selected)
        Column(Modifier.weight(1f)) {
            VFitText(title, style = VoLumType.label(13.sp, if (selected) VoLumColors.TextBright else VoLumColors.TextMed), minSize = 10.5.sp)
            if (subtitle != null) VText(subtitle, style = VoLumType.label(9.sp, VoLumColors.TealDim))
        }
        trailing?.invoke(this)
    }
}

/** A section caption inside a list ("CUSTOM", "SOUNDS"): Josefin Bold 9, cream-dim. */
@Composable
fun VoLumListCaption(text: String, modifier: Modifier = Modifier) {
    Box(modifier.padding(horizontal = 10.dp, vertical = 4.dp)) { VText(text, style = VoLumType.Caption) }
}

/** `DrawVoLumScrollbar` on a [ScrollState]: faint brass track, gold thumb, 1 dp in from the right edge. */
fun Modifier.volumScrollbar(state: ScrollState): Modifier = drawWithContent {
    drawContent()
    val viewport = size.height
    val total = viewport + state.maxValue
    if (state.maxValue <= 0) return@drawWithContent
    drawBar(state.value.toFloat() / total, viewport / total, state.isScrollInProgress)
}

/** `DrawVoLumScrollbar` on a [LazyListState] (rows of similar height). */
fun Modifier.volumScrollbar(state: LazyListState): Modifier = drawWithContent {
    drawContent()
    val info = state.layoutInfo
    val count = info.totalItemsCount
    val visible = info.visibleItemsInfo
    if (count == 0 || visible.isEmpty() || visible.size >= count) return@drawWithContent
    var sum = 0
    for (i in visible.indices) sum += visible[i].size
    val itemH = sum.toFloat() / visible.size
    val total = itemH * count
    val top = state.firstVisibleItemIndex * itemH + state.firstVisibleItemScrollOffset
    drawBar(top / total, size.height / total, state.isScrollInProgress)
}

private fun androidx.compose.ui.graphics.drawscope.ContentDrawScope.drawBar(start: Float, fraction: Float, dragging: Boolean) {
    val w = 6.dp.toPx()
    val px = 1.dp.toPx()
    val l = size.width - w + px
    val trackH = size.height - 2 * px
    drawRect(ic(40, 200, 162, 78), Offset(l, px), Size(w - 2 * px, trackH))
    val thumbH = (trackH * fraction).coerceAtLeast(18.dp.toPx())
    val y = px + (trackH - thumbH) * (start / (1f - fraction).coerceAtLeast(1e-3f)).coerceIn(0f, 1f)
    drawRect(if (dragging) VoLumColors.Gold else VoLumColors.GoldDim, Offset(l, y), Size(w - 2 * px, thumbH))
}
