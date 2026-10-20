package com.volum.ui.c

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectDragGesturesAfterLongPress
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
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.selection.toggleable
import androidx.compose.foundation.text.BasicText
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.semantics.CustomAccessibilityAction
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.customActions
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import com.volum.core.Action
import com.volum.core.InterfaceStatus
import com.volum.core.Overlay
import com.volum.core.Param
import com.volum.core.Permission
import com.volum.core.ScreenId
import com.volum.core.Section
import com.volum.core.View
import com.volum.harness.screen

private val RailWidth = 320.dp
private val RowH = Metrics.target

@Composable
internal fun PlayView(ui: Ui) {
    Row(Modifier.fillMaxSize().padding(top = Metrics.top).screen(ScreenId.PLAY)) {
        SoundRail(ui, Modifier.width(RailWidth).fillMaxHeight().padding(start = 8.dp, top = 4.dp, bottom = 8.dp))
        Stage(ui, Modifier.weight(1f).fillMaxHeight())
    }
}

@Composable
private fun SoundRail(ui: Ui, modifier: Modifier) {
    val lib = ui.s.library
    val programs = lib.sounds.keys.sorted()
    val scroll = rememberScrollState()
    val pitch = rowPitchPx()
    val step = pitch.toFloat()
    LaunchedEffect(Unit) {
        val i = programs.indexOf(lib.live ?: -1)
        if (i > 3) scroll.scrollTo((i - 3) * pitch)
    }
    var lifted by remember { mutableStateOf<Int?>(null) }
    val dispatch by rememberUpdatedState(ui.d)
    val order by rememberUpdatedState(programs)
    Glass(modifier) {
        Column(Modifier.padding(6.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                Column(Modifier.weight(1f)) {
                    Caption("Sounds")
                    BasicText("${programs.size} of 128", style = labelStyle(9f, Look.faint, bold = false), maxLines = 1)
                }
                Btn("+ THIS", listOf("play.add_this", "play.add_this.unnamed", "play.add_this.full"), { ui.d(Action.AddThisSound) }, sub = "SOUND")
                Btn("+ ADD", listOf("play.add_sound"), { ui.d(Action.Open(Overlay.SoundPicker(lib.firstFreeProgram() ?: 0, replace = false))) }, sub = "PICK")
            }
            SnapList(
                Modifier.weight(1f).fillMaxWidth(),
                scroll,
                Modifier.pointerInput(Unit) {
                        var acc = 0f
                        detectDragGesturesAfterLongPress(
                            onDragStart = { o -> acc = 0f; lifted = order.getOrNull((o.y / step).toInt()) },
                            onDragEnd = { lifted = null },
                            onDragCancel = { lifted = null },
                        ) { change, amount ->
                            change.consume()
                            val p = lifted ?: return@detectDragGesturesAfterLongPress
                            acc += amount.y
                            if (acc > step && p < 127) { dispatch(Action.MoveSound(p, 1)); lifted = p + 1; acc -= step }
                            if (acc < -step && p > 0) { dispatch(Action.MoveSound(p, -1)); lifted = p - 1; acc += step }
                        }
                    },
            ) {
                if (programs.isEmpty()) Note("No Sounds yet. + ADD picks a preset for a program number; + THIS adds what you are playing.", Modifier.padding(8.dp), maxLines = 4)
                programs.forEach { SoundRow(ui, it, lifted == it) }
            }
        }
    }
}

@Composable
private fun SoundRow(ui: Ui, program: Int, lifted: Boolean) {
    val lib = ui.s.library
    val sound = lib.sounds[program]
    val valid = lib.soundValid(program)
    val live = lib.live == program
    val preset = lib.preset(sound?.presetId)
    val src = remember { MutableInteractionSource() }
    val scale = pressScale(src)
    Row(Modifier.fillMaxWidth().height(RowH), horizontalArrangement = Arrangement.spacedBy(4.dp), verticalAlignment = Alignment.CenterVertically) {
        Row(
            Modifier
                .weight(1f)
                .fillMaxHeight()
                .graphicsLayer { scaleX = scale; scaleY = scale; if (lifted) { scaleX = 1.03f; scaleY = 1.03f } }
                .clip(Corner)
                .background(if (live) Look.listInk.copy(alpha = 0.22f) else if (lifted) Look.lineHi else Look.raised)
                .border(1.dp, if (live) Look.listInk else Look.line, Corner)
                .clickable(src, null, enabled = valid, role = Role.Button) { ui.d(Action.RecallSound(program)) }
                .semantics {
                    contentDescription = "Program $program"
                    customActions = listOf(
                        CustomAccessibilityAction("Move down") { ui.d(Action.MoveSound(program, 1)); true },
                        CustomAccessibilityAction("Move up") { ui.d(Action.MoveSound(program, -1)); true },
                    )
                }
                .ids("play.sound.recall", "play.sound.reorder")
                .padding(horizontal = 8.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            Column(Modifier.width(34.dp), horizontalAlignment = Alignment.CenterHorizontally) {
                BasicText("%03d".format(program), style = techStyle(11f, if (live) Look.text else Look.dim), maxLines = 1, modifier = Modifier.ids("play.sound.program"))
                if (live) BasicText("LIVE", style = labelStyle(8f, Look.listInk), maxLines = 1, modifier = Modifier.ids("play.sound.live"))
            }
            Column(Modifier.weight(1f)) {
                if (valid && preset != null) {
                    BasicText(preset.name, style = bodyStyle(14f, cap = 1.2f), maxLines = 1, overflow = TextOverflow.Ellipsis)
                    BasicText(lib.amp(sound?.ampId)?.name ?: "", style = labelStyle(9f, Look.dim, bold = false), maxLines = 1, overflow = TextOverflow.Ellipsis)
                } else {
                    BasicText("Invalid slot", style = bodyStyle(14f, Look.danger, cap = 1.2f), maxLines = 1, modifier = Modifier.ids("play.sound.invalid"))
                    BasicText("its preset was deleted · SWAP or ×", style = labelStyle(9f, Look.dim, bold = false), maxLines = 1, overflow = TextOverflow.Ellipsis)
                }
            }
        }
        Btn("SWAP", listOf("play.sound.replace"), { ui.d(Action.Open(Overlay.SoundPicker(program, replace = true))) })
        Btn("×", listOf("play.sound.clear"), { ui.d(Action.ClearSound(program)) })
    }
}

@Composable
private fun Stage(ui: Ui, modifier: Modifier) {
    val s = ui.s
    Column(modifier.padding(start = 8.dp, end = 8.dp, bottom = 8.dp)) {
        Row(Modifier.fillMaxWidth().height(Metrics.band), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Paddle("‹", "PREV", "play.sound.prev") { ui.d(Action.StepSound(-1)) }
            Column(Modifier.weight(1f), horizontalAlignment = Alignment.CenterHorizontally) {
                BasicText(s.presetName ?: "Default", style = displayStyle(30f), maxLines = 1, overflow = TextOverflow.Ellipsis, modifier = Modifier.ids("play.sound.name"))
                val live = s.library.live
                BasicText(
                    listOfNotNull(live?.let { "PROGRAM %03d · LIVE".format(it) }, s.amp?.name, if (s.rig.dirty) "unsaved" else null).joinToString("  ·  "),
                    style = labelStyle(10f), maxLines = 1, overflow = TextOverflow.Ellipsis,
                )
            }
            Paddle("›", "NEXT", "play.sound.next") { ui.d(Action.StepSound(1)) }
        }
        Box(Modifier.weight(1f).fillMaxWidth()) {
            if (!s.ui.buildTipSeen) {
                Glass(Modifier.align(Alignment.TopStart).padding(top = 4.dp).ids("tip.build"), border = Look.pick) {
                    Note("Tweak your sound: tap BUILD at the top to shape the amp, pedals and effects.", Modifier.padding(10.dp).width(260.dp), Look.text)
                }
            }
            val a = s.audio
            if (a.permission != Permission.GRANTED || a.status != InterfaceStatus.OK) {
                val text = when {
                    a.permission != Permission.GRANTED -> "VoLum cannot hear your guitar: microphone access is off."
                    a.status == InterfaceStatus.UNPLUGGED -> "Your USB interface was unplugged. Sound stopped."
                    else -> "No USB audio interface found."
                }
                Box(Modifier.align(Alignment.Center)) {
                    Btn(text, listOf("nav.audio_status.open"), { ui.d(Action.Open(Overlay.AudioStatus)) }, sub = "TAP TO FIX", danger = true, height = 56.dp)
                }
            }
            MeterBridge(ui, Modifier.align(Alignment.BottomCenter).padding(bottom = 8.dp))
        }
        Chain(ui)
    }
}

@Composable
private fun Paddle(glyph: String, label: String, id: String, onClick: () -> Unit) {
    val src = remember { MutableInteractionSource() }
    val scale = pressScale(src)
    Box(
        Modifier
            .size(width = 80.dp, height = 64.dp)
            .graphicsLayer { scaleX = scale; scaleY = scale }
            .clip(Corner)
            .background(Look.glass)
            .border(1.dp, Look.lineHi, Corner)
            .clickable(src, null, role = Role.Button, onClick = onClick)
            .ids(id),
        contentAlignment = Alignment.Center,
    ) {
        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            BasicText(glyph, style = displayStyle(26f))
            BasicText(label, style = labelStyle(9f))
        }
    }
}

@Composable
private fun MeterBridge(ui: Ui, modifier: Modifier) {
    val gateOn = ui.s.on(Param.GATE_ON)
    val threshold = ui.s.param(Param.GATE)
    Glass(modifier.ids("play.meters")) {
        Row(Modifier.padding(horizontal = 12.dp, vertical = 8.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Caption("In")
            Meter(ui.tel, Modifier.width(120.dp).height(10.dp), vertical = false) { it.inDb }
            Lamp(ui.tel, Look.warn) { gateOn && it.inDb < threshold }
            Caption("Gate")
            Gap(w = 8.dp)
            Caption("Out")
            Column(verticalArrangement = Arrangement.spacedBy(3.dp)) {
                Meter(ui.tel, Modifier.width(120.dp).height(6.dp), vertical = false) { it.outDb }
                Meter(ui.tel, Modifier.width(120.dp).height(6.dp), vertical = false) { it.outRightDb }
            }
            Lamp(ui.tel, Look.glow) { it.pickAgeMs in 0..90 }
            Caption("Pick")
        }
    }
}

private data class Stomp(val param: Param, val label: String, val id: String)

private val preStomps = listOf(
    Stomp(Param.PITCH_ON, "PITCH", "play.stomp.pitch"),
    Stomp(Param.COMP_ON, "COMP", "play.stomp.comp"),
    Stomp(Param.NAM1_ON, "NAM 1", "play.stomp.nam1"),
    Stomp(Param.NAM2_ON, "NAM 2", "play.stomp.nam2"),
)
private val postStomps = listOf(
    Stomp(Param.CHORUS_ON, "CHORUS", "play.stomp.chorus"),
    Stomp(Param.DELAY_ON, "DELAY", "play.stomp.delay"),
    Stomp(Param.REVERB_ON, "REVERB", "play.stomp.reverb"),
    Stomp(Param.TREM_ON, "TREM", "play.stomp.trem"),
)

@Composable
private fun Chain(ui: Ui) {
    Row(Modifier.fillMaxWidth().height(140.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        StompBlock(ui, "PRE", preStomps)
        AmpTile(ui, Modifier.weight(1f).fillMaxHeight())
        StompBlock(ui, "POST", postStomps)
    }
}

@Composable
private fun StompBlock(ui: Ui, title: String, stomps: List<Stomp>) {
    Glowing(ui.tel, stomps.any { ui.s.on(it.param) }, Modifier.fillMaxHeight()) {
        Column(Modifier.padding(6.dp).brackets(), verticalArrangement = Arrangement.spacedBy(4.dp)) {
            Caption(title, Modifier.padding(start = 4.dp))
            Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                stomps.forEach { StompPad(ui, it) }
            }
        }
    }
}

@Composable
private fun StompPad(ui: Ui, st: Stomp) {
    val on = ui.s.on(st.param)
    val src = remember { MutableInteractionSource() }
    val scale = pressScale(src)
    val sub = when (st.param) {
        Param.NAM1_ON -> captureName(ui, Param.NAM1_CAPTURE)
        Param.NAM2_ON -> captureName(ui, Param.NAM2_CAPTURE)
        else -> null
    }
    Column(horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(4.dp)) {
        Box(
            Modifier
                .size(width = 50.dp, height = 56.dp)
                .graphicsLayer { scaleX = scale; scaleY = scale }
                .clip(Corner)
                .background(if (on) Look.glassHi else Look.raised)
                .border(1.dp, if (on) Look.glow else Look.line, Corner)
                .toggleable(on, src, null, role = Role.Switch) { ui.d(Action.ToggleParam(st.param)) }
                .ids(st.id),
            contentAlignment = Alignment.Center,
        ) {
            Column(horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(3.dp)) {
                Led(on, 10.dp)
                BasicText(st.label, style = labelStyle(9f, if (on) Look.text else Look.dim, cap = 1f), maxLines = 1)
                if (sub != null) BasicText(sub, style = labelStyle(7f, Look.dim, bold = false, cap = 1f), maxLines = 1, overflow = TextOverflow.Ellipsis)
            }
        }
        Btn("EDIT", listOf("play.stomp.edit"), { ui.d(Action.EditStomp(st.param)) }, minWidth = 50.dp, modifier = Modifier.width(50.dp), pad = 0.dp, cap = 1f)
    }
}

@Composable
private fun AmpTile(ui: Ui, modifier: Modifier) {
    val s = ui.s
    val src = remember { MutableInteractionSource() }
    val scale = pressScale(src)
    Glowing(ui.tel, true, modifier.graphicsLayer { scaleX = scale; scaleY = scale }) {
        Column(
            Modifier
                .fillMaxSize()
                .clickable(src, null, role = Role.Button) { ui.d(Action.SetView(View.BUILD)); ui.d(Action.SetSection(Section.AMP)) }
                .padding(8.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.Center,
        ) {
            Caption("Amp")
            BasicText(s.amp?.name ?: "No amp", style = displayStyle(16f).copy(textAlign = TextAlign.Center), maxLines = 2, overflow = TextOverflow.Ellipsis)
            BasicText("CH ${s.rig.channel + 1} · ${cabLabel(s, s.rig.cab, s.amp)}", style = labelStyle(9f), maxLines = 1)
            if (s.dual) BasicText("+ " + (s.library.amp(s.rig.support.ampId)?.name ?: "support empty"), style = labelStyle(8f, Look.dim, bold = false), maxLines = 1, overflow = TextOverflow.Ellipsis)
        }
    }
}
