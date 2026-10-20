package com.volum.ui.c

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.text.BasicText
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.unit.dp
import com.volum.core.Action
import com.volum.core.Amp
import com.volum.core.Overlay
import com.volum.core.Param
import com.volum.core.Preset

/** Two snapped lists side by side: the bundled list on the left, the CUSTOM group always in view on the right. */
@Composable
private fun Split(left: @Composable () -> Unit, right: @Composable () -> Unit) {
    Row(Modifier.fillMaxWidth().fillMaxHeight(), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
        SnapList(Modifier.weight(0.62f).fillMaxHeight()) { left() }
        SnapList(Modifier.weight(0.38f).fillMaxHeight()) { right() }
    }
}

/** A row-high group label, so a list keeps its row rhythm. */
@Composable
internal fun GroupHeader(text: String, modifier: Modifier = Modifier) {
    Box(
        modifier.fillMaxWidth().height(Metrics.target).drawBehind {
            drawLine(Look.line, Offset(0f, size.height - 1f), Offset(size.width, size.height - 1f), 1f)
        }.padding(horizontal = 4.dp),
        contentAlignment = Alignment.BottomStart,
    ) {
        BasicText(text.uppercase(), style = labelStyle(10f, Look.dim), maxLines = 1, modifier = Modifier.padding(bottom = 8.dp))
    }
}

@Composable
internal fun AmpPicker(ui: Ui, modifier: Modifier) {
    val s = ui.s
    val bundled = s.library.amps.filter { !it.custom }
    val custom = s.library.amps.filter { it.custom }
    PanelFrame("Choose amp · ${bundled.size} bundled", (s.amp?.name ?: "AMP") + "  ▾", "", { ui.back() }, modifier) {
        Split(
            left = {
                Grid(bundled, 3) { a, m -> ListRow(a.name, listOf("amp.list.select"), { ui.d(Action.SelectAmp(a.id)) }, m, sub = "${a.channels} CH", selected = a.id == s.ampId) }
            },
            right = {
                GroupHeader("Custom · your amps", Modifier.ids("amp.list.custom"))
                custom.forEach { a -> CustomAmpRow(ui, a) }
                Btn("+ ADD YOUR AMP", listOf("custom.amp.add"), { ui.d(Action.OpenBuilder(null)) }, sub = "FROM .NAM CAPTURES", modifier = Modifier.fillMaxWidth())
            },
        )
    }
}

@Composable
private fun CustomAmpRow(ui: Ui, a: Amp) {
    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(4.dp), verticalAlignment = Alignment.CenterVertically) {
        ListRow(a.name, listOf("amp.list.select_custom"), { ui.d(Action.SelectAmp(a.id)) }, Modifier.weight(1f), sub = "${a.channels} CH · CUSTOM", selected = a.id == ui.s.ampId)
        Btn("EDIT", listOf("custom.amp.edit"), { ui.d(Action.OpenBuilder(a.id)) })
        Btn("DEL", listOf("custom.amp.delete"), { ui.d(Action.RequestDeleteAmp(a.id)) }, danger = true)
    }
}

@Composable
internal fun SupportAmpMenu(ui: Ui, modifier: Modifier) {
    val s = ui.s
    val current = s.rig.support.ampId
    val bundled: List<Amp?> = listOf<Amp?>(null) + s.library.amps.filter { !it.custom }
    val custom = s.library.amps.filter { it.custom }
    PanelFrame("Support amp", "SUPPORT · " + (s.library.amp(current)?.name ?: "empty"), "dual.support.close", { ui.back() }, modifier) {
        Split(
            left = {
                Grid(bundled, 3) { a, m ->
                    if (a == null) ListRow("(none)", listOf("dual.support.none"), { ui.d(Action.SelectSupportAmp(null)) }, m, sub = "EMPTY LANE", selected = current == null)
                    else ListRow(a.name, listOf("dual.support.select"), { ui.d(Action.SelectSupportAmp(a.id)) }, m, sub = "${a.channels} CH", selected = a.id == current)
                }
            },
            right = {
                GroupHeader("Custom")
                if (custom.isEmpty()) Note("No custom amps yet.")
                custom.forEach { a -> ListRow(a.name, listOf("dual.support.select_custom"), { ui.d(Action.SelectSupportAmp(a.id)) }, Modifier.fillMaxWidth(), sub = "${a.channels} CH · CUSTOM", selected = a.id == current) }
            },
        )
    }
}

@Composable
internal fun IrMenu(ui: Ui, modifier: Modifier) {
    val s = ui.s
    val support = s.supportFocused
    val playing = if (support) s.rig.support.irId else s.rig.irId
    PanelFrame("Custom IR", (if (support) "SUPPORT · " else "") + "CUSTOM IR", "ir.menu.close", { ui.back() }, modifier, actions = {
        Btn("MANAGE IRS…", listOf("ir.menu.manage"), { ui.d(Action.Open(Overlay.IrManage)) }, sub = "IMPORT · LEVEL · RENAME")
    }) {
        if (s.library.irs.isEmpty()) Note("No IRs yet. MANAGE IRS imports WAV files from your phone.")
        SnapList(Modifier.fillMaxWidth().weight(1f)) {
            Grid(s.library.irs, 3) { ir, m ->
                ListRow(ir.name, listOf("ir.menu.select", "dual.support.ir.select"), { ui.d(Action.SelectIr(ir.id)) }, m, sub = if (ir.id == playing) "PLAYING" else "%+.1f dB".format(ir.levelDb), selected = ir.id == playing)
            }
        }
    }
}

@Composable
internal fun CaptureMenu(ui: Ui, o: Overlay.CaptureMenu, modifier: Modifier) {
    val s = ui.s
    val p = if (o.slot == 1) Param.NAM1_CAPTURE else Param.NAM2_CAPTURE
    val current = s.param(p).toInt()
    val bundled = s.library.pedals.filter { !it.custom }
    val custom = s.library.pedals.filter { it.custom }
    PanelFrame("Capture · bundled pedals", "NAM ${o.slot} · " + captureName(ui, p), "pre.capture.close", { ui.back() }, modifier) {
        Split(
            left = {
                Grid(bundled, 3) { pd, m ->
                    ListRow(pd.name, listOf(if (pd.index == 0) "pre.capture.empty" else "pre.capture.select"), { ui.d(Action.SelectCapture(o.slot, pd.index)) }, m, selected = pd.index == current, sub = if (pd.index == 0) "NO PEDAL" else null)
                }
            },
            right = {
                GroupHeader("Custom · your pedals", Modifier.ids("pre.capture.custom"))
                custom.forEach { pd -> ListRow(pd.name, listOf("pre.capture.select_custom"), { ui.d(Action.SelectCapture(o.slot, pd.index)) }, Modifier.fillMaxWidth(), sub = "CUSTOM", selected = pd.index == current) }
                Btn("MANAGE PEDALS…", listOf("pre.capture.manage"), { ui.d(Action.Open(Overlay.PedalManage)) }, sub = "IMPORT · RENAME · DELETE", modifier = Modifier.fillMaxWidth())
            },
        )
    }
}

@Composable
internal fun SoundPicker(ui: Ui, o: Overlay.SoundPicker, modifier: Modifier) {
    val lib = ui.s.library
    val factory = lib.presets.filter { it.factory }
    val user = lib.presets.filter { !it.factory }
    val holds = lib.sounds[o.program]?.let { lib.preset(it.presetId)?.name ?: "invalid" } ?: "empty"
    PanelFrame(
        if (o.replace) "Replace" else "Add Sound",
        "P%03d · %s".format(o.program, holds),
        "play.picker.close",
        { ui.back() },
        modifier,
        actions = { Stepper("%03d".format(o.program), listOf("play.picker.program"), { ui.d(Action.SetPickerProgram((o.program + it).coerceIn(0, 127))) }, label = "PROGRAM", valueWidth = 44.dp) },
    ) {
        Row(Modifier.fillMaxWidth().fillMaxHeight(), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            PresetColumn(ui, "Factory", factory, "play.picker.factory", o.program, Modifier.weight(1f))
            PresetColumn(ui, "User", user, "play.picker.user", o.program, Modifier.weight(1f))
        }
    }
}

@Composable
private fun PresetColumn(ui: Ui, title: String, presets: List<Preset>, id: String, program: Int, modifier: Modifier) {
    val current = ui.s.library.sounds[program]?.presetId
    SnapList(modifier.fillMaxHeight()) {
        GroupHeader("$title · ${presets.size}")
        if (presets.isEmpty()) Note("No $title presets yet.")
        presets.forEach { p ->
            ListRow(p.name, listOf(id), { ui.d(Action.AssignSound(program, p.id)) }, Modifier.fillMaxWidth(), sub = ui.s.library.amp(p.ampId)?.name, selected = p.id == current)
        }
    }
}
