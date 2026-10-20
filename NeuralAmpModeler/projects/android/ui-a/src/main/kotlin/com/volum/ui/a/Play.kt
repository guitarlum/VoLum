package com.volum.ui.a

import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.Crossfade
import androidx.compose.animation.core.FastOutSlowInEasing
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInVertically
import androidx.compose.animation.slideOutVertically
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.LocalIndication
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsPressedAsState
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
import androidx.compose.foundation.selection.toggleable
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.semantics.CustomAccessibilityAction
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.customActions
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.InterfaceStatus
import com.volum.core.Overlay
import com.volum.core.Param
import com.volum.core.Permission
import com.volum.core.PostBlock
import com.volum.core.PreBlock
import com.volum.core.VoLumState
import com.volum.design.art.AmpThumbnail
import com.volum.design.art.ArtInput
import com.volum.design.art.ArtRef
import com.volum.design.art.Motif
import com.volum.design.components.MotifArt
import com.volum.design.components.PlayBanner
import com.volum.design.components.PlayStagePanel
import com.volum.design.components.VoLumBanner
import com.volum.design.components.VoLumBannerKind
import com.volum.design.components.VoLumButton
import com.volum.design.components.VoLumGlyph
import com.volum.design.components.VoLumIcon
import com.volum.design.components.VoLumLed
import com.volum.design.components.VoLumListCaption
import com.volum.design.components.VoLumSelectionStyle
import com.volum.design.components.cornerBrackets
import com.volum.design.components.hairlineFrame
import com.volum.design.components.panelDepth
import com.volum.design.components.twoDigits
import com.volum.design.components.volumSelection
import com.volum.design.theme.VFitText
import com.volum.design.theme.VText
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumType
import com.volum.design.theme.withOpacity

private val SoundPitch = 56.dp

/** A Sound recall slides the banner in the direction you stepped, so a step reads as a step even on a stage glance. */
private const val RecallMs = 280

@Composable
internal fun PlaySurface(c: Ctx) {
    val s = c.s
    Row(Modifier.fillMaxSize().padding(start = 4.dp, end = 4.dp, top = 6.dp, bottom = 6.dp)) {
        MeterColumn(c, out = false, Modifier.fillMaxHeight().ids("play.meters"))
        Column(Modifier.weight(1f).fillMaxHeight().padding(horizontal = 6.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            if (!inputLive(s)) InputProblem(c)
            if (s.ui.banner != null) BannerSlot(c)
            Stage(c, Modifier.weight(1f).fillMaxWidth())
            SoundBanner(c)
            Stomps(c)
        }
        SoundsColumn(c, Modifier.width(272.dp).fillMaxHeight())
        MeterColumn(c, out = true, Modifier.fillMaxHeight().padding(start = 4.dp), limited = s.outputSafety)
    }
}

/** The input problem in its own slot above the stage, with the way out next to it; nothing sits under it. */
@Composable
private fun InputProblem(c: Ctx) {
    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        VoLumBanner(inputProblem(c.s), VoLumBannerKind.ERROR, null, Modifier.weight(1f))
        VoLumButton("Fix input", { c.d(Action.Open(Overlay.AudioStatus)) }, Modifier.ids("nav.audio_status.open"), primary = true)
    }
}

internal fun inputProblem(s: VoLumState): String = when {
    s.audio.permission != Permission.GRANTED -> "No input: VoLum may not use the microphone."
    s.audio.status == InterfaceStatus.NO_INTERFACE -> "No input: no USB audio interface is connected."
    else -> "No input: the USB interface was unplugged."
}

/** The shared moving stage; a recall that changes the amp crossfades between the two arts. */
@Composable
private fun Stage(c: Ctx, modifier: Modifier) {
    val amp = c.s.amp
    val input = remember(c.tel) { ArtInput { c.tel.value } }
    Crossfade(amp?.let { ArtRef.of(it) }, modifier.ids("play.art"), tween(RecallMs), label = "stage") { art ->
        if (art != null) PlayStagePanel(art, input, Modifier.fillMaxSize(), animate = c.s.signal.animateArt)
    }
}

/** The LIVE Sound under the stage, with large prev / next on either side for a fingertip at arm's length. */
@Composable
private fun SoundBanner(c: Ctx) {
    val s = c.s
    val live = s.library.live
    val name = live?.let { s.library.sounds[it] }?.let { s.library.preset(it.presetId)?.name } ?: s.presetName ?: "Unsaved sound"
    Row(Modifier.fillMaxWidth().height(56.dp), verticalAlignment = Alignment.CenterVertically) {
        GlyphButton(VoLumGlyph.Prev, { c.d(Action.StepSound(-1)) }, "Previous sound", Modifier.ids("play.sound.prev").size(56.dp), tint = VoLumColors.Gold, size = 18.dp)
        AnimatedContent(
            Triple(live, name, s.rig.dirty),
            Modifier.weight(1f).fillMaxHeight(),
            transitionSpec = {
                val up = (targetState.first ?: 0) >= (initialState.first ?: 0)
                val dir = if (up) 1 else -1
                (fadeIn(tween(RecallMs, easing = FastOutSlowInEasing)) + slideInVertically(tween(RecallMs, easing = FastOutSlowInEasing)) { dir * it / 2 }) togetherWith
                    (fadeOut(tween(RecallMs / 2)) + slideOutVertically(tween(RecallMs)) { -dir * it / 2 })
            },
            contentKey = { it.first to it.second },
            label = "recall",
        ) { (program, title, dirty) ->
            PlayBanner(title, Modifier.fillMaxSize().ids("play.sound.name"), program = program, secondary = s.amp?.name, dirty = dirty)
        }
        GlyphButton(VoLumGlyph.Next, { c.d(Action.StepSound(1)) }, "Next sound", Modifier.ids("play.sound.next").size(56.dp), tint = VoLumColors.Gold, size = 18.dp)
    }
}

@Composable
private fun Stomps(c: Ctx) {
    Row(Modifier.fillMaxWidth().height(112.dp), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
        PreBlock.entries.forEach { b -> Stomp(c, b.motif, stompLabel(b, c.s), b.onParam, "play.stomp.${b.key}", pitchVariant(b, c.s), Modifier.weight(1f)) }
        Spacer(Modifier.width(6.dp).fillMaxHeight().drawBehind {
            drawLine(VoLumColors.Frame, androidx.compose.ui.geometry.Offset(size.width / 2, 8.dp.toPx()), androidx.compose.ui.geometry.Offset(size.width / 2, size.height - 8.dp.toPx()), 1.dp.toPx())
        })
        PostBlock.entries.forEach { b -> Stomp(c, b.motif, postCard(b), b.onParam, "play.stomp.${b.key}", 0, Modifier.weight(1f)) }
    }
}

/**
 * One card per pedal. The face (its motif and lamp) is the footswitch: a tap
 * turns the pedal on or off and the card shows it, lit and teal-framed or ghosted.
 * The footer with the pencil is the explicit way into that pedal's BUILD editor.
 */
@Composable
private fun Stomp(c: Ctx, motif: Motif, label: String, p: Param, id: String, variant: Int, modifier: Modifier) {
    val on = c.s.on(p)
    val source = remember { MutableInteractionSource() }
    val pressed by source.collectIsPressedAsState()
    val edge = when {
        pressed -> if (on) VoLumColors.Teal else VoLumColors.GoldDim
        on -> VoLumColors.TealDim
        else -> VoLumColors.Frame
    }
    Column(modifier.fillMaxHeight().panelDepth(4.dp).hairlineFrame(edge, radius = 4.dp).cornerBrackets(edge, 8.dp, 4.dp)) {
        Box(
            Modifier.ids(id).weight(1f).fillMaxWidth()
                .toggleable(on, source, indication = LocalIndication.current, role = Role.Switch) { c.d(Action.ToggleParam(p)) }
                .desc(label),
        ) {
            MotifArt(motif, on, Modifier.fillMaxSize().padding(start = 4.dp, end = 4.dp, top = 4.dp), variant)
            VoLumLed(on, Modifier.align(Alignment.TopEnd).padding(3.dp))
        }
        Row(
            Modifier.ids("play.stomp.edit").fillMaxWidth().height(48.dp).clickable(role = Role.Button, onClickLabel = "Edit $label") { c.d(Action.EditStomp(p)) }
                .drawBehind { drawLine(edge.withOpacity(0.5f), androidx.compose.ui.geometry.Offset(4.dp.toPx(), 0f), androidx.compose.ui.geometry.Offset(size.width - 4.dp.toPx(), 0f), 1.dp.toPx()) }
                .padding(horizontal = 6.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(3.dp),
        ) {
            VFitText(label, Modifier.weight(1f), style = VoLumType.label(11.sp, if (on) VoLumColors.Cream else VoLumColors.CreamDim), minSize = 7.sp)
            VoLumIcon(VoLumGlyph.Edit, color = VoLumColors.GoldDim, size = 13.dp)
        }
    }
}

@Composable
private fun SoundsColumn(c: Ctx, modifier: Modifier) {
    val s = c.s
    val programs = s.library.sounds.keys.sorted()
    val liveRow = programs.indexOf(s.library.live)
    Column(modifier.padding(start = 4.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
        VoLumListCaption("SOUNDS  ·  ${programs.size} OF 128")
        RowList(SoundPitch, Modifier.weight(1f).fillMaxWidth(), scrollToRow = liveRow - 1) {
            programs.forEach { p -> SoundRow(c, p) }
        }
        VoLumButton("+  Add this sound", { c.d(Action.AddThisSound) }, Modifier.fillMaxWidth().ids("play.add_this", "play.add_this.unnamed", "play.add_this.full"))
        VoLumButton("+  Add from list", { c.d(Action.Open(Overlay.SoundPicker(s.library.firstFreeProgram() ?: 0, replace = false))) }, Modifier.fillMaxWidth().ids("play.add_sound"))
    }
}

/** A Sound: program in Michroma, name in Poiret, amp in teal. The LIVE row carries Brass selection and opens its actions under it. */
@Composable
private fun SoundRow(c: Ctx, p: Int) {
    val s = c.s
    val sound = s.library.sounds.getValue(p)
    val valid = s.library.soundValid(p)
    val live = s.library.live == p
    val moveDown = Modifier.semantics { customActions = listOf(CustomAccessibilityAction("Move down") { c.d(Action.MoveSound(p, 1)); true }) }
    val source = remember { MutableInteractionSource() }
    val pressed by source.collectIsPressedAsState()
    val ids = if (valid) listOf("play.sound.recall", "play.sound.reorder") else listOf("play.sound.replace", "play.sound.reorder")
    val onTap = { if (valid) c.d(Action.RecallSound(p)) else c.d(Action.Open(Overlay.SoundPicker(p, replace = true))) }
    val amp = s.library.amp(sound.ampId)
    Column(Modifier.fillMaxWidth()) {
        Row(
            Modifier.ids(ids).then(moveDown).fillMaxWidth().height(SoundPitch)
                .clickable(source, indication = null, role = Role.Button, onClick = onTap)
                .padding(vertical = 2.dp)
                .then(if (!valid) Modifier.drawBehind { drawRect(VoLumColors.DangerFill.withOpacity(0.22f)) }.hairlineFrame(VoLumColors.Danger.withOpacity(0.7f), radius = 3.dp) else Modifier)
                .volumSelection(live, pressed, VoLumSelectionStyle.Brass, radius = 3.dp, inset = 0.dp)
                .padding(horizontal = 8.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            VText(twoDigits(p), Modifier.width(30.dp).ids("play.sound.program"), style = VoLumType.value(13.sp, if (live) VoLumColors.Gold else VoLumColors.GoldDim.withOpacity(0.7f)))
            if (valid && amp != null) AmpThumbnail(ArtRef.of(amp), Modifier.size(26.dp), live)
            Column(Modifier.weight(1f)) {
                if (valid) {
                    VFitText(s.library.preset(sound.presetId)?.name ?: "", style = VoLumType.SoundRowName, minSize = 12.sp)
                    VText(amp?.name ?: "", style = VoLumType.label(9.sp, VoLumColors.TealDim))
                } else {
                    VText("Invalid slot", Modifier.ids("play.sound.invalid"), style = VoLumType.label(13.sp, VoLumColors.Danger))
                    VText("Its Sound is gone: tap to pick a new one", style = VoLumType.label(9.sp, VoLumColors.Danger.withOpacity(0.8f)))
                }
            }
            if (live) VText("LIVE", Modifier.ids("play.sound.live"), style = VoLumType.label(10.sp, VoLumColors.Gold))
        }
        if (live) {
            Row(Modifier.fillMaxWidth().height(SoundPitch), horizontalArrangement = Arrangement.spacedBy(2.dp), verticalAlignment = Alignment.CenterVertically) {
                GlyphAction(VoLumGlyph.Edit, "REPLACE", { c.d(Action.Open(Overlay.SoundPicker(p, replace = true))) }, Modifier.weight(1f).ids("play.sound.replace"), vertical = true)
                GlyphAction(VoLumGlyph.ChevronDown, "MOVE DOWN", { c.d(Action.MoveSound(p, 1)) }, Modifier.weight(1f).ids("play.sound.reorder").then(moveDown), vertical = true)
                GlyphAction(VoLumGlyph.Delete, "CLEAR", { c.d(Action.ClearSound(p)) }, Modifier.weight(1f).ids("play.sound.clear"), tint = VoLumColors.Danger, vertical = true)
            }
        }
    }
}
