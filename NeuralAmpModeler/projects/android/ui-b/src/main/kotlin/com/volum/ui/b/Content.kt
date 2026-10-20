package com.volum.ui.b

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
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
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.PickKind
import com.volum.core.ScreenId
import com.volum.core.VoLumState
import com.volum.design.art.AmpArt
import com.volum.design.art.ArtRef
import com.volum.design.art.Motif
import com.volum.design.components.MotifArt
import com.volum.design.components.VoLumGlyph
import com.volum.design.components.VoLumIcon
import com.volum.design.components.hairlineFrame
import com.volum.design.components.panelDepth
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumType

@Composable
internal fun AmpBuilder(s: VoLumState) {
    val st = LocalStage.current
    val b = s.builder
    val canSave = b.name.isNotBlank() && b.files.isNotEmpty()
    Panel(s, ScreenId.CUSTOM_AMP_BUILDER, if (b.editAmpId == null) "New custom amp" else "Edit custom amp", arrayOf("builder.close"),
        subtitle = "your .nam captures become one amp with channels and cabs",
        actions = {
            TextPad(if (canSave) "SAVE AMP" else "NAME IT AND ADD FILES", { st.go(Action.BuilderSave) }, Modifier.width(176.dp).height(48.dp), arrayOf("builder.save"),
                enabled = canSave, tone = Tone.PRIMARY)
        }) {
        Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(16.dp)) {
            Column(Modifier.width(330.dp).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(5.dp)) {
                SectionTitle("NAME", Modifier.fillMaxWidth())
                Field(b.name, { st.go(Action.BuilderSetName(it)) }, Modifier.fillMaxWidth().height(48.dp), arrayOf("builder.name"), placeholder = "Amp name")
                SectionTitle("ART · how it moves on PLAY", Modifier.fillMaxWidth())
                Row(Modifier.fillMaxWidth().height(52.dp), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                    (0..5).forEach { i ->
                        Pad({ st.go(Action.BuilderSetArt(i)) }, Modifier.weight(1f).fillMaxHeight(), arrayOf("builder.art"), selected = b.art == i,
                            style = Sel.Brass, tone = Tone.FRAMED, role = Role.RadioButton, description = "Art ${i + 1}") { _ ->
                            AmpArt(ArtRef.custom(i), Modifier.fillMaxSize().padding(4.dp))
                        }
                    }
                }
                SectionTitle("CAB NAMES · the cab row on BUILD", Modifier.fillMaxWidth())
                (0..3).chunked(2).forEach { pair ->
                    Row(Modifier.fillMaxWidth().height(48.dp), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                        pair.forEach { slot -> CabNameField(b.cabNames[slot], slot, Modifier.weight(1f).fillMaxHeight()) }
                    }
                }
            }
            Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(5.dp)) {
                Row(Modifier.fillMaxWidth().height(48.dp), horizontalArrangement = Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.CenterVertically) {
                    Column(Modifier.weight(1f)) {
                        SectionTitle("FILES", Modifier.fillMaxWidth())
                        FitText("named CAB-NAME-CHANNEL.nam, e.g. V30-MyAmp-2.nam", VoLumType.body(11.sp, VoLumColors.CreamDim), Modifier.fillMaxWidth().height(16.dp), align = TextAlign.Start)
                    }
                    Pad({ st.go(Action.PickFiles(PickKind.NAM_AMP_FILES)) }, Modifier.width(176.dp).fillMaxHeight(), arrayOf("builder.add_files"), tone = Tone.PRIMARY) { ink ->
                        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                            VoLumIcon(VoLumGlyph.Plus, color = ink, size = 14.dp)
                            FitText("ADD .NAM FILES", VoLumType.label(12.sp, ink), Modifier.width(120.dp))
                        }
                    }
                }
                FitGrid(b.files.withIndex().toList(), 1, 56.dp, Modifier.fillMaxWidth().weight(1f), gap = 4.dp, empty = "No captures yet. ADD .NAM FILES picks them.") { (i, f) ->
                    Row(Modifier.fillMaxSize().panelDepth(4.dp).hairlineFrame(VoLumColors.Frame, radius = 4.dp).padding(start = 12.dp, end = 4.dp, top = 2.dp, bottom = 2.dp),
                        verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                        FitText(f.name, VoLumType.label(12.sp, VoLumColors.TextMed), Modifier.weight(1f), align = TextAlign.Start)
                        TextPad("CAB: ${b.cabNames.getOrElse(f.cabSlot) { "CAB" }}  >", { st.go(Action.BuilderSetFileCab(i, (f.cabSlot + 1) % 4)) },
                            Modifier.width(118.dp).fillMaxHeight(), arrayOf("builder.file.cab"), size = 11.sp)
                        TextPad("CH ${f.channel}  >", { st.go(Action.BuilderSetFileChannel(i, f.channel % 6 + 1)) }, Modifier.width(76.dp).fillMaxHeight(),
                            arrayOf("builder.file.channel"), size = 11.sp)
                        GlyphPad(VoLumGlyph.Close, "REMOVE", { st.go(Action.BuilderRemoveFile(i)) }, arrayOf("builder.file.remove"), danger = true)
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
    Panel(s, ScreenId.CUSTOM_IR_MANAGE, "Custom IRs", arrayOf("ir.manage.close"), subtitle = "WAV impulse responses for the Custom IR cab",
        actions = {
            Pad({ st.go(Action.PickFiles(PickKind.IR_FILES)) }, Modifier.width(150.dp).height(48.dp), arrayOf("ir.import", "ir.import.invalid"), tone = Tone.PRIMARY) { ink ->
                Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    VoLumIcon(VoLumGlyph.Plus, color = ink, size = 14.dp)
                    FitText("IMPORT WAV", VoLumType.label(12.sp, ink), Modifier.width(96.dp))
                }
            }
        }) {
        Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(14.dp)) {
            FitGrid(s.library.irs, 1, 56.dp, Modifier.weight(1f).fillMaxHeight(), gap = 4.dp, empty = "No IRs yet. IMPORT WAV adds your cabinet files.") { ir ->
                Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                    Box(Modifier.weight(1f).fillMaxHeight()) {
                        ListRow(ir.name, open?.id == ir.id, { st.go(Action.OpenIrConfig(ir.id)) }, arrayOf("ir.config.open"),
                            subtitle = String.format("%+.1f dB · %d Hz – %.1f kHz · tap to adjust", ir.levelDb, ir.lowCutHz.toInt(), ir.highCutHz / 1000),
                            lead = { VoLumIcon(VoLumGlyph.IrBars, color = VoLumColors.BtnIrOnBorder, size = 18.dp) })
                    }
                    GlyphPad(VoLumGlyph.Edit, "RENAME", { st.go(Action.RequestRenameIr(ir.id)) }, arrayOf("ir.rename"), width = 76.dp)
                    GlyphPad(VoLumGlyph.Delete, "DELETE", { st.go(Action.RequestDeleteIr(ir.id)) }, arrayOf("ir.delete"), danger = true, width = 76.dp)
                }
            }
            Card(Modifier.width(370.dp).fillMaxHeight(), if (open != null) "LEVEL AND CUTS" else "ADJUST") {
                if (open == null) {
                    Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                        FitText("Tap an IR to set its level and cuts.", VoLumType.body(13.sp, VoLumColors.CreamDim), Modifier.fillMaxWidth())
                    }
                } else {
                    FitText(open.name, VoLumType.label(15.sp, VoLumColors.BtnIrOnText), Modifier.fillMaxWidth().height(22.dp), align = TextAlign.Start)
                    Fader("LEVEL", open.levelDb, -20.0, 20.0, { String.format("%+.1f dB", it) }, { st.go(Action.SetIrLevel(open.id, it)) },
                        Modifier.fillMaxWidth().height(52.dp), arrayOf("ir.config.level"))
                    Fader("LOW CUT", open.lowCutHz, 20.0, 500.0, { "${it.toInt()} Hz" }, { st.go(Action.SetIrLowCut(open.id, it)) },
                        Modifier.fillMaxWidth().height(52.dp), arrayOf("ir.config.lowcut"))
                    Fader("HIGH CUT", open.highCutHz, 2000.0, 20000.0, { String.format("%.1f kHz", it / 1000) }, { st.go(Action.SetIrHighCut(open.id, it)) },
                        Modifier.fillMaxWidth().height(52.dp), arrayOf("ir.config.highcut"))
                }
            }
        }
    }
}

@Composable
internal fun PedalManage(s: VoLumState) {
    val st = LocalStage.current
    val custom = s.library.pedals.filter { it.custom }
    Panel(s, ScreenId.CUSTOM_PEDAL_MANAGE, "Custom pedals", arrayOf("pedal.manage.close"), subtitle = "your .nam pedal captures for NAM 1 and NAM 2 · ${custom.size} of 64 slots used",
        actions = {
            Pad({ st.go(Action.PickFiles(PickKind.PEDAL_FILES)) }, Modifier.width(150.dp).height(48.dp), arrayOf("pedal.import"), tone = Tone.PRIMARY) { ink ->
                Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    VoLumIcon(VoLumGlyph.Plus, color = ink, size = 14.dp)
                    FitText("IMPORT .NAM", VoLumType.label(12.sp, ink), Modifier.width(96.dp))
                }
            }
        }) {
        FitGrid(custom, 2, 56.dp, Modifier.fillMaxSize(), empty = "No custom pedals yet. IMPORT .NAM adds your captures.") { pedal ->
            Row(Modifier.fillMaxSize().panelDepth(4.dp).hairlineFrame(VoLumColors.Frame, radius = 4.dp).padding(start = 10.dp, end = 4.dp, top = 2.dp, bottom = 2.dp),
                verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                MotifArt(Motif.NAM1, true, Modifier.width(26.dp).fillMaxHeight())
                FitText(pedal.name, VoLumType.label(14.sp, VoLumColors.TextMed), Modifier.weight(1f).padding(start = 4.dp), align = TextAlign.Start)
                GlyphPad(VoLumGlyph.Edit, "RENAME", { st.go(Action.RequestRenamePedal(pedal.index)) }, arrayOf("pedal.rename"), width = 76.dp)
                GlyphPad(VoLumGlyph.Delete, "DELETE", { st.go(Action.RequestDeletePedal(pedal.index)) }, arrayOf("pedal.delete"), danger = true, width = 76.dp)
            }
        }
    }
}
