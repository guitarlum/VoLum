package com.volum.ui.a

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.DIVISIONS
import com.volum.core.KnobRows
import com.volum.core.Overlay
import com.volum.core.Param
import com.volum.core.PostBlock
import com.volum.core.PreBlock
import com.volum.core.Section
import kotlin.math.roundToInt

@Composable
internal fun PreEditor(c: Ctx, b: PreBlock) {
    val s = c.s
    Editor(preTitle(b, s), c, Texture.PRE,
        left = { Switch("ON", s.on(b.onParam), { c.d(Action.ToggleParam(b.onParam)) }, Modifier.fillMaxWidth(), listOf("pre.${b.key}.on")) },
        right = {
            ControlRow {
                T(preTitle(b, s), Type.display(22.sp), Modifier.weight(1f))
                CaptureSlot(c, 1, b == PreBlock.NAM1)
                CaptureSlot(c, 2, b == PreBlock.NAM2)
                LockControls(c, Section.PRE)
            }
            ControlRow {
                when (b) {
                    PreBlock.PITCH -> {
                        ModePicker(c, Param.PITCH_MODE, listOf("pre.pitch.mode.transpose", "pre.pitch.mode.octaver"))
                        Spacer(Modifier.width(10.dp))
                        if (s.param(Param.PITCH_MODE) == 0.0) {
                            T("CHARACTER", Type.label(10.sp))
                            ModePicker(c, Param.PITCH_CHAR, listOf(null, "pre.pitch.char.instant", "pre.pitch.char.poly"))
                        } else {
                            T("VOICING", Type.label(10.sp))
                            ModePicker(c, Param.PITCH_VOICING, listOf("pre.pitch.voicing.vintage", "pre.pitch.voicing.modern"))
                        }
                    }
                    PreBlock.COMP -> T("Evens out your picking before the pedals and the amp.", Type.label(11.sp, Ink.faint))
                    PreBlock.NAM1, PreBlock.NAM2 -> T("A captured pedal: GAIN drives the capture, the EQ and LEVEL shape what leaves it.", Type.label(11.sp, Ink.faint))
                }
            }
            KnobRow(c, KnobRows.forPre(b, s), Modifier.weight(1f))
        },
    )
}

/** PRE's two capture slots stay one tap away whichever PRE block is focused. */
@Composable
private fun CaptureSlot(c: Ctx, slot: Int, focused: Boolean) {
    Chip("NAM $slot · ${captureName(c.s, slot)}  ▾", { c.d(Action.Open(Overlay.CaptureMenu(slot))) }, Modifier.width(150.dp), listOf("pre.nam$slot.capture"),
        selected = focused, size = 11.sp)
}

@Composable
internal fun PostEditor(c: Ctx, b: PostBlock) {
    val s = c.s
    Editor(postTitle(b), c, Texture.POST,
        left = { Switch("ON", s.on(b.onParam), { c.d(Action.ToggleParam(b.onParam)) }, Modifier.fillMaxWidth(), listOf("post.${b.key}.on")) },
        right = {
            ControlRow {
                T(postTitle(b), Type.display(22.sp), Modifier.weight(1f))
                LockControls(c, Section.POST)
            }
            ControlRow {
                when (b) {
                    PostBlock.CHORUS -> ModePicker(c, Param.CHORUS_MODE, listOf("classic", "warped", "clear", "ensemble").map { "post.chorus.mode.$it" })
                    PostBlock.DELAY -> {
                        ModePicker(c, Param.DELAY_MODE, listOf("digital", "analog", "reverse").map { "post.delay.mode.$it" })
                        Spacer(Modifier.width(6.dp))
                        Switch("PING-PONG", s.on(Param.DELAY_PING_PONG), { c.d(Action.ToggleParam(Param.DELAY_PING_PONG)) }, ids = listOf("post.delay.pingpong"))
                        Switch("TEMPO SYNC", s.on(Param.DELAY_SYNC), { c.d(Action.ToggleParam(Param.DELAY_SYNC)) }, ids = listOf("post.delay.sync"))
                    }
                    PostBlock.REVERB -> {
                        ModePicker(c, Param.REVERB_MODE, listOf("hall", "plate", "oktaverb").map { "post.reverb.mode.$it" })
                        if (s.param(Param.REVERB_MODE) == 2.0) {
                            Spacer(Modifier.width(10.dp))
                            ModePicker(c, Param.REVERB_SUB, listOf("halo", "shimmer", "bloom").map { "post.reverb.sub.$it" })
                        }
                    }
                    PostBlock.TREM -> {
                        ModePicker(c, Param.TREM_MODE, listOf("optical", "bias", "harmonic").map { "post.trem.mode.$it" })
                        Spacer(Modifier.width(6.dp))
                        Switch("TEMPO SYNC", s.on(Param.TREM_SYNC), { c.d(Action.ToggleParam(Param.TREM_SYNC)) }, ids = listOf("post.trem.sync"))
                    }
                }
            }
            val division = when {
                b == PostBlock.DELAY && s.on(Param.DELAY_SYNC) -> Param.DELAY_DIVISION to "post.delay.division"
                b == PostBlock.TREM && s.on(Param.TREM_SYNC) -> Param.TREM_DIVISION to "post.trem.division"
                else -> null
            }
            KnobRow(c, KnobRows.forPost(b, s), Modifier.weight(1f), slot = division?.let { (p, id) -> { Division(c, p, id) } })
        },
    )
}

/** Tempo sync swaps TIME / RATE for the note division, in the same place in the row. */
@Composable
private fun Division(c: Ctx, p: Param, id: String) {
    val i = c.s.param(p).roundToInt().coerceIn(0, DIVISIONS.lastIndex)
    val step = { d: Int -> c.d(Action.SetParam(p, Math.floorMod(i + d, DIVISIONS.size).toDouble())) }
    Column(Modifier.padding(end = 6.dp), horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(4.dp)) {
        T("DIVISION", Type.label(11.sp))
        Stepper(null, DIVISIONS[i], { step(-1) }, { step(1) }, prevIds = listOf(id), nextIds = listOf(id), valueWidth = 44.dp)
    }
}
