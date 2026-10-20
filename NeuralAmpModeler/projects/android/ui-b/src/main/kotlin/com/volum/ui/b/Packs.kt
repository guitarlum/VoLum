package com.volum.ui.b

import androidx.compose.foundation.Canvas
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
import androidx.compose.foundation.text.BasicText
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.ExportScope
import com.volum.core.MergeMode
import com.volum.core.PackOutcome
import com.volum.core.PickKind
import com.volum.core.ScreenId
import com.volum.core.VoLumState
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
    Panel(ScreenId.PACK_EXPORT, "Export a Pack", arrayOf("pack.export.close"),
        subtitle = ex.savedAs?.let { "saved as $it" } ?: "one file to back up or share your library",
        actions = {
            TextPad("SAVE PACK…", { st.go(Action.PickFiles(PickKind.PACK_SAVE)) }, Modifier.width(150.dp).height(48.dp), arrayOf("pack.export.save"),
                enabled = canSave, selected = canSave, style = Type.label(14.sp, Ink.hot))
        }) {
        Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            Column(Modifier.width(250.dp).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                listOf(
                    Triple(ExportScope.EVERYTHING, "EVERYTHING", "the whole library and settings"),
                    Triple(ExportScope.SOUNDS, "SOUNDS", "chosen User presets"),
                    Triple(ExportScope.AMP, "ONE CUSTOM AMP", "an amp with its presets"),
                ).forEach { (scope, label, hint) ->
                    Pad({ st.go(Action.SetExportScope(scope)) }, Modifier.fillMaxWidth().height(64.dp), arrayOf("pack.export.scope.${scope.name.lowercase()}"),
                        selected = ex.scope == scope, sel = Sel.AMBER_PICKER, role = Role.Tab) {
                        Column(Modifier.fillMaxSize().padding(horizontal = 12.dp), verticalArrangement = Arrangement.Center) {
                            FitText(label, Type.label(13.sp, Ink.text), Modifier.fillMaxWidth().height(20.dp), align = TextAlign.Start)
                            FitText(hint, Type.value(11.sp, Ink.dim), Modifier.fillMaxWidth().height(16.dp), align = TextAlign.Start)
                        }
                    }
                }
            }
            Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                when (ex.scope) {
                    ExportScope.EVERYTHING -> Card(Modifier.fillMaxSize(), "THIS PACK HOLDS") {
                        listOf(
                            "${lib.amps.count { it.custom }} custom amps",
                            "${lib.presets.count { !it.factory }} User presets",
                            "${lib.irs.size} IRs · ${lib.pedals.count { it.custom }} custom pedals",
                            "${lib.sounds.size} Sounds on PLAY · MIDI and signal settings",
                        ).forEach { FitText(it, Type.value(15.sp), Modifier.fillMaxWidth().height(24.dp), align = TextAlign.Start) }
                    }
                    ExportScope.SOUNDS -> {
                        Caption("TICK THE SOUNDS TO SHARE", Modifier.fillMaxWidth())
                        FitGrid(lib.presets.filter { !it.factory }, 2, 52.dp, Modifier.fillMaxWidth().weight(1f), empty = "No User presets to export yet.") { p ->
                            TickRow(p.name, lib.amp(p.ampId)?.name ?: "", p.id in ex.ticked, arrayOf("pack.export.sound_tick"), onClick = { st.go(Action.ToggleExportSound(p.id)) })
                        }
                        AlsoIncluding(ex.alsoIncluding(lib))
                    }
                    ExportScope.AMP -> {
                        Caption("PICK THE CUSTOM AMP", Modifier.fillMaxWidth())
                        FitGrid(lib.amps.filter { it.custom }, 2, 52.dp, Modifier.fillMaxWidth().weight(1f), empty = "No custom amps to export yet.") { amp ->
                            TextPad(amp.name, { st.go(Action.SetExportAmp(amp.id)) }, Modifier.fillMaxSize(), arrayOf("pack.export.amp_pick"),
                                selected = ex.ampId == amp.id, sel = Sel.LIST_TEAL, style = Type.value(15.sp), role = Role.RadioButton, fill = Ink.panel)
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
    Column(Modifier.fillMaxWidth().clip(RoundedCornerShape(6.dp)).background(Ink.raised).padding(horizontal = 10.dp, vertical = 6.dp).featureId("pack.export.also")) {
        Caption("ALSO INCLUDING (what these need to play)", Modifier.fillMaxWidth())
        FitText(items.joinToString(" · "), Type.value(13.sp), Modifier.fillMaxWidth().height(20.dp), align = TextAlign.Start)
    }
}

@Composable
private fun TickRow(name: String, detail: String, ticked: Boolean, ids: Array<String>, onClick: () -> Unit, trailing: (@Composable () -> Unit)? = null) {
    Pad(onClick, Modifier.fillMaxSize(), ids, selected = ticked, sel = Sel.LIST_TEAL, role = Role.Checkbox, fill = Ink.panel) {
        Row(Modifier.fillMaxSize().padding(horizontal = 10.dp), verticalAlignment = Alignment.CenterVertically) {
            Canvas(Modifier.size(20.dp)) {
                drawRect(if (ticked) Ink.hot else Ink.dim, style = Stroke(2.dp.toPx()))
                if (ticked) {
                    drawLine(Ink.hot, Offset(size.width * 0.2f, size.height * 0.5f), Offset(size.width * 0.42f, size.height * 0.75f), 2.5.dp.toPx())
                    drawLine(Ink.hot, Offset(size.width * 0.42f, size.height * 0.75f), Offset(size.width * 0.82f, size.height * 0.25f), 2.5.dp.toPx())
                }
            }
            Column(Modifier.weight(1f).padding(start = 10.dp)) {
                FitText(name, Type.value(14.sp, if (ticked) Ink.hot else Ink.text), Modifier.fillMaxWidth().height(20.dp), align = TextAlign.Start)
                FitText(detail, Type.label(10.sp), Modifier.fillMaxWidth().height(14.dp), align = TextAlign.Start)
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
    Panel(ScreenId.PACK_IMPORT, "Import a Pack", arrayOf("pack.import.close"), subtitle = pi?.name ?: "",
        actions = {
            TextPad("IMPORT", { st.go(Action.ConfirmImport) }, Modifier.width(140.dp).height(48.dp), arrayOf("pack.import.confirm"), enabled = canImport,
                selected = canImport, style = Type.label(14.sp, Ink.hot))
        }) {
        when {
            pi == null -> Box(Modifier.fillMaxSize())
            pi.damaged -> Notice(Modifier.fillMaxSize(), "pack.import.damaged", "This Pack is damaged", "Nothing was changed. Export it again on the device it came from.")
            else -> Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                    if (pi.refused) {
                        Notice(Modifier.fillMaxWidth().height(72.dp), "pack.import.refused", "Not enough pedal slots",
                            "This Pack needs more of the 64 custom pedal slots than are free. Delete some pedals first.")
                    }
                    Caption("TAP TO SKIP OR KEEP · WHAT HAPPENS TO EACH ITEM", Modifier.fillMaxWidth())
                    Box(Modifier.fillMaxWidth().weight(1f).featureId("pack.import.rows")) {
                        FitGrid(pi.rows, 1, 52.dp, Modifier.fillMaxSize()) { row ->
                            TickRow(row.name, row.kind.uppercase(), row.ticked, arrayOf("pack.import.tick"), { st.go(Action.ToggleImportRow(row.id)) }) {
                                Box(Modifier.clip(RoundedCornerShape(4.dp)).background(outcomeFill(row.outcome)).padding(horizontal = 8.dp, vertical = 2.dp)
                                    .featureId("pack.import.outcome")) {
                                    BasicText(if (row.ticked) row.outcome.label.uppercase() else "SKIP", style = Type.label(11.sp, Ink.text))
                                }
                            }
                        }
                    }
                }
                Card(Modifier.width(290.dp).fillMaxHeight(), "MERGE") {
                    listOf(
                        Triple(MergeMode.OVERWRITE, "OVERWRITE", "Pack items replace yours"),
                        Triple(MergeMode.ADD, "ADD", "only new items; yours stay"),
                        Triple(MergeMode.RESET, "RESET", "library becomes exactly the Pack"),
                    ).forEach { (mode, label, hint) ->
                        Pad({ st.go(Action.SetMerge(mode)) }, Modifier.fillMaxWidth().height(52.dp), arrayOf("pack.import.merge.${mode.name.lowercase()}"),
                            selected = pi.merge == mode, enabled = pi.everything && (mode != MergeMode.RESET || pi.allTicked), sel = Sel.AMBER_PICKER, role = Role.RadioButton) {
                            Column(Modifier.fillMaxSize().padding(horizontal = 10.dp), verticalArrangement = Arrangement.Center) {
                                FitText(label, Type.label(12.sp, Ink.text), Modifier.fillMaxWidth().height(18.dp), align = TextAlign.Start)
                                FitText(hint, Type.value(10.sp, Ink.dim), Modifier.fillMaxWidth().height(14.dp), align = TextAlign.Start)
                            }
                        }
                    }
                    Switch("RESTORE MACHINE SETTINGS", pi.restoreMachine, { st.go(Action.SetRestoreMachine(!pi.restoreMachine)) }, Modifier.fillMaxWidth().height(48.dp),
                        arrayOf("pack.import.machine"), enabled = pi.everything)
                    FitText("a backup (.packbak) is kept first", Type.value(11.sp, Ink.faint), Modifier.fillMaxWidth().height(16.dp), align = TextAlign.Start)
                }
            }
        }
    }
}

private fun outcomeFill(o: PackOutcome) = when (o) {
    PackOutcome.ADD -> Ink.raised
    PackOutcome.REPLACE, PackOutcome.RELOADS -> Ink.line
    PackOutcome.KEEP_MINE -> Ink.panel
    PackOutcome.SKIP -> Ink.alert.copy(alpha = 0.35f)
}

@Composable
private fun Notice(modifier: Modifier, id: String, title: String, body: String) {
    Column(modifier.clip(RoundedCornerShape(8.dp)).background(Ink.alert.copy(alpha = 0.2f)).brackets(Ink.alert).padding(12.dp).featureId(id),
        verticalArrangement = Arrangement.spacedBy(4.dp)) {
        FitText(title, Type.display(20.sp, Ink.hot), Modifier.fillMaxWidth().height(26.dp), align = TextAlign.Start)
        BasicText(body, style = Type.value(12.sp, Ink.dim))
    }
}
