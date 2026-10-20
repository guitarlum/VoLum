package com.volum.ui.b

import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInHorizontally
import androidx.compose.animation.slideOutHorizontally
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsPressedAsState
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.requiredSize
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.stateDescription
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.Overlay
import com.volum.core.Param
import com.volum.core.PostBlock
import com.volum.core.PreBlock
import com.volum.core.Section
import com.volum.core.View
import com.volum.core.VoLumState
import androidx.compose.foundation.layout.size
import com.volum.design.art.AmpThumbnail
import com.volum.design.art.ArtRef
import com.volum.design.components.HeroAmp
import com.volum.design.components.panelDepth
import com.volum.design.theme.withOpacity
import com.volum.design.components.PedalCard
import com.volum.design.components.PresetBarFace
import com.volum.design.components.PresetBarPart
import com.volum.design.components.VoLumHero
import com.volum.design.components.VoLumLed
import com.volum.design.components.VoLumMeter
import com.volum.design.components.ampCircuitBoard
import com.volum.design.components.hairlineFrame
import com.volum.design.components.preCrosshatch
import com.volum.design.components.presetBarLabel
import com.volum.design.components.volumSectionFrame
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumMotion
import com.volum.design.theme.VoLumType
import com.volum.harness.featureId
import com.volum.harness.screen

internal fun PreBlock.onParam(): Param = when (this) {
    PreBlock.PITCH -> Param.PITCH_ON
    PreBlock.COMP -> Param.COMP_ON
    PreBlock.NAM1 -> Param.NAM1_ON
    PreBlock.NAM2 -> Param.NAM2_ON
}

internal fun PostBlock.onParam(): Param = when (this) {
    PostBlock.CHORUS -> Param.CHORUS_ON
    PostBlock.DELAY -> Param.DELAY_ON
    PostBlock.REVERB -> Param.REVERB_ON
    PostBlock.TREM -> Param.TREM_ON
}

/**
 * BUILD, the pedalboard: PRE pedals, the amp and POST pedals across the top, each
 * card over a footswitch you stomp; tap a card and its controls open below in the
 * section's own surface (PRE crosshatch, AMP circuit board, POST frame).
 */
@Composable
internal fun BuildSurface(s: VoLumState) {
    val st = LocalStage.current
    Column(Modifier.fillMaxSize().screen(s.screen())) {
        Header(s) { _ -> PresetBar(s) }
        Row(Modifier.weight(1f).fillMaxWidth().padding(start = 6.dp, end = 6.dp, top = 8.dp, bottom = 8.dp), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            VoLumMeter({ st.level(true) }, "IN", Modifier.width(28.dp).fillMaxHeight().featureId("meters.in"), frame = { st.frames.longValue })
            Column(Modifier.weight(1f).fillMaxHeight()) {
                PedalBoard(s, Modifier.fillMaxWidth().height(138.dp))
                if (s.ui.top == null && s.ui.banner != null) {
                    BannerSlot(s, Modifier.padding(top = 8.dp), tight = true)
                    Spacer(Modifier.height(4.dp))
                } else {
                    Spacer(Modifier.height(8.dp))
                }
                AnimatedContent(
                    targetState = editorKey(s),
                    transitionSpec = {
                        val dir = if (targetState.order > initialState.order) 1 else -1
                        (fadeIn(tween(VoLumMotion.PanelMs)) + slideInHorizontally(tween(200)) { it / 10 * dir }) togetherWith
                            (fadeOut(tween(100)) + slideOutHorizontally(tween(160)) { -it / 16 * dir })
                    },
                    modifier = Modifier.weight(1f).fillMaxWidth(),
                    label = "editor",
                ) { key ->
                    val locked = when (key.section) {
                        Section.PRE -> s.locks.pre != null
                        Section.POST -> s.locks.post != null
                        Section.AMP -> false
                    }
                    Box(
                        Modifier.fillMaxSize().volumSectionFrame(locked, expanded = true)
                            .then(when (key.section) {
                                Section.PRE -> Modifier.preCrosshatch(fill = false)
                                Section.AMP -> Modifier.ampCircuitBoard(fill = false)
                                Section.POST -> Modifier
                            })
                            .padding(horizontal = 10.dp, vertical = if (s.ui.banner != null) 2.dp else 6.dp),
                    ) {
                        when (key.section) {
                            Section.AMP -> AmpEditor(s)
                            Section.PRE -> PreEditor(s, s.ui.preFocus)
                            Section.POST -> PostEditor(s, s.ui.postFocus)
                        }
                    }
                }
            }
            VoLumMeter({ st.level(false) }, "OUT", Modifier.width(28.dp).fillMaxHeight().featureId("meters.out"), out = true, frame = { st.frames.longValue })
        }
    }
}

private data class EditorKey(val section: Section, val block: Int) {
    val order: Int get() = when (section) { Section.PRE -> block; Section.AMP -> 10; Section.POST -> 20 + block }
}

private fun editorKey(s: VoLumState) = EditorKey(s.ui.section, when (s.ui.section) {
    Section.PRE -> s.ui.preFocus.ordinal
    Section.POST -> s.ui.postFocus.ordinal
    Section.AMP -> 0
})

/**
 * The `:design` preset bar with 64 dp arrow targets (you step presets between songs):
 * `<` steps back, the name opens the preset menu, `>` steps on.
 */
@Composable
private fun RowScope.PresetBar(s: VoLumState) {
    val st = LocalStage.current
    val prev = remember { MutableInteractionSource() }
    val open = remember { MutableInteractionSource() }
    val next = remember { MutableInteractionSource() }
    val prevDown by prev.collectIsPressedAsState()
    val openDown by open.collectIsPressedAsState()
    val nextDown by next.collectIsPressedAsState()
    val part = when {
        prevDown -> PresetBarPart.PREV
        openDown -> PresetBarPart.NAME
        nextDown -> PresetBarPart.NEXT
        else -> null
    }
    val canStep = s.library.presetsFor(s.ampId).isNotEmpty()
    Box(Modifier.weight(1f).widthIn(min = 240.dp).height(HeaderHeight)) {
        PresetBarFace(s.presetName, s.rig.dirty, Modifier.fillMaxSize(), canStep, part)
        Row(Modifier.fillMaxSize()) {
            Box(Modifier.width(64.dp).fillMaxHeight()
                .clickable(prev, null, enabled = canStep, role = Role.Button) { st.go(Action.StepPreset(-1)) }
                .semantics { contentDescription = "Previous preset" }
                .featureId("header.preset.prev"))
            Box(Modifier.weight(1f).fillMaxHeight()
                .clickable(open, null, role = Role.Button, onClickLabel = "Open presets") { st.go(Action.Open(Overlay.PresetMenu)) }
                .semantics { stateDescription = presetBarLabel(s.presetName, s.rig.dirty) }
                .ids(if (s.rig.dirty) arrayOf("header.preset.menu", "header.preset_name", "header.unsaved") else arrayOf("header.preset.menu", "header.preset_name")))
            Box(Modifier.width(64.dp).fillMaxHeight()
                .clickable(next, null, enabled = canStep, role = Role.Button) { st.go(Action.StepPreset(1)) }
                .semantics { contentDescription = "Next preset" }
                .featureId("header.preset.next"))
        }
    }
}

@Composable
private fun PedalBoard(s: VoLumState, modifier: Modifier) {
    val editingPre = s.ui.view == View.BUILD && s.ui.section == Section.PRE
    val editingPost = s.ui.view == View.BUILD && s.ui.section == Section.POST
    val names = rememberFitGroup()
    val switches = rememberFitGroup()
    Row(modifier, horizontalArrangement = Arrangement.spacedBy(5.dp)) {
        PreBlock.entries.forEach { b -> BoardPedal(s, b.onParam(), Ids.preCard(b), editingPre && s.ui.preFocus == b, names, switches) }
        AmpBlock(s, Modifier.width(168.dp).fillMaxHeight(), switches)
        PostBlock.entries.forEach { b -> BoardPedal(s, b.onParam(), Ids.postCard(b), editingPost && s.ui.postFocus == b, names, switches) }
    }
}

/**
 * A pedal on the board: the `:design` pedal card (tap to edit, amber when it is the one
 * open below) over a 64 dp footswitch you stomp to bypass. The card's own lamp is left
 * out here because the footswitch carries it, so a tap on the card only ever edits.
 */
@Composable
private fun RowScope.BoardPedal(s: VoLumState, p: Param, cardIds: Array<String>, focused: Boolean, names: FitGroup, switches: FitGroup) {
    val st = LocalStage.current
    val on = s.on(p)
    Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(4.dp)) {
        StompCard(p, stompLabel(p, s), on, onClick = { focusOf(st, p) }, onToggle = { st.go(Action.ToggleParam(p)) }, Modifier.fillMaxWidth().weight(1f),
            Modifier.ids(cardIds).semantics { contentDescription = "Edit ${stompLabel(p, s)}" }, names, focused = focused, lamp = false)
        Footswitch(on, { st.go(Action.ToggleParam(p)) }, Modifier.fillMaxWidth().height(64.dp), Ids.footswitch(p),
            "${stompLabel(p, s)} ${if (on) "on" else "bypassed"}", if (on) "ON" else "BYPASS", switches)
    }
}

/** A stomp target: brass-selected and lit when the pedal is on, a lamp and its state under the foot. */
@Composable
internal fun Footswitch(on: Boolean, onClick: () -> Unit, modifier: Modifier, ids: Array<out String>, description: String, caption: String, group: FitGroup? = null) {
    Pad(onClick, modifier, ids, selected = on, role = Role.Switch, style = Sel.Brass, haptic = true, description = description) { ink ->
        Column(Modifier.fillMaxWidth().padding(horizontal = 4.dp), horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(2.dp)) {
            VoLumLed(on)
            FitText(caption, VoLumType.label(10.sp, if (on) ink else VoLumColors.TextDim), Modifier.fillMaxWidth().height(15.dp), group = group)
        }
    }
}

private fun focusOf(st: Stage, p: Param) {
    PreBlock.entries.firstOrNull { it.onParam() == p }?.let { st.go(Action.FocusPre(it)); return }
    PostBlock.entries.firstOrNull { it.onParam() == p }?.let { st.go(Action.FocusPost(it)) }
}

/**
 * The amp in the middle of the board: its static hero art (tap to edit), Dual Amp as its
 * footswitch. With Dual Amp on, the SUPPORT amp's art sits teal-framed in the corner, so
 * both amps read from the board.
 */
@Composable
private fun AmpBlock(s: VoLumState, modifier: Modifier, switches: FitGroup) {
    val st = LocalStage.current
    val focused = s.ui.section == Section.AMP
    val amp = s.amp
    val support = s.library.amp(s.rig.support.ampId)
    val source = remember { MutableInteractionSource() }
    val pressed by source.collectIsPressedAsState()
    Column(modifier, verticalArrangement = Arrangement.spacedBy(4.dp)) {
        Box(
            Modifier.fillMaxWidth().weight(1f)
                .clickable(source, null, role = Role.Button) { st.go(Action.SetSection(Section.AMP)) }
                .semantics { contentDescription = "Edit the amp" }
                .featureId("nav.section.amp")
                .then(if (focused || pressed) Modifier.hairlineFrame(if (focused) VoLumColors.Amber else VoLumColors.GoldDim, 1.6.dp, 4.dp) else Modifier),
        ) {
            VoLumHero(HeroAmp(amp?.name ?: "", amp?.let { ArtRef.of(it) }), Modifier.fillMaxSize().padding(2.dp).featureId("amp.hero"))
            if (s.dual && support != null) {
                Box(Modifier.align(Alignment.TopEnd).padding(top = 6.dp, end = 6.dp).size(30.dp).panelDepth(3.dp)
                    .hairlineFrame(VoLumColors.Teal.withOpacity(0.8f), radius = 3.dp).padding(3.dp)) {
                    AmpThumbnail(ArtRef.of(support), Modifier.fillMaxSize(), true)
                }
            }
            Column(
                Modifier.align(Alignment.BottomCenter).fillMaxWidth().padding(horizontal = 6.dp, vertical = 5.dp)
                    .drawBehind { drawRect(VoLumColors.HeroBg.copy(alpha = 0.82f)) },
            ) {
                Box(Modifier.fillMaxWidth().featureId("amp.name")) {
                    FitText(amp?.name ?: "", VoLumType.label(12.sp, VoLumColors.Gold), Modifier.fillMaxWidth().height(17.dp))
                }
                FitText(if (s.dual) "+ ${support?.name ?: "no SUPPORT yet"}" else "CH ${s.rig.channel + 1} · ${s.rig.cab.label}",
                    VoLumType.label(9.sp, if (s.dual) VoLumColors.Teal else VoLumColors.CreamDim), Modifier.fillMaxWidth().height(13.dp))
            }
        }
        Footswitch(s.dual, { st.go(Action.ToggleParam(Param.DUAL_ON)) }, Modifier.fillMaxWidth().height(64.dp), arrayOf("dual.toggle"),
            "Dual Amp ${if (s.dual) "on" else "off"}", if (s.dual) "DUAL AMP ON" else "DUAL AMP", switches)
    }
}
