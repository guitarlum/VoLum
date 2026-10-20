package com.volum.ui.c

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.width
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.ExportScope
import com.volum.core.MergeMode
import com.volum.core.Overlay
import com.volum.core.PackOutcome
import com.volum.core.PackRow
import com.volum.core.PickKind
import com.volum.design.components.VoLumSwitchTrack
import com.volum.design.components.hairlineFrame
import com.volum.design.components.insetWell
import com.volum.design.theme.VFitText
import com.volum.design.theme.VText
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumType

@Composable
internal fun PackExport(ui: Ui, modifier: Modifier) {
    val s = ui.s
    val e = s.packExport
    val also = e.alsoIncluding(s.library)
    PanelFrame(ui, "Export pack", "pack.export.close", { ui.back() }, modifier, crumb = "Settings  ·  System", actions = {
        if (e.savedAs != null) VFitText("Saved as ${e.savedAs}", Modifier.width(200.dp), style = VoLumType.label(10.sp, VoLumColors.Teal), minSize = 7.sp)
        Btn("Save pack", listOf("pack.export.save"), { ui.d(Action.PickFiles(PickKind.PACK_SAVE)) }, primary = true)
    }) {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(16.dp)) {
            Stack(Modifier.width(250.dp), gap = 4) {
                Caption("What goes in")
                ListRow("Everything", listOf("pack.export.scope.everything"), { ui.d(Action.SetExportScope(ExportScope.EVERYTHING)) }, Modifier.fillMaxWidth(), sub = "FULL BACKUP", selected = e.scope == ExportScope.EVERYTHING)
                ListRow("Sounds", listOf("pack.export.scope.sounds"), { ui.d(Action.SetExportScope(ExportScope.SOUNDS)) }, Modifier.fillMaxWidth(), sub = "PICKED USER PRESETS", selected = e.scope == ExportScope.SOUNDS)
                ListRow("One amp", listOf("pack.export.scope.amp"), { ui.d(Action.SetExportScope(ExportScope.AMP)) }, Modifier.fillMaxWidth(), sub = "A CUSTOM AMP AND ITS PRESETS", selected = e.scope == ExportScope.AMP)
            }
            Stack(Modifier.weight(1f), gap = 6) {
                if (also.isNotEmpty()) {
                    Row(Modifier.fillMaxWidth().insetWell(4.dp).hairlineFrame(VoLumColors.TealDim, radius = 4.dp).ids("pack.export.also"), verticalAlignment = Alignment.CenterVertically) {
                        Gap(w = 10.dp)
                        Caption("Also including", color = VoLumColors.Teal)
                        Gap(w = 10.dp)
                        Note(also.joinToString("  ·  "), Modifier.weight(1f), VoLumColors.Cream, maxLines = 2)
                    }
                }
                when (e.scope) {
                    ExportScope.EVERYTHING -> Note("Every User preset, Sound, custom amp, IR and pedal, plus your settings. Use it as a backup or to move to another device.")
                    ExportScope.SOUNDS -> {
                        Caption("Tick the presets to share")
                        val user = s.library.presets.filter { !it.factory }
                        SnapList(gridRows(user.size, 2), Modifier.fillMaxWidth()) {
                            Grid(user, 2) { p, m ->
                                TickRow(p.name, s.library.amp(p.ampId)?.name ?: "", p.id in e.ticked, listOf("pack.export.sound_tick"), { ui.d(Action.ToggleExportSound(p.id)) }, m)
                            }
                        }
                    }
                    ExportScope.AMP -> {
                        Caption("Custom amp")
                        val custom = s.library.amps.filter { it.custom }
                        if (custom.isEmpty()) Note("No custom amps to export.")
                        SnapList(gridRows(custom.size, 2), Modifier.fillMaxWidth()) {
                            Grid(custom, 2) { a, m -> ListRow(a.name, listOf("pack.export.amp_pick"), { ui.d(Action.SetExportAmp(a.id)) }, m, selected = e.ampId == a.id, art = artOf(a)) }
                        }
                    }
                }
            }
        }
    }
}

/** A row you tick in or out: the `:design` switch, the name, and what it is. */
@Composable
private fun TickRow(title: String, sub: String, on: Boolean, ids: List<String>, onToggle: () -> Unit, modifier: Modifier, trailing: @Composable () -> Unit = {}) {
    Tappable(onToggle, modifier.height(Metrics.target), ids, role = androidx.compose.ui.semantics.Role.Switch) { pressed ->
        Row(Modifier.fillMaxWidth().height(Metrics.target), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            VoLumSwitchTrack(on, pressed = pressed)
            Column(Modifier.weight(1f)) {
                VFitText(title, style = VoLumType.label(13.sp, if (on) VoLumColors.TextBright else VoLumColors.CreamDim), minSize = 9.sp)
                VText(sub, style = VoLumType.label(9.sp, VoLumColors.TealDim))
            }
            trailing()
        }
    }
}

/** What will happen to a row, in words, coloured by how much it changes your library. */
private fun outcome(r: PackRow): Pair<String, androidx.compose.ui.graphics.Color> = when {
    !r.ticked -> "Not imported" to VoLumColors.CreamDim
    r.outcome == PackOutcome.ADD -> "New: added" to VoLumColors.Teal
    r.outcome == PackOutcome.REPLACE -> "Replaces yours" to VoLumColors.Amber
    r.outcome == PackOutcome.RELOADS -> "Replaces yours, reloads" to VoLumColors.Amber
    r.outcome == PackOutcome.KEEP_MINE -> "Yours is kept" to VoLumColors.Gold
    else -> "Skipped: file missing" to VoLumColors.CreamDim
}

@Composable
internal fun PackImport(ui: Ui, modifier: Modifier) {
    val pi = ui.s.packImport
    PanelFrame(ui, "Import pack", "pack.import.close", { ui.back() }, modifier, crumb = pi?.name ?: "Pack", actions = {
        if (pi != null && !pi.damaged && !pi.refused) Btn(if (pi.imported) "Imported" else "Import", listOf("pack.import.confirm"), { ui.d(Action.ConfirmImport) }, primary = true, enabled = !pi.imported)
    }) {
        when {
            pi == null -> Note("No Pack open.")
            pi.damaged -> {
                Alert("This Pack is damaged", "It could not be read and nothing was changed. Ask for a fresh copy, or open another file.", "pack.import.damaged")
                Spaced(gap = 8) { Btn("Choose another file", listOf("pack.import.open"), { ui.d(Action.PickFiles(PickKind.PACK_OPEN)) }, primary = true) }
            }
            pi.refused -> {
                Alert("Not enough custom pedal slots", "This Pack needs more custom pedal slots than you have left. Delete some custom pedals, then open it again.", "pack.import.refused")
                Spaced(gap = 8) {
                    Btn("Manage pedals", listOf("pre.capture.manage"), { ui.d(Action.Open(Overlay.PedalManage)) }, primary = true)
                    Btn("Choose another file", listOf("pack.import.open"), { ui.d(Action.PickFiles(PickKind.PACK_OPEN)) })
                }
            }
            else -> Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(16.dp)) {
                SnapList(pi.rows.size, Modifier.weight(1f), inner = Modifier.ids("pack.import.rows")) {
                    pi.rows.forEach { r ->
                        TickRow(r.name, r.kind.uppercase(), r.ticked, listOf("pack.import.tick"), { ui.d(Action.ToggleImportRow(r.id)) }, Modifier.fillMaxWidth()) {
                            val (text, color) = outcome(r)
                            VFitText(text, Modifier.width(190.dp).ids("pack.import.outcome"), style = VoLumType.label(11.sp, color), minSize = 8.sp)
                        }
                    }
                }
                Stack(Modifier.width(330.dp), gap = 6) {
                    Caption("When you already have it")
                    Spaced(gap = 4) {
                        Brass("Overwrite", pi.merge == MergeMode.OVERWRITE, listOf("pack.import.merge.overwrite"), { ui.d(Action.SetMerge(MergeMode.OVERWRITE)) })
                        Brass("Add", pi.merge == MergeMode.ADD, listOf("pack.import.merge.add"), { ui.d(Action.SetMerge(MergeMode.ADD)) })
                        Brass("Reset", pi.merge == MergeMode.RESET, listOf("pack.import.merge.reset"), { ui.d(Action.SetMerge(MergeMode.RESET)) }, enabled = pi.everything && pi.allTicked)
                    }
                    Note(
                        when (pi.merge) {
                            MergeMode.OVERWRITE -> "Items you already have are replaced by the Pack's."
                            MergeMode.ADD -> "Only new items come in; yours stay as they are."
                            MergeMode.RESET -> "Your library is cleared first, then filled from the Pack."
                        },
                        maxLines = 2, size = 11.sp,
                    )
                    Toggle("Restore settings too", pi.restoreMachine, listOf("pack.import.machine"), { ui.d(Action.SetRestoreMachine(!pi.restoreMachine)) }, enabled = pi.everything)
                    Note("A backup of your library is kept before anything changes.", maxLines = 2, size = 11.sp)
                }
            }
        }
    }
}
