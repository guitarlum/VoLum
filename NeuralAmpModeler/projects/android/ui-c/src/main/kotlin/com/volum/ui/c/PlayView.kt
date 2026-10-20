package com.volum.ui.c

import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.Crossfade
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInVertically
import androidx.compose.animation.slideOutVertically
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.detectDragGesturesAfterLongPress
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
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.LongState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.semantics.CustomAccessibilityAction
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.customActions
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.stateDescription
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.InterfaceStatus
import com.volum.core.Overlay
import com.volum.core.Param
import com.volum.core.Permission
import com.volum.core.ScreenId
import com.volum.core.Section
import com.volum.core.View
import com.volum.design.art.ArtInput
import com.volum.design.art.AmpThumbnail
import com.volum.design.art.Motif
import com.volum.design.art.rememberArtFrames
import com.volum.design.components.CrossButton
import com.volum.design.components.PedalCard
import com.volum.design.components.PlayStagePanel
import com.volum.design.components.VoLumGlyph
import com.volum.design.components.VoLumSelectionStyle
import com.volum.design.components.WarnGlyph
import com.volum.design.components.ampCircuitBoard
import com.volum.design.components.cornerBrackets
import com.volum.design.components.hairlineFrame
import com.volum.design.components.insetWell
import com.volum.design.components.twoDigits
import com.volum.design.components.volumGlyph
import com.volum.design.components.volumSelection
import com.volum.design.theme.VFitText
import com.volum.design.theme.VText
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumType
import com.volum.design.theme.ic
import com.volum.design.theme.withOpacity
import com.volum.harness.screen

private val ChainH = 112.dp

/** PLAY: the moving art fills the stage, the Sound rail floats on its right, the chain of stomps runs under it. */
@Composable
internal fun PlayView(ui: Ui, fade: () -> Float, banner: Boolean) {
    val s = ui.s
    val frames = rememberArtFrames()
    Box(Modifier.fillMaxSize().padding(top = Metrics.top).screen(ScreenId.PLAY)) {
        Stage(ui, Modifier.fillMaxWidth().fillMaxHeight().padding(bottom = ChainH))
        Box(Modifier.fillMaxSize().graphicsLayer { alpha = fade() }) {
            Column(Modifier.fillMaxSize()) {
                Box(Modifier.weight(1f).fillMaxWidth()) {
                    Column(Modifier.fillMaxHeight().padding(start = 12.dp, end = Metrics.rail + 16.dp, top = 10.dp, bottom = 10.dp)) {
                        if (banner) FloatingBanner(ui, Modifier.align(Alignment.CenterHorizontally))
                        Box(Modifier.weight(1f).fillMaxWidth(), contentAlignment = Alignment.Center) { InputAlert(ui) }
                        TitleRow(ui)
                    }
                    SoundRail(ui, Modifier.align(Alignment.TopEnd).width(Metrics.rail).fillMaxHeight().padding(top = 8.dp, end = 8.dp, bottom = 8.dp))
                }
                Chain(ui, frames)
            }
        }
    }
}

/** The PLAY stage art: one `:design` panel, or MAIN and SUPPORT side by side; a new amp crossfades in. */
@Composable
private fun Stage(ui: Ui, modifier: Modifier) {
    val s = ui.s
    val input = remember(ui.tel) { ArtInput { ui.tel.value } }
    val support = s.library.amp(s.rig.support.ampId)
    Box(modifier.ids("play.art")) {
        Crossfade(Triple(s.ampId, s.dual, s.rig.support.ampId), animationSpec = tween(360), label = "stage") { (_, dual, _) ->
            Row(Modifier.fillMaxSize().padding(horizontal = 4.dp, vertical = 4.dp), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                PlayStagePanel(artOf(s.amp), input, Modifier.weight(1f).fillMaxHeight(), role = if (dual) "MAIN  ·  ${s.amp?.name ?: ""}" else null, animate = s.signal.animateArt)
                if (dual && support != null) PlayStagePanel(artOf(support), input, Modifier.weight(1f).fillMaxHeight(), support = true, role = "SUPPORT  ·  ${support.name}", animate = s.signal.animateArt)
            }
        }
    }
}

/** PREV, the Sound on stage, NEXT: arm's-length paddles; every recall slides the new name in. */
@Composable
private fun TitleRow(ui: Ui) {
    val s = ui.s
    Row(Modifier.fillMaxWidth().height(64.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        Paddle(VoLumGlyph.Prev, "Previous sound", "play.sound.prev") { ui.d(Action.StepSound(-1)) }
        AnimatedContent(
            Triple(s.library.live, s.presetName, s.ampId),
            transitionSpec = { (slideInVertically(tween(260)) { it / 2 } + fadeIn(tween(260))) togetherWith (slideOutVertically(tween(200)) { -it / 2 } + fadeOut(tween(160))) },
            modifier = Modifier.weight(1f).fillMaxHeight(),
            label = "recall",
        ) { (live, name, _) ->
            Row(
                Modifier.fillMaxSize().background(ic(205, 5, 7, 11)).drawBehind { drawLine(VoLumColors.GoldDim.withOpacity(0.5f), Offset(0f, 0f), Offset(size.width, 0f), 1.dp.toPx()) }.padding(horizontal = 14.dp),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(12.dp),
            ) {
                if (live != null) VText(twoDigits(live), Modifier.ids("play.sound.program"), style = VoLumType.value(19.sp, VoLumColors.Gold))
                Column(Modifier.weight(1f)) {
                    VFitText(name ?: "No Preset", Modifier.ids("play.sound.name"), style = VoLumType.SoundTitle, minSize = 18.sp)
                    VFitText(s.amp?.name ?: "", style = VoLumType.label(10.sp, VoLumColors.TealDim), minSize = 8.sp)
                }
                if (s.rig.dirty) VText("(unsaved)", style = VoLumType.label(10.sp, VoLumColors.Amber))
            }
        }
        Paddle(VoLumGlyph.Next, "Next sound", "play.sound.next") { ui.d(Action.StepSound(1)) }
    }
}

@Composable
private fun Paddle(glyph: VoLumGlyph, description: String, id: String, onClick: () -> Unit) {
    Tappable(onClick, Modifier.size(72.dp, 64.dp).semantics { contentDescription = description }, listOf(id)) { pressed ->
        val ink by rememberUpdatedState(if (pressed) VoLumColors.Gold else VoLumColors.GoldDim)
        Box(
            Modifier.fillMaxSize().background(ic(205, 5, 7, 11)).volumSelection(false, pressed, VoLumSelectionStyle.Brass, radius = 4.dp, inset = 2.dp)
                .hairlineFrame(if (pressed) VoLumColors.Gold else VoLumColors.Frame, radius = 4.dp),
            contentAlignment = Alignment.Center,
        ) { Spacer(Modifier.size(22.dp, 26.dp).volumGlyph(glyph, { ink })) }
    }
}

/** No input: the stage says so in the middle of the art and one tap fixes it. */
@Composable
private fun InputAlert(ui: Ui) {
    val a = ui.s.audio
    if (a.permission == Permission.GRANTED && a.status == InterfaceStatus.OK) return
    val text = when {
        a.permission != Permission.GRANTED -> "VoLum cannot hear your guitar: microphone access is off."
        a.status == InterfaceStatus.UNPLUGGED -> "Your USB interface was unplugged. Sound stopped."
        else -> "No USB audio interface found."
    }
    Tappable({ ui.d(Action.Open(Overlay.AudioStatus)) }, Modifier.widthIn(max = 460.dp), listOf("nav.audio_status.open")) { pressed ->
        Row(
            Modifier.insetWell(4.dp).drawBehind { drawRoundRect(VoLumColors.DangerFill.withOpacity(if (pressed) 0.5f else 0.3f), cornerRadius = CornerRadius(4.dp.toPx())) }
                .hairlineFrame(VoLumColors.Danger, radius = 4.dp).padding(horizontal = 16.dp, vertical = 12.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            WarnGlyph(VoLumColors.Danger)
            Column(Modifier.weight(1f, fill = false)) {
                VText(text, style = VoLumType.label(13.sp, VoLumColors.TextBright), maxLines = 2)
                VText("TAP TO FIX", style = VoLumType.label(10.sp, VoLumColors.Danger))
            }
        }
    }
}

/** The Sound rail: a glass column on the art. The live row and broken rows carry SWAP and CLEAR on their own line. */
@Composable
private fun SoundRail(ui: Ui, modifier: Modifier) {
    val lib = ui.s.library
    val programs = lib.sounds.keys.sorted()
    val scroll = rememberScrollState()
    var lifted by remember { mutableStateOf<Int?>(null) }
    val dispatch by rememberUpdatedState(ui.d)
    val order by rememberUpdatedState(programs)
    val pitchPx = rowPitchPx()
    val pitch = pitchPx.toFloat()
    LaunchedEffect(Unit) {
        val i = programs.indexOf(lib.live ?: -1)
        if (i > 0) scroll.scrollTo(i * pitchPx)
    }
    Column(
        modifier.background(ic(214, 12, 12, 18), androidx.compose.foundation.shape.RoundedCornerShape(6.dp)).hairlineFrame(VoLumColors.Frame, radius = 6.dp)
            .cornerBrackets(VoLumColors.HeroCorner, 10.dp, 4.dp).padding(start = 10.dp, end = 8.dp, top = 6.dp, bottom = 8.dp),
        verticalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        Spaced(gap = 6) {
            Column(Modifier.weight(1f)) {
                Caption("Sounds")
                VText("${programs.size}/128", style = VoLumType.label(9.sp, VoLumColors.CreamDim))
            }
            Btn("+ This", listOf("play.add_this", "play.add_this.unnamed", "play.add_this.full"), { ui.d(Action.AddThisSound) })
            Btn("+ Pick", listOf("play.add_sound"), { ui.d(Action.Open(Overlay.SoundPicker(lib.firstFreeProgram() ?: 0, replace = false))) })
        }
        SnapList(
            programs.size,
            Modifier.weight(1f).fillMaxWidth(),
            scroll,
            Modifier.pointerInput(Unit) {
                var acc = 0f
                detectDragGesturesAfterLongPress(
                    onDragStart = { o -> acc = 0f; lifted = order.getOrNull((o.y / pitch).toInt()) },
                    onDragEnd = { lifted = null },
                    onDragCancel = { lifted = null },
                ) { change, amount ->
                    change.consume()
                    val p = lifted ?: return@detectDragGesturesAfterLongPress
                    acc += amount.y
                    if (acc > pitch && p < 127) { dispatch(Action.MoveSound(p, 1)); lifted = p + 1; acc -= pitch }
                    if (acc < -pitch && p > 0) { dispatch(Action.MoveSound(p, -1)); lifted = p - 1; acc += pitch }
                }
            },
        ) {
            if (programs.isEmpty()) Note("No Sounds yet. + Pick puts a preset on a program number; + This adds what you hear.", Modifier.padding(6.dp))
            programs.forEach { p -> SoundRow(ui, p, lifted == p) }
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
    Tappable(
        { ui.d(Action.RecallSound(program)) },
        Modifier.fillMaxWidth().height(Metrics.target)
            .graphicsLayer { if (lifted) { scaleX = 1.03f; scaleY = 1.03f } }
            .semantics {
                contentDescription = "Program $program"
                stateDescription = if (live) "Live" else ""
                customActions = listOf(
                    CustomAccessibilityAction("Move down") { ui.d(Action.MoveSound(program, 1)); true },
                    CustomAccessibilityAction("Move up") { ui.d(Action.MoveSound(program, -1)); true },
                )
            },
        listOf("play.sound.recall", "play.sound.reorder"),
        enabled = valid,
    ) { pressed ->
        Row(
            Modifier.fillMaxSize()
                .drawBehind { if (!valid) drawRoundRect(VoLumColors.DangerFill.withOpacity(0.25f), cornerRadius = CornerRadius(4.dp.toPx())) }
                .volumSelection(live || lifted, pressed, VoLumSelectionStyle.Brass, radius = 4.dp, inset = 0.dp)
                .then(if (!valid) Modifier.hairlineFrame(VoLumColors.Danger.withOpacity(0.6f), radius = 4.dp) else Modifier)
                .padding(horizontal = 8.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            if (preset != null) AmpThumbnail(artOf(lib.amp(sound?.ampId)), Modifier.size(26.dp), live)
            Column(Modifier.weight(1f)) {
                Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    VText(twoDigits(program), Modifier.ids("play.sound.program"), style = VoLumType.value(11.sp, if (live) VoLumColors.Gold else VoLumColors.GoldDim))
                    if (live) VText("LIVE", Modifier.ids("play.sound.live"), style = VoLumType.label(8.sp, VoLumColors.Teal))
                }
                if (valid && preset != null) VFitText(preset.name, style = VoLumType.display(16.sp, VoLumColors.Cream), minSize = 8.sp)
                else VFitText("Invalid slot: preset deleted", Modifier.ids("play.sound.invalid"), style = VoLumType.label(11.sp, VoLumColors.Danger), minSize = 8.sp)
            }
            if (live || !valid) {
                GlyphBtn(VoLumGlyph.Edit, "Swap the preset on program $program", listOf("play.sound.replace"), { ui.d(Action.Open(Overlay.SoundPicker(program, replace = true))) }, color = VoLumColors.GoldDim)
                GlyphBtn(VoLumGlyph.Delete, "Clear program $program", listOf("play.sound.clear"), { ui.d(Action.ClearSound(program)) }, color = VoLumColors.Danger.withOpacity(0.8f))
            }
        }
    }
}

private class Stomp(val param: Param, val label: String, val id: String, val motif: Motif)

private val preStomps = listOf(
    Stomp(Param.PITCH_ON, "Pitch", "play.stomp.pitch", Motif.PITCH),
    Stomp(Param.COMP_ON, "Comp", "play.stomp.comp", Motif.COMP),
    Stomp(Param.NAM1_ON, "NAM 1", "play.stomp.nam1", Motif.NAM1),
    Stomp(Param.NAM2_ON, "NAM 2", "play.stomp.nam2", Motif.NAM2),
)
private val postStomps = listOf(
    Stomp(Param.CHORUS_ON, "Chorus", "play.stomp.chorus", Motif.CHORUS),
    Stomp(Param.DELAY_ON, "Delay", "play.stomp.delay", Motif.DELAY),
    Stomp(Param.REVERB_ON, "Reverb", "play.stomp.reverb", Motif.REVERB),
    Stomp(Param.TREM_ON, "Trem", "play.stomp.trem", Motif.TREM),
)

/** IN meter, PRE stomps, the amp, POST stomps, OUT meter: the desktop PLAY floor, each stomp a full-card bypass with EDIT under it. */
@Composable
private fun Chain(ui: Ui, frames: LongState) {
    Row(
        Modifier.fillMaxWidth().height(ChainH).padding(start = 6.dp, end = 6.dp, bottom = 6.dp, top = 2.dp),
        horizontalArrangement = Arrangement.spacedBy(5.dp),
    ) {
        Column(Modifier.fillMaxHeight().width(26.dp).ids("play.meters"), horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(3.dp)) {
            InMeter(ui, frames, Modifier.weight(1f))
            Lamp(ui.tel, VoLumColors.Teal) { it.pickAgeMs in 0..90 }
        }
        preStomps.forEach { StompCard(ui, it, frames, Modifier.weight(1f)) }
        AmpTile(ui, Modifier.width(112.dp).fillMaxHeight())
        postStomps.forEach { StompCard(ui, it, frames, Modifier.weight(1f)) }
        Column(Modifier.fillMaxHeight().width(26.dp).ids("play.meters"), horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(3.dp)) {
            OutMeter(ui, frames, Modifier.weight(1f))
            Lamp(ui.tel, VoLumColors.Danger) { it.outDb > -1.0 }
        }
    }
}

@Composable
private fun StompCard(ui: Ui, st: Stomp, frames: LongState, modifier: Modifier) {
    val on = ui.s.on(st.param)
    val title = when (st.param) {
        Param.NAM1_ON -> captureName(ui, Param.NAM1_CAPTURE)
        Param.NAM2_ON -> captureName(ui, Param.NAM2_CAPTURE)
        else -> st.label
    }
    Column(modifier.fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(4.dp)) {
        PedalCard(
            st.motif,
            title,
            on,
            onClick = { ui.d(Action.ToggleParam(st.param)) },
            onToggle = { ui.d(Action.ToggleParam(st.param)) },
            modifier = Modifier.fillMaxWidth().weight(1f).signalGlow(ui.tel, frames, on).semantics { stateDescription = if (on) "On" else "Bypassed" }.ids(st.id),
            ledModifier = Modifier.ids(st.id),
        )
        Tappable({ ui.d(Action.EditStomp(st.param)) }, Modifier.fillMaxWidth().height(Metrics.target).semantics { contentDescription = "Edit ${st.label}" }, listOf("play.stomp.edit")) { pressed ->
            Row(
                Modifier.fillMaxSize().padding(vertical = 6.dp).volumSelection(false, pressed, VoLumSelectionStyle.Brass, radius = 3.dp, inset = 0.dp).hairlineFrame(if (pressed) VoLumColors.GoldDim else VoLumColors.BtnOffBorder, radius = 3.dp),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.Center,
            ) {
                val ink by rememberUpdatedState(if (pressed) VoLumColors.Gold else VoLumColors.CreamDim)
                Spacer(Modifier.size(14.dp).volumGlyph(VoLumGlyph.Edit, { ink }))
                Gap(w = 4.dp)
                VFitText("EDIT", style = VoLumType.label(10.sp, ink), minSize = 8.sp)
            }
        }
    }
}

/** The amp between the pedals: its circuit board, its art and name; a tap goes to BUILD on the AMP. */
@Composable
private fun AmpTile(ui: Ui, modifier: Modifier) {
    val s = ui.s
    Tappable({ ui.d(Action.SetView(View.BUILD)); ui.d(Action.SetSection(Section.AMP)) }, modifier.semantics { contentDescription = "Edit amp" }, listOf("nav.view.build")) { pressed ->
        Column(
            Modifier.fillMaxSize().ampCircuitBoard().hairlineFrame(if (pressed) VoLumColors.Gold else VoLumColors.Frame, radius = 4.dp)
                .cornerBrackets(VoLumColors.HeroCorner, 8.dp, 4.dp).padding(6.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.spacedBy(3.dp, Alignment.CenterVertically),
        ) {
            AmpThumbnail(artOf(s.amp), Modifier.size(30.dp), true)
            VFitText(s.amp?.name ?: "No amp", style = VoLumType.label(12.sp, VoLumColors.Gold), minSize = 8.sp, textAlign = TextAlign.Center)
            VFitText(chCab(s), style = VoLumType.label(9.sp, VoLumColors.CreamDim), minSize = 7.sp, textAlign = TextAlign.Center)
            if (s.dual) VFitText("+ " + (s.library.amp(s.rig.support.ampId)?.name ?: "support empty"), style = VoLumType.label(9.sp, VoLumColors.Teal), minSize = 7.sp)
        }
    }
}
