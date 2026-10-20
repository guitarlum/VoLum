package com.volum.ui.c

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.Ir
import com.volum.core.PickKind
import com.volum.design.art.AmpThumbnail
import com.volum.design.art.ArtRef
import com.volum.design.components.VoLumChannelStepper
import com.volum.design.components.VoLumGlyph
import com.volum.design.components.VoLumSelectionStyle
import com.volum.design.components.hairlineFrame
import com.volum.design.components.insetWell
import com.volum.design.components.volumSelection
import com.volum.design.theme.VFitText
import com.volum.design.theme.VText
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumType

private val builderArts = listOf("Sunset", "Ember", "Tide", "Static", "Orbit", "Smoke")

@Composable
internal fun Builder(ui: Ui, modifier: Modifier) {
    val b = ui.s.builder
    val ready = b.name.isNotBlank() && b.files.isNotEmpty()
    PanelFrame(ui, if (b.editAmpId != null) "Edit custom amp" else "New custom amp", "builder.close", { ui.back() }, modifier, crumb = "Custom", actions = {
        Btn("+ Add .nam files", listOf("builder.add_files"), { ui.d(Action.PickFiles(PickKind.NAM_AMP_FILES)) })
        Btn(if (b.files.isEmpty()) "Add files to save" else "Save amp", listOf("builder.save"), { ui.d(Action.BuilderSave) }, primary = true, enabled = ready)
    }) {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(16.dp)) {
            Stack(Modifier.width(300.dp), gap = 4) {
                Caption("Name")
                Field(b.name, listOf("builder.name"), { ui.d(Action.BuilderSetName(it)) }, Modifier.fillMaxWidth(), hint = "Amp name", key = b.editAmpId)
                Caption("Art")
                Row(horizontalArrangement = Arrangement.spacedBy(2.dp)) {
                    builderArts.forEachIndexed { i, name ->
                        Tappable({ ui.d(Action.BuilderSetArt(i)) }, Modifier.size(Metrics.target).semantics { contentDescription = "$name art" }, listOf("builder.art")) { pressed ->
                            Box(Modifier.fillMaxSize().volumSelection(b.art == i, pressed, VoLumSelectionStyle.Brass, radius = 3.dp, inset = 0.dp, glow = b.art == i), contentAlignment = Alignment.Center) {
                                AmpThumbnail(ArtRef.custom(i), Modifier.size(34.dp), b.art == i)
                            }
                        }
                    }
                }
            }
            Stack(Modifier.weight(1f), gap = 4) {
                Spaced(gap = 6) {
                    Caption("Cab names", Modifier.width(70.dp))
                    (1..3).forEach { slot ->
                        Field(b.cabNames[slot], listOf("builder.cab_name"), { t -> if (t.isNotBlank()) ui.d(Action.BuilderSetCabName(slot, t)) }, Modifier.weight(1f), key = b.editAmpId)
                    }
                }
                Caption("Captures · ${b.files.size}")
                if (b.files.isEmpty()) Note("Pick one .nam per channel and cab. Files named DI-, V30-, G12- ... -1.nam fill in the cab and channel for you.", maxLines = 3)
                SnapList(b.files.size, Modifier.fillMaxWidth()) {
                    b.files.forEachIndexed { i, f ->
                        Row(Modifier.fillMaxWidth().height(Metrics.target), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                            VFitText(f.name, Modifier.weight(1f), style = VoLumType.label(12.sp, VoLumColors.TextMed), minSize = 8.sp)
                            VoLumChannelStepper(
                                "${f.channel}",
                                onPrev = { ui.d(Action.BuilderSetFileChannel(i, f.channel - 1)) },
                                onNext = { ui.d(Action.BuilderSetFileChannel(i, f.channel + 1)) },
                                prevModifier = Modifier.ids("builder.file.channel"),
                                nextModifier = Modifier.ids("builder.file.channel"),
                            )
                            Pills(b.cabNames, f.cabSlot, { "builder.file.cab" }, { ui.d(Action.BuilderSetFileCab(i, it)) }, Modifier.width(230.dp))
                            GlyphBtn(VoLumGlyph.Delete, "Remove ${f.name}", listOf("builder.file.remove"), { ui.d(Action.BuilderRemoveFile(i)) }, color = VoLumColors.Danger)
                        }
                    }
                }
            }
        }
    }
}

/** IRs on the left; LEVEL & CUTS of the one being tuned on the right, as `:design` knobs. */
@Composable
internal fun IrManage(ui: Ui, modifier: Modifier) {
    val irs = ui.s.library.irs
    val open = irs.firstOrNull { it.id == ui.s.ui.irConfigOpen }
    PanelFrame(ui, "Manage IRs", "ir.manage.close", { ui.back() }, modifier, crumb = "Custom IR", actions = {
        Btn("+ Import WAV", listOf("ir.import", "ir.import.invalid"), { ui.d(Action.PickFiles(PickKind.IR_FILES)) })
    }) {
        if (irs.isEmpty()) Note("No IRs yet. Import WAV impulse responses; the same file twice is skipped.")
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(16.dp)) {
            SnapList(irs.size, Modifier.weight(1f)) { irs.forEach { IrRow(ui, it, it.id == open?.id) } }
            if (open != null) {
                Column(Modifier.width(290.dp).insetWell(4.dp).hairlineFrame(VoLumColors.Frame, radius = 4.dp).padding(horizontal = 8.dp, vertical = 4.dp)) {
                    VFitText("LEVEL & CUTS  ·  ${open.name}", style = VoLumType.label(10.sp, VoLumColors.GoldDim), minSize = 7.sp)
                    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceEvenly) {
                        Knob("LEVEL", open.levelDb, -20.0, 20.0, listOf("ir.config.level"), { "%+.1f dB".format(it) }, { it.toDoubleOrNull() }, { ui.d(Action.SetIrLevel(open.id, it)) }, { ui.d(Action.SetIrLevel(open.id, 0.0)) }, width = 88.dp, dial = 52.dp)
                        Knob("LOW CUT", open.lowCutHz, 20.0, 500.0, listOf("ir.config.lowcut"), { "%.0f Hz".format(it) }, { it.toDoubleOrNull() }, { ui.d(Action.SetIrLowCut(open.id, it)) }, { ui.d(Action.SetIrLowCut(open.id, 20.0)) }, width = 88.dp, dial = 52.dp)
                        Knob("HIGH CUT", open.highCutHz, 2000.0, 20000.0, listOf("ir.config.highcut"), { "%.1f kHz".format(it / 1000) }, { it.toDoubleOrNull()?.let { v -> if (v < 100) v * 1000 else v } }, { ui.d(Action.SetIrHighCut(open.id, it)) }, { ui.d(Action.SetIrHighCut(open.id, 20000.0)) }, width = 88.dp, dial = 52.dp)
                    }
                }
            }
        }
    }
}

@Composable
private fun IrRow(ui: Ui, ir: Ir, open: Boolean) {
    Row(Modifier.fillMaxWidth().height(Metrics.target), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
        Column(Modifier.weight(1f)) {
            VFitText(ir.name, style = VoLumType.label(13.sp, VoLumColors.TextBright), minSize = 9.sp)
            VFitText("%+.1f dB  ·  %.0f Hz to %.1f kHz".format(ir.levelDb, ir.lowCutHz, ir.highCutHz / 1000), style = VoLumType.label(9.sp, VoLumColors.TealDim), minSize = 7.sp)
        }
        Brass("Level & cuts", open, listOf("ir.config.open"), { ui.d(Action.OpenIrConfig(ir.id)) })
        Btn("Rename", listOf("ir.rename"), { ui.d(Action.RequestRenameIr(ir.id)) })
        Btn("Delete", listOf("ir.delete"), { ui.d(Action.RequestDeleteIr(ir.id)) }, danger = true)
    }
}

@Composable
internal fun PedalManage(ui: Ui, modifier: Modifier) {
    val custom = ui.s.library.pedals.filter { it.custom }
    PanelFrame(ui, "Manage pedals", "pedal.manage.close", { ui.back() }, modifier, crumb = "${64 - custom.size} slots left", actions = {
        Btn("+ Import .nam", listOf("pedal.import"), { ui.d(Action.PickFiles(PickKind.PEDAL_FILES)) })
    }) {
        if (custom.isEmpty()) Note("No custom pedals yet. Import .nam captures of your drives and boosts.")
        SnapList(gridRows(custom.size, 2), Modifier.fillMaxWidth()) {
            Grid(custom, 2) { p, m ->
                Row(m.height(Metrics.target), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    Column(Modifier.weight(1f)) {
                        VFitText(p.name, style = VoLumType.label(13.sp, VoLumColors.TextBright), minSize = 9.sp)
                        VText("SLOT ${p.index}", style = VoLumType.value(9.sp, VoLumColors.GoldDim))
                    }
                    Btn("Rename", listOf("pedal.rename"), { ui.d(Action.RequestRenamePedal(p.index)) })
                    Btn("Delete", listOf("pedal.delete"), { ui.d(Action.RequestDeletePedal(p.index)) }, danger = true)
                }
            }
        }
    }
}
