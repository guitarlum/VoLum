package com.volum.ui.c

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.BasicText
import androidx.compose.foundation.verticalScroll
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import com.volum.core.Action
import com.volum.core.Ir
import com.volum.core.PickKind

private val builderArts = listOf("SUNSET", "EMBER", "TIDE", "STATIC", "ORBIT", "SMOKE")

@Composable
internal fun Builder(ui: Ui, modifier: Modifier) {
    val b = ui.s.builder
    PanelFrame(if (b.editAmpId != null) "Edit custom amp" else "New custom amp", "CUSTOM", "builder.close", { ui.back() }, modifier, actions = {
        Btn("SAVE AMP", listOf("builder.save"), { ui.d(Action.BuilderSave) }, strong = true, enabled = b.name.isNotBlank() && b.files.isNotEmpty(), sub = if (b.files.isEmpty()) "ADD FILES FIRST" else "${b.files.size} FILES")
    }) {
        Row(Modifier.fillMaxWidth().fillMaxHeight(), horizontalArrangement = Arrangement.spacedBy(16.dp)) {
            Column(Modifier.width(250.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Labeled("Name") { Field(b.name, listOf("builder.name"), { ui.d(Action.BuilderSetName(it)) }, Modifier.fillMaxWidth(), hint = "Amp name", key = b.editAmpId) }
                Labeled("Art") {
                    Grid(builderArts.indices.toList(), 3) { i, m -> Choice(builderArts[i], b.art == i, listOf("builder.art"), { ui.d(Action.BuilderSetArt(i)) }, m) }
                }
            }
            Column(Modifier.width(190.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                Caption("Cab names")
                (1..3).forEach { slot ->
                    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                        BasicText("$slot", style = techStyle(11f, Look.dim), modifier = Modifier.width(14.dp))
                        Field(b.cabNames[slot], listOf("builder.cab_name"), { t -> if (t.isNotBlank()) ui.d(Action.BuilderSetCabName(slot, t)) }, Modifier.weight(1f), key = b.editAmpId)
                    }
                }
            }
            Column(Modifier.weight(1f).fillMaxHeight().verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Caption("Captures · ${b.files.size}", Modifier.weight(1f))
                    Btn("+ ADD .NAM FILES", listOf("builder.add_files"), { ui.d(Action.PickFiles(PickKind.NAM_AMP_FILES)) }, sub = "CAB-NAME-CH.nam")
                }
                if (b.files.isEmpty()) Note("Pick one .nam per channel and cab. Files named DI-, V30-, G12- … -1.nam fill in the cab and channel for you.", maxLines = 3)
                b.files.forEachIndexed { i, f ->
                    Column(Modifier.fillMaxWidth().background(Look.glass, Corner).padding(4.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                            BasicText(f.name, style = bodyStyle(12f, cap = 1.15f), maxLines = 1, overflow = TextOverflow.Ellipsis, modifier = Modifier.weight(1f).padding(start = 4.dp))
                            Stepper("CH ${f.channel}", listOf("builder.file.channel"), { ui.d(Action.BuilderSetFileChannel(i, f.channel + it)) }, valueWidth = 40.dp)
                            Btn("×", listOf("builder.file.remove"), { ui.d(Action.BuilderRemoveFile(i)) })
                        }
                        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                            b.cabNames.forEachIndexed { slot, label ->
                                Choice(label, f.cabSlot == slot, listOf("builder.file.cab"), { ui.d(Action.BuilderSetFileCab(i, slot)) }, Modifier.weight(1f))
                            }
                        }
                    }
                }
            }
        }
    }
}

@Composable
internal fun IrManage(ui: Ui, modifier: Modifier) {
    val irs = ui.s.library.irs
    PanelFrame("Manage IRs", "CUSTOM IR", "ir.manage.close", { ui.back() }, modifier, actions = {
        Btn("+ IMPORT WAV", listOf("ir.import", "ir.import.invalid"), { ui.d(Action.PickFiles(PickKind.IR_FILES)) }, sub = "FROM YOUR PHONE")
    }) {
        Column(Modifier.fillMaxWidth().verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            if (irs.isEmpty()) Note("No IRs yet. Import WAV impulse responses; the same file twice is skipped.")
            irs.forEach { IrRow(ui, it) }
        }
    }
}

@Composable
private fun IrRow(ui: Ui, ir: Ir) {
    val open = ui.s.ui.irConfigOpen == ir.id
    Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            Column(Modifier.weight(1f)) {
                BasicText(ir.name, style = bodyStyle(14f), maxLines = 1, overflow = TextOverflow.Ellipsis)
                BasicText("%+.1f dB · %.0f Hz – %.1f kHz".format(ir.levelDb, ir.lowCutHz, ir.highCutHz / 1000), style = techStyle(10f, Look.dim))
            }
            Choice(if (open) "LEVEL & CUTS ▴" else "LEVEL & CUTS ▾", open, listOf("ir.config.open"), { ui.d(Action.OpenIrConfig(ir.id)) })
            Btn("RENAME", listOf("ir.rename"), { ui.d(Action.RequestRenameIr(ir.id)) })
            Btn("DELETE", listOf("ir.delete"), { ui.d(Action.RequestDeleteIr(ir.id)) }, danger = true)
        }
        if (open) {
            Row(Modifier.fillMaxWidth().padding(start = 8.dp), horizontalArrangement = Arrangement.spacedBy(20.dp)) {
                Fader("Level", ir.levelDb, -20.0, 20.0, listOf("ir.config.level"), { "%+.1f dB".format(it) }, { ui.d(Action.SetIrLevel(ir.id, it)) }, Modifier.weight(1f))
                Fader("Low cut", ir.lowCutHz, 20.0, 500.0, listOf("ir.config.lowcut"), { "%.0f Hz".format(it) }, { ui.d(Action.SetIrLowCut(ir.id, it)) }, Modifier.weight(1f))
                Fader("High cut", ir.highCutHz, 2000.0, 20000.0, listOf("ir.config.highcut"), { "%.1f kHz".format(it / 1000) }, { ui.d(Action.SetIrHighCut(ir.id, it)) }, Modifier.weight(1f))
            }
        }
    }
}

@Composable
internal fun PedalManage(ui: Ui, modifier: Modifier) {
    val custom = ui.s.library.pedals.filter { it.custom }
    PanelFrame("Manage pedals", "CAPTURE", "pedal.manage.close", { ui.back() }, modifier, actions = {
        Btn("+ IMPORT .NAM", listOf("pedal.import"), { ui.d(Action.PickFiles(PickKind.PEDAL_FILES)) }, sub = "${64 - custom.size} SLOTS LEFT")
    }) {
        Column(Modifier.fillMaxWidth().verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            if (custom.isEmpty()) Note("No custom pedals yet. Import .nam captures of your drives and boosts.")
            custom.forEach { p ->
                Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    Column(Modifier.weight(1f)) {
                        BasicText(p.name, style = bodyStyle(14f), maxLines = 1, overflow = TextOverflow.Ellipsis)
                        BasicText("SLOT ${p.index}", style = techStyle(10f, Look.dim))
                    }
                    Btn("RENAME", listOf("pedal.rename"), { ui.d(Action.RequestRenamePedal(p.index)) })
                    Btn("DELETE", listOf("pedal.delete"), { ui.d(Action.RequestDeletePedal(p.index)) }, danger = true)
                }
            }
        }
    }
}
