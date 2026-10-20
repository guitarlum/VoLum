package com.volum.ui.a

import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsPressedAsState
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.semantics.Role
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
import com.volum.design.components.VoLumChannelStepper
import com.volum.design.components.VoLumGlyph
import com.volum.design.components.VoLumIcon
import com.volum.design.components.VoLumSelectionStyle
import com.volum.design.components.VoLumToggle
import com.volum.design.components.hairlineFrame
import com.volum.design.components.volumSelection
import com.volum.design.theme.VFitText
import com.volum.design.theme.VText
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumType
import kotlin.math.roundToInt

@Composable
internal fun PreEditor(c: Ctx, b: PreBlock) {
    val s = c.s
    Editor(
        left = { EffectArt(b.motif, s.on(b.onParam), pitchVariant(b, s), { c.d(Action.ToggleParam(b.onParam)) }, "pre.${b.key}.on") },
        right = {
            EffectHeader(c, preTitle(b, s), Section.PRE) {
                CaptureSlot(c, 1, b == PreBlock.NAM1)
                CaptureSlot(c, 2, b == PreBlock.NAM2)
            }
            Row(Modifier.fillMaxWidth().heightIn(min = 62.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(14.dp)) {
                when (b) {
                    PreBlock.PITCH -> {
                        ModeGroup(c, "MODE", Param.PITCH_MODE, listOf("pre.pitch.mode.transpose", "pre.pitch.mode.octaver"), 200.dp)
                        if (s.param(Param.PITCH_MODE) == 0.0) ModeGroup(c, "CHARACTER", Param.PITCH_CHAR, listOf(null, "pre.pitch.char.instant", "pre.pitch.char.poly"), 230.dp)
                        else ModeGroup(c, "VOICING", Param.PITCH_VOICING, listOf("pre.pitch.voicing.vintage", "pre.pitch.voicing.modern"), 180.dp)
                    }
                    PreBlock.COMP -> Hint("Evens out your picking before the pedals and the amp: INPUT sets how hard it works.")
                    PreBlock.NAM1, PreBlock.NAM2 -> Hint("A captured pedal in front of the amp. GAIN drives the capture; BASS, MID and TREBLE shape it; LEVEL sets what leaves it.")
                }
            }
            KnobRow(c, KnobRows.forPre(b, s))
        },
    )
}

/** PRE's two capture slots stay one tap away whichever PRE block is focused: "NAM 1" over the capture's full name, the menu chevron. */
@Composable
private fun CaptureSlot(c: Ctx, slot: Int, focused: Boolean) {
    val source = remember { MutableInteractionSource() }
    val pressed by source.collectIsPressedAsState()
    Row(
        Modifier.ids("pre.nam$slot.capture").width(132.dp).heightIn(min = 48.dp)
            .clickable(source, indication = null, role = Role.Button, onClickLabel = "Choose NAM $slot capture") { c.d(Action.Open(Overlay.CaptureMenu(slot))) }
            .padding(vertical = 4.dp)
            .then(if (!focused) Modifier.hairlineFrame(if (pressed) VoLumColors.GoldDim else VoLumColors.BtnOffBorder, radius = 3.dp) else Modifier)
            .volumSelection(focused, pressed, VoLumSelectionStyle.Brass, radius = 3.dp, inset = 0.dp)
            .padding(horizontal = 8.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        Column(Modifier.weight(1f)) {
            VText("NAM $slot", style = VoLumType.label(9.sp, VoLumColors.GoldDim))
            VFitText(captureFullName(c.s, slot), style = VoLumType.label(12.sp, if (focused) VoLumColors.SelText else VoLumColors.Cream), minSize = 8.sp)
        }
        VoLumIcon(VoLumGlyph.ChevronDown, color = VoLumColors.Gold, size = 10.dp)
    }
}

@Composable
internal fun PostEditor(c: Ctx, b: PostBlock) {
    val s = c.s
    Editor(
        left = { EffectArt(b.motif, s.on(b.onParam), 0, { c.d(Action.ToggleParam(b.onParam)) }, "post.${b.key}.on") },
        right = {
            EffectHeader(c, postTitle(b), Section.POST)
            Row(Modifier.fillMaxWidth().heightIn(min = 62.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(14.dp)) {
                when (b) {
                    PostBlock.CHORUS -> ModeGroup(c, "MODE", Param.CHORUS_MODE, listOf("classic", "warped", "clear", "ensemble").map { "post.chorus.mode.$it" }, 340.dp)
                    PostBlock.DELAY -> {
                        ModeGroup(c, "MODE", Param.DELAY_MODE, listOf("digital", "analog", "reverse").map { "post.delay.mode.$it" }, 240.dp)
                        VoLumToggle(s.on(Param.DELAY_PING_PONG), { c.d(Action.ToggleParam(Param.DELAY_PING_PONG)) }, Modifier.ids("post.delay.pingpong"), label = "PING-PONG")
                        VoLumToggle(s.on(Param.DELAY_SYNC), { c.d(Action.ToggleParam(Param.DELAY_SYNC)) }, Modifier.ids("post.delay.sync"), label = "TEMPO SYNC")
                    }
                    PostBlock.REVERB -> {
                        ModeGroup(c, "MODE", Param.REVERB_MODE, listOf("hall", "plate", "oktaverb").map { "post.reverb.mode.$it" }, 240.dp)
                        if (s.param(Param.REVERB_MODE) == 2.0) ModeGroup(c, "OKTAVERB VOICE", Param.REVERB_SUB, listOf("halo", "shimmer", "bloom").map { "post.reverb.sub.$it" }, 240.dp)
                    }
                    PostBlock.TREM -> {
                        ModeGroup(c, "MODE", Param.TREM_MODE, listOf("optical", "bias", "harmonic").map { "post.trem.mode.$it" }, 270.dp)
                        VoLumToggle(s.on(Param.TREM_SYNC), { c.d(Action.ToggleParam(Param.TREM_SYNC)) }, Modifier.ids("post.trem.sync"), label = "TEMPO SYNC")
                    }
                }
            }
            val division = when {
                b == PostBlock.DELAY && s.on(Param.DELAY_SYNC) -> Param.DELAY_DIVISION to "post.delay.division"
                b == PostBlock.TREM && s.on(Param.TREM_SYNC) -> Param.TREM_DIVISION to "post.trem.division"
                else -> null
            }
            KnobRow(c, KnobRows.forPost(b, s), slot = division?.let { (p, id) -> { Division(c, p, id) } })
        },
    )
}

/** Tempo sync swaps TIME / RATE for the note division, in the same place in the row. */
@Composable
private fun Division(c: Ctx, p: Param, id: String) {
    val i = c.s.param(p).roundToInt().coerceIn(0, DIVISIONS.lastIndex)
    val step = { d: Int -> c.d(Action.SetParam(p, Math.floorMod(i + d, DIVISIONS.size).toDouble())) }
    Column(Modifier.widthIn(min = 140.dp), horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(2.dp)) {
        VText("DIVISION", style = VoLumType.KnobLabel)
        VoLumChannelStepper(DIVISIONS[i], { step(-1) }, { step(1) }, prevModifier = Modifier.ids(id).desc("Shorter division"), nextModifier = Modifier.ids(id).desc("Longer division"))
        VText("follows the metronome", style = VoLumType.label(9.sp, VoLumColors.CreamDim))
    }
}
