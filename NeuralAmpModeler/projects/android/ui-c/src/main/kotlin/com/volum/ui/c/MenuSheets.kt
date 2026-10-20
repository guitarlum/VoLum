package com.volum.ui.c

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.unit.dp
import com.volum.core.Action
import com.volum.core.Amp
import com.volum.core.Overlay
import com.volum.core.Param
import com.volum.core.Preset
import com.volum.design.art.ArtRef
import com.volum.design.components.VoLumGlyph
import com.volum.design.components.VoLumSelectionStyle
import com.volum.design.theme.VoLumColors

/** Scroll a snapped list so its [index]th row of [columns] is in view. */
@Composable
private fun rememberScrolledTo(index: Int, columns: Int): androidx.compose.foundation.ScrollState {
    val st = rememberScrollState()
    val pitch = rowPitchPx()
    LaunchedEffect(Unit) { if (index >= 0) st.scrollTo((index / columns - 1).coerceAtLeast(0) * pitch) }
    return st
}

/** The amp list: bundled amps with their art thumbnails, the current one marked and scrolled into view; CUSTOM beside it. */
@Composable
internal fun AmpPicker(ui: Ui, modifier: Modifier) {
    val s = ui.s
    val bundled = s.library.amps.filter { !it.custom }
    val custom = s.library.amps.filter { it.custom }
    PanelFrame(ui, "Choose amp", "", { ui.back() }, modifier, crumb = "${bundled.size} bundled") {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(14.dp)) {
            SnapList(gridRows(bundled.size, 3), Modifier.weight(0.66f), rememberScrolledTo(bundled.indexOfFirst { it.id == s.ampId }, 3)) {
                Grid(bundled, 3) { a, m -> ListRow(a.name, listOf("amp.list.select"), { ui.d(Action.SelectAmp(a.id)) }, m, sub = "${a.channels} CH", selected = a.id == s.ampId, art = ArtRef.of(a)) }
            }
            SnapList(custom.size + 2, Modifier.weight(0.34f)) {
                GroupHeader("Custom · your amps", Modifier.ids("amp.list.custom"))
                Btn("+ Add your amp", listOf("custom.amp.add"), { ui.d(Action.OpenBuilder(null)) }, Modifier.fillMaxWidth())
                custom.forEach { a -> CustomAmpRow(ui, a) }
            }
        }
    }
}

@Composable
private fun CustomAmpRow(ui: Ui, a: Amp) {
    ListRow(a.name, listOf("amp.list.select_custom"), { ui.d(Action.SelectAmp(a.id)) }, Modifier.fillMaxWidth(), sub = "${a.channels} CH", selected = a.id == ui.s.ampId, art = ArtRef.of(a)) {
        GlyphBtn(VoLumGlyph.Edit, "Edit ${a.name}", listOf("custom.amp.edit"), { ui.d(Action.OpenBuilder(a.id)) }, color = VoLumColors.GoldDim)
        GlyphBtn(VoLumGlyph.Delete, "Delete ${a.name}", listOf("custom.amp.delete"), { ui.d(Action.RequestDeleteAmp(a.id)) }, color = VoLumColors.Danger)
    }
}

@Composable
internal fun SupportAmpMenu(ui: Ui, modifier: Modifier) {
    val s = ui.s
    val current = s.rig.support.ampId
    val bundled: List<Amp?> = listOf<Amp?>(null) + s.library.amps.filter { !it.custom }
    val custom = s.library.amps.filter { it.custom }
    PanelFrame(ui, "Support amp", "dual.support.close", { ui.back() }, modifier, crumb = "Dual Amp") {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(14.dp)) {
            SnapList(gridRows(bundled.size, 3), Modifier.weight(0.66f), rememberScrolledTo(bundled.indexOfFirst { it?.id == current }, 3)) {
                Grid(bundled, 3) { a, m ->
                    if (a == null) ListRow("No support amp", listOf("dual.support.none"), { ui.d(Action.SelectSupportAmp(null)) }, m, sub = "EMPTY LANE", selected = current == null)
                    else ListRow(a.name, listOf("dual.support.select"), { ui.d(Action.SelectSupportAmp(a.id)) }, m, sub = "${a.channels} CH", selected = a.id == current, art = ArtRef.of(a))
                }
            }
            SnapList(custom.size + 2, Modifier.weight(0.34f)) {
                GroupHeader("Custom")
                if (custom.isEmpty()) Note("No custom amps yet.")
                custom.forEach { a -> ListRow(a.name, listOf("dual.support.select_custom"), { ui.d(Action.SelectSupportAmp(a.id)) }, Modifier.fillMaxWidth(), sub = "${a.channels} CH", selected = a.id == current, art = ArtRef.of(a)) }
            }
        }
    }
}

@Composable
internal fun IrMenu(ui: Ui, modifier: Modifier) {
    val s = ui.s
    val support = s.supportFocused
    val playing = if (support) s.rig.support.irId else s.rig.irId
    PanelFrame(ui, "Custom IR", "ir.menu.close", { ui.back() }, modifier, crumb = if (support) "Support cab" else "Cab", actions = {
        Btn("Manage IRs", listOf("ir.menu.manage"), { ui.d(Action.Open(Overlay.IrManage)) })
    }) {
        if (s.library.irs.isEmpty()) Note("No IRs yet. Manage IRs imports WAV files from your phone.")
        SnapList(gridRows(s.library.irs.size, 3), Modifier.fillMaxWidth()) {
            Grid(s.library.irs, 3) { ir, m ->
                ListRow(ir.name, listOf("ir.menu.select", "dual.support.ir.select"), { ui.d(Action.SelectIr(ir.id)) }, m, sub = if (ir.id == playing) "PLAYING" else "%+.1f dB".format(ir.levelDb), selected = ir.id == playing)
            }
        }
    }
}

/** The capture menu grouped the desktop way: Klon, TS / Boost, Distortion, Fuzz, then CUSTOM, with full names. */
@Composable
internal fun CaptureMenu(ui: Ui, o: Overlay.CaptureMenu, modifier: Modifier) {
    val s = ui.s
    val p = if (o.slot == 1) Param.NAM1_CAPTURE else Param.NAM2_CAPTURE
    val current = s.param(p).toInt()
    val custom = s.library.pedals.filter { it.custom }
    PanelFrame(ui, "NAM ${o.slot} capture", "pre.capture.close", { ui.back() }, modifier, crumb = "PRE", actions = {
        ListRow("Empty", listOf("pre.capture.empty"), { ui.d(Action.SelectCapture(o.slot, 0)) }, Modifier.width(150.dp), sub = "NO PEDAL", selected = current == 0)
    }) {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            Captures.groups.forEach { (group, indices) ->
                SnapList(indices.size + 1, Modifier.weight(1f)) {
                    GroupHeader(group)
                    indices.forEach { i ->
                        ListRow(Captures.fullName(i, ""), listOf("pre.capture.select"), { ui.d(Action.SelectCapture(o.slot, i)) }, Modifier.fillMaxWidth(), selected = i == current)
                    }
                }
            }
            SnapList(custom.size + 2, Modifier.weight(1.2f)) {
                GroupHeader("Custom · your pedals", Modifier.ids("pre.capture.custom"))
                custom.forEach { pd -> ListRow(pd.name, listOf("pre.capture.select_custom"), { ui.d(Action.SelectCapture(o.slot, pd.index)) }, Modifier.fillMaxWidth(), selected = pd.index == current) }
                Btn("Manage pedals", listOf("pre.capture.manage"), { ui.d(Action.Open(Overlay.PedalManage)) }, Modifier.fillMaxWidth())
            }
        }
    }
}

/** Add or replace a Sound: Factory and User presets side by side, the program stepper in the header. */
@Composable
internal fun SoundPicker(ui: Ui, o: Overlay.SoundPicker, modifier: Modifier) {
    val lib = ui.s.library
    val factory = lib.presets.filter { it.factory }
    val user = lib.presets.filter { !it.factory }
    val holds = lib.sounds[o.program]?.let { lib.preset(it.presetId)?.name ?: "invalid slot" } ?: "empty"
    PanelFrame(
        ui,
        if (o.replace) "Swap the Sound" else "Add a Sound",
        "play.picker.close",
        { ui.back() },
        modifier,
        crumb = "Program holds: $holds",
        actions = {
            Caption("Program", color = VoLumColors.CreamDim)
            com.volum.design.components.VoLumChannelStepper(
                "%03d".format(o.program),
                onPrev = { ui.d(Action.SetPickerProgram((o.program - 1 + 128) % 128)) },
                onNext = { ui.d(Action.SetPickerProgram((o.program + 1) % 128)) },
                prevModifier = Modifier.ids("play.picker.program"),
                nextModifier = Modifier.ids("play.picker.program"),
            )
        },
    ) {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            PresetColumn(ui, "Factory", factory, "play.picker.factory", o.program, Modifier.weight(1f))
            PresetColumn(ui, "User", user, "play.picker.user", o.program, Modifier.weight(1f))
        }
    }
}

@Composable
private fun PresetColumn(ui: Ui, title: String, presets: List<Preset>, id: String, program: Int, modifier: Modifier) {
    val current = ui.s.library.sounds[program]?.presetId
    SnapList(presets.size + 1, modifier) {
        GroupHeader("$title · ${presets.size}")
        if (presets.isEmpty()) Note("No $title presets yet.")
        presets.forEach { p ->
            ListRow(p.name, listOf(id), { ui.d(Action.AssignSound(program, p.id)) }, Modifier.fillMaxWidth(), sub = ui.s.library.amp(p.ampId)?.name, selected = p.id == current)
        }
    }
}
