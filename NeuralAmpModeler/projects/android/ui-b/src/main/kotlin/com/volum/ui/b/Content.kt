package com.volum.ui.b

import androidx.compose.foundation.background
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
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.PickKind
import com.volum.core.ScreenId
import com.volum.core.VoLumState

@Composable
internal fun AmpBuilder(s: VoLumState) {
    val st = LocalStage.current
    val b = s.builder
    val canSave = b.name.isNotBlank() && b.files.isNotEmpty()
    Panel(ScreenId.CUSTOM_AMP_BUILDER, if (b.editAmpId == null) "New custom amp" else "Edit custom amp", arrayOf("builder.close"),
        subtitle = "your .nam captures become one amp with channels and cabs",
        actions = {
            TextPad("SAVE AMP", { st.go(Action.BuilderSave) }, Modifier.width(132.dp).height(48.dp), arrayOf("builder.save"), enabled = canSave, selected = canSave,
                style = Type.label(14.sp, Ink.hot))
        }) {
        Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(16.dp)) {
            Column(Modifier.width(330.dp).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                Caption("NAME", Modifier.fillMaxWidth())
                Field(b.name, { st.go(Action.BuilderSetName(it)) }, Modifier.fillMaxWidth().height(52.dp), arrayOf("builder.name"), placeholder = "Amp name")
                Caption("ART", Modifier.fillMaxWidth())
                Row(Modifier.fillMaxWidth().height(48.dp), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                    (0..5).forEach { i ->
                        Pad({ st.go(Action.BuilderSetArt(i)) }, Modifier.weight(1f).fillMaxHeight(), arrayOf("builder.art"), selected = b.art == i,
                            sel = Sel.AMBER_PICKER, role = Role.RadioButton, description = "Art ${i + 1}") {
                            StageArt(i, st.tele, animate = false, Modifier.fillMaxSize())
                        }
                    }
                }
                Caption("CAB NAMES", Modifier.fillMaxWidth())
                (0..3).chunked(2).forEach { pair ->
                    Row(Modifier.fillMaxWidth().height(48.dp), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                        pair.forEach { slot -> CabNameField(b.cabNames[slot], slot, Modifier.weight(1f).fillMaxHeight()) }
                    }
                }
            }
            Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                Row(Modifier.fillMaxWidth().height(48.dp), horizontalArrangement = Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.CenterVertically) {
                    Column(Modifier.weight(1f)) {
                        Caption("FILES", Modifier.fillMaxWidth())
                        FitText("named CAB-NAME-CHANNEL.nam, e.g. V30-MyAmp-2.nam", Type.value(11.sp, Ink.faint), Modifier.fillMaxWidth().height(16.dp), align = TextAlign.Start)
                    }
                    TextPad("+ ADD .NAM FILES", { st.go(Action.PickFiles(PickKind.NAM_AMP_FILES)) }, Modifier.width(170.dp).fillMaxHeight(), arrayOf("builder.add_files"))
                }
                FitGrid(b.files.withIndex().toList(), 1, 52.dp, Modifier.fillMaxWidth().weight(1f), empty = "No captures yet. + ADD .NAM FILES picks them.") { (i, f) ->
                    Row(Modifier.fillMaxSize().clip(RoundedCornerShape(6.dp)).background(Ink.panel).padding(start = 12.dp, end = 4.dp, top = 2.dp, bottom = 2.dp),
                        verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                        FitText(f.name, Type.value(13.sp), Modifier.weight(1f), align = TextAlign.Start)
                        TextPad(b.cabNames.getOrElse(f.cabSlot) { "CAB" } + " ▸", { st.go(Action.BuilderSetFileCab(i, (f.cabSlot + 1) % 4)) },
                            Modifier.width(104.dp).fillMaxHeight(), arrayOf("builder.file.cab"))
                        TextPad("CH ${f.channel} ▸", { st.go(Action.BuilderSetFileChannel(i, f.channel % 6 + 1)) }, Modifier.width(76.dp).fillMaxHeight(),
                            arrayOf("builder.file.channel"))
                        TextPad("✕", { st.go(Action.BuilderRemoveFile(i)) }, Modifier.size(48.dp), arrayOf("builder.file.remove"), style = Type.display(18.sp, Ink.dim))
                    }
                }
            }
        }
    }
}

@Composable
private fun CabNameField(stored: String, slot: Int, modifier: Modifier) {
    val st = LocalStage.current
    var text by remember(stored) { mutableStateOf(stored) }
    Field(text, { text = it; if (it.isNotBlank()) st.go(Action.BuilderSetCabName(slot, it)) }, modifier, arrayOf("builder.cab_name"),
        placeholder = if (slot == 0) "No Cab" else "Cab $slot")
}

@Composable
internal fun IrManage(s: VoLumState) {
    val st = LocalStage.current
    val open = s.library.ir(s.ui.irConfigOpen)
    Panel(ScreenId.CUSTOM_IR_MANAGE, "Custom IRs", arrayOf("ir.manage.close"), subtitle = "WAV impulse responses for the Custom IR cab",
        actions = { TextPad("+ IMPORT WAV", { st.go(Action.PickFiles(PickKind.IR_FILES)) }, Modifier.width(150.dp).height(48.dp), arrayOf("ir.import", "ir.import.invalid")) }) {
        Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(16.dp)) {
            FitGrid(s.library.irs, 1, 56.dp, Modifier.weight(1f).fillMaxHeight(), empty = "No IRs yet. + IMPORT WAV adds your cabinet files.") { ir ->
                Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                    Pad({ st.go(Action.OpenIrConfig(ir.id)) }, Modifier.weight(1f).fillMaxHeight(), arrayOf("ir.config.open"), selected = open?.id == ir.id,
                        sel = Sel.LIST_TEAL, fill = Ink.panel, description = "Level and cuts of ${ir.name}") {
                        Column(Modifier.fillMaxSize().padding(horizontal = 12.dp), verticalArrangement = Arrangement.Center) {
                            FitText(ir.name, Type.value(15.sp), Modifier.fillMaxWidth().height(22.dp), align = TextAlign.Start)
                            FitText(String.format("%+.1f dB · %d Hz – %.1f kHz", ir.levelDb, ir.lowCutHz.toInt(), ir.highCutHz / 1000), Type.label(10.sp),
                                Modifier.fillMaxWidth().height(14.dp), align = TextAlign.Start)
                        }
                    }
                    TextPad("RENAME", { st.go(Action.RequestRenameIr(ir.id)) }, Modifier.width(96.dp).fillMaxHeight(), arrayOf("ir.rename"))
                    TextPad("DELETE", { st.go(Action.RequestDeleteIr(ir.id)) }, Modifier.width(96.dp).fillMaxHeight(), arrayOf("ir.delete"), style = Type.label(13.sp, Ink.alert))
                }
            }
            Column(Modifier.width(380.dp).fillMaxHeight().clip(RoundedCornerShape(8.dp)).background(Ink.panel).padding(12.dp),
                verticalArrangement = Arrangement.spacedBy(10.dp)) {
                if (open == null) {
                    Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                        FitText("Tap an IR to set its level and cuts.", Type.value(14.sp, Ink.dim), Modifier.fillMaxWidth())
                    }
                } else {
                    FitText(open.name, Type.display(20.sp), Modifier.fillMaxWidth().height(28.dp), align = TextAlign.Start)
                    Fader("LEVEL", open.levelDb, -20.0, 20.0, { String.format("%+.1f dB", it) }, { st.go(Action.SetIrLevel(open.id, it)) },
                        Modifier.fillMaxWidth().height(56.dp), arrayOf("ir.config.level"))
                    Fader("LOW CUT", open.lowCutHz, 20.0, 500.0, { "${it.toInt()} Hz" }, { st.go(Action.SetIrLowCut(open.id, it)) },
                        Modifier.fillMaxWidth().height(56.dp), arrayOf("ir.config.lowcut"))
                    Fader("HIGH CUT", open.highCutHz, 2000.0, 20000.0, { String.format("%.1f kHz", it / 1000) }, { st.go(Action.SetIrHighCut(open.id, it)) },
                        Modifier.fillMaxWidth().height(56.dp), arrayOf("ir.config.highcut"))
                }
            }
        }
    }
}

@Composable
internal fun PedalManage(s: VoLumState) {
    val st = LocalStage.current
    val custom = s.library.pedals.filter { it.custom }
    Panel(ScreenId.CUSTOM_PEDAL_MANAGE, "Custom pedals", arrayOf("pedal.manage.close"), subtitle = "your .nam pedal captures for NAM 1 and NAM 2 · ${custom.size} of 64 slots used",
        actions = { TextPad("+ IMPORT .NAM", { st.go(Action.PickFiles(PickKind.PEDAL_FILES)) }, Modifier.width(150.dp).height(48.dp), arrayOf("pedal.import")) }) {
        FitGrid(custom, 2, 56.dp, Modifier.fillMaxSize(), empty = "No custom pedals yet. + IMPORT .NAM adds your captures.") { pedal ->
            Row(Modifier.fillMaxSize().clip(RoundedCornerShape(6.dp)).background(Ink.panel).padding(start = 12.dp, end = 4.dp, top = 4.dp, bottom = 4.dp),
                verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                PedalMotif(pedal.index, true, Modifier.width(32.dp).fillMaxHeight())
                FitText(pedal.name, Type.value(15.sp), Modifier.weight(1f).padding(start = 8.dp), align = TextAlign.Start)
                TextPad("RENAME", { st.go(Action.RequestRenamePedal(pedal.index)) }, Modifier.width(92.dp).fillMaxHeight(), arrayOf("pedal.rename"))
                TextPad("DELETE", { st.go(Action.RequestDeletePedal(pedal.index)) }, Modifier.width(92.dp).fillMaxHeight(), arrayOf("pedal.delete"),
                    style = Type.label(13.sp, Ink.alert))
            }
        }
    }
}
