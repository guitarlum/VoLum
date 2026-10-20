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
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.drawBehind
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
import com.volum.design.art.AmpThumbnail
import com.volum.design.art.ArtRef
import com.volum.design.art.Motif
import com.volum.design.components.MotifArt
import com.volum.design.components.VoLumGlyph
import com.volum.design.components.VoLumIcon
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumType

/** A catalog row in the ListTeal dialect with the gold edge bar on the selected one, an art at its head, one shared name size per grid. */
@Composable
internal fun ListRow(
    title: String,
    selected: Boolean,
    onClick: () -> Unit,
    ids: Array<out String>,
    modifier: Modifier = Modifier,
    subtitle: String? = null,
    lead: (@Composable () -> Unit)? = null,
) {
    val nameStyle = LocalGridText.current ?: VoLumType.label(13.sp, VoLumColors.TextMed)
    Pad(onClick, modifier.fillMaxSize(), ids, selected = selected, style = Sel.ListTeal, tone = Tone.BARE, role = Role.RadioButton) { _ ->
        Row(Modifier.fillMaxSize().padding(start = 10.dp, end = 8.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            if (selected) GoldBar()
            lead?.invoke()
            Column(Modifier.weight(1f)) {
                FitText(title, nameStyle.copy(color = if (selected) VoLumColors.TextBright else VoLumColors.TextMed), Modifier.fillMaxWidth().height(20.dp), align = TextAlign.Start)
                if (subtitle != null) FitText(subtitle, VoLumType.label(9.sp, VoLumColors.TealDim), Modifier.fillMaxWidth().height(14.dp), align = TextAlign.Start)
            }
        }
    }
}

/** The gold edge bar of the selected list row. */
@Composable
private fun GoldBar() = Box(Modifier.width(3.dp).height(30.dp).drawBehind { drawRect(VoLumColors.Gold) })

private val rowName = VoLumType.label(13.sp, VoLumColors.TextMed)

@Composable
internal fun AmpPicker(s: VoLumState) {
    val st = LocalStage.current
    val bundled = s.library.amps.filter { !it.custom }
    val custom = s.library.amps.filter { it.custom }
    Panel(s, ScreenId.AMP_PICKER, "Amps", emptyArray(), subtitle = "Tap an amp to play it. Your rig on each amp is remembered.") {
        Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(14.dp)) {
            Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                SectionTitle("BUNDLED", Modifier.fillMaxWidth())
                FitGrid(bundled, 2, 52.dp, Modifier.fillMaxWidth().weight(1f), gap = 4.dp, shared = SharedText(bundled.map { it.name }, rowName, 64.dp)) { amp ->
                    AmpRow(amp, s.ampId == amp.id, arrayOf("amp.list.select")) { st.go(Action.SelectAmp(amp.id)) }
                }
            }
            Column(Modifier.width(320.dp).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                SectionTitle("CUSTOM · built from your own .nam captures", Modifier.fillMaxWidth())
                Pad({ st.go(Action.OpenBuilder(null)) }, Modifier.fillMaxWidth().height(48.dp), arrayOf("custom.amp.add"), tone = Tone.PRIMARY) { ink ->
                    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        VoLumIcon(VoLumGlyph.Plus, color = ink, size = 14.dp)
                        FitText("NEW CUSTOM AMP", VoLumType.label(12.sp, ink), Modifier.width(150.dp))
                    }
                }
                FitGrid(custom, 1, 52.dp, Modifier.fillMaxWidth().weight(1f), gap = 4.dp, empty = "Build an amp from your own .nam captures.",
                    shared = SharedText(custom.map { it.name }, rowName, 170.dp)) { amp ->
                    Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                        Box(Modifier.weight(1f).fillMaxHeight()) {
                            AmpRow(amp, s.ampId == amp.id, arrayOf("amp.list.select_custom", "amp.list.custom")) { st.go(Action.SelectAmp(amp.id)) }
                        }
                        GlyphPad(VoLumGlyph.Edit, "EDIT", { st.go(Action.OpenBuilder(amp.id)) }, arrayOf("custom.amp.edit"))
                        GlyphPad(VoLumGlyph.Delete, "DELETE", { st.go(Action.RequestDeleteAmp(amp.id)) }, arrayOf("custom.amp.delete"), danger = true)
                    }
                }
            }
        }
    }
}

/** A small tool target: the `:design` glyph over its spelled-out name. */
@Composable
internal fun GlyphPad(glyph: VoLumGlyph, label: String, onClick: () -> Unit, ids: Array<out String>, danger: Boolean = false, width: androidx.compose.ui.unit.Dp = 58.dp) {
    Pad(onClick, Modifier.width(width).fillMaxHeight(), ids, tone = if (danger) Tone.DANGER else Tone.FRAMED, description = label) { ink ->
        Column(horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(2.dp)) {
            VoLumIcon(glyph, color = ink, size = 16.dp)
            FitText(label, VoLumType.label(8.sp, ink), Modifier.width(width - 6.dp).height(12.dp))
        }
    }
}

@Composable
private fun AmpRow(amp: Amp, selected: Boolean, ids: Array<String>, onClick: () -> Unit) {
    ListRow(amp.name, selected, onClick, ids, lead = { AmpThumbnail(ArtRef.of(amp), Modifier.size(26.dp), selected) })
}

@Composable
internal fun IrMenu(s: VoLumState) {
    val st = LocalStage.current
    val support = s.supportFocused
    val cab = if (support) s.rig.support.cab else s.rig.cab
    val current = if (support) s.rig.support.irId else s.rig.irId
    val playing = s.library.ir(current)?.takeIf { cab == Cab.CUSTOM_IR }
    Panel(s, ScreenId.CAB_IR_MENU, "Custom IR", arrayOf("ir.menu.close"), subtitle = "an impulse response in place of the cab, on the ${if (support) "SUPPORT" else "MAIN"} amp") {
        Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(14.dp)) {
            Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                SectionTitle("YOUR IRs", Modifier.fillMaxWidth())
                FitGrid(s.library.irs, 1, 52.dp, Modifier.fillMaxWidth().weight(1f), gap = 4.dp, empty = "No IRs yet. MANAGE IRs imports your WAV files.") { ir ->
                    ListRow(ir.name, cab == Cab.CUSTOM_IR && current == ir.id, { st.go(Action.SelectIr(ir.id)) }, arrayOf("ir.menu.select", "dual.support.ir.select"),
                        subtitle = if (cab == Cab.CUSTOM_IR && current == ir.id) "PLAYING" else String.format("%+.1f dB", ir.levelDb),
                        lead = { VoLumIcon(VoLumGlyph.IrBars, color = VoLumColors.BtnIrOnBorder, size = 18.dp) })
                }
            }
            Card(Modifier.width(300.dp).fillMaxHeight(), "NOW ON ${if (support) "SUPPORT" else "MAIN"}") {
                FitText(playing?.name ?: "a cab (${cab.label})", VoLumType.label(15.sp, if (playing != null) VoLumColors.BtnIrOnText else VoLumColors.Cream),
                    Modifier.fillMaxWidth().height(24.dp), align = TextAlign.Start)
                FitText(if (playing != null) "Tap another IR to switch, or pick a cab on BUILD." else "Tap an IR to play it instead of the cab.",
                    VoLumType.body(11.sp, VoLumColors.CreamDim), Modifier.fillMaxWidth().height(16.dp), align = TextAlign.Start)
                Box(Modifier.weight(1f))
                FitText("Import, rename, level and cuts", VoLumType.body(11.sp, VoLumColors.CreamDim), Modifier.fillMaxWidth().height(16.dp), align = TextAlign.Start)
                TextPad("MANAGE IRs", { st.go(Action.Open(Overlay.IrManage)) }, Modifier.fillMaxWidth().height(52.dp), arrayOf("ir.menu.manage"), tone = Tone.PRIMARY)
            }
        }
    }
}

@Composable
internal fun CaptureMenu(s: VoLumState, slot: Int) {
    val st = LocalStage.current
    val p = if (slot == 1) Param.NAM1_CAPTURE else Param.NAM2_CAPTURE
    val current = s.param(p).toInt()
    val bundled = s.library.pedals.filter { !it.custom }
    val custom = s.library.pedals.filter { it.custom }
    Panel(s, ScreenId.PRE_CAPTURE_MENU, "NAM $slot capture", arrayOf("pre.capture.close"), subtitle = "the pedal NAM $slot plays in front of the amp",
        actions = { TextPad("MANAGE PEDALS", { st.go(Action.Open(Overlay.PedalManage)) }, Modifier.width(150.dp).height(48.dp), arrayOf("pre.capture.manage")) }) {
        Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(14.dp)) {
            Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                SectionTitle("BUNDLED", Modifier.fillMaxWidth())
                FitGrid(bundled, 3, 52.dp, Modifier.fillMaxWidth().weight(1f), gap = 4.dp, shared = SharedText(bundled.map { if (it.index == 0) "Empty" else it.name }, rowName, 64.dp)) { pedal ->
                    PedalRow(pedal, slot, current == pedal.index, if (pedal.index == 0) arrayOf("pre.capture.empty") else arrayOf("pre.capture.select")) {
                        st.go(Action.SelectCapture(slot, pedal.index))
                    }
                }
            }
            Column(Modifier.width(270.dp).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                SectionTitle("CUSTOM", Modifier.fillMaxWidth())
                FitGrid(custom, 1, 52.dp, Modifier.fillMaxWidth().weight(1f), gap = 4.dp, empty = "MANAGE PEDALS imports your own .nam pedal captures.") { pedal ->
                    PedalRow(pedal, slot, current == pedal.index, arrayOf("pre.capture.select_custom", "pre.capture.custom")) { st.go(Action.SelectCapture(slot, pedal.index)) }
                }
            }
        }
    }
}

@Composable
private fun PedalRow(pedal: Pedal, slot: Int, selected: Boolean, ids: Array<String>, onClick: () -> Unit) {
    ListRow(if (pedal.index == 0) "Empty" else pedal.name, selected, onClick, ids,
        lead = { MotifArt(if (slot == 1) Motif.NAM1 else Motif.NAM2, pedal.index != 0, Modifier.width(22.dp).height(28.dp)) })
}

@Composable
internal fun SupportAmpMenu(s: VoLumState) {
    val st = LocalStage.current
    val current = s.rig.support.ampId
    val bundled = s.library.amps.filter { !it.custom }
    Panel(s, ScreenId.SUPPORT_AMP_MENU, "SUPPORT amp", arrayOf("dual.support.close"), subtitle = "the second amp Dual Amp blends with ${s.amp?.name ?: "MAIN"}") {
        Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(14.dp)) {
            Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                SectionTitle("BUNDLED", Modifier.fillMaxWidth())
                FitGrid(bundled, 2, 52.dp, Modifier.fillMaxWidth().weight(1f), gap = 4.dp, shared = SharedText(bundled.map { it.name }, rowName, 64.dp)) { amp ->
                    AmpRow(amp, current == amp.id, arrayOf("dual.support.select")) { st.go(Action.SelectSupportAmp(amp.id)) }
                }
            }
            Column(Modifier.width(280.dp).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                SectionTitle("NO SUPPORT", Modifier.fillMaxWidth())
                Box(Modifier.fillMaxWidth().height(52.dp)) {
                    ListRow("None · MAIN plays alone", current == null, { st.go(Action.SelectSupportAmp(null)) }, arrayOf("dual.support.none"))
                }
                SectionTitle("CUSTOM", Modifier.fillMaxWidth())
                FitGrid(s.library.amps.filter { it.custom }, 1, 52.dp, Modifier.fillMaxWidth().weight(1f), gap = 4.dp, empty = "No custom amps yet.") { amp ->
                    AmpRow(amp, current == amp.id, arrayOf("dual.support.select_custom")) { st.go(Action.SelectSupportAmp(amp.id)) }
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
    val factory = s.library.presets.filter { it.factory }
    val user = s.library.presets.filter { !it.factory }
    Panel(s, ScreenId.PLAY_PICKER, if (o.replace) "Replace a Sound" else "Add a Sound", arrayOf("play.picker.close"),
        subtitle = "program ${twoDigit(program)} ${if (holds != null) "holds $holds" else "is empty"} · pick the preset it recalls",
        actions = {
            Stepper("PROGRAM ${twoDigit(program)}", { st.go(Action.SetPickerProgram((program + 127) % 128)) },
                { st.go(Action.SetPickerProgram((program + 1) % 128)) }, Modifier.width(230.dp).height(48.dp),
                arrayOf("play.picker.program"), arrayOf("play.picker.program"), valueStyle = VoLumType.value(14.sp, VoLumColors.Gold), description = "program")
        }) {
        Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(14.dp)) {
            Column(Modifier.weight(2f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                SectionTitle("FACTORY", Modifier.fillMaxWidth())
                FitGrid(factory, 2, 52.dp, Modifier.fillMaxWidth().weight(1f), gap = 4.dp, shared = SharedText(factory.map { it.name }, presetName, 40.dp)) { p ->
                    PresetRow(s, p, s.library.sounds[program]?.presetId == p.id, arrayOf("play.picker.factory")) { pick(p) }
                }
            }
            Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                SectionTitle("USER", Modifier.fillMaxWidth())
                FitGrid(user, 1, 52.dp, Modifier.fillMaxWidth().weight(1f), gap = 4.dp, empty = "Save a preset in BUILD to see it here.",
                    shared = SharedText(user.map { it.name }, presetName, 40.dp)) { p ->
                    PresetRow(s, p, s.library.sounds[program]?.presetId == p.id, arrayOf("play.picker.user")) { pick(p) }
                }
            }
        }
    }
}

internal val presetName = VoLumType.display(17.sp, VoLumColors.Cream)

/** A preset in a list: its name in Poiret (the desktop's sound-row face), its amp in teal. */
@Composable
internal fun PresetRow(s: VoLumState, p: Preset, selected: Boolean, ids: Array<String>, onClick: () -> Unit) {
    val style = LocalGridText.current ?: presetName
    Pad(onClick, Modifier.fillMaxSize(), ids, selected = selected, style = Sel.ListTeal, tone = Tone.BARE, role = Role.RadioButton) { _ ->
        Row(Modifier.fillMaxSize().padding(start = 10.dp, end = 10.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            if (selected) GoldBar()
            Column(Modifier.weight(1f)) {
                FitText(p.name, style.copy(color = if (selected) VoLumColors.TextBright else VoLumColors.Cream), Modifier.fillMaxWidth().height(22.dp), align = TextAlign.Start)
                FitText(s.library.amp(p.ampId)?.name ?: "", VoLumType.label(9.sp, VoLumColors.TealDim), Modifier.fillMaxWidth().height(14.dp), align = TextAlign.Start)
            }
        }
    }
}
