package com.volum.ui.a

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
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
import com.volum.core.ExportScope
import com.volum.core.MergeMode
import com.volum.core.PackOutcome
import com.volum.core.PickKind

@Composable
internal fun PackExport(c: Ctx) {
    val s = c.s
    val e = s.packExport
    Sheet("Export a Pack", listOf("pack.export.close"), c, actions = {
        listOf(ExportScope.EVERYTHING to "EVERYTHING", ExportScope.SOUNDS to "SOUNDS", ExportScope.AMP to "ONE AMP").forEach { (scope, label) ->
            Chip(label, { c.d(Action.SetExportScope(scope)) }, ids = listOf("pack.export.scope.${scope.name.lowercase()}"), selected = e.scope == scope)
        }
    }) {
        Row(Modifier.fillMaxWidth().weight(1f).padding(bottom = 8.dp), horizontalArrangement = Arrangement.spacedBy(14.dp)) {
            Column(Modifier.weight(1f).fillMaxHeight()) {
                when (e.scope) {
                    ExportScope.EVERYTHING -> Notice("Every custom amp, IR, pedal and User preset, your Sounds and this phone's settings. Restoring it on another phone rebuilds this rig.")
                    ExportScope.SOUNDS -> {
                        SectionLabel("USER PRESETS")
                        RowList(48.dp, Modifier.weight(1f).fillMaxWidth()) {
                            s.library.presets.filter { !it.factory }.forEach { p ->
                                val ticked = p.id in e.ticked
                                ListRow({ c.d(Action.ToggleExportSound(p.id)) }, ids = listOf("pack.export.sound_tick"), selected = ticked, pick = Pick.BRASS) {
                                    Row(verticalAlignment = Alignment.CenterVertically) {
                                        Led(ticked, Modifier.width(12.dp))
                                        T("   ${p.name}", Type.value(14.sp), Modifier.weight(1f))
                                        T(s.library.amp(p.ampId)?.name ?: "", Type.label(10.sp))
                                    }
                                }
                            }
                        }
                    }
                    ExportScope.AMP -> {
                        SectionLabel("CUSTOM AMP AND ITS PRESETS")
                        RowList(48.dp, Modifier.weight(1f).fillMaxWidth()) {
                            s.library.amps.filter { it.custom }.forEach { a ->
                                ListRow({ c.d(Action.SetExportAmp(a.id)) }, ids = listOf("pack.export.amp_pick"), selected = e.ampId == a.id) { T(a.name, Type.value(14.sp)) }
                            }
                        }
                    }
                }
            }
            Column(Modifier.width(300.dp).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                val also = e.alsoIncluding(s.library)
                if (also.isNotEmpty()) {
                    SectionLabel("ALSO INCLUDING")
                    Notice(also.joinToString("\n"), ids = listOf("pack.export.also"))
                }
                androidx.compose.foundation.layout.Spacer(Modifier.weight(1f))
                e.savedAs?.let { T("Saved $it", Type.label(11.sp, Ink.text)) }
                Chip("SAVE PACK", { c.d(Action.PickFiles(PickKind.PACK_SAVE)) }, Modifier.fillMaxWidth(), listOf("pack.export.save"), selected = true)
            }
        }
    }
}

private fun outcomeNote(o: PackOutcome) = when (o) {
    PackOutcome.ADD -> "new"
    PackOutcome.REPLACE -> "replaces yours"
    PackOutcome.KEEP_MINE -> "yours is kept"
    PackOutcome.RELOADS -> "reloads"
    PackOutcome.SKIP -> "skipped"
}

@Composable
internal fun PackImport(c: Ctx) {
    val pi = c.s.packImport
    Sheet("Import ${pi?.name ?: "a Pack"}", listOf("pack.import.close"), c) {
        when {
            pi == null -> Notice("No Pack open.")
            pi.damaged -> Notice("This Pack is damaged and cannot be read. Nothing was changed.", alert = true, ids = listOf("pack.import.damaged"))
            else -> Row(Modifier.fillMaxWidth().weight(1f).padding(bottom = 8.dp), horizontalArrangement = Arrangement.spacedBy(14.dp)) {
                Column(Modifier.weight(1f).fillMaxHeight()) {
                    if (pi.refused) Notice("This Pack needs more custom pedal slots than are left (64). Delete some pedals first.", Modifier.padding(bottom = 6.dp), alert = true, ids = listOf("pack.import.refused"))
                    RowList(48.dp, Modifier.weight(1f).fillMaxWidth().ids("pack.import.rows")) {
                        pi.rows.forEach { r ->
                            ListRow({ c.d(Action.ToggleImportRow(r.id)) }, ids = listOf("pack.import.tick"), selected = r.ticked, pick = Pick.BRASS) {
                                Row(verticalAlignment = Alignment.CenterVertically) {
                                    Led(r.ticked, Modifier.width(12.dp))
                                    T("   ${r.name}", Type.value(14.sp), Modifier.weight(1f))
                                    T(r.kind.uppercase(), Type.label(10.sp), Modifier.width(56.dp))
                                    T("${r.outcome.label} · ${outcomeNote(r.outcome)}", Type.label(10.sp, if (r.outcome == PackOutcome.SKIP) Ink.alert else Ink.text),
                                        Modifier.width(150.dp).ids("pack.import.outcome"))
                                }
                            }
                        }
                    }
                }
                Column(Modifier.width(260.dp).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                    SectionLabel("WHEN NAMES MATCH")
                    listOf(MergeMode.OVERWRITE to "OVERWRITE", MergeMode.ADD to "ADD AS NEW", MergeMode.RESET to "RESET LIBRARY").forEach { (m, label) ->
                        Chip(label, { c.d(Action.SetMerge(m)) }, Modifier.fillMaxWidth(), listOf("pack.import.merge.${m.name.lowercase()}"), selected = pi.merge == m,
                            enabled = pi.everything && (m != MergeMode.RESET || pi.allTicked))
                    }
                    Switch("RESTORE PHONE SETTINGS", pi.restoreMachine, { c.d(Action.SetRestoreMachine(!pi.restoreMachine)) }, Modifier.fillMaxWidth(), listOf("pack.import.machine"))
                    androidx.compose.foundation.layout.Spacer(Modifier.weight(1f))
                    Chip("IMPORT", { c.d(Action.ConfirmImport) }, Modifier.fillMaxWidth(), listOf("pack.import.confirm"), selected = true, enabled = !pi.refused)
                }
            }
        }
    }
}
