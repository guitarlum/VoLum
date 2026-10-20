package com.volum.ui.c

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.BasicText
import androidx.compose.foundation.verticalScroll
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import com.volum.core.Action
import com.volum.core.Confirm
import com.volum.core.ConfirmKind
import com.volum.core.NamePurpose
import com.volum.core.Overlay
import com.volum.core.ScreenId
import com.volum.harness.screen

@Composable
internal fun PresetMenu(ui: Ui, modifier: Modifier) {
    val s = ui.s
    val presets = s.library.presetsFor(s.ampId)
    val current = s.currentPreset
    PanelFrame("Presets · ${s.amp?.name ?: ""}", null, "presets.menu.close", { ui.back() }, modifier) {
        Row(Modifier.fillMaxWidth().fillMaxHeight(), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            Column(Modifier.width(230.dp).fillMaxHeight().verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                Caption("This rig")
                if (current != null && !current.factory && s.rig.dirty) {
                    Btn("OVERWRITE “${current.name}”", listOf("presets.menu.overwrite"), { ui.d(Action.RequestOverwritePreset(current.id)) }, strong = true, modifier = Modifier.fillMaxWidth())
                }
                Btn("SAVE AS NEW…", listOf("presets.menu.save_new"), { ui.d(Action.Open(Overlay.NameDialog(NamePurpose.SAVE_PRESET, "New Preset"))) }, modifier = Modifier.fillMaxWidth())
                Btn("DEFAULT", listOf("presets.menu.default"), { ui.d(Action.ResetToDefault) }, sub = "RESET THIS AMP'S RIG", modifier = Modifier.fillMaxWidth())
                Btn("MANAGE…", listOf("presets.menu.manage"), { ui.d(Action.Open(Overlay.PresetManage)) }, sub = "RENAME · OVERWRITE · DELETE", modifier = Modifier.fillMaxWidth())
            }
            PresetList(ui, "Factory", presets.filter { it.factory }, "presets.menu.factory", Modifier.weight(1f))
            PresetList(ui, "User", presets.filter { !it.factory }, "presets.menu.user", Modifier.weight(1f))
        }
    }
}

@Composable
private fun PresetList(ui: Ui, title: String, list: List<com.volum.core.Preset>, id: String, modifier: Modifier) {
    Column(modifier.fillMaxHeight().verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(4.dp)) {
        Caption("$title · ${list.size}")
        if (list.isEmpty()) Note(if (title == "User") "Nothing saved for this amp yet. SAVE AS NEW keeps what you hear." else "No factory presets for this amp.")
        list.forEach { p ->
            val on = p.id == ui.s.rig.presetId
            ListRow(p.name, listOf(id), { ui.d(Action.LoadPreset(p.id)) }, Modifier.fillMaxWidth(), selected = on, sub = if (on && ui.s.rig.dirty) "LOADED · CHANGED" else if (on) "LOADED" else null)
        }
    }
}

@Composable
internal fun PresetManage(ui: Ui, modifier: Modifier) {
    val s = ui.s
    val user = s.library.presets.filter { !it.factory }.sortedWith(compareBy({ it.ampId != s.ampId }, { it.name.lowercase() }))
    PanelFrame("Manage presets", "PRESETS ▾", "presets.manage.close", { ui.back() }, modifier) {
        Column(Modifier.fillMaxWidth().verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(4.dp)) {
            if (user.isEmpty()) Note("No User presets yet. Factory presets cannot be changed.")
            user.forEach { p ->
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
                    Column(Modifier.weight(1f)) {
                        BasicText(p.name, style = bodyStyle(14f), maxLines = 1, overflow = TextOverflow.Ellipsis)
                        BasicText(s.library.amp(p.ampId)?.name ?: "", style = labelStyle(9f, Look.dim, bold = false), maxLines = 1)
                    }
                    Btn("OVERWRITE", listOf("presets.manage.overwrite"), { ui.d(Action.RequestOverwritePreset(p.id)) }, sub = "WITH THIS RIG")
                    Btn("RENAME", listOf("presets.manage.rename"), { ui.d(Action.RequestRenamePreset(p.id)) })
                    Btn("DELETE", listOf("presets.manage.delete"), { ui.d(Action.RequestDeletePreset(p.id)) }, danger = true)
                }
            }
        }
    }
}

/** Name box: sits high so the keyboard leaves it readable; the commit button names what it will do. */
@Composable
internal fun NameDialog(ui: Ui, o: Overlay.NameDialog) {
    val s = ui.s
    val title = when (o.purpose) {
        NamePurpose.SAVE_PRESET -> "Save preset"
        NamePurpose.ADD_SOUND -> "Name this sound"
        NamePurpose.RENAME_PRESET -> "Rename preset"
        NamePurpose.RENAME_IR -> "Rename IR"
        NamePurpose.RENAME_PEDAL -> "Rename pedal"
    }
    val current = s.currentPreset
    val updates = o.purpose == NamePurpose.SAVE_PRESET && current != null && !current.factory && current.name.equals(o.text.trim(), ignoreCase = true)
    val (commit, commitId) = when {
        o.purpose == NamePurpose.RENAME_PRESET || o.purpose == NamePurpose.RENAME_IR || o.purpose == NamePurpose.RENAME_PEDAL -> "RENAME" to "name.rename"
        updates -> "UPDATE" to "name.update"
        else -> "SAVE" to "name.save"
    }
    Modal(ui, ScreenId.DIALOG_NAME, width = 700.dp) {
        Caption(title)
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.CenterVertically) {
            Field(o.text, listOf("name.field"), { ui.d(Action.SetNameText(it)) }, Modifier.weight(1f), onDone = { ui.d(Action.CommitName) }, hint = "Name", autofocus = true)
            Btn("CANCEL", listOf("name.cancel"), { ui.back() })
            Btn(commit, listOf(commitId), { ui.d(Action.CommitName) }, strong = true, enabled = o.text.isNotBlank(), minWidth = 110.dp)
        }
        Note(
            when {
                updates -> "Same name as the loaded preset: UPDATE overwrites it."
                o.purpose == NamePurpose.SAVE_PRESET && current?.factory == true -> "Factory presets stay untouched; this saves a new User preset."
                o.purpose == NamePurpose.ADD_SOUND -> "Saves a User preset and adds it as the next free Sound."
                else -> " "
            },
            maxLines = 1,
        )
    }
}

@Composable
internal fun ConfirmDialog(ui: Ui, c: Confirm) {
    val verb = when (c.kind) {
        ConfirmKind.OVERWRITE_PRESET -> "OVERWRITE"
        else -> "DELETE"
    }
    Modal(ui, null, onScrim = { ui.d(Action.CancelConfirm) }) {
        Caption(if (c.kind == ConfirmKind.OVERWRITE_PRESET) "Overwrite" else "Delete")
        BasicText(c.message, style = bodyStyle(17f), maxLines = 2, overflow = TextOverflow.Ellipsis, modifier = Modifier.ids("dialog.confirm.message"))
        Note(if (c.kind == ConfirmKind.OVERWRITE_PRESET) "The preset takes what you hear now." else "This cannot be undone. Anything that used it falls back to a stock choice.")
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp, Alignment.End)) {
            Btn("CANCEL", listOf("dialog.confirm.cancel"), { ui.d(Action.CancelConfirm) })
            Btn(verb, listOf("dialog.confirm.accept"), { ui.d(Action.AcceptConfirm) }, strong = verb != "DELETE", danger = verb == "DELETE", minWidth = 120.dp)
        }
    }
}

@Composable
private fun Modal(ui: Ui, screen: ScreenId?, onScrim: () -> Unit = { ui.back() }, width: androidx.compose.ui.unit.Dp = 520.dp, content: @Composable () -> Unit) {
    val src = remember { MutableInteractionSource() }
    Box(Modifier.fillMaxSize(), contentAlignment = Alignment.TopCenter) {
        Box(Modifier.fillMaxSize().background(Look.ground.copy(alpha = 0.6f)).clickable(src, null, onClick = onScrim))
        Glass(
            Modifier
                .padding(top = 12.dp)
                .width(width)
                .pointerInput(Unit) { detectTapGestures { } }
                .then(if (screen != null) Modifier.screen(screen) else Modifier),
            fill = Look.glassHi,
            border = Look.lineHi,
        ) {
            Column(Modifier.padding(16.dp).brackets(), verticalArrangement = Arrangement.spacedBy(10.dp)) { content() }
        }
    }
}
