package com.volum.ui.c

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInVertically
import androidx.compose.animation.slideOutVertically
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.ScrollState
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxScope
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicText
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.graphics.Shape
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import com.volum.core.Action
import com.volum.core.Amp
import com.volum.core.BannerKind
import com.volum.core.Cab
import com.volum.core.Param
import com.volum.core.Telemetry
import com.volum.core.VoLumState
import kotlinx.coroutines.flow.StateFlow

/** A surface whose rim breathes with the signal while [on]; telemetry is read in the draw phase only. */
@Composable
internal fun Glowing(tel: StateFlow<Telemetry>, on: Boolean, modifier: Modifier = Modifier, selected: Boolean = false, shape: Shape = Corner, content: @Composable BoxScope.() -> Unit) {
    val t by tel.collectAsState()
    Box(
        modifier
            .clip(shape)
            .background(if (selected) Look.glassHi else Look.glass)
            .drawBehind {
                val e = if (on) t.energy.toFloat() else 0f
                val r = CornerRadius(10.dp.toPx())
                val w = 1.dp.toPx()
                drawRoundRect(if (selected) Look.pick else Look.line, cornerRadius = r, style = Stroke(w))
                if (e > 0f) drawRoundRect(Look.glow.copy(alpha = 0.15f + 0.6f * e), cornerRadius = r, style = Stroke(w + 2.dp.toPx() * e))
            },
        content = content,
    )
}

internal fun captureName(ui: Ui, p: Param): String {
    val idx = ui.s.param(p).toInt()
    return ui.s.library.pedals.firstOrNull { it.index == idx }?.name ?: "EMPTY"
}

internal fun cabLabel(s: VoLumState, cab: Cab, amp: Amp?): String = when {
    cab == Cab.CUSTOM_IR -> "CUSTOM IR"
    amp?.custom == true -> amp.cabNames.getOrNull(cab.ordinal) ?: cab.label
    else -> cab.label.uppercase()
}

/**
 * Every panel rises to the same line under the top bar and the block row. Its header
 * echoes the control it was opened from ([trigger]), so that control is never hidden.
 */
@Composable
internal fun PanelFrame(
    title: String,
    trigger: String?,
    closeId: String?,
    onClose: () -> Unit,
    modifier: Modifier = Modifier,
    actions: @Composable RowScope.() -> Unit = {},
    lead: @Composable RowScope.() -> Unit = {},
    content: @Composable ColumnScope.() -> Unit,
) {
    val shape = RoundedCornerShape(topStart = 18.dp, topEnd = 18.dp)
    Column(
        modifier
            .clip(shape)
            .background(Look.panel)
            .border(1.dp, Look.lineHi, shape)
            .padding(horizontal = 10.dp, vertical = 6.dp),
        verticalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        Row(Modifier.fillMaxWidth().height(Metrics.target), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            if (trigger != null) {
                Box(Modifier.widthIn(max = 240.dp).clip(Pill).background(Look.pick).padding(horizontal = 12.dp, vertical = 6.dp)) {
                    BasicText(trigger, style = labelStyle(10f, Look.pickInk), maxLines = 1, overflow = TextOverflow.Ellipsis)
                }
                BasicText("›", style = labelStyle(14f))
            }
            lead()
            BasicText(title.uppercase(), style = labelStyle(13f, Look.text), maxLines = 1, overflow = TextOverflow.Ellipsis, modifier = Modifier.weight(1f))
            actions()
            if (closeId != null) Btn("CLOSE", listOfNotNull(closeId.takeIf { it.isNotEmpty() }), onClose)
        }
        Column(Modifier.fillMaxWidth().weight(1f), content = content)
    }
}

/** A 48 dp list row in the list-selection style. */
@Composable
internal fun ListRow(
    label: String,
    ids: List<String>,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    sub: String? = null,
    selected: Boolean = false,
    enabled: Boolean = true,
) {
    val src = remember { MutableInteractionSource() }
    val scale = pressScale(src)
    Box(
        modifier
            .height(Metrics.target)
            .graphicsLayer { scaleX = scale; scaleY = scale }
            .clip(Corner)
            .background(if (selected) Look.listInk.copy(alpha = 0.22f) else Look.raised)
            .border(1.dp, if (selected) Look.listInk else Look.line, Corner)
            .clickable(src, null, enabled = enabled, role = Role.Button, onClick = onClick)
            .ids(ids)
            .padding(horizontal = 10.dp),
        contentAlignment = Alignment.CenterStart,
    ) {
        Column {
            BasicText(label, style = bodyStyle(13f, if (enabled) Look.text else Look.faint, cap = 1.2f), maxLines = 1, overflow = TextOverflow.Ellipsis)
            if (sub != null) BasicText(sub, style = labelStyle(8f, Look.dim, bold = false), maxLines = 1, overflow = TextOverflow.Ellipsis)
        }
    }
}

/**
 * A scrolling list of 48 dp rows whose viewport is cut to a whole number of rows, so
 * at rest no row is sliced by the edge into something too small to hit.
 */
@Composable
internal fun SnapList(modifier: Modifier, state: ScrollState = rememberScrollState(), inner: Modifier = Modifier, content: @Composable ColumnScope.() -> Unit) {
    BoxWithConstraints(modifier) {
        val density = LocalDensity.current
        val gap = with(density) { RowGap.roundToPx() }
        val pitch = rowPitchPx()
        val rows = ((constraints.maxHeight + gap) / pitch).coerceAtLeast(1)
        val height = with(density) { (rows * pitch - gap + 2).toDp() }
        Column(Modifier.fillMaxWidth().height(height).verticalScroll(state).then(inner), verticalArrangement = Arrangement.spacedBy(RowGap), content = content)
    }
}

internal val RowGap = 4.dp

/** One list row plus its gap, in whole pixels as the layout places them. */
@Composable
internal fun rowPitchPx(): Int = with(LocalDensity.current) { Metrics.target.roundToPx() + RowGap.roundToPx() }

/** Rows of [columns] equal cells. */
@Composable
internal fun <T> Grid(items: List<T>, columns: Int, modifier: Modifier = Modifier, cell: @Composable (T, Modifier) -> Unit) {
    Column(modifier, verticalArrangement = Arrangement.spacedBy(4.dp)) {
        items.chunked(columns.coerceAtLeast(1)).forEach { row ->
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                row.forEach { cell(it, Modifier.weight(1f)) }
                repeat(columns - row.size) { Box(Modifier.weight(1f)) }
            }
        }
    }
}

/** Load / import errors and info notices, under the top bar, until dismissed. */
@Composable
internal fun BannerStrip(ui: Ui) {
    val banner = ui.s.ui.banner
    Box(Modifier.fillMaxSize().padding(top = Metrics.top + 6.dp), contentAlignment = Alignment.TopCenter) {
        AnimatedVisibility(banner != null, enter = slideInVertically { -it } + fadeIn(), exit = slideOutVertically { -it } + fadeOut()) {
            val b = banner ?: return@AnimatedVisibility
            val error = b.kind == BannerKind.ERROR
            Row(
                Modifier
                    .clip(Corner)
                    .background(if (error) Look.danger else Look.glassHi)
                    .border(1.dp, if (error) Look.danger else Look.lineHi, Corner)
                    .padding(start = 14.dp, end = 4.dp, top = 4.dp, bottom = 4.dp)
                    .then(if (error) Modifier.ids("banner.error") else Modifier),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(10.dp),
            ) {
                BasicText(if (error) "!" else "✓", style = labelStyle(14f, Look.text))
                BasicText(b.message, style = bodyStyle(13f, Look.text, cap = 1.15f), maxLines = 2, overflow = TextOverflow.Ellipsis, modifier = Modifier.weight(1f, fill = false))
                Btn("DISMISS", listOf("banner.dismiss"), { ui.d(Action.DismissBanner) })
            }
        }
    }
}

/** A tappable plain container used by blocks that navigate. */
@Composable
internal fun Tappable(onClick: () -> Unit, modifier: Modifier = Modifier, ids: List<String> = emptyList(), role: Role = Role.Button, content: @Composable BoxScope.() -> Unit) {
    val src = remember { MutableInteractionSource() }
    val scale = pressScale(src)
    Box(modifier.graphicsLayer { scaleX = scale; scaleY = scale }.clickable(src, null, role = role, onClick = onClick).ids(ids), content = content)
}
