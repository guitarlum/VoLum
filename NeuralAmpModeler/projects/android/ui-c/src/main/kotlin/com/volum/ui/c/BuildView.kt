package com.volum.ui.c

import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.Crossfade
import androidx.compose.animation.core.FastOutSlowInEasing
import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.LinearOutSlowInEasing
import androidx.compose.animation.core.animateDpAsState
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInHorizontally
import androidx.compose.animation.slideOutHorizontally
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.shape.RoundedCornerShape
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
import androidx.compose.ui.draw.clipToBounds
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.graphics.lerp
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.Lane
import com.volum.core.Overlay
import com.volum.core.Param
import com.volum.core.PostBlock
import com.volum.core.PreBlock
import com.volum.core.ScreenId
import com.volum.core.Section
import com.volum.core.VoLumState
import com.volum.design.art.ArtInput
import com.volum.design.art.Motif
import com.volum.design.art.rememberArtFrames
import com.volum.design.components.MotifArt
import com.volum.design.components.PlayStagePanel
import com.volum.design.components.VoLumGlyph
import com.volum.design.components.VoLumIcon
import com.volum.design.components.VoLumSelectionStyle
import com.volum.design.components.ampCircuitBoard
import com.volum.design.components.cornerBrackets
import com.volum.design.components.diamondRule
import com.volum.design.components.hairlineFrame
import com.volum.design.components.lockInk
import com.volum.design.components.panelDepth
import com.volum.design.components.preCrosshatch
import com.volum.design.components.volumGlyph
import com.volum.design.components.volumSectionFrame
import com.volum.design.components.volumSelection
import com.volum.design.theme.VFitText
import com.volum.design.theme.VText
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumType
import com.volum.design.theme.withOpacity
import com.volum.harness.screen
import kotlinx.coroutines.delay

/** Menus opened from a sheet row: they fold the sheet down to that row and rise above it. */
internal val Overlay.fromSheet: Boolean
    get() = this is Overlay.AmpPicker || this is Overlay.IrMenu || this is Overlay.CaptureMenu || this is Overlay.SupportAmpMenu

/** The overlay stack started at a sheet row (a picker, maybe with a manage panel opened from it). */
internal fun VoLumState.folded(): Boolean = ui.view == com.volum.core.View.BUILD && ui.overlays.firstOrNull()?.fromSheet == true

/**
 * The support amp list was opened from the SUPPORT lane on the art (MAIN has the
 * sheet): it fills the MAIN half, the sheet steps aside and the lane keeps the rest.
 */
internal fun VoLumState.supportFromLane(): Boolean =
    ui.view == com.volum.core.View.BUILD && ui.overlays.lastOrNull() is Overlay.SupportAmpMenu && !supportFocused

@Composable
internal fun BuildView(ui: Ui, fade: () -> Float, banner: Boolean) {
    val s = ui.s
    val screen = when (s.ui.section) {
        Section.PRE -> ScreenId.BUILD_PRE
        Section.AMP -> ScreenId.BUILD_AMP
        Section.POST -> ScreenId.BUILD_POST
    }
    val folded = s.folded()
    val lane = s.supportFromLane()
    val frames = rememberArtFrames()
    val chained = folded && s.ui.overlays.size > 1
    val sheetH by animateDpAsState(if (lane || chained) 0.dp else if (folded) Metrics.tray else Metrics.sheet, tween(260), label = "fold")
    val grow by animateDpAsState(if (lane) Metrics.sheet else 0.dp, tween(260), label = "grow")
    val blocks by animateFloatAsState(if (s.ui.overlays.any { it !is Overlay.NameDialog }) 0f else 1f, tween(200), label = "blocks")
    BoxWithConstraints(Modifier.fillMaxSize().padding(top = Metrics.top).screen(screen)) {
        val artH = maxHeight - Metrics.sheet + grow
        ArtWindow(ui, Modifier.fillMaxWidth().height(artH))
        Box(Modifier.fillMaxWidth().height(maxHeight - Metrics.sheet).graphicsLayer { alpha = fade() }) { Floating(ui, frames, banner) { blocks } }
        if (sheetH > 0.dp) Sheet(ui, Modifier.align(Alignment.BottomCenter).fillMaxWidth().height(sheetH).graphicsLayer { alpha = fade() })
    }
}

/** The living art window: the amp's moving art (two lanes in Dual Amp), lit by the signal. */
@Composable
private fun ArtWindow(ui: Ui, modifier: Modifier) {
    val s = ui.s
    val input = remember(ui.tel) { ArtInput { ui.tel.value } }
    Box(modifier.ids("amp.hero")) {
        Crossfade(Triple(s.ampId, s.dual, s.rig.support.ampId), animationSpec = tween(360), label = "hero") { (_, dual, supportId) ->
            Row(Modifier.fillMaxSize().padding(4.dp), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                if (!dual) {
                    PlayStagePanel(artOf(s.amp), input, Modifier.weight(1f).fillMaxHeight(), animate = s.signal.animateArt)
                } else {
                    val support = s.library.amp(supportId)
                    Lane(ui, false, "MAIN  ·  ${s.amp?.name ?: ""}", Modifier.weight(1f).fillMaxHeight()) {
                        PlayStagePanel(artOf(s.amp), input, Modifier.fillMaxSize(), animate = s.signal.animateArt)
                    }
                    Lane(ui, true, "SUPPORT  ·  ${support?.name ?: "empty"}", Modifier.weight(1f).fillMaxHeight(), tab = support != null || s.supportFocused) {
                        if (support != null) PlayStagePanel(artOf(support), input, Modifier.fillMaxSize(), support = true, animate = s.signal.animateArt)
                        else EmptyLane(ui)
                    }
                }
            }
        }
    }
}

/**
 * A Dual Amp lane. Its role tab at the top aims the sheet at it (the floating blocks
 * cover the lower half); the focused lane carries its accent frame and tab. An
 * empty SUPPORT lane that MAIN still edits has no tab: its chooser is the way in.
 */
@Composable
private fun Lane(ui: Ui, support: Boolean, role: String, modifier: Modifier, tab: Boolean = true, content: @Composable () -> Unit) {
    val focused = ui.s.supportFocused == support
    val accent = if (support) VoLumColors.Teal else VoLumColors.Amber
    Box(modifier.then(if (focused) Modifier.hairlineFrame(accent, 1.6.dp) else Modifier)) {
        content()
        if (tab) Tappable(
            { ui.d(Action.FocusLane(if (support) Lane.SUPPORT else Lane.MAIN)) },
            Modifier.align(Alignment.TopStart).padding(start = if (support) 8.dp else LaneInset, top = 2.dp).height(Metrics.target).widthIn(min = 150.dp, max = 230.dp)
                .semantics { contentDescription = if (support) "Edit the SUPPORT amp" else "Edit the MAIN amp" },
            listOf(if (support) "dual.focus.support" else "dual.focus.main"),
            role = Role.Tab,
        ) { pressed ->
            Row(
                Modifier.fillMaxSize().padding(vertical = 8.dp).background(Glass.Float, RoundedCornerShape(3.dp))
                    .volumSelection(focused, pressed, VoLumSelectionStyle.ListTeal, radius = 3.dp, inset = 0.dp)
                    .hairlineFrame(if (focused || pressed) accent else VoLumColors.Frame, radius = 3.dp).padding(horizontal = 10.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) { VFitText(role, style = VoLumType.label(10.sp, if (support) VoLumColors.Teal else VoLumColors.GoldDim), minSize = 7.sp) }
        }
    }
}

/** The IN meter column on the art's left edge: what sits at the art's top-left starts right of it. */
private val LaneInset = 44.dp

/**
 * The empty SUPPORT lane. While MAIN has the sheet, its chooser opens the support
 * list beside it and stays lit; a second tap closes the list. With SUPPORT
 * focused, the sheet's amp selector is the chooser and the lane only says so.
 */
@Composable
private fun EmptyLane(ui: Ui) {
    val s = ui.s
    Box(Modifier.fillMaxSize().panelDepth().hairlineFrame(VoLumColors.TealDim.withOpacity(0.6f)).cornerBrackets(VoLumColors.Teal, 12.dp, 6.dp)) {
        Column(Modifier.align(Alignment.Center), horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(4.dp)) {
            VText("SUPPORT LANE IS EMPTY", style = VoLumType.label(10.sp, VoLumColors.Teal))
            if (s.supportFocused) VText("Choose its amp in the sheet below", style = helper(11.sp))
        }
        if (!s.supportFocused) {
            val open = s.ui.overlays.contains(Overlay.SupportAmpMenu)
            Tappable(
                { ui.d(if (open) Action.Back else Action.Open(Overlay.SupportAmpMenu)) },
                Modifier.align(Alignment.TopEnd).padding(top = 2.dp, end = 44.dp).height(Metrics.target).semantics { contentDescription = "Choose support amp" },
                listOf("dual.support.choose"),
            ) { pressed ->
                Row(
                    Modifier.fillMaxHeight().padding(vertical = 6.dp).background(Glass.Float, RoundedCornerShape(4.dp))
                        .volumSelection(open, pressed, VoLumSelectionStyle.ListTeal, radius = 4.dp, inset = 0.dp)
                        .hairlineFrame(if (open) VoLumColors.Amber else if (pressed) VoLumColors.Teal else VoLumColors.TealDim, if (open) 1.4.dp else 1.dp, 4.dp)
                        .padding(horizontal = 12.dp),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                ) {
                    VoLumIcon(VoLumGlyph.Plus, color = if (open) VoLumColors.Amber else VoLumColors.Teal)
                    VText("Choose support amp", style = VoLumType.label(12.sp, if (open) VoLumColors.Amber else VoLumColors.TextBright))
                    val ink by rememberUpdatedState(if (open) VoLumColors.Amber else VoLumColors.TealDim)
                    androidx.compose.foundation.layout.Spacer(Modifier.size(10.dp).volumGlyph(if (open) VoLumGlyph.Prev else VoLumGlyph.ChevronDown, { ink }))
                }
            }
        }
    }
}

private class Pad(val param: Param, val motif: Motif, val label: String, val id: String, val focus: Action, val focused: (VoLumState) -> Boolean)

private val prePads = listOf(
    Pad(Param.PITCH_ON, Motif.PITCH, "Pitch", "pre.card.pitch", Action.FocusPre(PreBlock.PITCH)) { it.ui.preFocus == PreBlock.PITCH },
    Pad(Param.COMP_ON, Motif.COMP, "Comp", "pre.card.comp", Action.FocusPre(PreBlock.COMP)) { it.ui.preFocus == PreBlock.COMP },
    Pad(Param.NAM1_ON, Motif.NAM1, "NAM 1", "pre.card.nam1", Action.FocusPre(PreBlock.NAM1)) { it.ui.preFocus == PreBlock.NAM1 },
    Pad(Param.NAM2_ON, Motif.NAM2, "NAM 2", "pre.card.nam2", Action.FocusPre(PreBlock.NAM2)) { it.ui.preFocus == PreBlock.NAM2 },
)
private val postPads = listOf(
    Pad(Param.CHORUS_ON, Motif.CHORUS, "Chorus", "post.card.chorus", Action.FocusPost(PostBlock.CHORUS)) { it.ui.postFocus == PostBlock.CHORUS },
    Pad(Param.DELAY_ON, Motif.DELAY, "Delay", "post.card.delay", Action.FocusPost(PostBlock.DELAY)) { it.ui.postFocus == PostBlock.DELAY },
    Pad(Param.REVERB_ON, Motif.REVERB, "Reverb", "post.card.reverb", Action.FocusPost(PostBlock.REVERB)) { it.ui.postFocus == PostBlock.REVERB },
    Pad(Param.TREM_ON, Motif.TREM, "Trem", "post.card.trem", Action.FocusPost(PostBlock.TREM)) { it.ui.postFocus == PostBlock.TREM },
)

/**
 * What floats on the art: IN / OUT meters at its edges, the PRE and POST blocks
 * beside the amp, the amp title under it. Under a picker or panel the blocks step
 * back ([blocks] fades them, and they take no touch); the art and meters stay.
 */
@Composable
private fun Floating(ui: Ui, frames: LongState, banner: Boolean, blocks: () -> Float) {
    val s = ui.s
    Box(Modifier.fillMaxSize().padding(vertical = 10.dp)) {
        val gateOn = s.on(Param.GATE_ON)
        val threshold = s.param(Param.GATE)
        Column(Modifier.align(Alignment.CenterStart).padding(start = 10.dp).width(26.dp).fillMaxHeight(), horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(3.dp)) {
            InMeter(ui, frames, Modifier.weight(1f).ids("meters.in"))
            Lamp(ui.tel, VoLumColors.Amber) { gateOn && it.inDb < threshold }
            VText("GATE", style = VoLumType.label(7.sp, VoLumColors.CreamDim))
        }
        Column(Modifier.align(Alignment.CenterEnd).padding(end = 10.dp).width(30.dp).fillMaxHeight(), horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(3.dp)) {
            OutMeter(ui, frames, Modifier.weight(1f).ids("meters.out"))
            if (s.outputSafety) {
                Box(Modifier.background(VoLumColors.Amber, RoundedCornerShape(3.dp)).padding(horizontal = 2.dp).ids("status.output_safety")) {
                    VFitText("SAFE", style = VoLumType.label(8.sp, VoLumColors.AmberInk), minSize = 6.sp)
                }
            } else {
                Lamp(ui.tel, VoLumColors.Danger) { it.outDb > -1.0 }
                VText("CLIP", style = VoLumType.label(7.sp, VoLumColors.CreamDim))
            }
        }
        val step = Modifier.graphicsLayer { alpha = blocks() }.inert(s.ui.overlays.any { it !is Overlay.NameDialog })
        SectionBlock(ui, Section.PRE, "PRE", prePads, frames, Modifier.align(Alignment.BottomStart).padding(start = 46.dp).then(step))
        SectionBlock(ui, Section.POST, "POST", postPads, frames, Modifier.align(Alignment.BottomEnd).padding(end = 50.dp).then(step))
        AmpBlock(ui, frames, Modifier.align(Alignment.BottomCenter).then(step))
        Column(Modifier.align(Alignment.TopCenter), horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(6.dp)) {
            if (banner) FloatingBanner(ui, Modifier)
            EditReadout(ui)
        }
    }
}

/**
 * A floating PRE or POST block: the `:design` section frame (PRE crosshatched), its
 * lock state and four pedal pads whose figures ghost when bypassed. The block picks
 * the section; a pad opens that pedal. Its halo breathes with the signal.
 */
@Composable
private fun SectionBlock(ui: Ui, section: Section, title: String, pads: List<Pad>, frames: LongState, modifier: Modifier) {
    val s = ui.s
    val selected = s.ui.section == section
    val locked = (if (section == Section.PRE) s.locks.pre else s.locks.post) != null
    val engaged = pads.any { s.on(it.param) }
    val lit by animateFloatAsState(if (selected) 1f else 0f, tween(SectionSwitch.TravelMs, easing = FastOutSlowInEasing), label = "lit")
    Tappable(
        { ui.d(Action.SetSection(section)) },
        modifier.signalGlow(ui.tel, frames, engaged, selected),
        listOf(if (section == Section.PRE) "nav.section.pre" else "nav.section.post"),
        role = Role.Tab,
    ) { pressed ->
        Column(
            Modifier.volumSectionFrame(locked, expanded = selected)
                .then(if (section == Section.PRE) Modifier.preCrosshatch(fill = false) else Modifier)
                .then(
                    when {
                        pressed && !selected -> Modifier.hairlineFrame(VoLumColors.GoldDim, 1.4.dp)
                        lit > 0.01f -> Modifier.hairlineFrame(VoLumColors.Amber.withOpacity(lit), 1.4.dp)
                        else -> Modifier
                    },
                )
                .padding(start = 8.dp, end = 8.dp, top = 4.dp, bottom = 6.dp),
            verticalArrangement = Arrangement.spacedBy(2.dp),
        ) {
            Row(Modifier.height(24.dp).width(204.dp), verticalAlignment = Alignment.CenterVertically) {
                VText(title, style = VoLumType.label(10.sp, if (locked) VoLumColors.Gold else VoLumColors.GoldDim))
                Fill()
                VoLumIcon(if (locked) VoLumGlyph.Lock else VoLumGlyph.Unlock, color = lockInk(locked, false))
            }
            Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                pads.forEach { p ->
                    val on = s.on(p.param)
                    val focused = selected && p.focused(s)
                    Tappable({ ui.d(p.focus) }, Modifier.size(Metrics.target).semantics { contentDescription = p.label + if (on) " on" else " bypassed" }, listOf(p.id), role = Role.Tab) { padPressed ->
                        Box(Modifier.fillMaxSize().volumSelection(focused, padPressed, VoLumSelectionStyle.ListTeal, radius = 3.dp, inset = 0.dp), contentAlignment = Alignment.Center) {
                            MotifArt(p.motif, on, Modifier.size(34.dp), motifVariant(s, p.param))
                        }
                    }
                }
            }
        }
    }
}

/** The amp under its art: the `:design` title and diamond rule over channel and cab; it picks the AMP sheet. */
@Composable
private fun AmpBlock(ui: Ui, frames: LongState, modifier: Modifier) {
    val s = ui.s
    val selected = s.ui.section == Section.AMP
    val support = s.supportFocused
    val name = if (support) s.library.amp(s.rig.support.ampId)?.name ?: "Support empty" else s.amp?.name ?: "No amp"
    val lit by animateFloatAsState(if (selected) 1f else 0f, tween(SectionSwitch.TravelMs, easing = FastOutSlowInEasing), label = "lit")
    Tappable({ ui.d(Action.SetSection(Section.AMP)) }, modifier.widthIn(min = 220.dp, max = 330.dp).signalGlow(ui.tel, frames, true, selected), listOf("nav.section.amp"), role = Role.Tab) { pressed ->
        Column(
            Modifier.fillMaxWidth().background(Glass.Float, RoundedCornerShape(4.dp)).ampCircuitBoard(fill = false)
                .hairlineFrame(if (pressed && !selected) VoLumColors.GoldDim else lerp(VoLumColors.Frame, VoLumColors.Amber, lit), if (lit > 0.5f) 1.4.dp else 1.dp, 4.dp)
                .cornerBrackets(VoLumColors.HeroCorner, 8.dp, 4.dp)
                .padding(horizontal = 16.dp, vertical = 4.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
        ) {
            VFitText(name, Modifier.ids("amp.name"), style = if (support) VoLumType.label(21.sp, VoLumColors.Teal) else VoLumType.AmpTitle, minSize = 13.sp, textAlign = TextAlign.Center)
            Box(Modifier.width(110.dp).height(10.dp).diamondRule())
            VFitText(
                if (support) "SUPPORT  ·  " + chCab(s, support = true) else chCab(s) + if (s.dual) "  ·  DUAL" else "",
                style = VoLumType.label(9.sp, if (support) VoLumColors.TealDim else VoLumColors.CreamDim), minSize = 7.sp, textAlign = TextAlign.Center,
            )
        }
    }
}

/** The art window names the knob being moved and its value for a moment, like a hardware modeler's display. */
@Composable
private fun EditReadout(ui: Ui) {
    val params = ui.s.rig.params
    var prev by remember { mutableStateOf(params) }
    var last by remember { mutableStateOf<Param?>(null) }
    var shown by remember { mutableStateOf(false) }
    LaunchedEffect(params) {
        val changed = params.keys.firstOrNull { k -> prev[k] != params[k] && k.continuous }
        prev = params
        if (changed != null) {
            last = changed
            shown = true
            delay(1400)
            shown = false
        }
    }
    AnimatedVisibility(shown, enter = fadeIn(tween(120)), exit = fadeOut(tween(420))) {
        val p = last ?: return@AnimatedVisibility
        Row(
            Modifier.background(Glass.Readout, RoundedCornerShape(4.dp)).hairlineFrame(VoLumColors.GoldDim, radius = 4.dp).padding(horizontal = 14.dp, vertical = 2.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(10.dp),
        ) {
            VText(p.label.uppercase(), style = VoLumType.label(11.sp, VoLumColors.CreamDim))
            VText(Fmt.of(p, ui.s.param(p)), style = VoLumType.label(24.sp, VoLumColors.Gold))
        }
    }
}

private val Param.continuous: Boolean
    get() = options.isEmpty() && !name.endsWith("_ON") && !name.endsWith("_SYNC") && !name.contains("CAPTURE") && !name.contains("PING") && !name.contains("POLARITY") && !name.contains("DIVISION")

/**
 * A section switch: the old sheet slides toward the block it belonged to and fades
 * while the new one, surface and all, slides in from the side of its block; the notch
 * and the blocks' amber frame travel across in the same time. Long enough to read as
 * a move from one block to the next, not a swap.
 */
internal object SectionSwitch {
    const val OutMs = 420
    const val InDelayMs = 180
    const val InMs = 570
    const val TravelMs = InDelayMs + InMs
}

/**
 * The section sheet: capped under half the screen, surfaced like its block (PRE
 * crosshatch, AMP circuit board), with an amber notch pointing up at the block it
 * belongs to. While a picker is open it folds to the row the picker came from.
 */
@Composable
private fun Sheet(ui: Ui, modifier: Modifier) {
    val s = ui.s
    val notch by animateFloatAsState(
        when (s.ui.section) { Section.PRE -> 0.16f; Section.AMP -> 0.5f; Section.POST -> 0.84f },
        tween(SectionSwitch.TravelMs, easing = FastOutSlowInEasing),
        label = "notch",
    )
    Box(
        modifier
            .drawBehind {
                val x = size.width * notch
                val w = 10.dp.toPx()
                val path = Path().apply { moveTo(x - w, 0f); lineTo(x, -8.dp.toPx()); lineTo(x + w, 0f); close() }
                drawPath(path, VoLumColors.Amber)
            }
            .clipToBounds()
            .panelDepth()
            .drawBehind { drawLine(VoLumColors.GoldDim.withOpacity(0.7f), Offset(0f, 0.5f), Offset(size.width, 0.5f), 1.dp.toPx()) }
            .cornerBrackets(VoLumColors.Corner, 10.dp, 5.dp),
    ) {
        AnimatedContent(
            s.ui.section,
            transitionSpec = {
                val dir = if (targetState.ordinal > initialState.ordinal) 1 else -1
                val enter = slideInHorizontally(tween(SectionSwitch.InMs, SectionSwitch.InDelayMs, LinearOutSlowInEasing)) { it / 3 * dir } +
                    fadeIn(tween(SectionSwitch.InMs, SectionSwitch.InDelayMs, LinearOutSlowInEasing))
                val exit = slideOutHorizontally(tween(SectionSwitch.OutMs, easing = LinearEasing)) { -it / 3 * dir } +
                    fadeOut(tween(SectionSwitch.OutMs, easing = LinearEasing))
                enter togetherWith exit
            },
            label = "section",
            modifier = Modifier.fillMaxWidth().height(Metrics.sheet),
        ) { section ->
            Column(
                Modifier.fillMaxSize()
                    .then(
                        when (section) {
                            Section.PRE -> Modifier.preCrosshatch(fill = false)
                            Section.AMP -> Modifier.ampCircuitBoard(fill = false)
                            Section.POST -> Modifier
                        },
                    )
                    .padding(start = 20.dp, end = 20.dp, top = 8.dp, bottom = 8.dp),
                verticalArrangement = Arrangement.spacedBy(6.dp),
            ) {
                when (section) {
                    Section.PRE -> PreSheet(ui)
                    Section.AMP -> AmpSheet(ui)
                    Section.POST -> PostSheet(ui)
                }
            }
        }
    }
}
