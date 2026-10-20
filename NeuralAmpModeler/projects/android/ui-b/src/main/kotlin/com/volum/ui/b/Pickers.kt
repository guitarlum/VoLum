package com.volum.ui.b

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.text.BasicText
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.Amp
import com.volum.core.Cab
import com.volum.core.Overlay
import com.volum.core.Param
import com.volum.core.Pedal
import com.volum.core.Preset
import com.volum.core.ScreenId
import com.volum.core.VoLumState

@Composable
internal fun AmpPicker(s: VoLumState) {
    val st = LocalStage.current
    Panel(ScreenId.AMP_PICKER, "Amps", emptyArray(), subtitle = "Tap an amp to play it. Your rig on each amp is remembered.") {
        Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                Caption("BUNDLED", Modifier.fillMaxWidth())
                FitGrid(s.library.amps.filter { !it.custom }, 3, 52.dp, Modifier.fillMaxWidth().weight(1f)) { amp ->
                    AmpRow(s, amp, s.ampId == amp.id, arrayOf("amp.list.select")) { st.go(Action.SelectAmp(amp.id)) }
                }
            }
            Column(Modifier.width(300.dp).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                Caption("CUSTOM", Modifier.fillMaxWidth())
                TextPad("+ NEW CUSTOM AMP", { st.go(Action.OpenBuilder(null)) }, Modifier.fillMaxWidth().height(48.dp), arrayOf("custom.amp.add"))
                FitGrid(s.library.amps.filter { it.custom }, 1, 52.dp, Modifier.fillMaxWidth().weight(1f),
                    empty = "Build an amp from your own .nam captures.") { amp ->
                    Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                        Box(Modifier.weight(1f).fillMaxHeight()) {
                            AmpRow(s, amp, s.ampId == amp.id, arrayOf("amp.list.select_custom", "amp.list.custom")) { st.go(Action.SelectAmp(amp.id)) }
                        }
                        TextPad("EDIT", { st.go(Action.OpenBuilder(amp.id)) }, Modifier.width(56.dp).fillMaxHeight(), arrayOf("custom.amp.edit"))
                        TextPad("DEL", { st.go(Action.RequestDeleteAmp(amp.id)) }, Modifier.width(52.dp).fillMaxHeight(), arrayOf("custom.amp.delete"),
                            style = Type.label(12.sp, Ink.alert))
                    }
                }
            }
        }
    }
}

@Composable
private fun AmpRow(s: VoLumState, amp: Amp, selected: Boolean, ids: Array<String>, onClick: () -> Unit) {
    val st = LocalStage.current
    Pad(onClick, Modifier.fillMaxSize(), ids, selected = selected, sel = Sel.LIST_TEAL, fill = Ink.panel, role = Role.RadioButton) {
        Row(Modifier.fillMaxSize(), verticalAlignment = Alignment.CenterVertically) {
            StageArt(amp.art, st.tele, animate = false, Modifier.width(52.dp).fillMaxHeight())
            FitText(amp.name, Type.value(14.sp, if (selected) Ink.hot else Ink.text), Modifier.weight(1f).padding(horizontal = 10.dp), align = TextAlign.Start)
        }
    }
}

@Composable
internal fun IrMenu(s: VoLumState) {
    val st = LocalStage.current
    val support = s.supportFocused
    val cab = if (support) s.rig.support.cab else s.rig.cab
    val current = if (support) s.rig.support.irId else s.rig.irId
    Panel(ScreenId.CAB_IR_MENU, "Custom IR", arrayOf("ir.menu.close"), subtitle = if (support) "plays on the SUPPORT amp" else "plays on the MAIN amp",
        actions = { TextPad("MANAGE IRs", { st.go(Action.Open(Overlay.IrManage)) }, Modifier.width(140.dp).height(48.dp), arrayOf("ir.menu.manage")) }) {
        FitGrid(s.library.irs, 2, 56.dp, Modifier.fillMaxSize(), empty = "No IRs yet. MANAGE IRs imports your WAV files.") { ir ->
            TextPad(ir.name, { st.go(Action.SelectIr(ir.id)) }, Modifier.fillMaxSize(), arrayOf("ir.menu.select", "dual.support.ir.select"),
                selected = cab == Cab.CUSTOM_IR && current == ir.id, sel = Sel.LIST_TEAL, style = Type.value(15.sp), role = Role.RadioButton, fill = Ink.panel)
        }
    }
}

@Composable
internal fun CaptureMenu(s: VoLumState, slot: Int) {
    val st = LocalStage.current
    val p = if (slot == 1) Param.NAM1_CAPTURE else Param.NAM2_CAPTURE
    val current = s.param(p).toInt()
    Panel(ScreenId.PRE_CAPTURE_MENU, "NAM $slot capture", arrayOf("pre.capture.close"), subtitle = "the pedal NAM $slot plays in front of the amp",
        actions = { TextPad("MANAGE PEDALS", { st.go(Action.Open(Overlay.PedalManage)) }, Modifier.width(160.dp).height(48.dp), arrayOf("pre.capture.manage")) }) {
        Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                Caption("BUNDLED", Modifier.fillMaxWidth())
                FitGrid(s.library.pedals.filter { !it.custom }, 3, 52.dp, Modifier.fillMaxWidth().weight(1f)) { pedal ->
                    PedalRow(pedal, current == pedal.index, if (pedal.index == 0) arrayOf("pre.capture.empty") else arrayOf("pre.capture.select")) {
                        st.go(Action.SelectCapture(slot, pedal.index))
                    }
                }
            }
            Column(Modifier.width(280.dp).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                Caption("CUSTOM", Modifier.fillMaxWidth())
                FitGrid(s.library.pedals.filter { it.custom }, 1, 52.dp, Modifier.fillMaxWidth().weight(1f),
                    empty = "MANAGE PEDALS imports your own .nam pedal captures.") { pedal ->
                    PedalRow(pedal, current == pedal.index, arrayOf("pre.capture.select_custom", "pre.capture.custom")) { st.go(Action.SelectCapture(slot, pedal.index)) }
                }
            }
        }
    }
}

@Composable
private fun PedalRow(pedal: Pedal, selected: Boolean, ids: Array<String>, onClick: () -> Unit) {
    Pad(onClick, Modifier.fillMaxSize(), ids, selected = selected, sel = Sel.LIST_TEAL, fill = Ink.panel, role = Role.RadioButton) {
        Row(Modifier.fillMaxSize().padding(horizontal = 8.dp), verticalAlignment = Alignment.CenterVertically) {
            PedalMotif(pedal.index, selected, Modifier.width(36.dp).fillMaxHeight())
            FitText(if (pedal.index == 0) "EMPTY" else pedal.name, Type.value(14.sp, if (selected) Ink.hot else Ink.text),
                Modifier.weight(1f).padding(start = 8.dp), align = TextAlign.Start)
        }
    }
}

@Composable
internal fun SupportAmpMenu(s: VoLumState) {
    val st = LocalStage.current
    val current = s.rig.support.ampId
    Panel(ScreenId.SUPPORT_AMP_MENU, "SUPPORT amp", arrayOf("dual.support.close"), subtitle = "the second amp Dual Amp blends with ${s.amp?.name ?: "MAIN"}") {
        Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                Caption("BUNDLED", Modifier.fillMaxWidth())
                FitGrid(s.library.amps.filter { !it.custom }, 3, 52.dp, Modifier.fillMaxWidth().weight(1f)) { amp ->
                    AmpRow(s, amp, current == amp.id, arrayOf("dual.support.select")) { st.go(Action.SelectSupportAmp(amp.id)) }
                }
            }
            Column(Modifier.width(280.dp).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                Caption("NO SUPPORT", Modifier.fillMaxWidth())
                TextPad("NONE", { st.go(Action.SelectSupportAmp(null)) }, Modifier.fillMaxWidth().height(52.dp), arrayOf("dual.support.none"),
                    selected = current == null, sel = Sel.LIST_TEAL, fill = Ink.panel)
                Caption("CUSTOM", Modifier.fillMaxWidth())
                FitGrid(s.library.amps.filter { it.custom }, 1, 52.dp, Modifier.fillMaxWidth().weight(1f), empty = "No custom amps yet.") { amp ->
                    AmpRow(s, amp, current == amp.id, arrayOf("dual.support.select_custom")) { st.go(Action.SelectSupportAmp(amp.id)) }
                }
            }
        }
    }
}

@Composable
internal fun SoundPicker(s: VoLumState, o: Overlay.SoundPicker) {
    val st = LocalStage.current
    val program = o.program
    val holds = s.library.sounds[program]?.let { s.library.preset(it.presetId)?.name ?: "an invalid slot" }
    val pick = { p: Preset -> st.go(Action.AssignSound(program, p.id)) }
    Panel(ScreenId.PLAY_PICKER, if (o.replace) "Replace a Sound" else "Add a Sound", arrayOf("play.picker.close"),
        subtitle = "program ${program.toString().padStart(2, '0')} ${if (holds != null) "holds $holds" else "is empty"} · pick the preset it recalls",
        actions = {
            Stepper("PROGRAM ${program.toString().padStart(2, '0')}", { st.go(Action.SetPickerProgram((program + 127) % 128)) },
                { st.go(Action.SetPickerProgram((program + 1) % 128)) }, Modifier.width(250.dp).height(48.dp),
                arrayOf("play.picker.program"), arrayOf("play.picker.program"), valueStyle = Type.tech(16.sp), description = "program")
        }) {
        Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            Column(Modifier.weight(2f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                Caption("FACTORY", Modifier.fillMaxWidth())
                FitGrid(s.library.presets.filter { it.factory }, 2, 52.dp, Modifier.fillMaxWidth().weight(1f)) { p ->
                    PresetRow(s, p, s.library.sounds[program]?.presetId == p.id, arrayOf("play.picker.factory")) { pick(p) }
                }
            }
            Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                Caption("USER", Modifier.fillMaxWidth())
                FitGrid(s.library.presets.filter { !it.factory }, 1, 52.dp, Modifier.fillMaxWidth().weight(1f), empty = "Save a preset in BUILD to see it here.") { p ->
                    PresetRow(s, p, s.library.sounds[program]?.presetId == p.id, arrayOf("play.picker.user")) { pick(p) }
                }
            }
        }
    }
}

@Composable
internal fun PresetRow(s: VoLumState, p: Preset, selected: Boolean, ids: Array<String>, onClick: () -> Unit) {
    Pad(onClick, Modifier.fillMaxSize(), ids, selected = selected, sel = Sel.LIST_TEAL, fill = Ink.panel, role = Role.RadioButton) {
        Column(Modifier.fillMaxSize().padding(horizontal = 12.dp), verticalArrangement = Arrangement.Center) {
            FitText(p.name, Type.value(15.sp, if (selected) Ink.hot else Ink.text), Modifier.fillMaxWidth().height(22.dp), align = TextAlign.Start)
            FitText(s.library.amp(p.ampId)?.name ?: "", Type.label(10.sp), Modifier.fillMaxWidth().height(14.dp), align = TextAlign.Start)
        }
    }
}