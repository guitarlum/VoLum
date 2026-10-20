package com.volum.ui.c

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.Confirm
import com.volum.core.ConfirmKind
import com.volum.core.NamePurpose
import com.volum.core.Overlay
import com.volum.core.Preset
import com.volum.core.ScreenId
import com.volum.design.components.VoLumConfirmDialog
import com.volum.design.components.cornerBrackets
import com.volum.design.components.hairlineFrame
import com.volum.design.components.panelDepth
import com.volum.design.theme.VFitText
import com.volum.design.theme.VText
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumType
import com.volum.harness.screen

@Composable
internal fun PresetMenu(ui: Ui, modifier: Modifier) {
    val s = ui.s
    val presets = s.library.presetsFor(s.ampId)
    val current = s.currentPreset
    PanelFrame(ui, "Presets", "presets.menu.close", { ui.back() }, modifier, crumb = s.amp?.name) {
        val overwrite = current != null && !current.factory && s.rig.dirty
        BoxWithConstraints(Modifier.fillMaxWidth()) {
            val room = rowTitleRoom((maxWidth - ActionsW - 28.dp) / 2 - 8.dp, art = false)
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(14.dp)) {
                Column(Modifier.width(ActionsW), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                    Caption("This rig")
                    Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                        Btn("Save as new", listOf("presets.menu.save_new"), { ui.d(Action.Open(Overlay.NameDialog(NamePurpose.SAVE_PRESET, "New Preset"))) }, Modifier.weight(1f))
                        Btn("Default", listOf("presets.menu.default"), { ui.d(Action.ResetToDefault) }, Modifier.weight(1f))
                    }
                    Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                        Btn("Manage", listOf("presets.menu.manage"), { ui.d(Action.Open(Overlay.PresetManage)) }, Modifier.weight(1f))
                        if (overwrite) Btn("Overwrite", listOf("presets.menu.overwrite"), { ui.d(Action.RequestOverwritePreset(current!!.id)) }, Modifier.weight(1f), primary = true)
                        else Box(Modifier.weight(1f))
                    }
                    if (overwrite) VFitText("Overwrite stores what you hear in \u201C${current!!.name}\u201D", style = VoLumType.body(11.sp, VoLumColors.Cream), minSize = 8.sp)
                }
                SharedFitRows(presets.map { it.name to room }) {
                    Row(Modifier.weight(1f), horizontalArrangement = Arrangement.spacedBy(14.dp)) {
                        PresetList(ui, "Factory", presets.filter { it.factory }, "presets.menu.factory", Modifier.weight(1f))
                        PresetList(ui, "User", presets.filter { !it.factory }, "presets.menu.user", Modifier.weight(1f))
                    }
                }
            }
        }
    }
}

private val ActionsW = 290.dp

@Composable
private fun PresetList(ui: Ui, title: String, list: List<Preset>, id: String, modifier: Modifier) {
    SnapList(list.size + 1 + if (list.isEmpty()) 1 else 0, modifier) {
        GroupHeader("$title · ${list.size}")
        if (list.isEmpty()) Note(if (title == "User") "Nothing saved for this amp yet. Save as new keeps what you hear." else "No factory presets for this amp.", maxLines = 2)
        list.forEach { p ->
            val on = p.id == ui.s.rig.presetId
            ListRow(p.name, listOf(id), { ui.d(Action.LoadPreset(p.id)) }, Modifier.fillMaxWidth(), selected = on, sub = if (on && ui.s.rig.dirty) "LOADED  ·  CHANGED" else if (on) "LOADED" else null)
        }
    }
}

@Composable
internal fun PresetManage(ui: Ui, modifier: Modifier) {
    val s = ui.s
    val user = s.library.presets.filter { !it.factory }.sortedWith(compareBy({ it.ampId != s.ampId }, { it.name.lowercase() }))
    PanelFrame(ui, "Manage presets", "presets.manage.close", { ui.back() }, modifier, crumb = "Presets") {
        if (user.isEmpty()) Note("No User presets yet. Factory presets cannot be changed.")
        SnapList(user.size, Modifier.fillMaxWidth()) {
            user.forEach { p ->
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.CenterVertically) {
                    Column(Modifier.weight(1f)) {
                        VFitText(p.name, style = VoLumType.label(14.sp, VoLumColors.TextBright), minSize = 10.sp)
                        VText(s.library.amp(p.ampId)?.name ?: "", style = VoLumType.label(9.sp, VoLumColors.TealDim))
                    }
                    Btn("Overwrite", listOf("presets.manage.overwrite"), { ui.d(Action.RequestOverwritePreset(p.id)) })
                    Btn("Rename", listOf("presets.manage.rename"), { ui.d(Action.RequestRenamePreset(p.id)) })
                    Gap(w = 12.dp)
                    Btn("Delete", listOf("presets.manage.delete"), { ui.d(Action.RequestDeletePreset(p.id)) }, danger = true)
                }
            }
        }
    }
}

/** Name box: high on the screen so the keyboard leaves it readable; the commit button names what it will do. */
@Composable
internal fun NameDialog(ui: Ui, o: Overlay.NameDialog) {
    val s = ui.s
    val title = when (o.purpose) {
        NamePurpose.SAVE_PRESET -> "Save preset"
        NamePurpose.ADD_SOUND -> "Name this Sound"
        NamePurpose.RENAME_PRESET -> "Rename preset"
        NamePurpose.RENAME_IR -> "Rename IR"
        NamePurpose.RENAME_PEDAL -> "Rename pedal"
    }
    val current = s.currentPreset
    val updates = o.purpose == NamePurpose.SAVE_PRESET && current != null && !current.factory && current.name.equals(o.text.trim(), ignoreCase = true)
    val (commit, commitId) = when {
        o.purpose == NamePurpose.RENAME_PRESET || o.purpose == NamePurpose.RENAME_IR || o.purpose == NamePurpose.RENAME_PEDAL -> "Rename" to "name.rename"
        updates -> "Update" to "name.update"
        else -> "Save" to "name.save"
    }
    val src = remember { MutableInteractionSource() }
    Box(Modifier.fillMaxSize(), contentAlignment = Alignment.TopCenter) {
        Box(Modifier.fillMaxSize().background(VoLumColors.Scrim).clickable(src, null) { ui.back() })
        Column(
            Modifier
                .padding(top = 14.dp)
                .width(720.dp)
                .pointerInput(Unit) { detectTapGestures { } }
                .screen(ScreenId.DIALOG_NAME)
                .panelDepth(6.dp)
                .hairlineFrame(VoLumColors.Amber, 1.4.dp, 6.dp)
                .cornerBrackets(VoLumColors.Corner, 10.dp, 5.dp)
                .padding(start = 22.dp, end = 22.dp, top = 12.dp, bottom = 14.dp),
            verticalArrangement = Arrangement.spacedBy(10.dp),
        ) {
            VText(title, style = VoLumType.display(22.sp, VoLumColors.Gold))
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(10.dp), verticalAlignment = Alignment.CenterVertically) {
                Field(o.text, listOf("name.field"), { ui.d(Action.SetNameText(it)) }, Modifier.weight(1f), onDone = { ui.d(Action.CommitName) }, hint = "Name", autofocus = true)
                Btn("Cancel", listOf("name.cancel"), { ui.back() })
                Btn(commit, listOf(commitId), { ui.d(Action.CommitName) }, primary = true, enabled = o.text.isNotBlank())
            }
            Note(
                when {
                    updates -> "Same name as the loaded preset: Update overwrites it."
                    o.purpose == NamePurpose.SAVE_PRESET && current?.factory == true -> "Factory presets stay untouched; this saves a new User preset."
                    o.purpose == NamePurpose.ADD_SOUND -> "Saves a User preset and adds it as the next free Sound."
                    else -> "Names are kept short so they fit everywhere."
                },
                maxLines = 2,
            )
        }
    }
}

/** The `:design` confirm dialog: red for Delete, amber for Overwrite, Cancel first. */
@Composable
internal fun ConfirmDialog(ui: Ui, c: Confirm) {
    val overwrite = c.kind == ConfirmKind.OVERWRITE_PRESET
    VoLumConfirmDialog(
        if (overwrite) "Overwrite preset" else "Delete",
        c.message + if (overwrite) " The preset takes what you hear now." else " This cannot be undone; anything that used it falls back to a stock choice.",
        if (overwrite) "Overwrite" else "Delete",
        { ui.d(Action.AcceptConfirm) },
        { ui.d(Action.CancelConfirm) },
        Modifier.ids("dialog.confirm.message"),
        confirmModifier = Modifier.ids("dialog.confirm.accept"),
        cancelModifier = Modifier.ids("dialog.confirm.cancel"),
    )
}
