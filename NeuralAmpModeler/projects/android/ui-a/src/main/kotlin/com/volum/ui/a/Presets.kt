package com.volum.ui.a

import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.NamePurpose
import com.volum.core.Overlay

@Composable
internal fun PresetMenu(c: Ctx) {
    val s = c.s
    val presets = s.library.presetsFor(s.ampId)
    val current = s.currentPreset
    Sheet("Presets · ${s.amp?.name ?: ""}", listOf("presets.menu.close"), c, width = 760.dp, align = Alignment.TopCenter) {
        TwoColumns(
            left = {
                RowList(48.dp, Modifier.weight(1f).fillMaxWidth()) {
                    listOf(true, false).forEach { factory ->
                        val group = presets.filter { it.factory == factory }
                        if (group.isEmpty()) return@forEach
                        Box(Modifier.height(48.dp), contentAlignment = Alignment.BottomStart) { SectionLabel(if (factory) "FACTORY" else "USER") }
                        group.forEach { p ->
                            val selected = p.id == s.rig.presetId
                            ListRow({ c.d(Action.LoadPreset(p.id)) }, ids = listOf(if (factory) "presets.menu.factory" else "presets.menu.user"), selected = selected) {
                                Row(verticalAlignment = Alignment.CenterVertically) {
                                    T(p.name, Type.value(14.sp, if (selected) Ink.selected else Ink.text), Modifier.weight(1f))
                                    if (selected && s.rig.dirty) T("UNSAVED", Type.label(10.sp))
                                }
                            }
                        }
                    }
                }
            },
            right = {
                Chip("DEFAULT", { c.d(Action.ResetToDefault) }, Modifier.fillMaxWidth(), listOf("presets.menu.default"))
                if (current != null && !current.factory && s.rig.dirty) {
                    Chip("OVERWRITE \"${current.name}\"", { c.d(Action.RequestOverwritePreset(current.id)) }, Modifier.fillMaxWidth(), listOf("presets.menu.overwrite"))
                }
                Chip("SAVE AS NEW", { c.d(Action.Open(Overlay.NameDialog(NamePurpose.SAVE_PRESET, "New Preset"))) }, Modifier.fillMaxWidth(), listOf("presets.menu.save_new"))
                Chip("MANAGE PRESETS", { c.d(Action.Open(Overlay.PresetManage)) }, Modifier.fillMaxWidth(), listOf("presets.menu.manage"))
            },
            rightWidth = 260.dp,
        )
    }
}

@Composable
internal fun PresetManage(c: Ctx) {
    val s = c.s
    val user = s.library.presetsFor(s.ampId).filter { !it.factory }
    Sheet("Manage presets · ${s.amp?.name ?: ""}", listOf("presets.manage.close"), c, width = 760.dp) {
        RowList(48.dp, Modifier.weight(1f).fillMaxWidth()) {
            user.forEach { p ->
                Row(Modifier.fillMaxWidth().height(48.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    T(p.name, Type.value(14.sp), Modifier.weight(1f).padding(start = 6.dp))
                    Chip("OVERWRITE", { c.d(Action.RequestOverwritePreset(p.id)) }, ids = listOf("presets.manage.overwrite"))
                    Chip("RENAME", { c.d(Action.RequestRenamePreset(p.id)) }, ids = listOf("presets.manage.rename"))
                    Chip("DELETE", { c.d(Action.RequestDeletePreset(p.id)) }, ids = listOf("presets.manage.delete"))
                }
            }
        }
        if (user.isEmpty()) Notice("No User presets for this amp yet. SAVE stores the current sound as one.", Modifier.padding(bottom = 8.dp))
    }
}

private fun primaryLabel(c: Ctx, o: Overlay.NameDialog): String = when (o.purpose) {
    NamePurpose.SAVE_PRESET -> c.s.currentPreset?.takeIf { !it.factory && it.name.equals(o.text.trim(), ignoreCase = true) }?.let { "UPDATE" } ?: "SAVE"
    NamePurpose.ADD_SOUND -> "SAVE AND ADD"
    else -> "RENAME"
}

private fun title(p: NamePurpose) = when (p) {
    NamePurpose.SAVE_PRESET -> "Save preset"
    NamePurpose.ADD_SOUND -> "Name this sound"
    NamePurpose.RENAME_PRESET -> "Rename preset"
    NamePurpose.RENAME_IR -> "Rename IR"
    NamePurpose.RENAME_PEDAL -> "Rename pedal"
}

@Composable
internal fun NameDialog(c: Ctx, o: Overlay.NameDialog) {
    Sheet(title(o.purpose), emptyList(), c, width = 520.dp, align = Alignment.TopCenter, fullHeight = false) {
        Column(Modifier.padding(bottom = 10.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
            NameField(o.text, { c.d(Action.SetNameText(it)) }, { c.d(Action.CommitName) }, listOf("name.field"))
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(10.dp, Alignment.End)) {
                Chip("CANCEL", { c.d(Action.Back) }, ids = listOf("name.cancel"))
                Chip(primaryLabel(c, o), { c.d(Action.CommitName) }, ids = listOf("name.save", "name.update", "name.rename"), selected = true, enabled = o.text.isNotBlank())
            }
        }
    }
}

/** Single-line text entry, 48 dp tall; `:design` replaces the field drawing. */
@Composable
internal fun NameField(text: String, onChange: (String) -> Unit, onDone: () -> Unit, ids: List<String>, modifier: Modifier = Modifier) {
    BasicTextField(
        text,
        onChange,
        modifier.fillMaxWidth().height(48.dp).ids(ids).border(1.dp, Ink.dim, Corner),
        textStyle = Type.value(16.sp),
        singleLine = true,
        cursorBrush = SolidColor(Ink.selected),
        keyboardOptions = KeyboardOptions(imeAction = ImeAction.Done),
        keyboardActions = KeyboardActions(onDone = { onDone() }),
        decorationBox = { inner -> Box(Modifier.padding(horizontal = 12.dp), contentAlignment = Alignment.CenterStart) { inner() } },
    )
}
