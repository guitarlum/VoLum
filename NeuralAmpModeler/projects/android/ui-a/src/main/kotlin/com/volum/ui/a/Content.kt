package com.volum.ui.a

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.width
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.Ir
import com.volum.core.PickKind
import com.volum.design.components.VoLumButton
import com.volum.design.components.VoLumGlyph
import com.volum.design.components.VoLumKnob
import com.volum.design.components.VoLumListCaption
import com.volum.design.components.VoLumListRow
import com.volum.design.theme.VoLumColors

private val ActionPitch = 50.dp

private fun irSummary(ir: Ir) = "LEVEL ${f("%+.1f dB", ir.levelDb)}  ·  LOW ${hz(ir.lowCutHz)}  ·  HIGH ${hz(ir.highCutHz)}"

/** Custom IRs: the list on the left (tap one to shape it), its LEVEL / LOW CUT / HIGH CUT knobs on the right. */
@Composable
internal fun IrManage(c: Ctx) {
    val s = c.s
    val open = s.library.ir(s.ui.irConfigOpen)
    val many = s.library.irs.size > 3
    Sheet("Custom IRs", listOf("ir.manage.close"), c, fill = many, actions = {
        VoLumButton("+  Import WAV", { c.d(Action.PickFiles(PickKind.IR_FILES)) }, Modifier.ids("ir.import", "ir.import.invalid"), primary = true)
    }) {
        Row(Modifier.fillMaxWidth().then(if (many) Modifier.weight(1f) else Modifier.height(196.dp)), horizontalArrangement = Arrangement.spacedBy(14.dp)) {
            Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                VoLumListCaption("YOUR IRs  ·  ${s.library.irs.size}")
                if (s.library.irs.isEmpty()) {
                    Note("No IRs yet. Import WAV impulse responses; they play from the cab row's Custom IR.")
                } else {
                    RowList(ActionPitch, Modifier.weight(1f).fillMaxWidth(), scrollToRow = s.library.irs.indexOfFirst { it.id == open?.id }) {
                        s.library.irs.forEach { ir ->
                            VoLumListRow(ir.name, ir.id == open?.id, { c.d(Action.OpenIrConfig(ir.id)) }, Modifier.fillMaxWidth().height(ActionPitch).ids("ir.config.open"), subtitle = irSummary(ir)) {
                                GlyphAction(VoLumGlyph.Edit, "RENAME", { c.d(Action.RequestRenameIr(ir.id)) }, Modifier.ids("ir.rename"))
                                GlyphAction(VoLumGlyph.Delete, "DELETE", { c.d(Action.RequestDeleteIr(ir.id)) }, Modifier.ids("ir.delete"), tint = VoLumColors.Danger)
                            }
                        }
                    }
                }
            }
            Column(Modifier.width(300.dp).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                if (open == null) {
                    VoLumListCaption("TONE")
                    Note("Tap an IR to set its level and its low and high cut.")
                } else {
                    VoLumListCaption("TONE  ·  ${open.name.uppercase()}")
                    Row(Modifier.fillMaxWidth().weight(1f), horizontalArrangement = Arrangement.SpaceEvenly, verticalAlignment = Alignment.CenterVertically) {
                        VoLumKnob(open.levelDb, { c.d(Action.SetIrLevel(open.id, it)) }, -20.0..20.0, 0.0, "LEVEL", f("%+.1f dB", open.levelDb), Modifier.ids("ir.config.level"), parseEntry = ::parseNumber)
                        VoLumKnob(open.lowCutHz, { c.d(Action.SetIrLowCut(open.id, it)) }, 20.0..500.0, 20.0, "LOW CUT", hz(open.lowCutHz), Modifier.ids("ir.config.lowcut"), parseEntry = ::parseNumber)
                        VoLumKnob(open.highCutHz, { c.d(Action.SetIrHighCut(open.id, it)) }, 2000.0..20000.0, 20000.0, "HIGH CUT", hz(open.highCutHz), Modifier.ids("ir.config.highcut"), parseEntry = ::parseNumber)
                    }
                    Hint("Double-tap a knob to reset it.")
                }
            }
        }
    }
}

/** Custom pedals: as tall as its list, the slot count beside Import, each row with its words for Rename and Delete. */
@Composable
internal fun PedalManage(c: Ctx) {
    val custom = c.s.library.pedals.filter { it.custom }
    Sheet("Custom pedals", listOf("pedal.manage.close"), c, width = 700.dp, fill = custom.size > 4, actions = {
        VoLumListCaption("${custom.size} OF 64 SLOTS")
        VoLumButton("+  Import .nam", { c.d(Action.PickFiles(PickKind.PEDAL_FILES)) }, Modifier.ids("pedal.import"), primary = true)
    }) {
        if (custom.isEmpty()) {
            Note("No custom pedals yet. Import a pedal or boost capture; it appears in NAM 1 and NAM 2 under Custom.")
            return@Sheet
        }
        RowList(ActionPitch, if (custom.size > 4) Modifier.weight(1f).fillMaxWidth() else Modifier.fillMaxWidth()) {
            custom.forEach { p ->
                VoLumListRow(p.name, false, { c.d(Action.RequestRenamePedal(p.index)) }, Modifier.fillMaxWidth().height(ActionPitch).ids("pedal.rename"), subtitle = "IN NAM 1 AND NAM 2  ·  CUSTOM") {
                    GlyphAction(VoLumGlyph.Edit, "RENAME", { c.d(Action.RequestRenamePedal(p.index)) }, Modifier.ids("pedal.rename"))
                    GlyphAction(VoLumGlyph.Delete, "DELETE", { c.d(Action.RequestDeletePedal(p.index)) }, Modifier.ids("pedal.delete"), tint = VoLumColors.Danger)
                }
            }
        }
        Hint("Pedal captures are .nam files of a pedal alone, without an amp.", lines = 1)
    }
}
