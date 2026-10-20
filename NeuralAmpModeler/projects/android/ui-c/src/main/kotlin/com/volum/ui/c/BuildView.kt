package com.volum.ui.c

import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInHorizontally
import androidx.compose.animation.slideOutHorizontally
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.background
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.selection.selectable
import androidx.compose.foundation.text.BasicText
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import com.volum.core.Action
import com.volum.core.Param
import com.volum.core.Reducer
import com.volum.core.ScreenId
import com.volum.core.Section
import com.volum.harness.screen

@Composable
internal fun BuildView(ui: Ui) {
    val s = ui.s
    val screen = when (s.ui.section) {
        Section.PRE -> ScreenId.BUILD_PRE
        Section.AMP -> ScreenId.BUILD_AMP
        Section.POST -> ScreenId.BUILD_POST
    }
    Column(Modifier.fillMaxSize().padding(top = Metrics.top).screen(screen)) {
        BlockBand(ui)
        AnimatedContent(
            s.ui.section,
            transitionSpec = {
                val dir = if (targetState.ordinal > initialState.ordinal) 1 else -1
                (slideInHorizontally(tween(240)) { it / 6 * dir } + fadeIn(tween(240))) togetherWith (slideOutHorizontally(tween(180)) { -it / 6 * dir } + fadeOut(tween(160)))
            },
            label = "section",
            modifier = Modifier.weight(1f).fillMaxWidth(),
        ) { section ->
            val frame = Modifier.fillMaxSize().padding(horizontal = 8.dp)
            when (section) {
                Section.PRE -> PreSheet(ui, frame)
                Section.AMP -> AmpSheet(ui, frame)
                Section.POST -> PostSheet(ui, frame)
            }
        }
    }
}

/** IN meter, the three floating blocks, OUT meter. The selected block points down into its sheet. */
@Composable
private fun BlockBand(ui: Ui) {
    val s = ui.s
    Row(
        Modifier.fillMaxWidth().height(Metrics.band).padding(horizontal = 8.dp, vertical = 6.dp),
        horizontalArrangement = Arrangement.spacedBy(8.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        InMeter(ui)
        val pre = listOf(Param.PITCH_ON to "P", Param.COMP_ON to "C", Param.NAM1_ON to "N1", Param.NAM2_ON to "N2")
        val post = listOf(Param.CHORUS_ON to "CH", Param.DELAY_ON to "DL", Param.REVERB_ON to "RV", Param.TREM_ON to "TR")
        ChainBlock(ui, Section.PRE, "nav.section.pre", Modifier.weight(1f)) {
            BlockTitle("PRE", if (s.locks.pre != null) "LOCKED" else null)
            Leds(ui, pre)
        }
        ChainBlock(ui, Section.AMP, "nav.section.amp", Modifier.weight(1.5f)) {
            BasicText(s.amp?.name ?: "No amp", style = displayStyle(19f).copy(textAlign = TextAlign.Center), maxLines = 1, overflow = TextOverflow.Ellipsis, modifier = Modifier.ids("amp.name"))
            val support = if (s.dual) "  +  " + (s.library.amp(s.rig.support.ampId)?.name ?: "SUPPORT EMPTY") else ""
            BasicText("CH ${s.rig.channel + 1} · ${cabLabel(s, s.rig.cab, s.amp)}$support", style = labelStyle(9f), maxLines = 1, overflow = TextOverflow.Ellipsis)
        }
        ChainBlock(ui, Section.POST, "nav.section.post", Modifier.weight(1f)) {
            BlockTitle("POST", if (s.locks.post != null) "LOCKED" else null)
            Leds(ui, post)
        }
        OutMeter(ui)
    }
}

@Composable
private fun BlockTitle(title: String, badge: String?) {
    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
        BasicText(title, style = labelStyle(12f, Look.text))
        if (badge != null) BasicText(badge, style = labelStyle(8f, Look.warn))
    }
}

@Composable
private fun Leds(ui: Ui, items: List<Pair<Param, String>>) {
    Row(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.CenterVertically) {
        items.forEach { (p, l) ->
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(3.dp)) {
                Led(ui.s.on(p), 7.dp)
                BasicText(l, style = labelStyle(8f, if (ui.s.on(p)) Look.text else Look.faint, cap = 1f))
            }
        }
    }
}

@Composable
private fun ChainBlock(ui: Ui, section: Section, id: String, modifier: Modifier, content: @Composable () -> Unit) {
    val selected = ui.s.ui.section == section
    val src = remember { MutableInteractionSource() }
    val scale = pressScale(src)
    val engaged = when (section) {
        Section.PRE -> listOf(Param.PITCH_ON, Param.COMP_ON, Param.NAM1_ON, Param.NAM2_ON).any { ui.s.on(it) }
        Section.AMP -> true
        Section.POST -> listOf(Param.CHORUS_ON, Param.DELAY_ON, Param.REVERB_ON, Param.TREM_ON).any { ui.s.on(it) }
    }
    Box(
        modifier
            .fillMaxHeight()
            .graphicsLayer { scaleX = scale; scaleY = scale }
            .drawBehind {
                if (selected) {
                    val w = 14.dp.toPx()
                    val p = Path().apply {
                        moveTo(size.width / 2 - w, size.height)
                        lineTo(size.width / 2, size.height + 6.dp.toPx())
                        lineTo(size.width / 2 + w, size.height)
                        close()
                    }
                    drawPath(p, Look.pick)
                }
            },
    ) {
        Glowing(ui.tel, engaged, Modifier.fillMaxSize(), selected = selected) {
            Column(
                Modifier
                    .fillMaxSize()
                    .selectable(selected, src, null, role = Role.Tab) { ui.d(Action.SetSection(section)) }
                    .ids(id)
                    .brackets(if (selected) Look.pick else Look.metal)
                    .padding(horizontal = 10.dp, vertical = 4.dp),
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.Center,
            ) { content() }
        }
    }
}

@Composable
private fun InMeter(ui: Ui) {
    val gateOn = ui.s.on(Param.GATE_ON)
    val threshold = ui.s.param(Param.GATE)
    Row(Modifier.fillMaxHeight().ids("meters.in"), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(4.dp)) {
        Column(horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(4.dp)) {
            Caption("In")
            Lamp(ui.tel, Look.warn) { gateOn && it.inDb < threshold }
            BasicText("GATE", style = labelStyle(7f, Look.faint, cap = 1f))
        }
        Meter(ui.tel, Modifier.width(8.dp).fillMaxHeight()) { it.inDb }
    }
}

@Composable
private fun OutMeter(ui: Ui) {
    Row(Modifier.fillMaxHeight(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(4.dp)) {
        Row(Modifier.fillMaxHeight().ids("meters.out"), horizontalArrangement = Arrangement.spacedBy(2.dp)) {
            Meter(ui.tel, Modifier.width(7.dp).fillMaxHeight()) { it.outDb }
            Meter(ui.tel, Modifier.width(7.dp).fillMaxHeight()) { it.outRightDb }
        }
        Column(horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(4.dp)) {
            Caption("Out")
            if (ui.s.outputSafety) {
                Box(Modifier.background(Look.warn, Pill).padding(horizontal = 6.dp, vertical = 2.dp).ids("status.output_safety")) {
                    BasicText("SAFETY", style = labelStyle(8f, Look.pickInk, cap = 1f))
                }
            } else {
                Lamp(ui.tel, Look.danger) { it.outDb > -1.0 }
                BasicText("CLIP", style = labelStyle(7f, Look.faint, cap = 1f))
            }
        }
    }
}

/** Header shared by the PRE and POST sheets: the lock that carries them across amps, and Store when it differs. */
@Composable
internal fun LockControls(ui: Ui, section: Section, lockId: String, storeId: String) {
    val locked = (if (section == Section.PRE) ui.s.locks.pre else ui.s.locks.post) != null
    if (locked && Reducer.lockDiffers(ui.s, section)) {
        Btn("STORE TO AMP", listOf(storeId), { ui.d(Action.StoreLock(section)) }, sub = "differs from this amp")
    }
    Switch(if (locked) "LOCKED" else "LOCK", locked, listOf(lockId), { ui.d(Action.SetLock(section, !locked)) })
}

/** The section sheet: the art stays visible above it, the selected block points into it. */
@Composable
internal fun SectionSheet(title: String, modifier: Modifier, actions: @Composable androidx.compose.foundation.layout.RowScope.() -> Unit = {}, content: @Composable androidx.compose.foundation.layout.ColumnScope.() -> Unit) {
    PanelFrame(title, null, null, {}, modifier.padding(top = 4.dp), actions = actions, content = content)
}
