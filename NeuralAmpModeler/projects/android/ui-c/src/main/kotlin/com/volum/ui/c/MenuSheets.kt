package com.volum.ui.c

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.Amp
import com.volum.core.Cab
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

private const val BundledShare = 0.62f
private val CustomTrailing = Metrics.target * 2 + 8.dp

/** The amp list: bundled amps with their art thumbnails, the current one marked and scrolled into view; CUSTOM beside it. All names share one size. */
@Composable
internal fun AmpPicker(ui: Ui, modifier: Modifier) {
    val s = ui.s
    val bundled = s.library.amps.filter { !it.custom }
    val custom = s.library.amps.filter { it.custom }
    PanelFrame(ui, "Choose amp", "", { ui.back() }, modifier, crumb = "${bundled.size} bundled") {
        BoxWithConstraints(Modifier.fillMaxWidth()) {
            val (left, right) = split(maxWidth, BundledShare)
            val cell = rowTitleRoom(cellWidth(left, 3), art = true)
            val customRow = rowTitleRoom(right - 8.dp, art = true, trailing = CustomTrailing)
            SharedFitRows(bundled.map { it.name to cell } + custom.map { it.name to customRow }) {
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(SplitGap)) {
                    SnapList(gridRows(bundled.size, 3), Modifier.width(left), rememberScrolledTo(bundled.indexOfFirst { it.id == s.ampId }, 3)) {
                        Grid(bundled, 3) { a, m -> ListRow(a.name, listOf("amp.list.select"), { ui.d(Action.SelectAmp(a.id)) }, m, sub = "${a.channels} CH", selected = a.id == s.ampId, art = ArtRef.of(a)) }
                    }
                    SnapList(custom.size + 2, Modifier.width(right)) {
                        GroupHeader("Custom · your amps", Modifier.ids("amp.list.custom"))
                        Btn("+ Add your amp", listOf("custom.amp.add"), { ui.d(Action.OpenBuilder(null)) }, Modifier.fillMaxWidth())
                        custom.forEach { a -> CustomAmpRow(ui, a) }
                    }
                }
            }
        }
    }
}

private val SplitGap = 14.dp

/** Two list columns in [width]: the first takes [share] of what the gap leaves. */
private fun split(width: Dp, share: Float): Pair<Dp, Dp> {
    val room = width - SplitGap
    return room * share to room * (1f - share)
}

@Composable
private fun CustomAmpRow(ui: Ui, a: Amp) {
    ListRow(a.name, listOf("amp.list.select_custom"), { ui.d(Action.SelectAmp(a.id)) }, Modifier.fillMaxWidth(), sub = "${a.channels} CH", selected = a.id == ui.s.ampId, art = ArtRef.of(a)) {
        GlyphBtn(VoLumGlyph.Edit, "Edit ${a.name}", listOf("custom.amp.edit"), { ui.d(Action.OpenBuilder(a.id)) }, color = VoLumColors.GoldDim)
        GlyphBtn(VoLumGlyph.Delete, "Delete ${a.name}", listOf("custom.amp.delete"), { ui.d(Action.RequestDeleteAmp(a.id)) }, color = VoLumColors.Danger)
    }
}

/**
 * The support amp list. Opened from the SUPPORT lane it is one column of two over
 * the MAIN half, CUSTOM under the bundled amps, so the lane and its chooser stay
 * in view beside it; opened from the sheet it spreads like the amp list.
 */
@Composable
internal fun SupportAmpMenu(ui: Ui, modifier: Modifier) {
    val s = ui.s
    val current = s.rig.support.ampId
    val bundled: List<Amp?> = listOf<Amp?>(null) + s.library.amps.filter { !it.custom }
    val custom = s.library.amps.filter { it.custom }
    val lane = s.supportFromLane()
    @Composable
    fun Option(a: Amp?, m: Modifier) {
        when {
            a == null -> ListRow("No support amp", listOf("dual.support.none"), { ui.d(Action.SelectSupportAmp(null)) }, m, sub = "MAIN PLAYS ALONE", selected = current == null)
            a.custom -> ListRow(a.name, listOf("dual.support.select_custom"), { ui.d(Action.SelectSupportAmp(a.id)) }, m, sub = "${a.channels} CH  ·  CUSTOM", selected = a.id == current, art = ArtRef.of(a))
            else -> ListRow(a.name, listOf("dual.support.select"), { ui.d(Action.SelectSupportAmp(a.id)) }, m, sub = "${a.channels} CH", selected = a.id == current, art = ArtRef.of(a))
        }
    }
    fun title(a: Amp?) = a?.name ?: "No support amp"
    PanelFrame(ui, "Support amp", "dual.support.close", { ui.back() }, modifier, crumb = if (lane) "Support lane" else "Dual Amp") {
        BoxWithConstraints(Modifier.fillMaxWidth()) {
            if (lane) {
                val cell = cellWidth(maxWidth, 2)
                val rows = gridRows(bundled.size, 2) + 1 + gridRows(custom.size.coerceAtLeast(1), 2)
                SharedFitRows((bundled + custom).map { title(it) to rowTitleRoom(cell, art = it != null) }) {
                    SnapList(rows, Modifier.fillMaxWidth(), rememberScrolledTo((bundled + custom).indexOfFirst { it?.id == current } + 1, 2)) {
                        Grid(bundled, 2) { a, m -> Option(a, m) }
                        GroupHeader("Custom · your amps")
                        if (custom.isEmpty()) Note("No custom amps yet.") else Grid(custom, 2) { a, m -> Option(a, m) }
                    }
                }
            } else {
                val (left, right) = split(maxWidth, BundledShare)
                val cell = cellWidth(left, 3)
                SharedFitRows(bundled.map { title(it) to rowTitleRoom(cell, art = it != null) } + custom.map { it.name to rowTitleRoom(right - 8.dp, art = true) }) {
                    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(SplitGap)) {
                        SnapList(gridRows(bundled.size, 3), Modifier.width(left), rememberScrolledTo(bundled.indexOfFirst { it?.id == current }, 3)) {
                            Grid(bundled, 3) { a, m -> Option(a, m) }
                        }
                        SnapList(custom.size + 2, Modifier.width(right)) {
                            GroupHeader("Custom · your amps")
                            if (custom.isEmpty()) Note("No custom amps yet.")
                            custom.forEach { a -> Option(a, Modifier.fillMaxWidth()) }
                        }
                    }
                }
            }
        }
    }
}

@Composable
internal fun IrMenu(ui: Ui, modifier: Modifier) {
    val s = ui.s
    val support = s.supportFocused
    val playing = if (support) s.rig.support.irId else s.rig.irId
    val cabIsIr = (if (support) s.rig.support.cab else s.rig.cab) == Cab.CUSTOM_IR
    PanelFrame(ui, "Custom IR", "ir.menu.close", { ui.back() }, modifier, crumb = if (support) "Support cab" else "Cab", actions = {
        Btn("Manage IRs", listOf("ir.menu.manage"), { ui.d(Action.Open(Overlay.IrManage)) })
    }) {
        if (s.library.irs.isEmpty()) Note("No IRs yet. Manage IRs imports WAV files from your phone.")
        else if (!cabIsIr) Note("The cab row plays ${cabLabel(if (support) s.rig.support.cab else s.rig.cab, if (support) s.library.amp(s.rig.support.ampId) else s.amp)} now. Pick an IR to play it instead.", color = VoLumColors.Cream, maxLines = 1, size = 11.sp)
        BoxWithConstraints(Modifier.fillMaxWidth()) {
            val cell = rowTitleRoom(cellWidth(maxWidth, 3), art = false)
            SharedFitRows(s.library.irs.map { it.name to cell }) {
                SnapList(gridRows(s.library.irs.size, 3), Modifier.fillMaxWidth()) {
                    Grid(s.library.irs, 3) { ir, m ->
                        val inUse = cabIsIr && ir.id == playing
                        ListRow(ir.name, listOf("ir.menu.select", "dual.support.ir.select"), { ui.d(Action.SelectIr(ir.id)) }, m, sub = if (inUse) "PLAYING  ·  %+.1f dB".format(ir.levelDb) else "%+.1f dB".format(ir.levelDb), selected = inUse)
                    }
                }
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
        Gap(w = 28.dp)
    }) {
        BoxWithConstraints(Modifier.fillMaxWidth()) {
            val unit = (maxWidth - 12.dp * Captures.groups.size) / (Captures.groups.size + 1.2f)
            val room = rowTitleRoom(unit - 8.dp, art = false)
            val customRoom = rowTitleRoom(unit * 1.2f - 8.dp, art = false)
            SharedFitRows(Captures.groups.flatMap { (_, ix) -> ix.map { Captures.fullName(it, "") to room } } + custom.map { it.name to customRoom }) {
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
    }
}

/** Add or replace a Sound: Factory and User presets side by side, the program stepper in the header. */
@Composable
internal fun SoundPicker(ui: Ui, o: Overlay.SoundPicker, modifier: Modifier) {
    val lib = ui.s.library
    val factory = lib.presets.filter { it.factory }
    val user = lib.presets.filter { !it.factory }
    val holds = lib.sounds[o.program]?.let { lib.preset(it.presetId)?.name ?: "a deleted preset" } ?: "nothing yet"
    PanelFrame(ui, if (o.replace) "Swap the Sound" else "Add a Sound", "play.picker.close", { ui.back() }, modifier, crumb = "Sounds") {
        Row(Modifier.fillMaxWidth().height(Metrics.target), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            Caption("Program", color = VoLumColors.CreamDim)
            com.volum.design.components.VoLumChannelStepper(
                "%03d".format(o.program),
                onPrev = { ui.d(Action.SetPickerProgram((o.program - 1 + 128) % 128)) },
                onNext = { ui.d(Action.SetPickerProgram((o.program + 1) % 128)) },
                prevModifier = Modifier.ids("play.picker.program"),
                nextModifier = Modifier.ids("play.picker.program"),
            )
            Fit("holds $holds", Modifier.weight(1f), size = 11.sp, color = VoLumColors.TealDim, min = 8.sp)
        }
        BoxWithConstraints(Modifier.fillMaxWidth()) {
            val room = rowTitleRoom((maxWidth - 12.dp) / 2 - 8.dp, art = false)
            SharedFitRows((factory + user).map { it.name to room }) {
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                    PresetColumn(ui, "Factory", factory, "play.picker.factory", o.program, Modifier.weight(1f))
                    PresetColumn(ui, "User", user, "play.picker.user", o.program, Modifier.weight(1f))
                }
            }
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
