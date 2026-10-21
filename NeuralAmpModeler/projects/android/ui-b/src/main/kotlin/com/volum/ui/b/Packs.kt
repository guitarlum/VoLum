package com.volum.ui.b

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.ExportScope
import com.volum.core.MergeMode
import com.volum.core.PackOutcome
import com.volum.core.PackRow
import com.volum.core.PickKind
import com.volum.core.ScreenId
import com.volum.core.VoLumState
import com.volum.design.components.VoLumModePicker
import com.volum.design.components.WarnGlyph
import com.volum.design.components.hairlineFrame
import com.volum.design.components.insetWell
import com.volum.design.components.panelDepth
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumType
import com.volum.design.theme.withOpacity
import com.volum.harness.featureId

@Composable
internal fun PackExport(s: VoLumState) {
    val st = LocalStage.current
    val ex = s.packExport
    val lib = s.library
    val canSave = when (ex.scope) {
        ExportScope.EVERYTHING -> true
        ExportScope.SOUNDS -> ex.ticked.isNotEmpty()
        ExportScope.AMP -> ex.ampId != null
    }
    Panel(s, ScreenId.PACK_EXPORT, "Export a Pack", arrayOf("pack.export.close"),
        subtitle = ex.savedAs?.let { "saved as $it" } ?: "one file to back up or share your library",
        actions = {
            TextPad(if (canSave) "SAVE PACK…" else "PICK WHAT TO SHARE", { st.go(Action.PickFiles(PickKind.PACK_SAVE)) }, Modifier.width(170.dp).height(48.dp),
                arrayOf("pack.export.save"), enabled = canSave, tone = Tone.PRIMARY)
        }) {
        Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(14.dp)) {
            Column(Modifier.width(250.dp).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                val scopes = rememberFitGroup()
                val details = rememberFitGroup()
                SectionTitle("WHAT GOES IN", Modifier.fillMaxWidth())
                listOf(
                    Triple(ExportScope.EVERYTHING, "EVERYTHING", "the whole library and settings"),
                    Triple(ExportScope.SOUNDS, "SOUNDS", "chosen User presets"),
                    Triple(ExportScope.AMP, "ONE CUSTOM AMP", "an amp with its presets"),
                ).forEach { (scope, label, detail) ->
                    Pad({ st.go(Action.SetExportScope(scope)) }, Modifier.fillMaxWidth().height(60.dp), arrayOf("pack.export.scope.${scope.name.lowercase()}"),
                        selected = ex.scope == scope, style = Sel.AmberPicker, tone = Tone.FRAMED, role = Role.RadioButton) { ink ->
                        Column(Modifier.fillMaxSize().padding(horizontal = 12.dp), verticalArrangement = Arrangement.Center) {
                            FitText(label, VoLumType.label(13.sp, ink), Modifier.fillMaxWidth().height(20.dp), align = TextAlign.Start, group = scopes)
                            FitText(detail, if (ex.scope == scope) VoLumType.label(11.sp, ink) else hint(size = 11.sp), Modifier.fillMaxWidth().height(17.dp),
                                align = TextAlign.Start, group = details)
                        }
                    }
                }
            }
            Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                when (ex.scope) {
                    ExportScope.EVERYTHING -> Card(Modifier.fillMaxSize(), "THIS PACK HOLDS") {
                        listOf(
                            "${lib.amps.count { it.custom }} custom amps",
                            "${lib.presets.count { !it.factory }} User presets",
                            "${lib.irs.size} IRs · ${lib.pedals.count { it.custom }} custom pedals",
                            "${lib.sounds.size} Sounds on PLAY · MIDI and signal settings",
                        ).forEach { FitText(it, VoLumType.label(14.sp, VoLumColors.Cream), Modifier.fillMaxWidth().height(24.dp), align = TextAlign.Start) }
                    }
                    ExportScope.SOUNDS -> {
                        SectionTitle("TICK THE SOUNDS TO SHARE", Modifier.fillMaxWidth())
                        FitGrid(lib.presets.filter { !it.factory }, 2, 52.dp, Modifier.fillMaxWidth().weight(1f), gap = 4.dp, empty = "No User presets to export yet.") { p ->
                            TickRow(p.name, lib.amp(p.ampId)?.name ?: "", p.id in ex.ticked, arrayOf("pack.export.sound_tick")) { st.go(Action.ToggleExportSound(p.id)) }
                        }
                        AlsoIncluding(ex.alsoIncluding(lib))
                    }
                    ExportScope.AMP -> {
                        SectionTitle("PICK THE CUSTOM AMP", Modifier.fillMaxWidth())
                        FitGrid(lib.amps.filter { it.custom }, 2, 52.dp, Modifier.fillMaxWidth().weight(1f), gap = 4.dp, empty = "No custom amps to export yet.") { amp ->
                            ListRow(amp.name, ex.ampId == amp.id, { st.go(Action.SetExportAmp(amp.id)) }, arrayOf("pack.export.amp_pick"))
                        }
                        AlsoIncluding(ex.alsoIncluding(lib))
                    }
                }
            }
        }
    }
}

@Composable
private fun AlsoIncluding(items: List<String>) {
    if (items.isEmpty()) return
    Column(Modifier.fillMaxWidth().insetWell(3.dp).padding(horizontal = 12.dp, vertical = 6.dp).featureId("pack.export.also")) {
        FitText("ALSO INCLUDING · what these need to play", VoLumType.SectionHeader, Modifier.fillMaxWidth().height(16.dp), align = TextAlign.Start)
        FitText(items.joinToString(" · "), VoLumType.label(12.sp, VoLumColors.Teal), Modifier.fillMaxWidth().height(20.dp), align = TextAlign.Start)
    }
}

/** A gold check box drawn with the palette: framed when clear, filled amber with a dark tick when ticked. */
@Composable
private fun CheckBox(ticked: Boolean) {
    Spacer(Modifier.size(20.dp).drawBehind {
        val px = 1.dp.toPx()
        val cr = CornerRadius(3.dp.toPx())
        if (ticked) drawRoundRect(VoLumColors.Amber, cornerRadius = cr)
        else drawRoundRect(VoLumColors.GoldDim, Offset(px / 2, px / 2), Size(size.width - px, size.height - px), cr, style = Stroke(1.4f * px))
        if (ticked) {
            drawLine(VoLumColors.AmberInk, Offset(size.width * 0.22f, size.height * 0.52f), Offset(size.width * 0.43f, size.height * 0.74f), 2.2f * px)
            drawLine(VoLumColors.AmberInk, Offset(size.width * 0.43f, size.height * 0.74f), Offset(size.width * 0.8f, size.height * 0.28f), 2.2f * px)
        }
    })
}

@Composable
private fun TickRow(name: String, detail: String, ticked: Boolean, ids: Array<String>, trailing: (@Composable () -> Unit)? = null, onClick: () -> Unit) {
    Pad(onClick, Modifier.fillMaxSize(), ids, selected = ticked, style = Sel.ListTeal, tone = Tone.FRAMED, role = Role.Checkbox) { _ ->
        Row(Modifier.fillMaxSize().padding(horizontal = 12.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            CheckBox(ticked)
            Column(Modifier.weight(1f)) {
                FitText(name, VoLumType.label(13.sp, if (ticked) VoLumColors.TextBright else VoLumColors.Cream), Modifier.fillMaxWidth().height(20.dp), align = TextAlign.Start)
                FitText(detail, VoLumType.label(9.sp, VoLumColors.TealDim), Modifier.fillMaxWidth().height(14.dp), align = TextAlign.Start)
            }
            trailing?.invoke()
        }
    }
}

@Composable
internal fun PackImport(s: VoLumState) {
    val st = LocalStage.current
    val pi = s.packImport
    val canImport = pi != null && !pi.damaged && !pi.refused && !pi.imported
    Panel(s, ScreenId.PACK_IMPORT, "Import a Pack", arrayOf("pack.import.close"), subtitle = pi?.name ?: "",
        actions = {
            if (pi?.damaged != true) {
                TextPad(if (pi?.refused == true) "CANNOT IMPORT" else "IMPORT", { st.go(Action.ConfirmImport) }, Modifier.width(150.dp).height(48.dp),
                    arrayOf("pack.import.confirm"), enabled = canImport, tone = Tone.PRIMARY, size = 13.sp)
            }
        }) {
        when {
            pi == null -> Box(Modifier.fillMaxSize())
            pi.damaged -> Damaged(Modifier.fillMaxSize())
            else -> Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(14.dp)) {
                Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                    if (pi.refused) {
                        Notice(Modifier.fillMaxWidth(), "pack.import.refused", "Not enough pedal slots",
                            listOf("This Pack needs more of the 64 custom pedal slots than are free.", "Delete some custom pedals first, then import again."))
                    }
                    SectionTitle("WHAT HAPPENS TO EACH ITEM · tap a row to skip or keep it", Modifier.fillMaxWidth())
                    Box(Modifier.fillMaxWidth().weight(1f).featureId("pack.import.rows")) {
                        FitGrid(pi.rows, 1, 52.dp, Modifier.fillMaxSize(), gap = 4.dp) { row -> ImportRow(row) }
                    }
                }
                Card(Modifier.width(290.dp).fillMaxHeight(), "MERGE") {
                    val modes = listOf(MergeMode.OVERWRITE to "OVERWRITE", MergeMode.ADD to "ADD", MergeMode.RESET to "RESET")
                    VoLumModePicker(modes.map { it.second }, modes.indexOfFirst { it.first == pi.merge }, { i -> st.go(Action.SetMerge(modes[i].first)) }, Modifier.fillMaxWidth(),
                        itemModifier = { i ->
                            val enabled = pi.everything && (modes[i].first != MergeMode.RESET || pi.allTicked)
                            Modifier.fillMaxWidth().featureId("pack.import.merge.${modes[i].first.name.lowercase()}").alpha(if (enabled) 1f else 0.4f)
                        })
                    FitText(when (pi.merge) {
                        MergeMode.OVERWRITE -> "Pack items replace yours"
                        MergeMode.ADD -> "Only new items; yours stay"
                        MergeMode.RESET -> "Your library becomes exactly the Pack"
                    }, hint(VoLumColors.Cream), Modifier.fillMaxWidth().height(18.dp), align = TextAlign.Start)
                    Switch("RESTORE MACHINE SETTINGS", pi.restoreMachine, { st.go(Action.SetRestoreMachine(!pi.restoreMachine)) }, Modifier.fillMaxWidth().height(48.dp),
                        arrayOf("pack.import.machine"), enabled = pi.everything)
                    FitText("A backup (.packbak) is kept first.", hint(), Modifier.fillMaxWidth().height(18.dp), align = TextAlign.Start)
                }
            }
        }
    }
}



/**
 * One Pack item. Rows the import can act on carry a check box and say what happens
 * (or SKIP when unticked). Rows it cannot act on (yours is kept, or the file is
 * missing) show only their outcome, so a ticked box never sits next to SKIP.
 */
@Composable
private fun ImportRow(row: PackRow) {
    val st = LocalStage.current
    val actionable = row.outcome != PackOutcome.SKIP && row.outcome != PackOutcome.KEEP_MINE
    val label = if (actionable && !row.ticked) "SKIP" else row.outcome.label.uppercase()
    val badge: @Composable () -> Unit = {
        Box(Modifier.widthIn(min = 92.dp).drawBehind {
            val cr = CornerRadius(3.dp.toPx())
            drawRoundRect(outcomeInk(row, actionable).withOpacity(0.16f), cornerRadius = cr)
        }.hairlineFrame(outcomeInk(row, actionable).withOpacity(0.6f), radius = 3.dp).padding(horizontal = 8.dp, vertical = 3.dp).featureId("pack.import.outcome"),
            contentAlignment = Alignment.Center) {
            FitText(label, VoLumType.label(10.sp, outcomeInk(row, actionable)), Modifier.width(80.dp).height(16.dp))
        }
    }
    if (actionable) {
        TickRow(row.name, row.kind.uppercase(), row.ticked, arrayOf("pack.import.tick"), badge) { st.go(Action.ToggleImportRow(row.id)) }
    } else {
        Row(Modifier.fillMaxSize().panelDepth(4.dp).hairlineFrame(VoLumColors.Frame, radius = 4.dp).padding(horizontal = 12.dp),
            verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            Spacer(Modifier.size(20.dp))
            Column(Modifier.weight(1f)) {
                FitText(row.name, VoLumType.label(13.sp, VoLumColors.CreamDim), Modifier.fillMaxWidth().height(20.dp), align = TextAlign.Start)
                FitText(if (row.outcome == PackOutcome.SKIP) "${row.kind.uppercase()} · missing from the Pack, cannot import" else "${row.kind.uppercase()} · yours is kept",
                    VoLumType.label(9.sp, VoLumColors.TealDim), Modifier.fillMaxWidth().height(14.dp), align = TextAlign.Start)
            }
            badge()
        }
    }
}

private fun outcomeInk(row: PackRow, actionable: Boolean): Color = when {
    actionable && !row.ticked -> VoLumColors.CreamDim
    row.outcome == PackOutcome.ADD -> VoLumColors.Teal
    row.outcome == PackOutcome.REPLACE || row.outcome == PackOutcome.RELOADS -> VoLumColors.Amber
    row.outcome == PackOutcome.KEEP_MINE -> VoLumColors.Gold
    else -> VoLumColors.Danger
}

/** A damaged Pack: nothing changed, and the two ways on. */
@Composable
private fun Damaged(modifier: Modifier) {
    val st = LocalStage.current
    Box(modifier, contentAlignment = Alignment.Center) {
        Column(
            Modifier.width(520.dp).panelDepth(6.dp).hairlineFrame(VoLumColors.Danger.withOpacity(0.6f), 1.4.dp, 6.dp).padding(20.dp).featureId("pack.import.damaged"),
            verticalArrangement = Arrangement.spacedBy(10.dp),
        ) {
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                WarnGlyph(VoLumColors.Danger)
                FitText("This Pack is damaged", VoLumType.label(16.sp, VoLumColors.Danger), Modifier.weight(1f).height(24.dp), align = TextAlign.Start)
            }
            FitText("Nothing in your library was changed.", hint(VoLumColors.Cream, 13.sp), Modifier.fillMaxWidth().height(20.dp), align = TextAlign.Start)
            FitText("Export it again on the device it came from, or pick another file.", hint(), Modifier.fillMaxWidth().height(18.dp), align = TextAlign.Start)
            Row(Modifier.fillMaxWidth().height(52.dp), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                TextPad("CLOSE", { st.go(Action.Back) }, Modifier.weight(1f).fillMaxHeight(), arrayOf("pack.import.close"))
                TextPad("CHOOSE ANOTHER FILE", { st.go(Action.PickFiles(PickKind.PACK_OPEN)) }, Modifier.weight(1.4f).fillMaxHeight(), arrayOf("pack.import.open"), tone = Tone.PRIMARY)
            }
        }
    }
}

@Composable
private fun Notice(modifier: Modifier, id: String, title: String, lines: List<String>) {
    Column(modifier.insetWell(3.dp).hairlineFrame(VoLumColors.Danger.withOpacity(0.55f), radius = 3.dp).padding(horizontal = 12.dp, vertical = 8.dp).featureId(id),
        verticalArrangement = Arrangement.spacedBy(2.dp)) {
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            WarnGlyph(VoLumColors.Danger)
            FitText(title, VoLumType.label(14.sp, VoLumColors.Danger), Modifier.weight(1f).height(22.dp), align = TextAlign.Start)
        }
        val fit = rememberFitGroup()
        lines.forEach { FitText(it, hint(VoLumColors.Cream), Modifier.fillMaxWidth().height(18.dp), align = TextAlign.Start, group = fit) }
    }
}
