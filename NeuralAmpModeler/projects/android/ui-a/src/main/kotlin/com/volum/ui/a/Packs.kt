package com.volum.ui.a

import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsPressedAsState
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.ExportScope
import com.volum.core.Library
import com.volum.core.MergeMode
import com.volum.core.PackImportState
import com.volum.core.PackOutcome
import com.volum.core.PackRow
import com.volum.core.PickKind
import com.volum.design.components.MiniPill
import com.volum.design.components.VoLumButton
import com.volum.design.components.VoLumListCaption
import com.volum.design.components.VoLumListRow
import com.volum.design.components.VoLumSelectionStyle
import com.volum.design.components.VoLumToggle
import com.volum.design.components.hairlineFrame
import com.volum.design.components.insetWell
import com.volum.design.components.twoDigits
import com.volum.design.components.volumSelection
import com.volum.design.theme.VFitText
import com.volum.design.theme.VText
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumType
import com.volum.design.theme.withOpacity

private val Pitch = 48.dp

/** A row you include or leave out: the on / off pill first, so a tap anywhere on the row reads as ticking it. */
@Composable
private fun TickRow(ticked: Boolean, onToggle: () -> Unit, id: String, modifier: Modifier = Modifier, content: @Composable androidx.compose.foundation.layout.RowScope.() -> Unit) {
    val source = remember { MutableInteractionSource() }
    val pressed by source.collectIsPressedAsState()
    Row(
        modifier.ids(id).fillMaxWidth().height(Pitch)
            .clickable(source, indication = null, role = Role.Checkbox, onClick = onToggle)
            .volumSelection(false, pressed, VoLumSelectionStyle.ListTeal, radius = 3.dp, inset = 0.dp)
            .padding(horizontal = 8.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        MiniPill(ticked)
        content()
    }
}

private fun kindOf(lib: Library, name: String): String = when {
    lib.irs.any { it.name == name } -> "IR"
    lib.pedals.any { it.custom && it.name == name } -> "Pedal"
    lib.amps.any { it.custom && it.name == name } -> "Custom amp"
    else -> "Item"
}

@Composable
internal fun PackExport(c: Ctx) {
    val s = c.s
    val e = s.packExport
    Sheet("Export a Pack", listOf("pack.export.close"), c) {
        Row(Modifier.fillMaxWidth().weight(1f), horizontalArrangement = Arrangement.spacedBy(14.dp)) {
            Column(Modifier.width(330.dp).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(2.dp)) {
                listOf(
                    Triple(ExportScope.EVERYTHING, "Everything", "your whole library, phone settings and MIDI slots"),
                    Triple(ExportScope.SOUNDS, "Sounds", "pick presets; your PLAY Sounds are at the top"),
                    Triple(ExportScope.AMP, "A whole amp", "one custom amp and every preset saved on it"),
                ).forEach { (scope, label, desc) ->
                    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                        Choice(label, e.scope == scope, { c.d(Action.SetExportScope(scope)) }, Modifier.width(120.dp).ids("pack.export.scope.${scope.name.lowercase()}"), VoLumSelectionStyle.Brass)
                        Hint(desc, Modifier.weight(1f))
                    }
                }
                Gap(h = 6.dp)
                val also = e.alsoIncluding(s.library)
                if (also.isNotEmpty()) {
                    Column(Modifier.fillMaxWidth().ids("pack.export.also").insetWell(3.dp).hairlineFrame(VoLumColors.GoldDim.withOpacity(0.5f), radius = 3.dp).padding(10.dp), verticalArrangement = Arrangement.spacedBy(2.dp)) {
                        VText("${also.size} ${if (also.size == 1) "ITEM COMES" else "ITEMS COME"} ALONG", style = VoLumType.label(10.sp, VoLumColors.Gold))
                        also.forEach { name -> VFitText("${kindOf(s.library, name)}  \"$name\"", style = VoLumType.body(12.sp, VoLumColors.Cream), minSize = 9.sp) }
                    }
                }
                Fill()
                e.savedAs?.let { VText("Saved as $it", style = VoLumType.label(11.sp, VoLumColors.Teal)) }
                VoLumButton("Export...", { c.d(Action.PickFiles(PickKind.PACK_SAVE)) }, Modifier.fillMaxWidth().ids("pack.export.save"), primary = true)
            }
            Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                when (e.scope) {
                    ExportScope.EVERYTHING -> {
                        val lib = s.library
                        VoLumListCaption("IN THIS PACK")
                        Note(
                            "${lib.amps.count { it.custom }} custom amp(s), ${lib.irs.size} IR(s), ${lib.pedals.count { it.custom }} custom pedal(s), " +
                                "${lib.presets.count { !it.factory }} User preset(s), your ${lib.sounds.size} Sounds and this phone's settings. Restoring it on another phone rebuilds this rig.",
                            Modifier.fillMaxWidth(), lines = 4,
                        )
                    }
                    ExportScope.SOUNDS -> {
                        val programsOf = s.library.sounds.entries.groupBy({ it.value.presetId }, { it.key })
                        val user = s.library.presets.filter { !it.factory }.sortedBy { programsOf[it.id]?.minOrNull() ?: 1000 }
                        VoLumListCaption("USER PRESETS  ·  ${e.ticked.size} TICKED")
                        RowList(Pitch, Modifier.weight(1f).fillMaxWidth()) {
                            user.forEach { p ->
                                TickRow(p.id in e.ticked, { c.d(Action.ToggleExportSound(p.id)) }, "pack.export.sound_tick") {
                                    val programs = programsOf[p.id].orEmpty().sorted()
                                    Box(Modifier.widthIn(min = 30.dp)) {
                                        if (programs.isNotEmpty()) VText(programs.joinToString(" ") { twoDigits(it) }, Modifier.hairlineFrame(VoLumColors.GoldDim, radius = 2.dp).padding(horizontal = 4.dp, vertical = 1.dp), style = VoLumType.value(10.sp))
                                    }
                                    VFitText(p.name, Modifier.weight(1f), style = VoLumType.label(13.sp, VoLumColors.TextMed), minSize = 9.sp)
                                    VText(s.library.amp(p.ampId)?.name ?: "", style = VoLumType.label(10.sp, VoLumColors.Teal))
                                }
                            }
                        }
                    }
                    ExportScope.AMP -> {
                        VoLumListCaption("CUSTOM AMP")
                        RowList(Pitch, Modifier.weight(1f).fillMaxWidth()) {
                            s.library.amps.filter { it.custom }.forEach { a ->
                                val n = s.library.presets.count { it.ampId == a.id }
                                VoLumListRow(a.name, e.ampId == a.id, { c.d(Action.SetExportAmp(a.id)) }, Modifier.fillMaxWidth().height(Pitch).ids("pack.export.amp_pick"),
                                    art = com.volum.design.art.ArtRef.of(a), subtitle = "$n PRESET${if (n == 1) "" else "S"}")
                            }
                        }
                    }
                }
            }
        }
    }
}

/** What happens to a row on import, and why, in the current merge mode. */
private fun outcome(pi: PackImportState, r: PackRow): Pair<String, String> = when {
    r.outcome == PackOutcome.SKIP -> "Skip" to "its file is missing from the Pack"
    !r.ticked -> "Left out" to "unticked: yours stays as it is"
    r.outcome == PackOutcome.REPLACE && pi.merge == MergeMode.ADD -> "Keep mine" to "Add keeps yours; the Pack's is not added"
    r.outcome == PackOutcome.REPLACE -> "Replace" to "same name: the Pack's wins"
    r.outcome == PackOutcome.KEEP_MINE -> "Keep mine" to "yours is the same item; nothing to copy"
    r.outcome == PackOutcome.RELOADS -> "Reloads" to "playing now: reloads after the import"
    else -> "Add" to "new to your library"
}

private fun kindLabel(kind: String) = when (kind) {
    "amp" -> "Amp"
    "ir" -> "IR"
    "pedal" -> "Pedal"
    "preset" -> "Preset"
    else -> kind.replaceFirstChar { it.uppercase() }
}

private fun summary(pi: PackImportState): String {
    val counts = pi.rows.groupingBy { it.kind }.eachCount()
    val parts = listOf("amp" to "amp", "ir" to "IR", "pedal" to "pedal", "preset" to "preset").mapNotNull { (k, label) ->
        counts[k]?.let { n -> "$n $label${if (n > 1) "s" else ""}" }
    }
    return "${if (pi.everything) "Everything Pack" else "Pack"}  ·  ${parts.joinToString(", ")}"
}

@Composable
internal fun PackImport(c: Ctx) {
    val pi = c.s.packImport
    if (pi == null || pi.damaged) {
        Sheet("Import a Pack", listOf("pack.import.close"), c, width = 560.dp, fill = false) {
            Note("${pi?.name ?: "This file"} is damaged and cannot be read. Nothing was changed.", Modifier.fillMaxWidth(), alert = true, ids = listOf("pack.import.damaged"))
            Row(Modifier.fillMaxWidth().padding(top = 10.dp), horizontalArrangement = Arrangement.spacedBy(10.dp, Alignment.End)) {
                VoLumButton("Choose another file", { c.d(Action.PickFiles(PickKind.PACK_OPEN)) }, Modifier.ids("pack.import.open"))
                VoLumButton("OK", { c.d(Action.Back) }, Modifier.ids("pack.import.close"), primary = true)
            }
        }
        return
    }
    val reset = pi.merge == MergeMode.RESET
    Sheet("Import ${pi.name}", listOf("pack.import.close"), c) {
        VText(summary(pi), style = VoLumType.label(12.sp, VoLumColors.Cream))
        Gap(h = 4.dp)
        if (pi.refused) Note("This Pack needs more custom pedal slots than are left (64 in all). Delete some pedals first, then import again.", Modifier.fillMaxWidth().padding(bottom = 4.dp), alert = true, ids = listOf("pack.import.refused"))
        Row(Modifier.fillMaxWidth().weight(1f), horizontalArrangement = Arrangement.spacedBy(14.dp)) {
            Column(Modifier.weight(1f).fillMaxHeight()) {
                RowList(Pitch, Modifier.weight(1f).fillMaxWidth().ids("pack.import.rows")) {
                    pi.rows.forEach { r ->
                        val (verb, why) = outcome(pi, r)
                        TickRow(r.ticked, { c.d(Action.ToggleImportRow(r.id)) }, "pack.import.tick") {
                            VText(kindLabel(r.kind).uppercase(), Modifier.width(78.dp), style = VoLumType.label(9.sp, VoLumColors.TealDim))
                            VFitText(r.name, Modifier.weight(1f), style = VoLumType.label(13.sp, if (r.ticked) VoLumColors.TextMed else VoLumColors.CreamDim), minSize = 9.sp)
                            Column(Modifier.width(190.dp).ids("pack.import.outcome")) {
                                VText(verb, style = VoLumType.label(11.sp, if (r.outcome == PackOutcome.SKIP || !r.ticked) VoLumColors.CreamDim else VoLumColors.Amber))
                                VFitText(why, style = VoLumType.body(9.5.sp, VoLumColors.CreamDim), minSize = 7.sp)
                            }
                        }
                    }
                }
                Hint(if (reset) "Reset deletes your custom IRs, pedals and User presets first. A backup is kept." else "A backup of your library is kept before anything changes.", Modifier.padding(top = 4.dp))
            }
            Column(Modifier.width(270.dp).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(2.dp)) {
                VoLumListCaption("WHEN NAMES MATCH")
                listOf(
                    Triple(MergeMode.OVERWRITE, "Overwrite", "Pack wins, mine stay"),
                    Triple(MergeMode.ADD, "Add", "Mine stay, add new"),
                    Triple(MergeMode.RESET, "Reset", if (pi.allTicked) "Pack wins, mine deleted" else "tick every row to reset"),
                ).forEach { (m, label, desc) ->
                    val enabled = pi.everything && (m != MergeMode.RESET || pi.allTicked)
                    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        Choice(label, pi.merge == m, { c.d(Action.SetMerge(m)) }, Modifier.width(100.dp).ids("pack.import.merge.${m.name.lowercase()}")
                            .then(if (m == MergeMode.RESET && pi.merge == m) Modifier.hairlineFrame(VoLumColors.Danger, radius = 3.dp) else Modifier), enabled = enabled)
                        Hint(desc, Modifier.weight(1f), color = if (m == MergeMode.RESET) VoLumColors.Danger.withOpacity(0.85f) else VoLumColors.CreamDim)
                    }
                }
                VoLumToggle(pi.restoreMachine, { c.d(Action.SetRestoreMachine(it)) }, Modifier.ids("pack.import.machine"), label = "Restore phone settings", enabled = pi.everything)
                Fill()
                val ok = !pi.refused
                VoLumButton(if (reset) "Reset and import" else "Import", { c.d(Action.ConfirmImport) }, Modifier.fillMaxWidth().ids("pack.import.confirm").alpha(if (ok) 1f else 0.45f),
                    primary = ok, destructive = reset, enabled = ok)
            }
        }
    }
}
