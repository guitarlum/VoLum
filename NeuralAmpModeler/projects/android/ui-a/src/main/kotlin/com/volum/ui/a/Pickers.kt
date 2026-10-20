package com.volum.ui.a

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.Amp
import com.volum.core.Cab
import com.volum.core.Overlay
import com.volum.core.Param
import com.volum.core.Preset
import kotlin.math.roundToInt

/** Two side-by-side catalogs: bundled on the left, the user's own on the right, so neither hides the other. */
@Composable
internal fun ColumnScope.TwoColumns(left: @Composable ColumnScope.() -> Unit, right: @Composable ColumnScope.() -> Unit, rightWidth: androidx.compose.ui.unit.Dp = 320.dp) {
    Row(Modifier.fillMaxWidth().weight(1f).padding(bottom = 8.dp), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
        Column(Modifier.weight(1f).fillMaxHeight(), content = left)
        Column(Modifier.width(rightWidth).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(6.dp), content = right)
    }
}

@Composable
private fun AmpRowText(amp: Amp, selected: Boolean) =
    T(amp.name, Type.value(14.sp, if (selected) Ink.selected else Ink.text))

@Composable
internal fun AmpPicker(c: Ctx) {
    val s = c.s
    val custom = s.library.amps.filter { it.custom }
    Sheet("Amps", emptyList(), c, width = 760.dp, align = Alignment.CenterStart) {
        TwoColumns(
            left = {
                SectionLabel("BUNDLED")
                RowList(48.dp, Modifier.weight(1f).fillMaxWidth()) {
                    s.library.amps.filter { !it.custom }.forEach { a ->
                        ListRow({ c.d(Action.SelectAmp(a.id)) }, ids = listOf("amp.list.select"), selected = a.id == s.ampId) { AmpRowText(a, a.id == s.ampId) }
                    }
                }
            },
            right = {
                SectionLabel("CUSTOM", ids = if (custom.isNotEmpty()) listOf("amp.list.custom") else emptyList())
                RowList(48.dp, Modifier.weight(1f).fillMaxWidth()) {
                    custom.forEach { a ->
                        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                            ListRow({ c.d(Action.SelectAmp(a.id)) }, Modifier.weight(1f), listOf("amp.list.select_custom"), selected = a.id == s.ampId) { AmpRowText(a, a.id == s.ampId) }
                            IconTap(Glyph.EDIT, { c.d(Action.OpenBuilder(a.id)) }, ids = listOf("custom.amp.edit"), desc = "Edit ${a.name}")
                            IconTap(Glyph.CLOSE, { c.d(Action.RequestDeleteAmp(a.id)) }, ids = listOf("custom.amp.delete"), desc = "Delete ${a.name}")
                        }
                    }
                    if (custom.isEmpty()) Notice("Build an amp from your own .nam captures.")
                }
                Chip("+ BUILD CUSTOM AMP", { c.d(Action.OpenBuilder(null)) }, Modifier.fillMaxWidth(), listOf("custom.amp.add"))
            },
        )
    }
}

@Composable
internal fun IrMenu(c: Ctx) {
    val s = c.s
    val support = s.supportFocused
    val playing = if (support) s.rig.support.irId.takeIf { s.rig.support.cab == Cab.CUSTOM_IR } else s.rig.irId.takeIf { s.rig.cab == Cab.CUSTOM_IR }
    Sheet(if (support) "Custom IR · SUPPORT" else "Custom IR · MAIN", listOf("ir.menu.close"), c, width = 560.dp) {
        RowList(48.dp, Modifier.weight(1f).fillMaxWidth()) {
            s.library.irs.forEach { ir ->
                ListRow({ c.d(Action.SelectIr(ir.id)) }, ids = listOf("ir.menu.select", "dual.support.ir.select"), selected = ir.id == playing) {
                    T(ir.name, Type.value(14.sp, if (ir.id == playing) Ink.selected else Ink.text))
                }
            }
        }
        if (s.library.irs.isEmpty()) Notice("No IRs yet. Import WAV impulse responses in Manage.")
        Row(Modifier.fillMaxWidth().padding(vertical = 8.dp), horizontalArrangement = Arrangement.End) {
            Chip("MANAGE IRs", { c.d(Action.Open(Overlay.IrManage)) }, ids = listOf("ir.menu.manage"))
        }
    }
}

@Composable
internal fun CaptureMenu(c: Ctx, slot: Int) {
    val s = c.s
    val current = s.param(if (slot == 1) Param.NAM1_CAPTURE else Param.NAM2_CAPTURE).roundToInt()
    val custom = s.library.pedals.filter { it.custom }
    Sheet("NAM $slot capture", listOf("pre.capture.close"), c, width = 760.dp) {
        TwoColumns(
            left = {
                SectionLabel("BUNDLED")
                RowList(48.dp, Modifier.weight(1f).fillMaxWidth()) {
                    s.library.pedals.filter { !it.custom }.forEach { p ->
                        val id = if (p.index == 0) "pre.capture.empty" else "pre.capture.select"
                        ListRow({ c.d(Action.SelectCapture(slot, p.index)) }, ids = listOf(id), selected = p.index == current) {
                            T(p.name, Type.value(14.sp, if (p.index == current) Ink.selected else Ink.text))
                        }
                    }
                }
            },
            right = {
                SectionLabel("CUSTOM", ids = if (custom.isNotEmpty()) listOf("pre.capture.custom") else emptyList())
                RowList(48.dp, Modifier.weight(1f).fillMaxWidth()) {
                    custom.forEach { p ->
                        ListRow({ c.d(Action.SelectCapture(slot, p.index)) }, ids = listOf("pre.capture.select_custom"), selected = p.index == current) {
                            T(p.name, Type.value(14.sp, if (p.index == current) Ink.selected else Ink.text))
                        }
                    }
                    if (custom.isEmpty()) Notice("Import your own pedal captures in Manage.")
                }
                Chip("MANAGE PEDALS", { c.d(Action.Open(Overlay.PedalManage)) }, Modifier.fillMaxWidth(), listOf("pre.capture.manage"))
            },
        )
    }
}

@Composable
internal fun SupportAmpMenu(c: Ctx) {
    val s = c.s
    val current = s.rig.support.ampId
    Sheet("SUPPORT amp", listOf("dual.support.close"), c, width = 760.dp) {
        TwoColumns(
            left = {
                SectionLabel("BUNDLED")
                RowList(48.dp, Modifier.weight(1f).fillMaxWidth()) {
                    s.library.amps.filter { !it.custom }.forEach { a ->
                        ListRow({ c.d(Action.SelectSupportAmp(a.id)) }, ids = listOf("dual.support.select"), selected = a.id == current) { AmpRowText(a, a.id == current) }
                    }
                }
            },
            right = {
                SectionLabel("CUSTOM")
                RowList(48.dp, Modifier.weight(1f).fillMaxWidth()) {
                    s.library.amps.filter { it.custom }.forEach { a ->
                        ListRow({ c.d(Action.SelectSupportAmp(a.id)) }, ids = listOf("dual.support.select_custom"), selected = a.id == current) { AmpRowText(a, a.id == current) }
                    }
                }
                Chip("NO SUPPORT AMP", { c.d(Action.SelectSupportAmp(null)) }, Modifier.fillMaxWidth(), listOf("dual.support.none"), selected = current == null)
            },
        )
    }
}

@Composable
private fun PresetRowText(c: Ctx, p: Preset) {
    Column {
        T(p.name, Type.value(14.sp))
        T(c.s.library.amp(p.ampId)?.name ?: "", Type.label(10.sp))
    }
}

@Composable
internal fun SoundPicker(c: Ctx, o: Overlay.SoundPicker) {
    val s = c.s
    val pick = { id: String -> c.d(Action.AssignSound(o.program, id)) }
    val step = { d: Int -> c.d(Action.SetPickerProgram((o.program + d).coerceIn(0, 127))) }
    Sheet(if (o.replace) "Replace a Sound" else "Add a Sound", listOf("play.picker.close"), c, actions = {
        Stepper("PROGRAM", program(o.program), { step(-1) }, { step(1) }, prevIds = listOf("play.picker.program"), nextIds = listOf("play.picker.program"), valueWidth = 44.dp)
    }) {
        TwoColumns(
            left = {
                SectionLabel("FACTORY")
                RowList(48.dp, Modifier.weight(1f).fillMaxWidth()) {
                    s.library.presets.filter { it.factory }.forEach { p -> ListRow({ pick(p.id) }, ids = listOf("play.picker.factory")) { PresetRowText(c, p) } }
                }
            },
            right = {
                SectionLabel("USER")
                RowList(48.dp, Modifier.weight(1f).fillMaxWidth()) {
                    s.library.presets.filter { !it.factory }.forEach { p -> ListRow({ pick(p.id) }, ids = listOf("play.picker.user")) { PresetRowText(c, p) } }
                }
            },
            rightWidth = 400.dp,
        )
    }
}
