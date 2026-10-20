package com.volum.ui.a

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
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
import androidx.compose.ui.semantics.CustomAccessibilityAction
import androidx.compose.ui.semantics.customActions
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.Overlay
import com.volum.core.Param
import com.volum.core.PostBlock
import com.volum.core.PreBlock

private val SoundPitch = 56.dp

@Composable
internal fun PlaySurface(c: Ctx) {
    val s = c.s
    Row(Modifier.fillMaxSize().padding(start = 4.dp, end = 4.dp, bottom = 6.dp)) {
        Meter("IN", c::inLevel, Modifier.fillMaxHeight().ids("play.meters"))
        Column(Modifier.weight(1f).fillMaxHeight().padding(horizontal = 6.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            HeroStage(c, Modifier.weight(1f).fillMaxWidth())
            SoundCaption(c)
            Stomps(c)
        }
        SoundsColumn(c, Modifier.width(284.dp).fillMaxHeight())
        Meter(if (s.outputSafety) "LIM" else "OUT", c::outLevel, Modifier.fillMaxHeight().padding(start = 4.dp), alert = s.outputSafety)
    }
}

@Composable
private fun HeroStage(c: Ctx, modifier: Modifier) {
    val s = c.s
    Box(modifier) {
        ArtSlot(s.amp?.name ?: "", c::energy, Modifier.fillMaxSize().ids("play.art"), dim = !inputLive(s))
        if (!s.ui.buildTipSeen) {
            Notice("Tweak your sound: BUILD opens the amp, pedals and effects.", Modifier.align(Alignment.TopStart).padding(8.dp).width(260.dp), ids = listOf("tip.build"))
        }
        if (!inputLive(s)) {
            Row(Modifier.align(Alignment.BottomCenter).padding(8.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Notice(inputProblem(s), Modifier.width(300.dp), alert = true)
                Chip("FIX", { c.d(Action.Open(Overlay.AudioStatus)) }, ids = listOf("nav.audio_status.open"), selected = true)
            }
        }
    }
}

internal fun inputProblem(s: com.volum.core.VoLumState): String = when {
    s.audio.permission != com.volum.core.Permission.GRANTED -> "No input: VoLum may not use the microphone."
    s.audio.status == com.volum.core.InterfaceStatus.NO_INTERFACE -> "No input: no USB audio interface is connected."
    else -> "No input: the USB interface was unplugged."
}

@Composable
private fun SoundCaption(c: Ctx) {
    val s = c.s
    val live = s.library.live
    val sound = live?.let { s.library.sounds[it] }
    val name = sound?.let { s.library.preset(it.presetId)?.name } ?: s.presetName ?: "Unsaved sound"
    Row(Modifier.fillMaxWidth().height(56.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
        IconTap(Glyph.PREV, { c.d(Action.StepSound(-1)) }, Modifier.size(56.dp), listOf("play.sound.prev"), desc = "Previous sound")
        Column(Modifier.weight(1f)) {
            T(name, Type.display(24.sp))
            Row {
                T(listOfNotNull(live?.let { program(it) }, s.amp?.name).joinToString("  ·  "), Type.label(11.sp))
                if (s.rig.dirty) T("   UNSAVED", Type.label(11.sp, Ink.text))
            }
        }
        IconTap(Glyph.NEXT, { c.d(Action.StepSound(1)) }, Modifier.size(56.dp), listOf("play.sound.next"), desc = "Next sound")
        Chip("SAVE", { c.d(Action.Open(Overlay.NameDialog(com.volum.core.NamePurpose.SAVE_PRESET, com.volum.core.Reducer.nameSuggestion(s)))) },
            Modifier.width(72.dp).height(56.dp), listOf("play.save"))
    }
}

@Composable
private fun Stomps(c: Ctx) {
    Row(Modifier.fillMaxWidth().height(104.dp), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
        PreBlock.entries.forEach { b -> Stomp(c, preCard(b, c.s), b.onParam, "play.stomp.${b.key}", Texture.PRE, Modifier.weight(1f)) }
        Spacer(Modifier.width(4.dp))
        PostBlock.entries.forEach { b -> Stomp(c, b.name, b.onParam, "play.stomp.${b.key}", Texture.POST, Modifier.weight(1f)) }
    }
}

/** A pedal: the face opens the block in BUILD, the footswitch under it turns it on and off. */
@Composable
private fun Stomp(c: Ctx, label: String, p: Param, id: String, texture: Texture, modifier: Modifier) {
    val on = c.s.on(p)
    Column(modifier.fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(2.dp)) {
        Tap({ c.d(Action.EditStomp(p)) }, Modifier.fillMaxWidth().height(48.dp).semanticsDesc("Edit $label"), listOf("play.stomp.edit"), padding = 0.dp) {
            Frame(Modifier.fillMaxSize().padding(3.dp), texture) {
                T(label, Type.label(10.sp, if (on) Ink.selected else Ink.dim, 0.2.sp), Modifier.align(Alignment.Center))
            }
        }
        Tap({ c.d(Action.ToggleParam(p)) }, Modifier.fillMaxWidth().weight(1f).semanticsDesc(label), listOf(id), selected = on, pick = Pick.BRASS) {
            Led(on, Modifier.size(14.dp))
        }
    }
}

@Composable
private fun SoundsColumn(c: Ctx, modifier: Modifier) {
    val s = c.s
    val programs = s.library.sounds.keys.sorted()
    val liveRow = programs.indexOf(s.library.live)
    Column(modifier.padding(start = 6.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
        SectionLabel("SOUNDS")
        RowList(SoundPitch, Modifier.weight(1f).fillMaxWidth(), scrollToRow = (liveRow - 1).coerceAtLeast(0)) {
            programs.forEach { p -> SoundRow(c, p) }
        }
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            Chip("+ THIS SOUND", { c.d(Action.AddThisSound) }, Modifier.weight(1f), listOf("play.add_this", "play.add_this.unnamed", "play.add_this.full"), size = 11.sp)
            Chip("+ FROM LIST", { c.d(Action.Open(Overlay.SoundPicker(s.library.firstFreeProgram() ?: 0, replace = false))) }, Modifier.weight(1f), listOf("play.add_sound"), size = 11.sp)
        }
    }
}

@Composable
private fun SoundRow(c: Ctx, p: Int) {
    val s = c.s
    val sound = s.library.sounds.getValue(p)
    val valid = s.library.soundValid(p)
    val live = s.library.live == p
    val moveDown = Modifier.semantics { customActions = listOf(CustomAccessibilityAction("Move down") { c.d(Action.MoveSound(p, 1)); true }) }
    Column(Modifier.fillMaxWidth()) {
        val ids = if (valid) listOf("play.sound.recall", "play.sound.reorder") else listOf("play.sound.replace", "play.sound.reorder")
        val onTap = { if (valid) c.d(Action.RecallSound(p)) else c.d(Action.Open(Overlay.SoundPicker(p, replace = true))) }
        ListRow(onTap, moveDown, ids, selected = live, pitch = SoundPitch) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                T(program(p), Type.value(16.sp, if (live) Ink.selected else Ink.dim), Modifier.width(34.dp).ids("play.sound.program"))
                Column(Modifier.weight(1f)) {
                    if (valid) {
                        T(s.library.preset(sound.presetId)?.name ?: "", Type.value(15.sp), Modifier.ids("play.sound.name"))
                        T(s.library.amp(sound.ampId)?.name ?: "", Type.label(10.sp))
                    } else {
                        T("Invalid slot", Type.value(15.sp, Ink.alert), Modifier.ids("play.sound.invalid"))
                        T("tap to pick a new sound", Type.label(10.sp, Ink.alert))
                    }
                }
                if (live) T("LIVE", Type.label(10.sp, Ink.selected), Modifier.ids("play.sound.live"))
            }
        }
        if (live) {
            Row(Modifier.fillMaxWidth().height(SoundPitch).padding(vertical = 3.dp), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                Chip("REPLACE", { c.d(Action.Open(Overlay.SoundPicker(p, replace = true))) }, Modifier.weight(1f), listOf("play.sound.replace"))
                Chip("CLEAR", { c.d(Action.ClearSound(p)) }, Modifier.weight(1f), listOf("play.sound.clear"))
                Chip("MOVE ↓", { c.d(Action.MoveSound(p, 1)) }, Modifier.weight(1f).then(moveDown).semanticsDesc("Move down"), listOf("play.sound.reorder"))
            }
        }
    }
}
