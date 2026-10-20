package com.volum.ui.a

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.Cab
import com.volum.core.PickKind

@Composable
internal fun AmpBuilder(c: Ctx) {
    val b = c.s.builder
    Sheet(if (b.editAmpId != null) "Edit custom amp" else "Build a custom amp", listOf("builder.close"), c, actions = {
        Chip("SAVE AMP", { c.d(Action.BuilderSave) }, ids = listOf("builder.save"), selected = true, enabled = b.name.isNotBlank() && b.files.isNotEmpty())
    }) {
        Row(Modifier.fillMaxWidth().weight(1f).padding(bottom = 8.dp), horizontalArrangement = Arrangement.spacedBy(14.dp)) {
            Column(Modifier.width(540.dp).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                NameField(b.name, { c.d(Action.BuilderSetName(it)) }, {}, listOf("builder.name"))
                Row(verticalAlignment = Alignment.CenterVertically) {
                    SectionLabel("CAPTURES  ·  CAB-NAME-CHANNEL.nam", Modifier.weight(1f))
                    Chip("+ ADD .nam FILES", { c.d(Action.PickFiles(PickKind.NAM_AMP_FILES)) }, ids = listOf("builder.add_files"))
                }
                RowList(48.dp, Modifier.weight(1f).fillMaxWidth()) {
                    b.files.forEachIndexed { i, f ->
                        Row(Modifier.fillMaxWidth().height(48.dp), verticalAlignment = Alignment.CenterVertically) {
                            T(f.name, Type.value(13.sp), Modifier.weight(1f))
                            val cabIds = listOf("builder.file.cab")
                            Stepper(null, b.cabNames.getOrElse(f.cabSlot) { Cab.entries[f.cabSlot].label },
                                { c.d(Action.BuilderSetFileCab(i, Math.floorMod(f.cabSlot - 1, 4))) }, { c.d(Action.BuilderSetFileCab(i, (f.cabSlot + 1) % 4)) },
                                prevIds = cabIds, nextIds = cabIds, valueWidth = 76.dp, valueSize = 12.sp)
                            val chIds = listOf("builder.file.channel")
                            Stepper(null, "CH ${f.channel}", { c.d(Action.BuilderSetFileChannel(i, if (f.channel <= 1) 6 else f.channel - 1)) },
                                { c.d(Action.BuilderSetFileChannel(i, if (f.channel >= 6) 1 else f.channel + 1)) }, prevIds = chIds, nextIds = chIds, valueWidth = 48.dp, valueSize = 12.sp)
                            IconTap(Glyph.CLOSE, { c.d(Action.BuilderRemoveFile(i)) }, ids = listOf("builder.file.remove"), desc = "Remove ${f.name}")
                        }
                    }
                    if (b.files.isEmpty()) Notice("Add the amp's .nam captures. The file names fill in cab and channel.")
                }
            }
            Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                SectionLabel("CAB NAMES")
                b.cabNames.chunked(2).forEachIndexed { row, pair ->
                    Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                        pair.forEachIndexed { col, name -> CabNameField(c, row * 2 + col, name, Modifier.weight(1f)) }
                    }
                }
                SectionLabel("ART")
                (0..5).chunked(3).forEach { row ->
                    Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                        row.forEach { a -> Chip("ART ${a + 1}", { c.d(Action.BuilderSetArt(a)) }, Modifier.weight(1f), listOf("builder.art"), selected = b.art == a) }
                    }
                }
            }
        }
    }
}

@Composable
private fun CabNameField(c: Ctx, slot: Int, name: String, modifier: Modifier) {
    var text by remember(name) { mutableStateOf(name) }
    NameField(text, { text = it; if (it.isNotBlank()) c.d(Action.BuilderSetCabName(slot, it)) }, {}, listOf("builder.cab_name"), modifier)
}

@Composable
internal fun IrManage(c: Ctx) {
    val s = c.s
    val open = s.library.ir(s.ui.irConfigOpen)
    Sheet("Custom IRs", listOf("ir.manage.close"), c, actions = {
        Chip("+ IMPORT WAV", { c.d(Action.PickFiles(PickKind.IR_FILES)) }, ids = listOf("ir.import", "ir.import.invalid"))
    }) {
        Row(Modifier.fillMaxWidth().weight(1f).padding(bottom = 8.dp), horizontalArrangement = Arrangement.spacedBy(14.dp)) {
            Column(Modifier.weight(1f).fillMaxHeight()) {
                RowList(48.dp, Modifier.weight(1f).fillMaxWidth()) {
                    s.library.irs.forEach { ir ->
                        Row(Modifier.fillMaxWidth().height(48.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                            T(ir.name, Type.value(14.sp), Modifier.weight(1f).padding(start = 6.dp))
                            Chip("TONE", { c.d(Action.OpenIrConfig(ir.id)) }, ids = listOf("ir.config.open"), selected = ir.id == s.ui.irConfigOpen)
                            Chip("RENAME", { c.d(Action.RequestRenameIr(ir.id)) }, ids = listOf("ir.rename"))
                            Chip("DELETE", { c.d(Action.RequestDeleteIr(ir.id)) }, ids = listOf("ir.delete"))
                        }
                    }
                }
                if (s.library.irs.isEmpty()) Notice("No IRs yet. Import WAV impulse responses; they play from the cab row's Custom IR.")
            }
            Column(Modifier.width(280.dp).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                if (open == null) {
                    Notice("TONE sets an IR's level and its low and high cut.")
                } else {
                    SectionLabel(open.name.uppercase())
                    Slider("LEVEL", open.levelDb, -20.0, 20.0, f("%.1f dB", open.levelDb), { c.d(Action.SetIrLevel(open.id, it)) }, Modifier.fillMaxWidth(), listOf("ir.config.level"))
                    Slider("LOW CUT", open.lowCutHz, 20.0, 500.0, hz(open.lowCutHz), { c.d(Action.SetIrLowCut(open.id, it)) }, Modifier.fillMaxWidth(), listOf("ir.config.lowcut"))
                    Slider("HIGH CUT", open.highCutHz, 2000.0, 20000.0, hz(open.highCutHz), { c.d(Action.SetIrHighCut(open.id, it)) }, Modifier.fillMaxWidth(), listOf("ir.config.highcut"))
                }
            }
        }
    }
}

@Composable
internal fun PedalManage(c: Ctx) {
    val custom = c.s.library.pedals.filter { it.custom }
    Sheet("Custom pedals", listOf("pedal.manage.close"), c, width = 760.dp, actions = {
        T("${custom.size} / 64", Type.label(11.sp))
        Spacer(Modifier.width(6.dp))
        Chip("+ IMPORT .nam", { c.d(Action.PickFiles(PickKind.PEDAL_FILES)) }, ids = listOf("pedal.import"))
    }) {
        RowList(48.dp, Modifier.weight(1f).fillMaxWidth()) {
            custom.forEach { p ->
                Row(Modifier.fillMaxWidth().height(48.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    T(p.name, Type.value(14.sp), Modifier.weight(1f).padding(start = 6.dp))
                    Chip("RENAME", { c.d(Action.RequestRenamePedal(p.index)) }, ids = listOf("pedal.rename"))
                    Chip("DELETE", { c.d(Action.RequestDeletePedal(p.index)) }, ids = listOf("pedal.delete"))
                }
            }
        }
        if (custom.isEmpty()) Notice("No custom pedals yet. Import a pedal or boost capture; it appears in NAM 1 and NAM 2.", Modifier.padding(bottom = 8.dp))
    }
}
