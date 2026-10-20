package com.volum.ui.b

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
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.semantics.selected
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.Amp
import com.volum.core.Cab
import com.volum.core.Overlay
import com.volum.core.Param
import com.volum.core.Pedal
import com.volum.core.Preset
import com.volum.core.Reducer
import com.volum.core.ScreenId
import com.volum.core.VoLumState
import com.volum.design.art.ArtRef
import com.volum.design.art.Motif
import com.volum.design.components.MotifArt
import com.volum.design.components.VoLumGlyph
import com.volum.design.components.VoLumIcon
import com.volum.design.components.VoLumListRow
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumType

/** A catalog row: the `:design` list row (ListTeal with its gold edge bar on the selected one), filling its grid cell; the ids land on its clickable. */
@Composable
internal fun ListRow(
    title: String,
    selected: Boolean,
    onClick: () -> Unit,
    ids: Array<out String>,
    modifier: Modifier = Modifier,
    subtitle: String? = null,
    art: ArtRef? = null,
    trailing: (@Composable RowScope.() -> Unit)? = null,
) {
    VoLumListRow(title, selected, onClick, modifier.fillMaxSize().semantics { this.selected = selected }.ids(ids), art = art, subtitle = subtitle, trailing = trailing)
}

@Composable
internal fun AmpPicker(s: VoLumState) {
    val st = LocalStage.current
    val bundled = s.library.amps.filter { !it.custom }
    val custom = s.library.amps.filter { it.custom }
    Panel(s, ScreenId.AMP_PICKER, "Amps", emptyArray(), subtitle = "Tap an amp to play it. Your rig on each amp is remembered.") {
        Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(14.dp)) {
            Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                SectionTitle("BUNDLED", Modifier.fillMaxWidth())
                FitGrid(bundled, 2, 52.dp, Modifier.fillMaxWidth().weight(1f), gap = 4.dp, reveal = bundled.indexOfFirst { it.id == s.ampId }) { amp ->
                    AmpRow(amp, s.ampId == amp.id, arrayOf("amp.list.select")) { st.go(Action.SelectAmp(amp.id)) }
                }
            }
            Column(Modifier.width(390.dp).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                SectionTitle("CUSTOM · built from your own .nam captures", Modifier.fillMaxWidth())
                Pad({ st.go(Action.OpenBuilder(null)) }, Modifier.fillMaxWidth().height(48.dp), arrayOf("custom.amp.add"), tone = Tone.PRIMARY) { ink ->
                    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        VoLumIcon(VoLumGlyph.Plus, color = ink, size = 14.dp)
                        FitText("NEW CUSTOM AMP", VoLumType.label(12.sp, ink), Modifier.width(150.dp).height(18.dp))
                    }
                }
                FitGrid(custom, 1, 52.dp, Modifier.fillMaxWidth().weight(1f), gap = 4.dp, empty = "Build an amp from your own .nam captures.",
                    reveal = custom.indexOfFirst { it.id == s.ampId }) { amp ->
                    val tools = LocalGridGroup.current
                    Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                        Box(Modifier.weight(1f).fillMaxHeight()) {
                            AmpRow(amp, s.ampId == amp.id, arrayOf("amp.list.select_custom", "amp.list.custom")) { st.go(Action.SelectAmp(amp.id)) }
                        }
                        GlyphPad(VoLumGlyph.Edit, "EDIT", { st.go(Action.OpenBuilder(amp.id)) }, arrayOf("custom.amp.edit"), width = 58.dp, group = tools)
                        GlyphPad(VoLumGlyph.Delete, "DELETE", { st.go(Action.RequestDeleteAmp(amp.id)) }, arrayOf("custom.amp.delete"), danger = true, width = 58.dp, group = tools)
                    }
                }
            }
        }
    }
}

/** A small tool target: the `:design` glyph over its spelled-out name; tools in one row share a [group] size. */
@Composable
internal fun GlyphPad(glyph: VoLumGlyph, label: String, onClick: () -> Unit, ids: Array<out String>, danger: Boolean = false, width: Dp = 64.dp, group: FitGroup? = null) {
    Pad(onClick, Modifier.width(width).fillMaxHeight(), ids, tone = if (danger) Tone.DANGER else Tone.FRAMED, description = label) { ink ->
        Column(horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(2.dp)) {
            VoLumIcon(glyph, color = ink, size = 16.dp)
            FitText(label, VoLumType.label(9.sp, ink), Modifier.width(width - 8.dp).height(13.dp), group = group)
        }
    }
}

@Composable
private fun AmpRow(amp: Amp, selected: Boolean, ids: Array<String>, onClick: () -> Unit) {
    ListRow(amp.name, selected, onClick, ids, art = ArtRef.of(amp))
}

/** What a cab is called on its amp's cab row ("V30 cab", or a custom amp's own cab name). */
private fun cabName(s: VoLumState, support: Boolean, cab: Cab): String {
    val amp = if (support) s.library.amp(s.rig.support.ampId) else s.amp
    val name = if (amp?.custom == true) amp.cabNames.getOrElse(cab.ordinal) { cab.label } else cab.label
    return if (cab == Cab.NONE) "No Cab" else "$name cab"
}

@Composable
internal fun IrMenu(s: VoLumState) {
    val st = LocalStage.current
    val support = s.supportFocused
    val cab = if (support) s.rig.support.cab else s.rig.cab
    val channel = if (support) s.rig.support.channel else s.rig.channel
    val current = if (support) s.rig.support.irId else s.rig.irId
    val playing = s.library.ir(current)?.takeIf { cab == Cab.CUSTOM_IR }
    val lane = if (support) "SUPPORT" else "MAIN"
    val back = Reducer.stockCab(if (support) s.library.amp(s.rig.support.ampId) else s.amp, channel)
    val rows = s.library.irs.size + 1
    Panel(s, ScreenId.CAB_IR_MENU, "Custom IR", arrayOf("ir.menu.close"), subtitle = "an impulse response in place of the cab, on the $lane amp",
        bodyHeight = (22 + rows * 56 + 8).coerceAtLeast(230).dp) {
        Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(14.dp)) {
            Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                SectionTitle("YOUR IRs", Modifier.fillMaxWidth())
                Box(Modifier.fillMaxWidth().height(52.dp)) {
                    ListRow("No IR", playing == null, {
                        if (playing != null) st.go(if (support) Action.SelectSupportCab(back) else Action.SelectCab(back))
                        st.go(Action.Back)
                    }, if (playing == null) arrayOf("ir.menu.close") else emptyArray(), subtitle = if (playing == null) "the ${cabName(s, support, cab)} is playing" else "back to the ${cabName(s, support, back)}")
                }
                FitGrid(s.library.irs, 1, 52.dp, Modifier.fillMaxWidth().weight(1f), gap = 4.dp, empty = "No IRs yet. MANAGE IRs imports your WAV files.",
                    reveal = s.library.irs.indexOfFirst { it.id == current }) { ir ->
                    val on = cab == Cab.CUSTOM_IR && current == ir.id
                    ListRow(ir.name, on, { st.go(Action.SelectIr(ir.id)) }, arrayOf("ir.menu.select", "dual.support.ir.select"),
                        subtitle = if (on) "PLAYING · ${String.format("%+.1f dB", ir.levelDb)}" else String.format("%+.1f dB", ir.levelDb),
                        trailing = { VoLumIcon(VoLumGlyph.IrBars, color = VoLumColors.BtnIrOnBorder, size = 18.dp) })
                }
            }
            Card(Modifier.width(300.dp).fillMaxHeight(), "NOW ON $lane") {
                FitText(playing?.name ?: cabName(s, support, cab), VoLumType.label(15.sp, if (playing != null) VoLumColors.BtnIrOnText else VoLumColors.Cream),
                    Modifier.fillMaxWidth().height(24.dp), align = TextAlign.Start)
                FitText(if (playing != null) "Tap another IR to switch." else "Tap an IR to play it instead of the cab.",
                    hint(), Modifier.fillMaxWidth().height(18.dp), align = TextAlign.Start)
                Box(Modifier.weight(1f))
                FitText("Import, rename, level and cuts", hint(), Modifier.fillMaxWidth().height(18.dp), align = TextAlign.Start)
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
                FitGrid(bundled, 3, 52.dp, Modifier.fillMaxWidth().weight(1f), gap = 4.dp, reveal = bundled.indexOfFirst { it.index == current }) { pedal ->
                    PedalRow(pedal, slot, current == pedal.index, if (pedal.index == 0) arrayOf("pre.capture.empty") else arrayOf("pre.capture.select")) {
                        st.go(Action.SelectCapture(slot, pedal.index))
                    }
                }
            }
            Column(Modifier.width(270.dp).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                SectionTitle("CUSTOM", Modifier.fillMaxWidth())
                FitGrid(custom, 1, 52.dp, Modifier.fillMaxWidth().weight(1f), gap = 4.dp, empty = "MANAGE PEDALS imports your own .nam pedal captures.",
                    reveal = custom.indexOfFirst { it.index == current }) { pedal ->
                    PedalRow(pedal, slot, current == pedal.index, arrayOf("pre.capture.select_custom", "pre.capture.custom")) { st.go(Action.SelectCapture(slot, pedal.index)) }
                }
            }
        }
    }
}

@Composable
private fun PedalRow(pedal: Pedal, slot: Int, selected: Boolean, ids: Array<String>, onClick: () -> Unit) {
    ListRow(if (pedal.index == 0) "Empty" else pedal.name, selected, onClick, ids,
        trailing = { MotifArt(if (slot == 1) Motif.NAM1 else Motif.NAM2, pedal.index != 0, Modifier.width(22.dp).height(28.dp)) })
}

@Composable
internal fun SupportAmpMenu(s: VoLumState) {
    val st = LocalStage.current
    val current = s.rig.support.ampId
    val bundled = s.library.amps.filter { !it.custom }
    val custom = s.library.amps.filter { it.custom }
    Panel(s, ScreenId.SUPPORT_AMP_MENU, "SUPPORT amp", arrayOf("dual.support.close"), subtitle = "the second amp Dual Amp blends with ${s.amp?.name ?: "MAIN"}") {
        Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(14.dp)) {
            Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                SectionTitle("BUNDLED", Modifier.fillMaxWidth())
                FitGrid(bundled, 2, 52.dp, Modifier.fillMaxWidth().weight(1f), gap = 4.dp, reveal = bundled.indexOfFirst { it.id == current }) { amp ->
                    AmpRow(amp, current == amp.id, arrayOf("dual.support.select")) { st.go(Action.SelectSupportAmp(amp.id)) }
                }
            }
            Column(Modifier.width(280.dp).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                SectionTitle("NO SUPPORT", Modifier.fillMaxWidth())
                Box(Modifier.fillMaxWidth().height(52.dp)) {
                    ListRow("None", current == null, { st.go(Action.SelectSupportAmp(null)) }, arrayOf("dual.support.none"), subtitle = "MAIN plays alone")
                }
                SectionTitle("CUSTOM", Modifier.fillMaxWidth())
                FitGrid(custom, 1, 52.dp, Modifier.fillMaxWidth().weight(1f), gap = 4.dp, empty = "No custom amps yet.",
                    reveal = custom.indexOfFirst { it.id == current }) { amp ->
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
    val assigned = s.library.sounds[program]?.presetId
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
                FitGrid(factory, 2, 52.dp, Modifier.fillMaxWidth().weight(1f), gap = 4.dp, reveal = factory.indexOfFirst { it.id == assigned }) { p ->
                    PresetRow(s, p, assigned == p.id, arrayOf("play.picker.factory")) { pick(p) }
                }
            }
            Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                SectionTitle("USER", Modifier.fillMaxWidth())
                FitGrid(user, 1, 52.dp, Modifier.fillMaxWidth().weight(1f), gap = 4.dp, empty = "Save a preset in BUILD to see it here.",
                    reveal = user.indexOfFirst { it.id == assigned }) { p ->
                    PresetRow(s, p, assigned == p.id, arrayOf("play.picker.user")) { pick(p) }
                }
            }
        }
    }
}

/** A preset in a list: the `:design` list row, its amp under the name. */
@Composable
internal fun PresetRow(s: VoLumState, p: Preset, selected: Boolean, ids: Array<String>, onClick: () -> Unit) {
    val amp = s.library.amp(p.ampId)
    ListRow(p.name, selected, onClick, ids, subtitle = amp?.name, art = amp?.let { ArtRef.of(it) })
}
