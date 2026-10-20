package com.volum.ui.b

import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInHorizontally
import androidx.compose.animation.slideOutHorizontally
import androidx.compose.animation.togetherWith
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
import androidx.compose.foundation.layout.requiredSize
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
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
import com.volum.design.art.AmpThumbnail
import com.volum.design.art.ArtRef
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
import com.volum.design.theme.VoLumMotion
import com.volum.design.theme.VoLumType
import com.volum.design.theme.withOpacity
import com.volum.harness.featureId
import com.volum.harness.screen
import kotlin.math.roundToInt

private val preStomps = listOf(Param.PITCH_ON, Param.COMP_ON, Param.NAM1_ON, Param.NAM2_ON)
private val postStomps = listOf(Param.CHORUS_ON, Param.DELAY_ON, Param.REVERB_ON, Param.TREM_ON)

/** Programs per floor page: one bank, as on a MIDI foot controller and on the MIDI settings tab. */
internal const val BANK = 8
internal const val BANKS = 128 / BANK

/**
 * The bank the floor turns to when LIVE moves from [before] to [live] while bank [shown]
 * is up, or null to stay: a recall on another bank brings its bank up, so LIVE is
 * always on the floor after a recall.
 */
internal fun bankToFollow(before: Int?, live: Int?, shown: Int): Int? {
    if (live == null || live == before) return null
    return (live / BANK).takeIf { it != shown }
}

internal fun stompLabel(p: Param, s: VoLumState): String = if (p == Param.TREM_ON) "TREMOLO" else cardLabel(p, s)

/** The name on a pedal card's footer, as the desktop board writes it (TREM, the capture's name). */
internal fun cardLabel(p: Param, s: VoLumState): String = when (p) {
    Param.NAM1_ON -> captureName(s, Param.NAM1_CAPTURE) ?: "NAM 1"
    Param.NAM2_ON -> captureName(s, Param.NAM2_CAPTURE) ?: "NAM 2"
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

/**
 * The `:design` pedal card with its footer name drawn here at the size of [names], so a
 * row of stomps reads at one size (CHORUS and TREMOLO alike). [card] carries the ids and
 * semantics of the card's own clickable; [lamp] false takes the lamp's touch target away
 * (the board's footswitch toggles bypass), the lamp itself still shows the state.
 */
@Composable
internal fun StompCard(
    p: Param,
    name: String,
    on: Boolean,
    onClick: () -> Unit,
    onToggle: () -> Unit,
    modifier: Modifier,
    card: Modifier,
    names: FitGroup,
    focused: Boolean = false,
    lamp: Boolean = true,
) {
    Box(modifier) {
        PedalCard(motifOf(p), "", on, onClick = onClick, onToggle = { onToggle() }, card.fillMaxSize(), focused = focused,
            ledModifier = if (lamp) Modifier else Modifier.requiredSize(0.dp))
        FitText(name, VoLumType.label(11.5.sp, if (on) VoLumColors.Cream else VoLumColors.CreamDim),
            Modifier.align(Alignment.BottomStart).fillMaxWidth().height(22.dp).padding(start = 8.dp, end = 20.dp),
            align = TextAlign.Start, group = names)
    }
}

/**
 * PLAY, the stage floor: PRE stomps down the left edge and POST down the right
 * (signal flows across the phone), one bank of eight programs in the middle (a tap
 * only recalls), big PREV / NEXT and the bank arrows under it, IN and OUT meters at
 * the edges. Nothing on this surface scrolls.
 */
@Composable
internal fun PlaySurface(s: VoLumState) {
    val st = LocalStage.current
    val stompNames = rememberFitGroup()
    val live = s.library.live
    val bank = s.midi.bank
    val opened = remember { s.ui.top == null }
    var followed by remember { mutableStateOf(if (opened) null else live) }
    LaunchedEffect(live) {
        val target = bankToFollow(followed, live, bank)
        followed = live
        if (target != null) st.go(Action.SetMidiBank(target))
    }
    Column(Modifier.fillMaxSize().screen(ScreenId.PLAY)) {
        Header(s) { labels -> SlotTools(s, labels) }
        if (s.ui.top == null) BannerSlot(s, Modifier.padding(horizontal = 12.dp))
        Row(Modifier.weight(1f).fillMaxWidth().padding(start = 6.dp, end = 6.dp, top = 8.dp, bottom = 8.dp), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            VoLumMeter({ st.level(true) }, "IN", Modifier.width(28.dp).fillMaxHeight().featureId("play.meters"), frame = { st.frames.longValue })
            StompColumn(s, preStomps, editAfter = true, stompNames)
            Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                SoundFloor(s, bank, Modifier.weight(1f).fillMaxWidth())
                Transport(s, bank, { st.go(Action.SetMidiBank(it)) }, Modifier.fillMaxWidth().height(64.dp))
            }
            StompColumn(s, postStomps, editAfter = false, stompNames)
            VoLumMeter({ st.level(false) }, "OUT", Modifier.width(28.dp).fillMaxHeight().featureId("play.meters"), out = true, frame = { st.frames.longValue })
        }
    }
}

/**
 * The Sound tools, up in the header and away from the floor so a switching tap can
 * never land on them: ADD THIS SOUND keeps the rig on the next free program;
 * REPLACE and CLEAR act on the LIVE program (CLEAR leaves an undo toast).
 */
@Composable
private fun RowScope.SlotTools(s: VoLumState, labels: FitGroup) {
    val st = LocalStage.current
    val live = s.library.live
    val sound = live?.let { s.library.sounds[it] }
    val free = s.library.firstFreeProgram()
    val details = rememberFitGroup()
    Row(Modifier.weight(1f).height(48.dp), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
        HeaderPad("ADD THIS SOUND", if (free != null) "to program ${twoDigit(free)}" else "all 128 programs used", VoLumGlyph.Plus, Tone.PRIMARY,
            arrayOf("play.add_this", "play.add_this.unnamed", "play.add_this.full"), "Add this sound", labels, null, Modifier.weight(1.5f).fillMaxHeight()) {
            st.go(Action.AddThisSound)
        }
        if (live != null && sound != null) {
            HeaderPad("REPLACE", "Sound ${twoDigit(live)}", VoLumGlyph.Edit, Tone.FRAMED, arrayOf("play.sound.replace"),
                "Replace the Sound on program ${twoDigit(live)}", labels, details, Modifier.weight(1f).fillMaxHeight()) {
                st.go(Action.Open(Overlay.SoundPicker(live, replace = true)))
            }
            HeaderPad("CLEAR", "Sound ${twoDigit(live)}", VoLumGlyph.Delete, Tone.DANGER, arrayOf("play.sound.clear"),
                "Clear program ${twoDigit(live)}", labels, details, Modifier.weight(1f).fillMaxHeight()) {
                val name = s.library.preset(sound.presetId)?.name ?: "Invalid slot"
                st.cleared.value = Cleared(live, sound, name)
                st.go(Action.ClearSound(live))
            }
        }
    }
}

/** A header pad: glyph, the action in the row's shared size, and what it acts on under it (at the size of [details] when given). */
@Composable
private fun HeaderPad(
    label: String,
    detail: String,
    glyph: VoLumGlyph,
    tone: Tone,
    ids: Array<String>,
    description: String,
    labels: FitGroup,
    details: FitGroup?,
    modifier: Modifier,
    onClick: () -> Unit,
) {
    Pad(onClick, modifier, ids, tone = tone, description = description) { ink ->
        Column(Modifier.fillMaxSize().padding(horizontal = 8.dp), verticalArrangement = Arrangement.Center) {
            FitText(label, VoLumType.label(12.sp, ink), Modifier.fillMaxWidth().height(18.dp), align = TextAlign.Start, group = labels)
            Row(Modifier.fillMaxWidth().height(14.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                VoLumIcon(glyph, color = ink, size = 10.dp)
                FitText(detail, VoLumType.label(10.sp, VoLumColors.TextMed), Modifier.weight(1f).height(14.dp), align = TextAlign.Start, group = details)
            }
        }
    }
}

@Composable
private fun StompColumn(s: VoLumState, stomps: List<Param>, editAfter: Boolean, names: FitGroup) {
    Column(Modifier.width(146.dp).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
        stomps.forEach { Stomp(s, it, editAfter, names) }
    }
}

/** One stomp: the pedal card toggles bypass from anywhere on it; the inner EDIT tab opens it in BUILD. */
@Composable
private fun ColumnScope.Stomp(s: VoLumState, p: Param, editAfter: Boolean, names: FitGroup) {
    val st = LocalStage.current
    val haptics = LocalHapticFeedback.current
    val on = s.on(p)
    val toggle = { haptics.performHapticFeedback(HapticFeedbackType.LongPress); st.go(Action.ToggleParam(p)) }
    Row(Modifier.weight(1f).fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
        val card: @Composable RowScope.() -> Unit = {
            StompCard(p, cardLabel(p, s), on, onClick = toggle, onToggle = toggle, Modifier.weight(1f).fillMaxHeight(),
                Modifier.ids(Ids.stomp(p)).semantics { contentDescription = "${stompLabel(p, s)} ${if (on) "on" else "bypassed"}" }, names)
        }
        val edit: @Composable RowScope.() -> Unit = {
            Pad({ st.go(Action.EditStomp(p)) }, Modifier.width(48.dp).fillMaxHeight(), arrayOf("play.stomp.edit"), tone = Tone.FRAMED,
                description = "Edit ${stompLabel(p, s)} in BUILD") { ink ->
                Column(horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(4.dp)) {
                    VoLumIcon(VoLumGlyph.Edit, color = ink, size = 16.dp)
                    FitText("EDIT", VoLumType.label(10.sp, VoLumColors.TextMed), Modifier.width(42.dp).height(15.dp))
                }
            }
        }
        if (editAfter) { card(); edit() } else { edit(); card() }
    }
}

/**
 * One bank of eight programs, in program order like the foot controller that recalls
 * them: a Sound on its program, or an empty pad that adds one there. The page follows
 * the LIVE Sound; changing bank slides the floor the way the arrow points.
 */
@Composable
private fun SoundFloor(s: VoLumState, bank: Int, modifier: Modifier) {
    AnimatedContent(
        targetState = bank,
        transitionSpec = {
            val dir = if (targetState > initialState) 1 else -1
            (fadeIn(tween(VoLumMotion.PanelMs)) + slideInHorizontally(tween(220)) { it / 6 * dir }) togetherWith
                (fadeOut(tween(110)) + slideOutHorizontally(tween(180)) { -it / 10 * dir })
        },
        modifier = modifier,
        label = "bank",
    ) { b ->
        val names = rememberFitGroup()
        val amps = rememberFitGroup()
        val adds = rememberFitGroup()
        val order = s.library.sounds.keys.sorted()
        Column(Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            (0..1).forEach { r ->
                Row(Modifier.weight(1f).fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    (0..3).forEach { c ->
                        val program = b * BANK + r * 4 + c
                        val sound = s.library.sounds[program]
                        Box(Modifier.weight(1f).fillMaxHeight()) {
                            if (sound != null) SoundTile(s, program, sound, order, names, amps) else EmptyTile(program, adds)
                        }
                    }
                }
            }
        }
    }
}

/**
 * A Sound tile: a tap recalls, and that is all a tap does. Every tile keeps its program
 * numeral on the top row; under it the LIVE tile carries the moving PLAY stage art
 * (corona follows the playing level), the others show the amp's thumbnail in that row.
 * Long-press and drag swaps programs; Move down / up are accessibility actions.
 */
@Composable
private fun SoundTile(s: VoLumState, program: Int, sound: Sound, order: List<Int>, names: FitGroup, amps: FitGroup) {
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
    Box(
        Modifier.fillMaxSize()
            .graphicsLayer {
                translationX = drag.x; translationY = drag.y
                val k = if (drag != Offset.Zero) 1.04f else if (pressed) 0.97f else 1f
                scaleX = k; scaleY = k
            }
            .then(
                if (!valid) Modifier.drawBehind { drawRoundRect(VoLumColors.DangerFill.withOpacity(0.22f), cornerRadius = CornerRadius(4.dp.toPx())) }
                    .hairlineFrame(VoLumColors.Danger.withOpacity(0.7f), radius = 4.dp)
                else Modifier.panelDepth(4.dp).hairlineFrame(VoLumColors.Frame, radius = 4.dp),
            )
            .pointerInput(program, order) {
                val tileW = size.width.toFloat()
                val tileH = size.height.toFloat()
                detectDragGesturesAfterLongPress(
                    onDragEnd = {
                        val gap = 8.dp.toPx()
                        val col = (program % BANK) % 4
                        val row = (program % BANK) / 4
                        val toCol = (col + (drag.x / (tileW + gap)).roundToInt()).coerceIn(0, 3)
                        val toRow = (row + (drag.y / (tileH + gap)).roundToInt()).coerceIn(0, 1)
                        val target = program - program % BANK + toRow * 4 + toCol
                        if (target != program) st.go(Action.MoveSound(program, target - program))
                        drag = Offset.Zero
                    },
                    onDragCancel = { drag = Offset.Zero },
                ) { change, d -> change.consume(); drag += d }
            }
            .clickable(source, null, role = Role.Button) {
                haptics.performHapticFeedback(HapticFeedbackType.LongPress)
                st.go(if (valid) Action.RecallSound(program) else Action.Open(Overlay.SoundPicker(program, replace = true)))
            }
            .semantics {
                customActions = listOf(moveDown, moveUp)
                contentDescription = if (valid) "Program ${twoDigit(program)}" else "Program ${twoDigit(program)}: invalid, tap to replace"
            }
            .ids(if (valid) arrayOf("play.sound.recall", "play.sound.reorder") else arrayOf("play.sound.invalid", "play.sound.reorder", "play.sound.replace")),
    ) {
        Box(Modifier.fillMaxSize().volumSelection(live && valid, pressed, Sel.Brass, radius = 4.dp, inset = 0.dp))
        Column(Modifier.fillMaxSize()) {
            Row(Modifier.fillMaxWidth().height(30.dp).padding(start = 10.dp, end = 8.dp, top = 6.dp), verticalAlignment = Alignment.CenterVertically) {
                Box(Modifier.featureId("play.sound.program")) {
                    FitText(twoDigit(program), VoLumType.value(15.sp, if (valid) VoLumColors.Gold else VoLumColors.CreamDim), Modifier.width(44.dp).height(22.dp), align = TextAlign.Start)
                }
                Spacer(Modifier.weight(1f))
                when {
                    live && valid -> Box(Modifier.volumSelection(true, style = Sel.AmberPicker).padding(horizontal = 6.dp, vertical = 1.dp).featureId("play.sound.live")) {
                        FitText("LIVE", VoLumType.label(9.sp, VoLumColors.AmberInk), Modifier.width(26.dp).height(14.dp))
                    }
                    valid && amp != null -> AmpThumbnail(ArtRef.of(amp), Modifier.size(24.dp))
                }
            }
            if (live && valid && amp != null) {
                Box(Modifier.weight(1f).fillMaxWidth().padding(start = 2.dp, end = 2.dp, bottom = 2.dp)) {
                    PlayStagePanel(ArtRef.of(amp), st.input, Modifier.fillMaxSize().featureId("play.art"), animate = s.signal.animateArt)
                    Box(Modifier.fillMaxSize().drawBehind {
                        drawRect(Brush.verticalGradient(listOf(Color.Transparent, VoLumColors.HeroBg.withOpacity(0.92f)), startY = size.height * 0.45f, endY = size.height))
                    })
                    TileNames(preset?.name ?: "", amp.name, true, names, amps, Modifier.align(Alignment.BottomStart).padding(start = 8.dp, end = 8.dp, bottom = 5.dp))
                }
            } else if (valid) {
                Spacer(Modifier.weight(1f))
                TileNames(preset?.name ?: "", amp?.name ?: "", false, names, amps, Modifier.padding(start = 10.dp, end = 8.dp, bottom = 7.dp))
            } else {
                Spacer(Modifier.weight(1f))
                Column(Modifier.padding(start = 10.dp, end = 8.dp, bottom = 7.dp)) {
                    FitText("Invalid slot", VoLumType.label(14.sp, VoLumColors.Danger), Modifier.fillMaxWidth().height(22.dp), align = TextAlign.Start)
                    val lines = rememberFitGroup()
                    FitText("preset deleted", hint(VoLumColors.Cream), Modifier.fillMaxWidth().height(18.dp), align = TextAlign.Start, group = lines)
                    FitText("tap to replace", hint(VoLumColors.Cream), Modifier.fillMaxWidth().height(18.dp), align = TextAlign.Start, group = lines)
                }
            }
        }
    }
}

/** The Sound name in Poiret and its amp in teal, each at its grid's shared size. */
@Composable
private fun TileNames(name: String, amp: String, live: Boolean, names: FitGroup, amps: FitGroup, modifier: Modifier) {
    Column(modifier.fillMaxWidth()) {
        FitText(name, VoLumType.display(21.sp, if (live) VoLumColors.TextBright else VoLumColors.Cream), Modifier.fillMaxWidth().height(28.dp), align = TextAlign.Start, group = names)
        FitText(amp, VoLumType.label(10.sp, VoLumColors.Teal), Modifier.fillMaxWidth().height(16.dp), align = TextAlign.Start, group = amps)
    }
}

/** An empty program: the desktop's dashed add cell, on its program, opening the preset picker for it. */
@Composable
private fun EmptyTile(program: Int, adds: FitGroup) {
    val st = LocalStage.current
    val source = remember { MutableInteractionSource() }
    val pressed by source.collectIsPressedAsState()
    Box(
        Modifier.fillMaxSize()
            .clickable(source, null, role = Role.Button) { st.go(Action.Open(Overlay.SoundPicker(program, replace = false))) }
            .dashedAddFrame(pressed)
            .semantics { contentDescription = "Program ${twoDigit(program)} is empty: add a Sound from your presets" }
            .featureId("play.add_sound"),
    ) {
        FitText(twoDigit(program), VoLumType.value(15.sp, VoLumColors.CreamDim), Modifier.padding(start = 10.dp, top = 6.dp).width(44.dp).height(22.dp), align = TextAlign.Start)
        Column(Modifier.align(Alignment.Center).fillMaxWidth().padding(horizontal = 10.dp), horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.spacedBy(4.dp)) {
            VoLumIcon(VoLumGlyph.Plus, color = VoLumColors.Gold, size = 18.dp)
            FitText("ADD FROM PRESETS", VoLumType.label(11.sp, VoLumColors.Gold), Modifier.fillMaxWidth().height(16.dp), group = adds)
        }
    }
}

/**
 * Arm's-length transport: previous / next Sound around the readout of what is playing
 * now, with the bank arrows beside the readout to look at another eight programs
 * without changing the Sound.
 */
@Composable
private fun Transport(s: VoLumState, bank: Int, onBank: (Int) -> Unit, modifier: Modifier) {
    val st = LocalStage.current
    val live = s.library.live
    val sound = live?.let { s.library.sounds[it] }
    val amp = s.library.amp(sound?.ampId ?: s.ampId)
    val name = sound?.let { s.library.preset(it.presetId)?.name } ?: s.presetName ?: "Tap a Sound"
    val steps = rememberFitGroup()
    val banks = rememberFitGroup()
    Row(modifier, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        Pad({ st.go(Action.StepSound(-1)) }, Modifier.width(StepPadWidth).fillMaxHeight(), arrayOf("play.sound.prev"), haptic = true, description = "Previous Sound") { ink ->
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                VoLumIcon(VoLumGlyph.Prev, color = VoLumColors.Gold, size = 18.dp)
                FitText("PREV", VoLumType.label(15.sp, ink), Modifier.width(48.dp).height(22.dp), group = steps)
            }
        }
        BankPad(VoLumGlyph.Prev, bank - 1, bank > 0, arrayOf("midi.bank.prev"), banks) { onBank(bank - 1) }
        Row(
            Modifier.weight(1f).fillMaxHeight().panelDepth(4.dp).hairlineFrame(VoLumColors.GoldDim.withOpacity(0.5f), radius = 4.dp)
                .featureId("play.sound.name").padding(horizontal = 12.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(10.dp),
        ) {
            FitText(live?.let { twoDigit(it) } ?: "--", VoLumType.value(18.sp, VoLumColors.Gold), Modifier.width(40.dp).height(26.dp))
            Column(Modifier.weight(1f)) {
                FitText(name, VoLumType.display(24.sp, VoLumColors.TextBright), Modifier.fillMaxWidth().height(32.dp), align = TextAlign.Start)
                Row(Modifier.fillMaxWidth().height(16.dp), verticalAlignment = Alignment.CenterVertically) {
                    FitText(amp?.name ?: "", VoLumType.label(10.sp, VoLumColors.Teal), Modifier.weight(1f), align = TextAlign.Start)
                    if (s.rig.dirty) FitText("(unsaved)", VoLumType.label(10.sp, VoLumColors.Amber), Modifier.width(60.dp), align = TextAlign.End)
                }
            }
        }
        BankPad(VoLumGlyph.Next, bank + 1, bank < BANKS - 1, arrayOf("midi.bank.next"), banks) { onBank(bank + 1) }
        Pad({ st.go(Action.StepSound(1)) }, Modifier.width(StepPadWidth).fillMaxHeight(), arrayOf("play.sound.next"), haptic = true, description = "Next Sound") { ink ->
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                FitText("NEXT", VoLumType.label(15.sp, ink), Modifier.width(48.dp).height(22.dp), group = steps)
                VoLumIcon(VoLumGlyph.Next, color = VoLumColors.Gold, size = 18.dp)
            }
        }
    }
}

/**
 * Shows bank [target] on the floor (programs ×8): the MIDI tab's bank arrows, so the
 * floor and the MIDI tab always page together. Dimmed at either end of the 16 banks.
 */
@Composable
private fun BankPad(glyph: VoLumGlyph, target: Int, enabled: Boolean, ids: Array<String>, labels: FitGroup, onClick: () -> Unit) {
    val first = target.coerceIn(0, BANKS - 1) * BANK
    Pad(onClick, Modifier.width(BankPadWidth).fillMaxHeight(), ids, enabled = enabled, tone = Tone.FRAMED,
        description = "Show programs ${twoDigit(first)} to ${twoDigit(first + BANK - 1)}") { ink ->
        Column(horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(1.dp)) {
            VoLumIcon(glyph, color = if (enabled) VoLumColors.Gold else ink, size = 14.dp)
            FitText("BANK", VoLumType.label(11.sp, ink), Modifier.width(BankPadWidth - 8.dp).height(16.dp), group = labels)
            FitText(if (enabled) "${twoDigit(first)}–${twoDigit(first + BANK - 1)}" else "", VoLumType.label(11.sp, VoLumColors.Cream), Modifier.width(BankPadWidth - 8.dp).height(16.dp), group = labels)
        }
    }
}

/** A bank arrow is a live control: at least 64 dp each way, like PREV / NEXT. */
internal val BankPadWidth = 68.dp
internal val StepPadWidth = 84.dp
