package com.volum.ui.b

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.detectDragGesturesAfterLongPress
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
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicText
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.semantics.CustomAccessibilityAction
import androidx.compose.ui.semantics.customActions
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.NamePurpose
import com.volum.core.Overlay
import com.volum.core.Param
import com.volum.core.Reducer
import com.volum.core.ScreenId
import com.volum.core.Sound
import com.volum.core.VoLumState
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

internal fun captureName(s: VoLumState, slot: Param): String? {
    val idx = s.param(slot).toInt()
    return s.library.pedals.firstOrNull { it.index == idx }?.takeIf { it.index != 0 }?.name
}

/** PLAY: the stage floor. Sounds are big pads in the middle, PRE stomps on the left edge, POST on the right, prev/next at the bottom. */
@Composable
internal fun PlaySurface(s: VoLumState) {
    Box(Modifier.fillMaxSize().background(Ink.ground).screen(ScreenId.PLAY)) {
        Column(Modifier.fillMaxSize()) {
            TopBar(s) { PlayActions(s) }
            Row(Modifier.weight(1f).fillMaxWidth().padding(start = 8.dp, end = 8.dp, bottom = 8.dp, top = 2.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                StompColumn(s, preStomps, meterFirst = true)
                Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    SoundGrid(s, Modifier.weight(1f).fillMaxWidth())
                    Transport(s, Modifier.fillMaxWidth().height(72.dp))
                }
                StompColumn(s, postStomps, meterFirst = false)
            }
        }
        if (!s.ui.buildTipSeen) BuildTip()
    }
}

@Composable
private fun RowScope.PlayActions(s: VoLumState) {
    val st = LocalStage.current
    Spacer(Modifier.weight(1f))
    TextPad("+ THIS SOUND", { st.go(Action.AddThisSound) }, Modifier.width(116.dp).height(48.dp),
        arrayOf("play.add_this", "play.add_this.unnamed", "play.add_this.full"))
    TextPad("+ SOUND", { st.go(Action.Open(Overlay.SoundPicker(s.library.firstFreeProgram() ?: 0, replace = false))) },
        Modifier.width(92.dp).height(48.dp), arrayOf("play.add_sound"))
    TextPad("SAVE", { st.go(Action.Open(Overlay.NameDialog(NamePurpose.SAVE_PRESET, Reducer.nameSuggestion(s)))) },
        Modifier.width(72.dp).height(48.dp), arrayOf("play.save"))
}

@Composable
private fun RowScope.StompColumn(s: VoLumState, stomps: List<Param>, meterFirst: Boolean) {
    Row(Modifier.width(132.dp).fillMaxHeight(), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
        if (meterFirst) Meter(input = true, Modifier.width(6.dp).fillMaxHeight().featureId("play.meters"))
        Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            stomps.forEach { Stomp(s, it, editInside = meterFirst) }
        }
        if (!meterFirst) Meter(input = false, Modifier.width(6.dp).fillMaxHeight().featureId("play.meters"))
    }
}

/** One bypass stomp: the outer part toggles (big, at the screen edge), the inner tab opens the pedal in BUILD. */
@Composable
private fun ColumnScope.Stomp(s: VoLumState, p: Param, editInside: Boolean) {
    val st = LocalStage.current
    val on = s.on(p)
    Row(Modifier.weight(1f).fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
        val toggle: @Composable RowScope.() -> Unit = {
            Pad({ st.go(Action.ToggleParam(p)) }, Modifier.weight(1f).fillMaxHeight(), Ids.stomp(p), selected = on, sel = Sel.BRASS,
                haptic = true, fill = if (on) Ink.raised else Ink.panel, description = stompLabel(p, s)) {
                Column(Modifier.fillMaxSize().padding(6.dp), horizontalAlignment = Alignment.CenterHorizontally) {
                    Canvas(Modifier.fillMaxWidth().height(14.dp)) { led(on, Offset(size.width / 2, size.height / 2), 4.dp.toPx()) }
                    PedalMotif(p.ordinal, on, Modifier.weight(1f).fillMaxWidth())
                    FitText(stompLabel(p, s), Type.label(11.sp, if (on) Ink.text else Ink.dim), Modifier.fillMaxWidth().height(18.dp))
                }
            }
        }
        val edit: @Composable RowScope.() -> Unit = {
            Pad({ st.go(Action.EditStomp(p)) }, Modifier.width(48.dp).fillMaxHeight(), arrayOf("play.stomp.edit"), fill = Ink.panel,
                description = "Edit ${stompLabel(p, s)} in BUILD") {
                Column(horizontalAlignment = Alignment.CenterHorizontally) {
                    BasicText(if (editInside) "›" else "‹", style = Type.display(22.sp, Ink.dim))
                    BasicText("EDIT", style = Type.label(8.sp))
                }
            }
        }
        if (editInside) { toggle(); edit() } else { edit(); toggle() }
    }
}

@Composable
private fun SoundGrid(s: VoLumState, modifier: Modifier) {
    val sounds = s.library.sounds.toSortedMap().toList()
    FitGrid(sounds, columns = 4, minRow = 112.dp, modifier = modifier, maxRow = 140.dp,
        empty = "No Sounds yet. + SOUND picks a preset; + THIS SOUND keeps what you hear.") { (program, sound) ->
        SoundTile(s, program, sound, sounds.map { it.first })
    }
}

/** A Sound pad: tap recalls, the corner buttons replace and clear, long-press and drag sideways reorders. */
@Composable
private fun SoundTile(s: VoLumState, program: Int, sound: Sound, order: List<Int>) {
    val st = LocalStage.current
    val valid = s.library.soundValid(program)
    val live = s.library.live == program
    val preset = s.library.preset(sound.presetId)
    val amp = s.library.amp(sound.ampId)
    var drag by remember { mutableStateOf(Offset.Zero) }
    var width by remember { mutableStateOf(1f) }
    val moveDown = CustomAccessibilityAction("Move down") { st.go(Action.MoveSound(program, 1)); true }
    val moveUp = CustomAccessibilityAction("Move up") { st.go(Action.MoveSound(program, -1)); true }
    Box(Modifier.fillMaxSize().graphicsLayer { translationX = drag.x; translationY = drag.y; if (drag != Offset.Zero) { scaleX = 1.04f; scaleY = 1.04f } }) {
        Pad(
            { st.go(Action.RecallSound(program)) },
            Modifier.fillMaxSize()
                .pointerInput(program, order) {
                    width = size.width.toFloat()
                    detectDragGesturesAfterLongPress(
                        onDragEnd = {
                            val steps = (drag.x / (width + 8.dp.toPx())).roundToInt() + 4 * (drag.y / (size.height + 8.dp.toPx())).roundToInt()
                            val from = order.indexOf(program)
                            val to = (from + steps).coerceIn(0, order.lastIndex)
                            if (to != from) st.go(Action.MoveSound(program, order[to] - program))
                            drag = Offset.Zero
                        },
                        onDragCancel = { drag = Offset.Zero },
                    ) { change, d -> change.consume(); drag += d }
                }
                .semantics { customActions = listOf(moveDown, moveUp) },
            ids = if (valid) arrayOf("play.sound.recall", "play.sound.reorder") else arrayOf("play.sound.invalid", "play.sound.reorder"),
            selected = live,
            sel = Sel.LIST_TEAL,
            fill = if (valid) Ink.panel else Ink.alert.copy(alpha = 0.18f),
            haptic = true,
            description = "Program $program",
        ) {
            if (valid) StageArt(amp?.art ?: 0, st.tele, animate = live && s.signal.animateArt, Modifier.fillMaxSize().graphicsLayer { alpha = if (live) 0.9f else 0.45f })
            Box(Modifier.fillMaxSize().background(Brush.verticalGradient(listOf(Color.Transparent, Ink.ground.copy(alpha = 0.9f)))))
            Column(Modifier.fillMaxSize().padding(start = 10.dp, top = 8.dp, bottom = 8.dp, end = 10.dp)) {
                Box(Modifier.featureId("play.sound.program")) { BasicText(program.toString().padStart(2, '0'), style = Type.tech(16.sp, if (live) Ink.hot else Ink.dim)) }
                Spacer(Modifier.weight(1f))
                if (valid) {
                    FitText(preset?.name ?: "", Type.display(21.sp, if (live) Ink.hot else Ink.text), Modifier.fillMaxWidth().height(30.dp), align = TextAlign.Start)
                    Row(Modifier.fillMaxWidth().height(18.dp), verticalAlignment = Alignment.CenterVertically) {
                        FitText(amp?.name ?: "", Type.label(10.sp), Modifier.weight(1f), align = TextAlign.Start)
                        if (live) {
                            Box(Modifier.padding(start = 4.dp).clip(RoundedCornerShape(3.dp)).background(Ink.hot).padding(horizontal = 5.dp).featureId("play.sound.live")) {
                                BasicText("LIVE", style = Type.label(9.sp, Ink.ground))
                            }
                        }
                    }
                } else {
                    FitText("Invalid slot", Type.display(20.sp, Ink.alert), Modifier.fillMaxWidth().height(28.dp), align = TextAlign.Start)
                    FitText("its preset was deleted", Type.label(10.sp), Modifier.fillMaxWidth().height(18.dp), align = TextAlign.Start)
                }
            }
        }
        Row(Modifier.align(Alignment.TopEnd)) {
            Pad({ st.go(Action.Open(Overlay.SoundPicker(program, replace = true))) }, Modifier.size(48.dp), arrayOf("play.sound.replace"),
                fill = Ink.ground.copy(alpha = 0.6f), description = "Replace program $program") {
                BasicText("⟳", style = Type.display(20.sp, Ink.dim))
            }
            Pad({ st.go(Action.ClearSound(program)) }, Modifier.size(48.dp), arrayOf("play.sound.clear"),
                fill = Ink.ground.copy(alpha = 0.6f), description = "Clear program $program") {
                BasicText("✕", style = Type.display(18.sp, Ink.dim))
            }
        }
    }
}

/** Arm's-length transport: previous / next Sound and the moving art of what is playing now. */
@Composable
private fun Transport(s: VoLumState, modifier: Modifier) {
    val st = LocalStage.current
    val live = s.library.live
    val sound = live?.let { s.library.sounds[it] }
    val amp = s.library.amp(sound?.ampId ?: s.ampId)
    val name = sound?.let { s.library.preset(it.presetId)?.name } ?: s.presetName ?: "Tap a Sound"
    Row(modifier, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        Pad({ st.go(Action.StepSound(-1)) }, Modifier.width(150.dp).fillMaxHeight(), arrayOf("play.sound.prev"), haptic = true, description = "Previous Sound") {
            BasicText("◀  PREV", style = Type.label(16.sp, Ink.text))
        }
        Box(Modifier.weight(1f).fillMaxHeight().clip(RoundedCornerShape(8.dp)).brackets()) {
            StageArt(amp?.art ?: 0, st.tele, animate = s.signal.animateArt, Modifier.fillMaxSize().featureId("play.art"))
            Row(Modifier.fillMaxSize().background(Brush.horizontalGradient(listOf(Ink.ground.copy(alpha = 0.9f), Color.Transparent))).padding(horizontal = 12.dp),
                verticalAlignment = Alignment.CenterVertically) {
                BasicText(live?.toString()?.padStart(2, '0') ?: "--", style = Type.tech(20.sp, Ink.hot))
                Column(Modifier.weight(1f).padding(start = 12.dp).featureId("play.sound.name")) {
                    FitText(name, Type.display(28.sp, Ink.hot), Modifier.fillMaxWidth().height(36.dp), align = TextAlign.Start)
                    FitText(amp?.name ?: "", Type.label(11.sp), Modifier.fillMaxWidth().height(16.dp), align = TextAlign.Start)
                }
                if (s.rig.dirty) FitText("(unsaved)", Type.label(11.sp, Ink.text), Modifier.width(72.dp))
            }
        }
        Pad({ st.go(Action.StepSound(1)) }, Modifier.width(150.dp).fillMaxHeight(), arrayOf("play.sound.next"), haptic = true, description = "Next Sound") {
            BasicText("NEXT  ▶", style = Type.label(16.sp, Ink.text))
        }
    }
}

/** First-launch note pointing at the BUILD switch. */
@Composable
private fun BuildTip() {
    Column(Modifier.offset(150.dp, 54.dp).width(250.dp).featureId("tip.build")) {
        Canvas(Modifier.padding(start = 20.dp).size(14.dp, 8.dp)) {
            drawPath(androidx.compose.ui.graphics.Path().apply { moveTo(0f, size.height); lineTo(size.width / 2, 0f); lineTo(size.width, size.height); close() }, Ink.hot)
        }
        Column(Modifier.clip(RoundedCornerShape(8.dp)).background(Ink.hot).padding(horizontal = 12.dp, vertical = 8.dp)) {
            BasicText("Tweak your sound", style = Type.label(13.sp, Ink.ground))
            BasicText("BUILD opens the pedalboard: amp, cab, PRE and POST.", style = Type.value(12.sp, Ink.ground))
        }
    }
}
