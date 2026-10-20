package com.volum.ui.b

import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInHorizontally
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicText
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.NamePurpose
import com.volum.core.Overlay
import com.volum.core.Param
import com.volum.core.PostBlock
import com.volum.core.PreBlock
import com.volum.core.Reducer
import com.volum.core.Section
import com.volum.core.View
import com.volum.core.VoLumState
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

/** BUILD: the pedalboard across the top (tap a pedal to edit it, stomp its footswitch to bypass), the focused block's controls below. */
@Composable
internal fun BuildSurface(s: VoLumState) {
    Column(Modifier.fillMaxSize().background(Ink.ground).screen(s.screen())) {
        TopBar(s) { PresetBar(s) }
        PedalBoard(s, Modifier.fillMaxWidth().height(124.dp).padding(horizontal = 8.dp))
        Row(Modifier.weight(1f).fillMaxWidth().padding(start = 8.dp, end = 8.dp, top = 8.dp, bottom = 8.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Meter(input = true, Modifier.width(6.dp).fillMaxHeight().featureId("meters.in"))
            AnimatedContent(
                targetState = editorKey(s),
                transitionSpec = {
                    val dir = if (targetState.order > initialState.order) 1 else -1
                    (fadeIn(tween(160)) + slideInHorizontally(tween(180)) { it / 12 * dir }) togetherWith fadeOut(tween(90))
                },
                modifier = Modifier.weight(1f).fillMaxHeight(),
                label = "editor",
            ) { key ->
                Box(Modifier.fillMaxSize().clip(RoundedCornerShape(8.dp)).surface(key.surface).brackets().padding(8.dp)) {
                    when (key.section) {
                        Section.AMP -> AmpEditor(s)
                        Section.PRE -> PreEditor(s, s.ui.preFocus)
                        Section.POST -> PostEditor(s, s.ui.postFocus)
                    }
                }
            }
            Meter(input = false, Modifier.width(6.dp).fillMaxHeight().featureId("meters.out"))
        }
    }
}

private data class EditorKey(val section: Section, val block: Int) {
    val order: Int get() = when (section) { Section.PRE -> block; Section.AMP -> 10; Section.POST -> 20 + block }
    val surface: Surface get() = when (section) { Section.PRE -> Surface.PRE; Section.AMP -> Surface.AMP; Section.POST -> Surface.POST }
}

private fun editorKey(s: VoLumState) = EditorKey(s.ui.section, when (s.ui.section) {
    Section.PRE -> s.ui.preFocus.ordinal
    Section.POST -> s.ui.postFocus.ordinal
    Section.AMP -> 0
})

@Composable
private fun RowScope.PresetBar(s: VoLumState) {
    val st = LocalStage.current
    Row(Modifier.weight(1f).height(48.dp), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
        Pad({ st.go(Action.StepPreset(-1)) }, Modifier.width(48.dp).fillMaxHeight(), arrayOf("header.preset.prev"), description = "Previous preset") {
            BasicText("‹", style = Type.display(26.sp))
        }
        Pad({ st.go(Action.Open(Overlay.PresetMenu)) }, Modifier.weight(1f).fillMaxHeight(), arrayOf("header.preset.menu"), description = "Presets") {
            Column(Modifier.fillMaxSize().padding(horizontal = 8.dp), verticalArrangement = Arrangement.Center) {
                Box(Modifier.fillMaxWidth().featureId("header.preset_name")) {
                    FitText(s.presetName ?: "Default", Type.display(19.sp, Ink.hot), Modifier.fillMaxWidth().height(26.dp))
                }
                if (s.rig.dirty) Box(Modifier.fillMaxWidth().featureId("header.unsaved")) {
                    FitText("(unsaved)", Type.label(9.sp, Ink.text), Modifier.fillMaxWidth().height(12.dp))
                }
            }
        }
        Pad({ st.go(Action.StepPreset(1)) }, Modifier.width(48.dp).fillMaxHeight(), arrayOf("header.preset.next"), description = "Next preset") {
            BasicText("›", style = Type.display(26.sp))
        }
    }
    TextPad("SAVE", { st.go(Action.Open(Overlay.NameDialog(NamePurpose.SAVE_PRESET, Reducer.nameSuggestion(s)))) },
        Modifier.width(64.dp).height(48.dp), arrayOf("presets.save.open"))
}

@Composable
private fun PedalBoard(s: VoLumState, modifier: Modifier) {
    val editingPre = s.ui.view == View.BUILD && s.ui.section == Section.PRE
    val editingPost = s.ui.view == View.BUILD && s.ui.section == Section.POST
    Row(modifier, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
        PreBlock.entries.forEach { b -> BoardPedal(s, b.onParam(), Ids.preCard(b), editingPre && s.ui.preFocus == b, Surface.PRE) }
        AmpBlock(s, Modifier.width(176.dp).fillMaxHeight())
        PostBlock.entries.forEach { b -> BoardPedal(s, b.onParam(), Ids.postCard(b), editingPost && s.ui.postFocus == b, Surface.POST) }
    }
}

/** A pedal on the board: the card (tap to edit) over a footswitch (stomp to bypass). */
@Composable
private fun RowScope.BoardPedal(s: VoLumState, p: Param, cardIds: Array<String>, focused: Boolean, kind: Surface) {
    val st = LocalStage.current
    val on = s.on(p)
    Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(4.dp)) {
        Pad({ focusOf(st, p) }, Modifier.fillMaxWidth().height(56.dp).surface(kind), cardIds, selected = focused, sel = Sel.AMBER_PICKER,
            fill = if (focused) Ink.raised else Ink.panel, description = "Edit ${stompLabel(p, s)}") {
            Column(Modifier.fillMaxSize().padding(horizontal = 4.dp, vertical = 4.dp), horizontalAlignment = Alignment.CenterHorizontally) {
                PedalMotif(p.ordinal, on, Modifier.fillMaxWidth().weight(1f))
                FitText(stompLabel(p, s), Type.label(10.sp, if (on) Ink.text else Ink.dim), Modifier.fillMaxWidth().height(16.dp))
            }
        }
        Pad({ st.go(Action.ToggleParam(p)) }, Modifier.fillMaxWidth().weight(1f), Ids.footswitch(p), selected = on, sel = Sel.BRASS, haptic = true,
            description = "${stompLabel(p, s)} ${if (on) "on" else "bypassed"}") {
            Canvas(Modifier.fillMaxSize()) {
                led(on, Offset(size.width / 2, size.height * 0.3f), 4.dp.toPx())
                drawCircle(Ink.line, size.minDimension * 0.26f, Offset(size.width / 2, size.height * 0.66f))
                drawCircle(if (on) Ink.dim else Ink.pressed, size.minDimension * 0.19f, Offset(size.width / 2, size.height * 0.66f))
            }
        }
    }
}

private fun focusOf(st: Stage, p: Param) {
    PreBlock.entries.firstOrNull { it.onParam() == p }?.let { st.go(Action.FocusPre(it)); return }
    PostBlock.entries.firstOrNull { it.onParam() == p }?.let { st.go(Action.FocusPost(it)) }
}

/** The amp in the middle of the board: art and name (tap to edit), Dual Amp switch in place of a footswitch. */
@Composable
private fun AmpBlock(s: VoLumState, modifier: Modifier) {
    val st = LocalStage.current
    val focused = s.ui.section == Section.AMP
    val support = s.library.amp(s.rig.support.ampId)
    Column(modifier, verticalArrangement = Arrangement.spacedBy(4.dp)) {
        Pad({ st.go(Action.SetSection(Section.AMP)) }, Modifier.fillMaxWidth().height(56.dp), arrayOf("nav.section.amp"), selected = focused,
            sel = Sel.AMBER_PICKER, fill = Ink.raised, description = "Edit the amp") {
            StageArt(s.amp?.art ?: 0, st.tele, animate = s.signal.animateArt, Modifier.fillMaxSize().featureId("amp.hero"))
            Column(Modifier.fillMaxSize().background(Ink.ground.copy(alpha = 0.45f)).padding(horizontal = 8.dp), verticalArrangement = Arrangement.Center) {
                Box(Modifier.fillMaxWidth().featureId("amp.name")) {
                    FitText(s.amp?.name ?: "", Type.display(18.sp, Ink.hot), Modifier.fillMaxWidth().height(24.dp))
                }
                FitText(if (s.dual) "+ ${support?.name ?: "no SUPPORT"}" else "CH ${s.rig.channel + 1} · ${s.rig.cab.label}",
                    Type.label(9.sp), Modifier.fillMaxWidth().height(14.dp))
            }
        }
        Pad({ st.go(Action.ToggleParam(Param.DUAL_ON)) }, Modifier.fillMaxWidth().weight(1f), arrayOf("dual.toggle"), selected = s.dual, sel = Sel.BRASS,
            haptic = true, description = "Dual Amp ${if (s.dual) "on" else "off"}") {
            Row(Modifier.fillMaxSize().padding(horizontal = 12.dp), verticalAlignment = Alignment.CenterVertically) {
                Canvas(Modifier.width(12.dp).height(12.dp)) { led(s.dual, Offset(size.width / 2, size.height / 2), 4.dp.toPx()) }
                FitText("DUAL AMP", Type.label(12.sp, if (s.dual) Ink.text else Ink.dim), Modifier.weight(1f).padding(start = 8.dp), align = TextAlign.Start)
            }
        }
    }
}
