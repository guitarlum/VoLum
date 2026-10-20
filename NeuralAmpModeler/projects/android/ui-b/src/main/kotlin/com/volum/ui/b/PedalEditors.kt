package com.volum.ui.b

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.Spacer
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
import com.volum.design.art.Motif
import com.volum.design.components.MotifArt
import com.volum.design.components.VoLumBlockHeader
import com.volum.design.components.VoLumGlyph
import com.volum.design.components.VoLumIcon
import com.volum.design.components.VoLumSubModePill
import com.volum.design.components.insetWell
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumType

@Composable
internal fun PreEditor(s: VoLumState, block: PreBlock) {
    EditorFrame(s, Section.PRE, stompLabel(block.onParam(), s), header = { PreHeader(s, block) }, side = { CaptureColumn(s) }) { labels ->
        KnobRows.forPre(block, s).forEach { p -> ParamKnob(s, p, Ids.preKnob(p, s), labels) }
    }
}

/** Both NAM captures stay one tap away from every PRE pedal. */
@Composable
private fun CaptureColumn(s: VoLumState) {
    val st = LocalStage.current
    Column(Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(4.dp, Alignment.CenterVertically)) {
        FitText("CAPTURES · the pedals NAM 1 and NAM 2 play", VoLumType.SectionHeader, Modifier.fillMaxWidth().height(14.dp), align = TextAlign.Start)
        val names = rememberFitGroup()
        listOf(Triple(1, Param.NAM1_CAPTURE, Motif.NAM1), Triple(2, Param.NAM2_CAPTURE, Motif.NAM2)).forEach { (slot, p, motif) ->
            Pad({ st.go(Action.Open(Overlay.CaptureMenu(slot))) }, Modifier.fillMaxWidth().height(48.dp), arrayOf("pre.nam$slot.capture"),
                selected = s.ui.preFocus.ordinal == slot + 1, style = Sel.AmberPicker, description = "NAM $slot capture") { ink ->
                Row(Modifier.fillMaxSize().padding(horizontal = 8.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    MotifArt(motif, captureName(s, p) != null, Modifier.width(22.dp).height(26.dp))
                    FitText("NAM $slot", VoLumType.label(10.sp, if (ink == VoLumColors.AmberInk) ink else VoLumColors.GoldDim), Modifier.width(40.dp).height(16.dp), align = TextAlign.Start)
                    FitText(captureName(s, p) ?: "Empty", VoLumType.label(13.sp, if (ink == VoLumColors.AmberInk) ink else VoLumColors.TextBright), Modifier.weight(1f).height(20.dp),
                        align = TextAlign.Start, group = names)
                    VoLumIcon(VoLumGlyph.ChevronDown, color = if (ink == VoLumColors.AmberInk) ink else VoLumColors.Gold)
                }
            }
        }
    }
}

@Composable
internal fun PostEditor(s: VoLumState, block: PostBlock) {
    val side: (@Composable () -> Unit)? = when (block) {
        PostBlock.DELAY -> { { SyncColumn(s, Param.DELAY_SYNC, Param.DELAY_DIVISION, "post.delay", pingPong = true, speedHint = "TIME sets the echo spacing.") } }
        PostBlock.TREM -> { { SyncColumn(s, Param.TREM_SYNC, Param.TREM_DIVISION, "post.trem", pingPong = false, speedHint = "RATE sets the pulse speed.") } }
        else -> null
    }
    EditorFrame(s, Section.POST, stompLabel(block.onParam(), s), header = { PostHeader(s, block) }, side = side) { labels ->
        KnobRows.forPost(block, s).forEach { p ->
            val label = if (p == Param.DELAY_AGE) KnobRows.delayCharacterLabel(s) else p.label
            ParamKnob(s, p, Ids.postKnob(p, s), labels, label)
        }
    }
}

/**
 * The focused block: its name, the section's padlock (and store arrow) from `:design`,
 * the block's mode groups; a line explaining STORE when the lock differs; then the
 * knob row with an optional side column.
 */
@Composable
private fun EditorFrame(
    s: VoLumState,
    section: Section,
    title: String,
    header: @Composable RowScope.() -> Unit,
    side: (@Composable () -> Unit)?,
    knobs: @Composable RowScope.(labels: FitGroup) -> Unit,
) {
    val st = LocalStage.current
    val locked = (if (section == Section.PRE) s.locks.pre else s.locks.post) != null
    val differs = Reducer.lockDiffers(s, section)
    val key = if (section == Section.PRE) "pre" else "post"
    val labels = rememberFitGroup()
    Column(Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(4.dp)) {
        Row(Modifier.fillMaxWidth().height(48.dp), horizontalArrangement = Arrangement.spacedBy(10.dp), verticalAlignment = Alignment.CenterVertically) {
            FitText(title, VoLumType.display(22.sp, VoLumColors.TextBright), Modifier.width(112.dp), align = TextAlign.Start)
            VoLumBlockHeader(
                if (locked) "${section.name} LOCKED" else "LOCK ${section.name}", locked, { st.go(Action.SetLock(section, !locked)) },
                Modifier.width(if (differs) 196.dp else 150.dp), dirty = differs, onStore = { st.go(Action.StoreLock(section)) }, storeFirst = section == Section.POST,
                lockModifier = Modifier.ids(arrayOf("$key.lock")), storeModifier = Modifier.ids(arrayOf("$key.store")),
            )
            header()
        }
        Row(Modifier.fillMaxWidth().weight(1f), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(2.dp)) {
                if (differs) {
                    FitText("This amp saved a different ${section.name} · the teal arrow stores the locked one into it",
                        VoLumType.label(11.sp, VoLumColors.Teal), Modifier.fillMaxWidth().height(20.dp).insetWell(3.dp).padding(horizontal = 8.dp, vertical = 2.dp),
                        align = TextAlign.Start)
                }
                Row(Modifier.fillMaxWidth().weight(1f), horizontalArrangement = Arrangement.spacedBy(6.dp, Alignment.CenterHorizontally)) { knobs(labels) }
            }
            if (side != null) Box(Modifier.width(if (section == Section.POST) 280.dp else 236.dp).fillMaxHeight()) { side() }
        }
    }
}

@Composable
private fun ParamKnob(s: VoLumState, p: Param, ids: Array<String>, labels: FitGroup, label: String = p.label) {
    val st = LocalStage.current
    Knob(p, s.param(p), { st.go(Action.SetParam(p, it)) }, Modifier.width(96.dp).fillMaxHeight(), ids, label, group = labels)
}

/** One exclusive group: its name, then the AmberPicker pill. Groups sit apart behind a hairline so it is clear which choices exclude each other. */
@Composable
private fun RowScope.ModeGroup(s: VoLumState, caption: String, p: Param, ids: (Int) -> Array<out String>, weight: Float, divider: Boolean = false) {
    val st = LocalStage.current
    if (divider) Spacer(Modifier.width(1.dp).height(36.dp).background(VoLumColors.Frame))
    Row(Modifier.weight(weight).fillMaxHeight(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
        FitText(caption, VoLumType.label(10.sp, VoLumColors.GoldDim), Modifier.width(66.dp).height(16.dp), align = TextAlign.End)
        VoLumSubModePill(p.options, s.param(p).toInt(), { st.go(Action.SetParam(p, it.toDouble())) }, Modifier.weight(1f).height(48.dp),
            itemModifier = { i -> Modifier.ids(ids(i)) })
    }
}

@Composable
private fun RowScope.PreHeader(s: VoLumState, block: PreBlock) {
    when (block) {
        PreBlock.PITCH -> {
            ModeGroup(s, "MODE", Param.PITCH_MODE, { i -> arrayOf(if (i == 0) "pre.pitch.mode.transpose" else "pre.pitch.mode.octaver") }, 1f)
            if (s.param(Param.PITCH_MODE) == 0.0) {
                ModeGroup(s, "CHARACTER", Param.PITCH_CHAR, { i -> when (i) { 1 -> arrayOf("pre.pitch.char.instant"); 2 -> arrayOf("pre.pitch.char.poly"); else -> emptyArray() } }, 1.25f, divider = true)
            } else {
                ModeGroup(s, "VOICING", Param.PITCH_VOICING, { i -> arrayOf(if (i == 0) "pre.pitch.voicing.vintage" else "pre.pitch.voicing.modern") }, 1f, divider = true)
            }
        }
        PreBlock.COMP -> FitText("Evens out your picking before the amp.", hint(), Modifier.weight(1f).height(20.dp), align = TextAlign.Start)
        PreBlock.NAM1, PreBlock.NAM2 -> FitText("A captured pedal in front of the amp. Swap it under CAPTURES.", hint(),
            Modifier.weight(1f).height(20.dp), align = TextAlign.Start)
    }
}

@Composable
private fun RowScope.PostHeader(s: VoLumState, block: PostBlock) {
    when (block) {
        PostBlock.CHORUS -> ModeGroup(s, "MODE", Param.CHORUS_MODE, { i -> arrayOf("post.chorus.mode.${Param.CHORUS_MODE.options[i].lowercase()}") }, 1f)
        PostBlock.DELAY -> ModeGroup(s, "MODE", Param.DELAY_MODE, { i -> arrayOf("post.delay.mode.${Param.DELAY_MODE.options[i].lowercase()}") }, 1f)
        PostBlock.REVERB -> {
            ModeGroup(s, "MODE", Param.REVERB_MODE, { i -> arrayOf("post.reverb.mode.${Param.REVERB_MODE.options[i].lowercase()}") }, 1f)
            if (s.param(Param.REVERB_MODE) == 2.0) {
                ModeGroup(s, "VOICE", Param.REVERB_SUB, { i -> arrayOf("post.reverb.sub.${Param.REVERB_SUB.options[i].lowercase()}") }, 1f, divider = true)
            }
        }
        PostBlock.TREM -> ModeGroup(s, "MODE", Param.TREM_MODE, { i -> arrayOf("post.trem.mode.${Param.TREM_MODE.options[i].lowercase()}") }, 1f)
    }
}

/** Tempo sync (and Ping-Pong for the delay) with the note division it locks to; unsynced, [speedHint] names the knob that sets the speed. */
@Composable
private fun SyncColumn(s: VoLumState, sync: Param, division: Param, key: String, pingPong: Boolean, speedHint: String) {
    val st = LocalStage.current
    val synced = s.on(sync)
    val div = s.param(division).toInt()
    val labels = rememberFitGroup()
    Column(Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(4.dp, Alignment.CenterVertically)) {
        Row(Modifier.fillMaxWidth().height(48.dp), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
            if (pingPong) Switch("PING-PONG", s.on(Param.DELAY_PING_PONG), { st.go(Action.ToggleParam(Param.DELAY_PING_PONG)) }, Modifier.weight(1f).fillMaxHeight(),
                arrayOf("post.delay.pingpong"), group = labels)
            Switch("TEMPO SYNC", synced, { st.go(Action.ToggleParam(sync)) }, Modifier.weight(1f).fillMaxHeight(), arrayOf("$key.sync"), group = labels)
        }
        if (synced) {
            Row(Modifier.fillMaxWidth().height(48.dp), verticalAlignment = Alignment.CenterVertically) {
                FitText("DIVISION", VoLumType.label(10.sp, VoLumColors.GoldDim), Modifier.width(66.dp).height(16.dp), align = TextAlign.Start)
                Stepper(DIVISIONS[div.coerceIn(0, DIVISIONS.lastIndex)], { st.go(Action.SetParam(division, ((div - 1 + DIVISIONS.size) % DIVISIONS.size).toDouble())) },
                    { st.go(Action.SetParam(division, ((div + 1) % DIVISIONS.size).toDouble())) }, Modifier.weight(1f).fillMaxHeight(),
                    arrayOf("$key.division"), arrayOf("$key.division"), description = "division")
            }
        } else {
            val lines = rememberFitGroup()
            FitText(speedHint, hint(), Modifier.fillMaxWidth().height(18.dp).padding(horizontal = 6.dp), align = TextAlign.Start, group = lines)
            FitText("TEMPO SYNC locks it to the metronome.", hint(), Modifier.fillMaxWidth().height(18.dp).padding(horizontal = 6.dp), align = TextAlign.Start, group = lines)
        }
    }
}
