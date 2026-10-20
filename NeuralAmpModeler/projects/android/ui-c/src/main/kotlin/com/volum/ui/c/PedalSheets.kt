package com.volum.ui.c

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
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
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import com.volum.core.Action
import com.volum.core.DIVISIONS
import com.volum.core.KnobRows
import com.volum.core.Overlay
import com.volum.core.Param
import com.volum.core.PostBlock
import com.volum.core.PreBlock
import com.volum.core.Section

/** One pedal tab: the body focuses the pedal, the round button beside it is its power. */
internal class Tab(
    val title: String,
    val sub: String,
    val power: Param,
    val focusId: String,
    val powerId: String,
    val selected: Boolean,
    val focus: Action,
    val pickId: String? = null,
    val pick: Action? = null,
)

@Composable
private fun PedalTabs(ui: Ui, tabs: List<Tab>) {
    Row(Modifier.fillMaxWidth().height(52.dp), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
        tabs.forEach { t ->
            val on = ui.s.on(t.power)
            Glowing(ui.tel, on, Modifier.weight(1f).fillMaxHeight(), selected = t.selected) {
                Row(Modifier.fillMaxHeight().padding(end = 4.dp), verticalAlignment = Alignment.CenterVertically) {
                    val src = remember { MutableInteractionSource() }
                    val scale = pressScale(src)
                    Column(
                        Modifier
                            .weight(1f)
                            .fillMaxHeight()
                            .graphicsLayer { scaleX = scale; scaleY = scale }
                            .selectable(t.selected, src, null, role = Role.Tab) { ui.d(t.focus) }
                            .ids(t.focusId)
                            .padding(horizontal = 10.dp),
                        verticalArrangement = Arrangement.Center,
                    ) {
                        BasicText(t.title, style = labelStyle(12f, if (t.selected) Look.text else Look.dim), maxLines = 1)
                        BasicText(t.sub, style = labelStyle(9f, Look.dim, bold = false), maxLines = 1, overflow = TextOverflow.Ellipsis)
                    }
                    if (t.pickId != null && t.pick != null) Btn("▾", listOf(t.pickId), { ui.d(t.pick) }, minWidth = 48.dp)
                    Gap(w = 4.dp)
                    Power(on, listOf(t.powerId), { ui.d(Action.ToggleParam(t.power)) }, "${t.title} on")
                }
            }
        }
    }
}

@Composable
private fun ModeGroup(ui: Ui, title: String, p: Param, ids: List<String?>) {
    Group(title) {
        p.options.forEachIndexed { i, label ->
            Choice(label, ui.s.param(p).toInt() == i, listOfNotNull(ids.getOrNull(i)), { ui.d(Action.SetParam(p, i.toDouble())) })
        }
    }
}

@Composable
private fun KnobLine(ui: Ui, params: List<Param>, ids: (Param) -> List<String>, label: (Param) -> String = { it.label }, extra: @Composable () -> Unit = {}) {
    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceEvenly, verticalAlignment = Alignment.CenterVertically) {
        params.forEach { ParamKnob(ui, it, ids(it), label(it)) }
        extra()
    }
}

@Composable
private fun DivisionGrid(ui: Ui, p: Param, id: String) {
    Row(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.CenterVertically) {
        Column {
            Caption("Division")
            BasicText("♩ = %.0f".format(ui.s.metronome.bpm), style = techStyle(9f, Look.dim))
        }
        Column(verticalArrangement = Arrangement.spacedBy(4.dp)) { DIVISIONS.chunked(4).forEachIndexed { row, items ->
            Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                items.forEachIndexed { col, label ->
                    val i = row * 4 + col
                    Choice(label, ui.s.param(p).toInt() == i, listOf(id), { ui.d(Action.SetParam(p, i.toDouble())) }, minWidth = 52.dp)
                }
            }
        } }
    }
}

private val preKnobIds = mapOf(
    Param.PITCH_SEMI to "pre.pitch.semi", Param.PITCH_MIX to "pre.pitch.mix",
    Param.PITCH_OCT_DOWN to "pre.pitch.octdown", Param.PITCH_OCT_UP to "pre.pitch.octup", Param.PITCH_DRY to "pre.pitch.dry",
    Param.COMP_AMOUNT to "pre.comp.input", Param.COMP_ATTACK to "pre.comp.attack", Param.COMP_RELEASE to "pre.comp.release", Param.COMP_LEVEL to "pre.comp.output",
    Param.NAM1_GAIN to "pre.nam1.gain", Param.NAM1_BASS to "pre.nam1.bass", Param.NAM1_MID to "pre.nam1.mid", Param.NAM1_MID_HZ to "pre.nam1.midhz", Param.NAM1_TREBLE to "pre.nam1.treble", Param.NAM1_LEVEL to "pre.nam1.level",
    Param.NAM2_GAIN to "pre.nam2.gain", Param.NAM2_BASS to "pre.nam2.bass", Param.NAM2_MID to "pre.nam2.mid", Param.NAM2_MID_HZ to "pre.nam2.midhz", Param.NAM2_TREBLE to "pre.nam2.treble", Param.NAM2_LEVEL to "pre.nam2.level",
)

@Composable
internal fun PreSheet(ui: Ui, modifier: Modifier) {
    val s = ui.s
    val focus = s.ui.preFocus
    val transpose = s.param(Param.PITCH_MODE) == 0.0
    SectionSheet("PRE · ${focus.name.replace("NAM", "NAM ")}", modifier, actions = { LockControls(ui, Section.PRE, "pre.lock", "pre.store") }) {
        PedalTabs(
            ui,
            listOf(
                Tab("PITCH", Param.PITCH_MODE.options[s.param(Param.PITCH_MODE).toInt()], Param.PITCH_ON, "pre.card.pitch", "pre.pitch.on", focus == PreBlock.PITCH, Action.FocusPre(PreBlock.PITCH)),
                Tab("COMP", "INPUT %.1f".format(s.param(Param.COMP_AMOUNT)), Param.COMP_ON, "pre.card.comp", "pre.comp.on", focus == PreBlock.COMP, Action.FocusPre(PreBlock.COMP)),
                Tab("NAM 1", captureName(ui, Param.NAM1_CAPTURE), Param.NAM1_ON, "pre.card.nam1", "pre.nam1.on", focus == PreBlock.NAM1, Action.FocusPre(PreBlock.NAM1), "pre.nam1.capture", Action.Open(Overlay.CaptureMenu(1))),
                Tab("NAM 2", captureName(ui, Param.NAM2_CAPTURE), Param.NAM2_ON, "pre.card.nam2", "pre.nam2.on", focus == PreBlock.NAM2, Action.FocusPre(PreBlock.NAM2), "pre.nam2.capture", Action.Open(Overlay.CaptureMenu(2))),
            ),
        )
        Gap(h = 6.dp)
        Row(Modifier.fillMaxWidth().height(Metrics.target), horizontalArrangement = Arrangement.spacedBy(20.dp), verticalAlignment = Alignment.CenterVertically) {
            when (focus) {
                PreBlock.PITCH -> {
                    ModeGroup(ui, "Mode", Param.PITCH_MODE, listOf("pre.pitch.mode.transpose", "pre.pitch.mode.octaver"))
                    if (transpose) ModeGroup(ui, "Character", Param.PITCH_CHAR, listOf(null, "pre.pitch.char.instant", "pre.pitch.char.poly"))
                    else ModeGroup(ui, "Voicing", Param.PITCH_VOICING, listOf("pre.pitch.voicing.vintage", "pre.pitch.voicing.modern"))
                }
                PreBlock.COMP -> Note("Evens out your picking before the amp. INPUT sets how hard it squeezes.")
                PreBlock.NAM1, PreBlock.NAM2 -> {
                    val slot = if (focus == PreBlock.NAM1) 1 else 2
                    val p = if (slot == 1) Param.NAM1_CAPTURE else Param.NAM2_CAPTURE
                    Group("Capture") {
                        Btn(captureName(ui, p) + "  ▾", listOf(if (slot == 1) "pre.nam1.capture" else "pre.nam2.capture"), { ui.d(Action.Open(Overlay.CaptureMenu(slot))) }, sub = "CHANGE PEDAL", modifier = Modifier.width(220.dp))
                    }
                }
            }
        }
        Gap(h = 6.dp)
        KnobLine(ui, KnobRows.forPre(focus, s), { p ->
            when {
                p == Param.PITCH_LEVEL -> listOf(if (transpose) "pre.pitch.level" else "pre.pitch.oct_level")
                else -> listOfNotNull(preKnobIds[p])
            }
        })
    }
}

private val postKnobIds = mapOf(
    Param.CHORUS_RATE to "post.chorus.rate", Param.CHORUS_DEPTH to "post.chorus.depth", Param.CHORUS_TONE to "post.chorus.tone", Param.CHORUS_WIDTH to "post.chorus.width", Param.CHORUS_MIX to "post.chorus.mix",
    Param.DELAY_TIME to "post.delay.time", Param.DELAY_FEEDBACK to "post.delay.feedback", Param.DELAY_MIX to "post.delay.mix", Param.DELAY_TONE to "post.delay.tone",
    Param.REVERB_MIX to "post.reverb.mix", Param.REVERB_DECAY to "post.reverb.decay", Param.REVERB_TONE to "post.reverb.tone", Param.REVERB_PRE_DELAY to "post.reverb.predelay", Param.REVERB_SHIMMER to "post.reverb.intensity",
    Param.TREM_RATE to "post.trem.rate", Param.TREM_DEPTH to "post.trem.depth", Param.TREM_SHAPE to "post.trem.shape", Param.TREM_MIX to "post.trem.mix", Param.TREM_XOVER to "post.trem.xover",
)

@Composable
internal fun PostSheet(ui: Ui, modifier: Modifier) {
    val s = ui.s
    val focus = s.ui.postFocus
    SectionSheet("POST · ${focus.name}", modifier, actions = { LockControls(ui, Section.POST, "post.lock", "post.store") }) {
        PedalTabs(
            ui,
            listOf(
                Tab("CHORUS", Param.CHORUS_MODE.options[s.param(Param.CHORUS_MODE).toInt()], Param.CHORUS_ON, "post.card.chorus", "post.chorus.on", focus == PostBlock.CHORUS, Action.FocusPost(PostBlock.CHORUS)),
                Tab("DELAY", Param.DELAY_MODE.options[s.param(Param.DELAY_MODE).toInt()], Param.DELAY_ON, "post.card.delay", "post.delay.on", focus == PostBlock.DELAY, Action.FocusPost(PostBlock.DELAY)),
                Tab("REVERB", Param.REVERB_MODE.options[s.param(Param.REVERB_MODE).toInt()], Param.REVERB_ON, "post.card.reverb", "post.reverb.on", focus == PostBlock.REVERB, Action.FocusPost(PostBlock.REVERB)),
                Tab("TREMOLO", Param.TREM_MODE.options[s.param(Param.TREM_MODE).toInt()], Param.TREM_ON, "post.card.trem", "post.trem.on", focus == PostBlock.TREM, Action.FocusPost(PostBlock.TREM)),
            ),
        )
        Gap(h = 6.dp)
        Row(Modifier.fillMaxWidth().height(Metrics.target), horizontalArrangement = Arrangement.spacedBy(20.dp), verticalAlignment = Alignment.CenterVertically) {
            when (focus) {
                PostBlock.CHORUS -> ModeGroup(ui, "Mode", Param.CHORUS_MODE, listOf("post.chorus.mode.classic", "post.chorus.mode.warped", "post.chorus.mode.clear", "post.chorus.mode.ensemble"))
                PostBlock.DELAY -> {
                    ModeGroup(ui, "Mode", Param.DELAY_MODE, listOf("post.delay.mode.digital", "post.delay.mode.analog", "post.delay.mode.reverse"))
                    Group("Stereo") { Switch("PING-PONG", s.on(Param.DELAY_PING_PONG), listOf("post.delay.pingpong"), { ui.d(Action.ToggleParam(Param.DELAY_PING_PONG)) }) }
                    Group("Time") { Switch("TEMPO SYNC", s.on(Param.DELAY_SYNC), listOf("post.delay.sync"), { ui.d(Action.ToggleParam(Param.DELAY_SYNC)) }) }
                }
                PostBlock.REVERB -> {
                    ModeGroup(ui, "Mode", Param.REVERB_MODE, listOf("post.reverb.mode.hall", "post.reverb.mode.plate", "post.reverb.mode.oktaverb"))
                    if (s.param(Param.REVERB_MODE) == 2.0) ModeGroup(ui, "Voice", Param.REVERB_SUB, listOf("post.reverb.sub.halo", "post.reverb.sub.shimmer", "post.reverb.sub.bloom"))
                }
                PostBlock.TREM -> {
                    ModeGroup(ui, "Mode", Param.TREM_MODE, listOf("post.trem.mode.optical", "post.trem.mode.bias", "post.trem.mode.harmonic"))
                    Group("Rate") { Switch("TEMPO SYNC", s.on(Param.TREM_SYNC), listOf("post.trem.sync"), { ui.d(Action.ToggleParam(Param.TREM_SYNC)) }) }
                }
            }
        }
        Gap(h = 6.dp)
        val delayChar = KnobRows.delayCharacterLabel(s)
        KnobLine(
            ui,
            KnobRows.forPost(focus, s),
            { p -> if (p == Param.DELAY_AGE) listOf("post.delay." + delayChar.lowercase()) else listOfNotNull(postKnobIds[p]) },
            { p -> if (p == Param.DELAY_AGE) delayChar else p.label },
        ) {
            if (focus == PostBlock.DELAY && s.on(Param.DELAY_SYNC)) DivisionGrid(ui, Param.DELAY_DIVISION, "post.delay.division")
            if (focus == PostBlock.TREM && s.on(Param.TREM_SYNC)) DivisionGrid(ui, Param.TREM_DIVISION, "post.trem.division")
        }
    }
}