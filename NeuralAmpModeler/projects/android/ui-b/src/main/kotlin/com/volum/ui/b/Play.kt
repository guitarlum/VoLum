package com.volum.ui.b

import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectDragGesturesAfterLongPress
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsPressedAsState
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
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.draw.drawWithCache
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.PathEffect
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.hapticfeedback.HapticFeedbackType
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalHapticFeedback
import androidx.compose.ui.semantics.CustomAccessibilityAction
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.customActions
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.Overlay
import com.volum.core.Param
import com.volum.core.ScreenId
import com.volum.core.Sound
import com.volum.core.VoLumState
import com.volum.design.art.ArtRef
import com.volum.design.art.AmpThumbnail
import com.volum.design.art.Motif
import com.volum.design.components.PedalCard
import com.volum.design.components.PlayStagePanel
import com.volum.design.components.VoLumGlyph
import com.volum.design.components.VoLumIcon
import com.volum.design.components.VoLumMeter
import com.volum.design.components.hairlineFrame
import com.volum.design.components.panelDepth
import com.volum.design.components.volumSelection
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumType
import com.volum.design.theme.withOpacity
import com.volum.harness.featureId
import com.volum.harness.screen
import kotlin.math.roundToInt

private val preStomps = listOf(Param.PITCH_ON, Param.COMP_ON, Param.NAM1_ON, Param.NAM2_ON)
private val postStomps = listOf(Param.CHORUS_ON, Param.DELAY_ON, Param.REVERB_ON, Param.TREM_ON)

internal fun stompLabel(p: Param, s: VoLumState): String = when (p) {
    Param.NAM1_ON -> captureName(s, Param.NAM1_CAPTURE) ?: "NAM 1"
    Param.NAM2_ON -> captureName(s, Param.NAM2_CAPTURE) ?: "NAM 2"
    Param.TREM_ON -> "TREMOLO"
    else -> p.label
}

internal fun motifOf(p: Param): Motif = when (p) {
    Param.PITCH_ON -> Motif.PITCH
    Param.COMP_ON -> Motif.COMP
    Param.NAM1_ON -> Motif.NAM1
    Param.NAM2_ON -> Motif.NAM2
    Param.CHORUS_ON -> Motif.CHORUS
    Param.DELAY_ON -> Motif.DELAY
    Param.REVERB_ON -> Motif.REVERB
    else -> Motif.TREM
}

internal fun captureName(s: VoLumState, slot: Param): String? {
    val idx = s.param(slot).toInt()
    return s.library.pedals.firstOrNull { it.index == idx }?.takeIf { it.index != 0 }?.name
}

/** What a floor cell holds: a Sound on its program, or one of the two ways to add one. */
private sealed interface FloorCell {
    data class Slot(val program: Int, val sound: Sound) : FloorCell
    data object AddThis : FloorCell
    data object AddPreset : FloorCell
}

/**
 * PLAY, the stage floor: PRE stomps down the left edge and POST down the right
 * (signal flows across the phone), the Sound tiles in the middle (a tap only
 * recalls), big PREV / NEXT under them, IN and OUT meters at the edges.
 */
@Composable
internal fun PlaySurface(s: VoLumState) {
    val st = LocalStage.current
    Column(Modifier.fillMaxSize().screen(ScreenId.PLAY)) {
        Header(s) { SlotTools(s) }
        if (s.ui.top == null) BannerSlot(s, Modifier.padding(horizontal = 12.dp))
        Row(Modifier.weight(1f).fillMaxWidth().padding(start = 6.dp, end = 6.dp, top = 8.dp, bottom = 8.dp), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            VoLumMeter({ st.level(true) }, "IN", Modifier.width(28.dp).fillMaxHeight().featureId("play.meters"), frame = { st.frames.longValue })
            StompColumn(s, preStomps, editAfter = true)
            Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                SoundFloor(s, Modifier.weight(1f).fillMaxWidth())
                Transport(s, Modifier.fillMaxWidth().height(64.dp))
            }
            StompColumn(s, postStomps, editAfter = false)
            VoLumMeter({ st.level(false) }, "OUT", Modifier.width(28.dp).fillMaxHeight().featureId("play.meters"), out = true, frame = { st.frames.longValue })
        }
    }
}

/**
 * The LIVE Sound's management, up in the header and away from the floor so a
 * switching tap can never land on it: REPLACE opens the picker on its program,
 * CLEAR empties it (with an undo toast).
 */
@Composable
private fun RowScope.SlotTools(s: VoLumState) {
    val st = LocalStage.current
    val live = s.library.live
    val sound = live?.let { s.library.sounds[it] }
    if (live != null && sound != null) {
        Column(Modifier.width(52.dp)) {
            FitText("LIVE SLOT", VoLumType.label(8.sp, VoLumColors.CreamDim), Modifier.fillMaxWidth().height(12.dp), align = TextAlign.Start)
            FitText(twoDigit(live), VoLumType.value(16.sp, VoLumColors.Gold), Modifier.fillMaxWidth().height(22.dp), align = TextAlign.Start)
        }
        Pad({ st.go(Action.Open(Overlay.SoundPicker(live, replace = true))) }, Modifier.width(112.dp).height(48.dp), arrayOf("play.sound.replace"),
            description = "Replace the Sound on program ${twoDigit(live)}") { ink ->
            Row(Modifier.padding(horizontal = 10.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                VoLumIcon(VoLumGlyph.Edit, color = ink, size = 16.dp)
                FitText("REPLACE", VoLumType.label(12.sp, ink), Modifier.weight(1f))
            }
        }
        Pad({
            val name = s.library.preset(sound.presetId)?.name ?: "Invalid slot"
            st.cleared.value = Cleared(live, sound, name)
            st.go(Action.ClearSound(live))
        }, Modifier.width(96.dp).height(48.dp), arrayOf("play.sound.clear"), tone = Tone.DANGER, description = "Clear program ${twoDigit(live)}") { ink ->
            Row(Modifier.padding(horizontal = 10.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                VoLumIcon(VoLumGlyph.Delete, color = ink, size = 16.dp)
                FitText("CLEAR", VoLumType.label(12.sp, ink), Modifier.weight(1f))
            }
        }
    }
    Spacer(Modifier.weight(1f))
}

@Composable
private fun StompColumn(s: VoLumState, stomps: List<Param>, editAfter: Boolean) {
    Column(Modifier.width(146.dp).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
        stomps.forEach { Stomp(s, it, editAfter) }
    }
}

/** One stomp: the `:design` pedal card toggles bypass from anywhere on it; the inner EDIT tab opens it in BUILD. */
@Composable
private fun ColumnScope.Stomp(s: VoLumState, p: Param, editAfter: Boolean) {
    val st = LocalStage.current
    val haptics = LocalHapticFeedback.current
    val on = s.on(p)
    val toggle = { haptics.performHapticFeedback(HapticFeedbackType.LongPress); st.go(Action.ToggleParam(p)) }
    Row(Modifier.weight(1f).fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
        val card: @Composable RowScope.() -> Unit = {
            PedalCard(motifOf(p), stompLabel(p, s), on, onClick = toggle, onToggle = { toggle() },
                Modifier.weight(1f).fillMaxHeight().ids(Ids.stomp(p)).semantics { contentDescription = "${stompLabel(p, s)} ${if (on) "on" else "bypassed"}" })
        }
        val edit: @Composable RowScope.() -> Unit = {
            Pad({ st.go(Action.EditStomp(p)) }, Modifier.width(48.dp).fillMaxHeight(), arrayOf("play.stomp.edit"), tone = Tone.FRAMED,
                description = "Edit ${stompLabel(p, s)} in BUILD") { ink ->
                Column(horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(4.dp)) {
                    VoLumIcon(VoLumGlyph.Edit, color = ink, size = 16.dp)
                    FitText("EDIT", VoLumType.label(8.sp, VoLumColors.CreamDim), Modifier.width(40.dp).height(12.dp))
                }
            }
        }
        if (editAfter) { card(); edit() } else { edit(); card() }
    }
}

@Composable
private fun SoundFloor(s: VoLumState, modifier: Modifier) {
    val st = LocalStage.current
    val sounds = s.library.sounds.toSortedMap().toList()
    val cells = sounds.map { (p, sd) -> FloorCell.Slot(p, sd) } + FloorCell.AddThis + FloorCell.AddPreset
    val order = sounds.map { it.first }
    val names = sounds.map { (_, sd) -> s.library.preset(sd.presetId)?.name ?: "Invalid slot" }
    FitGrid(cells, columns = 4, minRow = 112.dp, modifier = modifier, gap = 8.dp, maxRow = 150.dp,
        shared = SharedText(names, VoLumType.display(21.sp, VoLumColors.TextBright), 20.dp)) { cell ->
        when (cell) {
            is FloorCell.Slot -> SoundTile(s, cell.program, cell.sound, order)
            FloorCell.AddThis -> AddTile("ADD THIS SOUND", "keep what you hear",
                arrayOf("play.add_this", "play.add_this.unnamed", "play.add_this.full")) { st.go(Action.AddThisSound) }
            FloorCell.AddPreset -> {
                AddTile("ADD FROM PRESETS", "pick a saved preset", arrayOf("play.add_sound")) {
                    st.go(Action.Open(Overlay.SoundPicker(s.library.firstFreeProgram() ?: 0, replace = false)))
                }
            }
        }
    }
}

/**
 * A Sound tile: a tap recalls, and that is all a tap does. The LIVE tile carries the
 * moving PLAY stage art (corona follows the playing level); the others show the amp's
 * thumbnail. Long-press and drag reorders; Move down / up are accessibility actions.
 */
@Composable
private fun SoundTile(s: VoLumState, program: Int, sound: Sound, order: List<Int>) {
    val st = LocalStage.current
    val haptics = LocalHapticFeedback.current
    val valid = s.library.soundValid(program)
    val live = s.library.live == program
    val preset = s.library.preset(sound.presetId)
    val amp = s.library.amp(sound.ampId)
    val source = remember { MutableInteractionSource() }
    val pressed by source.collectIsPressedAsState()
    var drag by remember { mutableStateOf(Offset.Zero) }
    val moveDown = CustomAccessibilityAction("Move down") { st.go(Action.MoveSound(program, 1)); true }
    val moveUp = CustomAccessibilityAction("Move up") { st.go(Action.MoveSound(program, -1)); true }
    val nameStyle = LocalGridText.current ?: VoLumType.display(21.sp, VoLumColors.TextBright)
    Box(
        Modifier.fillMaxSize()
            .graphicsLayer {
                translationX = drag.x; translationY = drag.y
                val k = if (drag != Offset.Zero) 1.04f else if (pressed) 0.97f else 1f
                scaleX = k; scaleY = k
            }
            .then(
                when {
                    !valid -> Modifier.drawBehind { drawRoundRect(VoLumColors.DangerFill.withOpacity(0.22f), cornerRadius = CornerRadius(4.dp.toPx())) }
                        .hairlineFrame(VoLumColors.Danger.withOpacity(0.7f), radius = 4.dp)
                    live -> Modifier
                    else -> Modifier.panelDepth(4.dp).hairlineFrame(VoLumColors.Frame, radius = 4.dp)
                },
            )
            .pointerInput(program, order) {
                val tileW = size.width.toFloat()
                val tileH = size.height.toFloat()
                detectDragGesturesAfterLongPress(
                    onDragEnd = {
                        val gap = 8.dp.toPx()
                        val steps = (drag.x / (tileW + gap)).roundToInt() + 4 * (drag.y / (tileH + gap)).roundToInt()
                        val from = order.indexOf(program)
                        val to = (from + steps).coerceIn(0, order.lastIndex)
                        if (to != from) st.go(Action.MoveSound(program, order[to] - program))
                        drag = Offset.Zero
                    },
                    onDragCancel = { drag = Offset.Zero },
                ) { change, d -> change.consume(); drag += d }
            }
            .clickable(source, null, role = Role.Button) {
                haptics.performHapticFeedback(HapticFeedbackType.LongPress)
                st.go(Action.RecallSound(program))
            }
            .semantics {
                customActions = listOf(moveDown, moveUp)
                contentDescription = "Program ${twoDigit(program)}"
            }
            .ids(if (valid) arrayOf("play.sound.recall", "play.sound.reorder") else arrayOf("play.sound.invalid", "play.sound.reorder")),
    ) {
        if (live && valid && amp != null) {
            PlayStagePanel(ArtRef.of(amp), st.input, Modifier.fillMaxSize().featureId("play.art"), animate = s.signal.animateArt)
            Box(Modifier.fillMaxSize().drawBehind {
                drawRect(Brush.verticalGradient(listOf(Color.Transparent, VoLumColors.HeroBg.withOpacity(0.92f)), startY = size.height * 0.42f, endY = size.height))
            })
        } else if (valid && amp != null) {
            AmpThumbnail(ArtRef.of(amp), Modifier.align(Alignment.TopEnd).padding(top = 9.dp, end = 9.dp).size(34.dp))
        }
        Box(Modifier.fillMaxSize().volumSelection(false, pressed, Sel.Brass, radius = 4.dp, inset = 0.dp))
        Column(Modifier.fillMaxSize().padding(start = 10.dp, end = 10.dp, top = 6.dp, bottom = 7.dp)) {
            Box(Modifier.featureId("play.sound.program")) {
                FitText(twoDigit(program), VoLumType.value(15.sp, if (valid) VoLumColors.Gold else VoLumColors.CreamDim), Modifier.width(44.dp).height(22.dp), align = TextAlign.Start)
            }
            Spacer(Modifier.weight(1f))
            if (valid) {
                FitText(preset?.name ?: "", nameStyle.copy(color = if (live) VoLumColors.TextBright else VoLumColors.Cream), Modifier.fillMaxWidth().height(28.dp), align = TextAlign.Start)
                Row(Modifier.fillMaxWidth().height(18.dp), verticalAlignment = Alignment.CenterVertically) {
                    FitText(amp?.name ?: "", VoLumType.label(10.sp, VoLumColors.Teal), Modifier.weight(1f), align = TextAlign.Start)
                    if (live) {
                        Box(Modifier.padding(start = 4.dp).volumSelection(true, style = Sel.AmberPicker).padding(horizontal = 6.dp, vertical = 1.dp).featureId("play.sound.live")) {
                            FitText("LIVE", VoLumType.label(9.sp, VoLumColors.AmberInk), Modifier.width(26.dp).height(14.dp))
                        }
                    }
                }
            } else {
                FitText("Invalid slot", VoLumType.label(14.sp, VoLumColors.Danger), Modifier.fillMaxWidth().height(22.dp), align = TextAlign.Start)
                FitText("its preset was deleted · REPLACE or CLEAR it", VoLumType.body(10.sp, VoLumColors.Cream), Modifier.fillMaxWidth().height(16.dp), align = TextAlign.Start)
            }
        }
    }
}

/** The desktop's dashed "+ Add this sound" cell, at the end of the floor. */
@Composable
private fun AddTile(title: String, hint: String, ids: Array<String>, onClick: () -> Unit) {
    val source = remember { MutableInteractionSource() }
    val pressed by source.collectIsPressedAsState()
    Column(
        Modifier.fillMaxSize()
            .clickable(source, null, role = Role.Button, onClick = onClick)
            .drawWithCache {
                val px = 1.dp.toPx()
                val dash = Stroke(1.2f * px, pathEffect = PathEffect.dashPathEffect(floatArrayOf(5 * px, 4 * px)))
                val cr = CornerRadius(4.dp.toPx())
                onDrawBehind {
                    if (pressed) drawRoundRect(VoLumColors.SelBg, cornerRadius = cr)
                    drawRoundRect(if (pressed) VoLumColors.Gold else VoLumColors.GoldDim.withOpacity(0.7f), Offset(px, px), Size(size.width - 2 * px, size.height - 2 * px), cr, style = dash)
                }
            }
            .ids(ids)
            .padding(10.dp),
        verticalArrangement = Arrangement.spacedBy(6.dp, Alignment.CenterVertically),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        VoLumIcon(VoLumGlyph.Plus, color = VoLumColors.Gold, size = 20.dp)
        FitText(title, VoLumType.label(12.sp, VoLumColors.Gold), Modifier.fillMaxWidth().height(18.dp))
        FitText(hint, VoLumType.body(11.sp, VoLumColors.CreamDim), Modifier.fillMaxWidth().height(16.dp))
    }
}

/** Arm's-length transport: previous / next Sound around the big readout of what is playing now. */
@Composable
private fun Transport(s: VoLumState, modifier: Modifier) {
    val st = LocalStage.current
    val live = s.library.live
    val sound = live?.let { s.library.sounds[it] }
    val amp = s.library.amp(sound?.ampId ?: s.ampId)
    val name = sound?.let { s.library.preset(it.presetId)?.name } ?: s.presetName ?: "Tap a Sound"
    Row(modifier, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        Pad({ st.go(Action.StepSound(-1)) }, Modifier.width(112.dp).fillMaxHeight(), arrayOf("play.sound.prev"), haptic = true, description = "Previous Sound") { ink ->
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                VoLumIcon(VoLumGlyph.Prev, color = VoLumColors.Gold, size = 18.dp)
                FitText("PREV", VoLumType.label(15.sp, ink), Modifier.width(48.dp))
            }
        }
        Row(
            Modifier.weight(1f).fillMaxHeight().panelDepth(4.dp).hairlineFrame(VoLumColors.GoldDim.withOpacity(0.5f), radius = 4.dp)
                .featureId("play.sound.name").padding(horizontal = 14.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            FitText(live?.let { twoDigit(it) } ?: "--", VoLumType.value(19.sp, VoLumColors.Gold), Modifier.width(40.dp))
            Column(Modifier.weight(1f)) {
                FitText(name, VoLumType.display(26.sp, VoLumColors.TextBright), Modifier.fillMaxWidth().height(32.dp), align = TextAlign.Start)
                Row(Modifier.fillMaxWidth().height(16.dp), verticalAlignment = Alignment.CenterVertically) {
                    FitText(amp?.name ?: "", VoLumType.label(10.sp, VoLumColors.TealDim), Modifier.weight(1f), align = TextAlign.Start)
                    if (s.rig.dirty) FitText("(unsaved)", VoLumType.label(10.sp, VoLumColors.Amber), Modifier.width(64.dp), align = TextAlign.End)
                }
            }
        }
        Pad({ st.go(Action.StepSound(1)) }, Modifier.width(112.dp).fillMaxHeight(), arrayOf("play.sound.next"), haptic = true, description = "Next Sound") { ink ->
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                FitText("NEXT", VoLumType.label(15.sp, ink), Modifier.width(48.dp))
                VoLumIcon(VoLumGlyph.Next, color = VoLumColors.Gold, size = 18.dp)
            }
        }
    }
}
