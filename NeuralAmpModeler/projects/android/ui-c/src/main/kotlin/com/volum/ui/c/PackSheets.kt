package com.volum.ui.c

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
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
import com.volum.core.ExportScope
import com.volum.core.MergeMode
import com.volum.core.PackOutcome
import com.volum.core.PickKind

@Composable
internal fun PackExport(ui: Ui, modifier: Modifier) {
    val s = ui.s
    val e = s.packExport
    val also = e.alsoIncluding(s.library)
    PanelFrame("Export pack", "SETTINGS · SYSTEM", "pack.export.close", { ui.back() }, modifier, actions = {
        Btn("SAVE PACK…", listOf("pack.export.save"), { ui.d(Action.PickFiles(PickKind.PACK_SAVE)) }, strong = true, sub = e.savedAs ?: "CHOOSE WHERE")
    }) {
        Row(Modifier.fillMaxWidth().fillMaxHeight(), horizontalArrangement = Arrangement.spacedBy(16.dp)) {
            Column(Modifier.width(250.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                Caption("What goes in")
                Choice("EVERYTHING", e.scope == ExportScope.EVERYTHING, listOf("pack.export.scope.everything"), { ui.d(Action.SetExportScope(ExportScope.EVERYTHING)) }, Modifier.fillMaxWidth(), sub = "FULL BACKUP")
                Choice("SOUNDS", e.scope == ExportScope.SOUNDS, listOf("pack.export.scope.sounds"), { ui.d(Action.SetExportScope(ExportScope.SOUNDS)) }, Modifier.fillMaxWidth(), sub = "PICKED USER PRESETS")
                Choice("ONE AMP", e.scope == ExportScope.AMP, listOf("pack.export.scope.amp"), { ui.d(Action.SetExportScope(ExportScope.AMP)) }, Modifier.fillMaxWidth(), sub = "A CUSTOM AMP + ITS PRESETS")
            }
            Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                if (also.isNotEmpty()) {
                    Row(Modifier.fillMaxWidth().background(Look.listInk.copy(alpha = 0.16f), Corner).padding(10.dp).ids("pack.export.also"), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                        Caption("Also including", color = Look.listInk)
                        BasicText(also.joinToString("  ·  "), style = bodyStyle(12f, cap = 1.15f), maxLines = 2, overflow = TextOverflow.Ellipsis)
                    }
                }
                Column(Modifier.fillMaxWidth().weight(1f).verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                    when (e.scope) {
                        ExportScope.EVERYTHING -> Note("Every User preset, Sound, custom amp, IR and pedal, plus your settings. Use it as a backup or to move to another device.", maxLines = 3)
                        ExportScope.SOUNDS -> {
                            Caption("Tick the presets to share")
                            Grid(s.library.presets.filter { !it.factory }, 2) { p, m ->
                                Choice(p.name, p.id in e.ticked, listOf("pack.export.sound_tick"), { ui.d(Action.ToggleExportSound(p.id)) }, m, sub = (if (p.id in e.ticked) "✓ " else "") + (s.library.amp(p.ampId)?.name ?: ""))
                            }
                        }
                        ExportScope.AMP -> {
                            Caption("Custom amp")
                            val custom = s.library.amps.filter { it.custom }
                            if (custom.isEmpty()) Note("No custom amps to export.")
                            Grid(custom, 2) { a, m -> Choice(a.name, e.ampId == a.id, listOf("pack.export.amp_pick"), { ui.d(Action.SetExportAmp(a.id)) }, m) }
                        }
                    }
                }
            }
        }
    }
}

@Composable
internal fun PackImport(ui: Ui, modifier: Modifier) {
    val pi = ui.s.packImport
    PanelFrame("Import pack", pi?.name ?: "PACK", "pack.import.close", { ui.back() }, modifier, actions = {
        if (pi != null && !pi.damaged && !pi.refused) Btn("IMPORT", listOf("pack.import.confirm"), { ui.d(Action.ConfirmImport) }, strong = true, sub = "BACKUP FIRST", enabled = !pi.imported)
    }) {
        when {
            pi == null -> Note("No Pack open.")
            pi.damaged -> Problem("This Pack is damaged", "It could not be read. Nothing was changed. Ask for a fresh copy.", "pack.import.damaged")
            pi.refused -> Problem("Not enough custom pedal slots", "This Pack needs more custom pedal slots than you have left. Delete some custom pedals, then try again.", "pack.import.refused")
            else -> Row(Modifier.fillMaxWidth().fillMaxHeight(), horizontalArrangement = Arrangement.spacedBy(16.dp)) {
                Column(Modifier.weight(1f).fillMaxHeight().verticalScroll(rememberScrollState()).ids("pack.import.rows"), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                    pi.rows.forEach { r ->
                        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                            Choice(if (r.ticked) "✓" else " ", r.ticked, listOf("pack.import.tick"), { ui.d(Action.ToggleImportRow(r.id)) })
                            Column(Modifier.weight(1f)) {
                                BasicText(r.name, style = bodyStyle(13f, cap = 1.2f), maxLines = 1, overflow = TextOverflow.Ellipsis)
                                BasicText(r.kind.uppercase(), style = labelStyle(8f, Look.dim, bold = false))
                            }
                            Outcome(r.outcome)
                        }
                    }
                }
                Column(Modifier.width(340.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                    Group("Merge") {
                        Choice("OVERWRITE", pi.merge == MergeMode.OVERWRITE, listOf("pack.import.merge.overwrite"), { ui.d(Action.SetMerge(MergeMode.OVERWRITE)) })
                        Choice("ADD", pi.merge == MergeMode.ADD, listOf("pack.import.merge.add"), { ui.d(Action.SetMerge(MergeMode.ADD)) })
                        Choice("RESET", pi.merge == MergeMode.RESET, listOf("pack.import.merge.reset"), { ui.d(Action.SetMerge(MergeMode.RESET)) }, enabled = pi.everything && pi.allTicked)
                    }
                    Note(
                        when (pi.merge) {
                            MergeMode.OVERWRITE -> "Items you already have are replaced by the Pack's."
                            MergeMode.ADD -> "Only new items come in; yours stay as they are."
                            MergeMode.RESET -> "Your library is cleared first, then filled from the Pack."
                        },
                    )
                    Switch("RESTORE SETTINGS TOO", pi.restoreMachine, listOf("pack.import.machine"), { ui.d(Action.SetRestoreMachine(!pi.restoreMachine)) }, enabled = pi.everything)
                    Note("A backup of your library is kept before anything changes.")
                }
            }
        }
    }
}

@Composable
private fun Outcome(o: PackOutcome) {
    val color = when (o) {
        PackOutcome.ADD -> Look.ok
        PackOutcome.REPLACE, PackOutcome.RELOADS -> Look.warn
        PackOutcome.KEEP_MINE -> Look.listInk
        PackOutcome.SKIP -> Look.faint
    }
    Box(Modifier.background(color.copy(alpha = 0.18f), Pill).padding(horizontal = 10.dp, vertical = 4.dp).ids("pack.import.outcome")) {
        BasicText(o.label.uppercase(), style = labelStyle(9f, color))
    }
}

@Composable
private fun Problem(title: String, body: String, id: String) {
    Row(Modifier.fillMaxWidth().background(Look.danger.copy(alpha = 0.14f), Corner).padding(14.dp).ids(id), horizontalArrangement = Arrangement.spacedBy(12.dp), verticalAlignment = Alignment.CenterVertically) {
        Led(true, 12.dp, Look.danger)
        Column {
            BasicText(title, style = bodyStyle(17f))
            Note(body, color = Look.text, maxLines = 3)
        }
    }
}
