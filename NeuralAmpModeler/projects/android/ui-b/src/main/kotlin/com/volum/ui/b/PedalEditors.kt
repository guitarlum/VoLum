package com.volum.ui.b

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.DIVISIONS
import com.volum.core.KnobRows
import com.volum.core.Overlay
import com.volum.core.Param
import com.volum.core.PostBlock
import com.volum.core.PreBlock
import com.volum.core.Reducer
import com.volum.core.Section
import com.volum.core.VoLumState

@Composable
internal fun PreEditor(s: VoLumState, block: PreBlock) {
    EditorFrame(s, Section.PRE, stompLabel(block.onParam(), s), header = { PreHeader(s, block) }, side = { CaptureColumn(s) }) {
        KnobRows.forPre(block, s).forEach { p -> ParamKnob(s, p, Ids.preKnob(p, s)) }
    }
}

/** Both NAM captures stay one tap away from every PRE pedal. */
@Composable
private fun CaptureColumn(s: VoLumState) {
    val st = LocalStage.current
    Column(Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
        Caption("CAPTURES", Modifier.fillMaxWidth())
        listOf(1 to Param.NAM1_CAPTURE, 2 to Param.NAM2_CAPTURE).forEach { (slot, p) ->
            Pad({ st.go(Action.Open(Overlay.CaptureMenu(slot))) }, Modifier.fillMaxWidth().height(50.dp), arrayOf("pre.nam$slot.capture"),
                selected = s.ui.preFocus.ordinal == slot + 1, sel = Sel.LIST_TEAL, description = "NAM $slot capture") {
                Row(Modifier.fillMaxSize().padding(horizontal = 10.dp), verticalAlignment = Alignment.CenterVertically) {
                    FitText("NAM $slot", Type.label(11.sp), Modifier.width(48.dp), align = TextAlign.Start)
                    FitText(captureName(s, p) ?: "EMPTY", Type.display(17.sp), Modifier.weight(1f), align = TextAlign.Start)
                    FitText("▾", Type.label(13.sp, Ink.text), Modifier.width(16.dp))
                }
            }
        }
    }
}

@Composable
internal fun PostEditor(s: VoLumState, block: PostBlock) {
    val side: (@Composable () -> Unit)? = when (block) {
        PostBlock.DELAY -> { { SyncColumn(s, Param.DELAY_SYNC, Param.DELAY_DIVISION, "post.delay", pingPong = true) } }
        PostBlock.TREM -> { { SyncColumn(s, Param.TREM_SYNC, Param.TREM_DIVISION, "post.trem", pingPong = false) } }
        else -> null
    }
    EditorFrame(s, Section.POST, stompLabel(block.onParam(), s), header = { PostHeader(s, block) }, side = side) {
        KnobRows.forPost(block, s).forEach { p ->
            val label = if (p == Param.DELAY_AGE) KnobRows.delayCharacterLabel(s) else p.label
            ParamKnob(s, p, Ids.postKnob(p, s), label)
        }
    }
}

/** Title, the section's LOCK / STORE, the block's mode controls; knob row below with an optional switch column. */
@Composable
private fun EditorFrame(
    s: VoLumState,
    section: Section,
    title: String,
    header: @Composable RowScope.() -> Unit,
    side: (@Composable () -> Unit)?,
    knobs: @Composable RowScope.() -> Unit,
) {
    Column(Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Row(Modifier.fillMaxWidth().height(56.dp), horizontalArrangement = Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.CenterVertically) {
            FitText(title, Type.display(24.sp, Ink.hot), Modifier.width(110.dp), align = TextAlign.Start)
            LockStore(s, section)
            header()
        }
        Row(Modifier.fillMaxWidth().weight(1f), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Row(Modifier.weight(1f).fillMaxHeight(), horizontalArrangement = Arrangement.spacedBy(4.dp, Alignment.CenterHorizontally)) { knobs() }
            if (side != null) Box(Modifier.width(220.dp).fillMaxHeight()) { side() }
        }
    }
}

@Composable
private fun RowScope.LockStore(s: VoLumState, section: Section) {
    val st = LocalStage.current
    val locked = (if (section == Section.PRE) s.locks.pre else s.locks.post) != null
    val key = if (section == Section.PRE) "pre" else "post"
    Switch("LOCK ${section.name}", locked, { st.go(Action.SetLock(section, !locked)) }, Modifier.width(118.dp).fillMaxHeight(), arrayOf("$key.lock"))
    if (Reducer.lockDiffers(s, section)) {
        TextPad("STORE", { st.go(Action.StoreLock(section)) }, Modifier.width(84.dp).fillMaxHeight(), arrayOf("$key.store"), style = Type.label(13.sp, Ink.hot))
    }
}

@Composable
private fun RowScope.ParamKnob(s: VoLumState, p: Param, ids: Array<String>, label: String = p.label) {
    val st = LocalStage.current
    Knob(label, s.param(p), p.min, p.max, p.default, p.unit, { st.go(Action.SetParam(p, it)) }, { st.go(Action.ResetParam(p)) },
        Modifier.width(100.dp).fillMaxHeight(), ids)
}

@Composable
private fun RowScope.ModeChoice(s: VoLumState, p: Param, ids: (Int) -> Array<out String>, weight: Float, sel: Sel = Sel.AMBER_PICKER) {
    val st = LocalStage.current
    Choice(p.options, s.param(p).toInt(), { st.go(Action.SetParam(p, it.toDouble())) }, Modifier.weight(weight).fillMaxHeight(), ids, sel = sel)
}

@Composable
private fun RowScope.PreHeader(s: VoLumState, block: PreBlock) {
    val st = LocalStage.current
    when (block) {
        PreBlock.PITCH -> {
            ModeChoice(s, Param.PITCH_MODE, { i -> arrayOf(if (i == 0) "pre.pitch.mode.transpose" else "pre.pitch.mode.octaver") }, 1f)
            if (s.param(Param.PITCH_MODE) == 0.0) {
                ModeChoice(s, Param.PITCH_CHAR, { i -> when (i) { 1 -> arrayOf("pre.pitch.char.instant"); 2 -> arrayOf("pre.pitch.char.poly"); else -> emptyArray() } }, 1.3f, Sel.BRASS)
            } else {
                ModeChoice(s, Param.PITCH_VOICING, { i -> arrayOf(if (i == 0) "pre.pitch.voicing.vintage" else "pre.pitch.voicing.modern") }, 1f, Sel.BRASS)
            }
        }
        PreBlock.COMP -> FitText("evens out your picking before the amp", Type.value(12.sp, Ink.dim), Modifier.weight(1f), align = TextAlign.Start)
        PreBlock.NAM1, PreBlock.NAM2 -> FitText("a captured pedal in front of the amp · swap it under CAPTURES", Type.value(12.sp, Ink.dim),
            Modifier.weight(1f), align = TextAlign.Start)
    }
}

@Composable
private fun RowScope.PostHeader(s: VoLumState, block: PostBlock) {
    when (block) {
        PostBlock.CHORUS -> ModeChoice(s, Param.CHORUS_MODE, { i -> arrayOf("post.chorus.mode.${Param.CHORUS_MODE.options[i].lowercase()}") }, 1f)
        PostBlock.DELAY -> ModeChoice(s, Param.DELAY_MODE, { i -> arrayOf("post.delay.mode.${Param.DELAY_MODE.options[i].lowercase()}") }, 1f)
        PostBlock.REVERB -> {
            ModeChoice(s, Param.REVERB_MODE, { i -> arrayOf("post.reverb.mode.${Param.REVERB_MODE.options[i].lowercase()}") }, 1f)
            if (s.param(Param.REVERB_MODE) == 2.0) {
                ModeChoice(s, Param.REVERB_SUB, { i -> arrayOf("post.reverb.sub.${Param.REVERB_SUB.options[i].lowercase()}") }, 1f, Sel.BRASS)
            }
        }
        PostBlock.TREM -> ModeChoice(s, Param.TREM_MODE, { i -> arrayOf("post.trem.mode.${Param.TREM_MODE.options[i].lowercase()}") }, 1f)
    }
}

/** Tempo sync (and Ping-Pong for the delay) with the note division it locks to. */
@Composable
private fun SyncColumn(s: VoLumState, sync: Param, division: Param, key: String, pingPong: Boolean) {
    val st = LocalStage.current
    val synced = s.on(sync)
    val div = s.param(division).toInt()
    Column(Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
        Row(Modifier.fillMaxWidth().height(48.dp), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            if (pingPong) Switch("PING-PONG", s.on(Param.DELAY_PING_PONG), { st.go(Action.ToggleParam(Param.DELAY_PING_PONG)) }, Modifier.weight(1f).fillMaxHeight(),
                arrayOf("post.delay.pingpong"))
            Switch("SYNC", synced, { st.go(Action.ToggleParam(sync)) }, Modifier.weight(1f).fillMaxHeight(), arrayOf("$key.sync"))
        }
        if (synced) {
            Column(Modifier.fillMaxWidth()) {
                Caption("DIVISION", Modifier.fillMaxWidth())
                Stepper(DIVISIONS[div.coerceIn(0, DIVISIONS.lastIndex)], { st.go(Action.SetParam(division, ((div - 1 + DIVISIONS.size) % DIVISIONS.size).toDouble())) },
                    { st.go(Action.SetParam(division, ((div + 1) % DIVISIONS.size).toDouble())) }, Modifier.fillMaxWidth().height(48.dp),
                    arrayOf("$key.division"), arrayOf("$key.division"), description = "division")
            }
        } else {
            FitText("follows TIME / RATE", Type.value(11.sp, Ink.faint), Modifier.fillMaxWidth().height(20.dp))
        }
    }
}
