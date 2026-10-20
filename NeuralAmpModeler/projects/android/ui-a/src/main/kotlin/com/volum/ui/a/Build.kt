package com.volum.ui.a

import androidx.compose.animation.Crossfade
import androidx.compose.animation.core.tween
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
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.Param
import com.volum.core.PostBlock
import com.volum.core.PreBlock
import com.volum.core.Section

internal val RowH = 48.dp

@Composable
internal fun BuildSurface(c: Ctx) {
    val s = c.s
    Row(Modifier.fillMaxSize().padding(start = 4.dp, end = 4.dp, bottom = 6.dp)) {
        Meter("IN", c::inLevel, Modifier.fillMaxHeight().ids("meters.in"))
        Column(Modifier.weight(1f).fillMaxHeight().padding(horizontal = 6.dp)) {
            ChainStrip(c)
            Spacer(Modifier.height(8.dp))
            val focus: Any = when (s.ui.section) {
                Section.PRE -> s.ui.preFocus
                Section.AMP -> Section.AMP
                Section.POST -> s.ui.postFocus
            }
            Crossfade(focus, Modifier.weight(1f).fillMaxWidth(), tween(140), label = "focus") { f ->
                when (f) {
                    is PreBlock -> PreEditor(c, f)
                    is PostBlock -> PostEditor(c, f)
                    else -> AmpEditor(c)
                }
            }
        }
        val ids = listOfNotNull("meters.out", "status.output_safety".takeIf { s.outputSafety })
        Meter(if (s.outputSafety) "LIM" else "OUT", c::outLevel, Modifier.fillMaxHeight().ids(ids), alert = s.outputSafety)
    }
}

/** PRE / AMP / POST in signal order, always on screen; tapping a block focuses it in the editor below. */
@Composable
private fun ChainStrip(c: Ctx) {
    val s = c.s
    Row(Modifier.fillMaxWidth().height(72.dp), horizontalArrangement = Arrangement.spacedBy(4.dp), verticalAlignment = Alignment.CenterVertically) {
        SectionTab("PRE", s.ui.section == Section.PRE, s.locks.pre != null, "nav.section.pre") { c.d(Action.SetSection(Section.PRE)) }
        PreBlock.entries.forEach { b ->
            BlockCard(preCard(b, s), s.on(b.onParam), s.ui.section == Section.PRE && s.ui.preFocus == b, s.ui.section == Section.PRE, Texture.PRE,
                "pre.card.${b.key}", Modifier.weight(1f)) { c.d(Action.FocusPre(b)) }
        }
        Tap({ c.d(Action.SetSection(Section.AMP)) }, Modifier.weight(1.7f).fillMaxHeight(), listOf("nav.section.amp"), selected = s.ui.section == Section.AMP) {
            Column(Modifier.fillMaxWidth(), horizontalAlignment = Alignment.CenterHorizontally) {
                T(s.amp?.name ?: "No amp", Type.display(15.sp))
                T(ampSummary(c), Type.label(10.sp))
            }
        }
        PostBlock.entries.forEach { b ->
            BlockCard(b.name, s.on(b.onParam), s.ui.section == Section.POST && s.ui.postFocus == b, s.ui.section == Section.POST, Texture.POST,
                "post.card.${b.key}", Modifier.weight(1f)) { c.d(Action.FocusPost(b)) }
        }
        SectionTab("POST", s.ui.section == Section.POST, s.locks.post != null, "nav.section.post") { c.d(Action.SetSection(Section.POST)) }
    }
}

private fun ampSummary(c: Ctx): String {
    val s = c.s
    val base = "CH ${s.rig.channel + 1} · ${cabLabel(s.amp, s.rig.cab)}"
    val support = s.library.amp(s.rig.support.ampId)?.takeIf { s.dual }?.name
    return if (support != null) "$base · + ${support.uppercase()}" else base
}

@Composable
private fun SectionTab(label: String, selected: Boolean, locked: Boolean, id: String, onClick: () -> Unit) {
    Tap(onClick, Modifier.width(52.dp).fillMaxHeight(), listOf(id), selected = selected) {
        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            T(label, Type.label(12.sp, if (selected) Ink.selected else Ink.dim))
            if (locked) T("LOCK", Type.label(9.sp, Ink.text))
        }
    }
}

@Composable
private fun BlockCard(label: String, on: Boolean, focused: Boolean, inSection: Boolean, texture: Texture, id: String, modifier: Modifier, onClick: () -> Unit) {
    Tap(onClick, modifier.fillMaxHeight(), listOf(id), selected = focused, pick = Pick.AMBER) {
        Frame(Modifier.fillMaxSize().padding(vertical = 4.dp), texture) {
            Led(on, Modifier.align(Alignment.TopEnd).padding(6.dp).size(9.dp))
            T(label, Type.label(10.sp, if (focused) Ink.selected else if (inSection) Ink.text else Ink.dim), Modifier.align(Alignment.BottomStart).padding(5.dp))
        }
    }
}

/** Shared editor shape: the focused block's art and ON switch on the left, its controls on the right. */
@Composable
internal fun Editor(
    art: String,
    c: Ctx,
    texture: Texture,
    artIds: List<String> = emptyList(),
    left: @Composable ColumnScope.() -> Unit,
    right: @Composable ColumnScope.() -> Unit,
) {
    Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
        Column(Modifier.width(196.dp).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Frame(Modifier.weight(1f).fillMaxWidth(), texture, framed = true) {
                ArtSlot(art, c::energy, Modifier.fillMaxSize().padding(4.dp).ids(artIds))
            }
            left()
        }
        Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(6.dp), content = right)
    }
}

@Composable
internal fun ControlRow(modifier: Modifier = Modifier, content: @Composable RowScope.() -> Unit) =
    Row(modifier.fillMaxWidth().height(RowH), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp), content = content)

/** The knob row of a block in its current mode; [slot] lets a mode swap one knob for another control in place. */
@Composable
internal fun KnobRow(c: Ctx, params: List<Param>, modifier: Modifier = Modifier, slot: (@Composable () -> Unit)? = null) {
    Row(modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(2.dp, Alignment.Start), verticalAlignment = Alignment.Top) {
        slot?.invoke()
        params.forEach { p ->
            Knob(
                label = if (p == Param.DELAY_AGE) com.volum.core.KnobRows.delayCharacterLabel(c.s) else p.label,
                value = c.s.param(p),
                min = p.min,
                max = p.max,
                display = fmt(p, c.s.param(p)),
                onSet = { c.d(Action.SetParam(p, it)) },
                onReset = { c.d(Action.ResetParam(p)) },
                ids = knobIds(p),
            )
        }
    }
}

/** Mutually exclusive options of an enum param, drawn with the shared selection. */
@Composable
internal fun ModePicker(c: Ctx, p: Param, ids: List<String?>, modifier: Modifier = Modifier) {
    Row(modifier, horizontalArrangement = Arrangement.spacedBy(4.dp)) {
        p.options.forEachIndexed { i, label ->
            Chip(label, { c.d(Action.SetParam(p, i.toDouble())) }, ids = listOfNotNull(ids.getOrNull(i)), selected = c.s.param(p).toInt() == i, size = 11.sp)
        }
    }
}

@Composable
internal fun LockControls(c: Ctx, section: Section) {
    val locked = (if (section == Section.PRE) c.s.locks.pre else c.s.locks.post) != null
    val key = if (section == Section.PRE) "pre" else "post"
    if (locked && com.volum.core.Reducer.lockDiffers(c.s, section)) {
        Chip("STORE", { c.d(Action.StoreLock(section)) }, ids = listOf("$key.store"))
    }
    Chip(if (locked) "LOCKED" else "LOCK", { c.d(Action.SetLock(section, !locked)) }, ids = listOf("$key.lock"), selected = locked, pick = Pick.BRASS)
}
