package com.volum.ui.a

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.NamePurpose
import com.volum.core.Overlay
import com.volum.design.components.VoLumButton
import com.volum.design.components.VoLumGlyph
import com.volum.design.components.VoLumListCaption
import com.volum.design.components.VoLumListRow
import com.volum.design.components.VoLumTextField
import com.volum.design.theme.VText
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumType

private val Pitch = 48.dp
private val ActionPitch = 50.dp

@Composable
internal fun PresetMenu(c: Ctx) {
    val s = c.s
    val presets = s.library.presetsFor(s.ampId)
    val current = s.currentPreset
    val rows = buildList {
        listOf(true, false).forEach { factory ->
            val group = presets.filter { it.factory == factory }
            if (group.isNotEmpty()) { add(null to factory); group.forEach { add(it to factory) } }
        }
    }
    Sheet("Presets  ·  ${s.amp?.name ?: ""}", listOf("presets.menu.close"), c, width = 780.dp, align = Alignment.TopCenter) {
        TwoColumns(
            left = {
                RowList(Pitch, Modifier.weight(1f).fillMaxWidth(), scrollToRow = rows.indexOfFirst { it.first?.id == s.rig.presetId } - 2) {
                    rows.forEach { (p, factory) ->
                        if (p == null) {
                            Box(Modifier.height(Pitch), contentAlignment = Alignment.BottomStart) { VoLumListCaption(if (factory) "FACTORY" else "USER") }
                        } else {
                            val selected = p.id == s.rig.presetId
                            VoLumListRow(p.name, selected, { c.d(Action.LoadPreset(p.id)) }, Modifier.fillMaxWidth().height(Pitch).ids(if (factory) "presets.menu.factory" else "presets.menu.user"),
                                subtitle = when {
                                    selected && s.rig.dirty -> "LOADED  ·  UNSAVED CHANGES"
                                    selected -> "LOADED"
                                    else -> null
                                })
                        }
                    }
                }
            },
            right = {
                VoLumButton("Save as new", { c.d(Action.Open(Overlay.NameDialog(NamePurpose.SAVE_PRESET, "New Preset"))) }, Modifier.fillMaxWidth().ids("presets.menu.save_new"), primary = true)
                if (current != null && !current.factory && s.rig.dirty) {
                    VoLumButton("Overwrite \"${current.name}\"", { c.d(Action.RequestOverwritePreset(current.id)) }, Modifier.fillMaxWidth().ids("presets.menu.overwrite"))
                }
                VoLumButton("Manage presets", { c.d(Action.Open(Overlay.PresetManage)) }, Modifier.fillMaxWidth().ids("presets.menu.manage"))
                Fill()
                VoLumButton("Default", { c.d(Action.ResetToDefault) }, Modifier.fillMaxWidth().ids("presets.menu.default"))
                Hint("Default puts every knob, pedal and cab of this amp back to the start.", lines = 3)
            },
            rightWidth = 260.dp,
        )
    }
}

@Composable
internal fun PresetManage(c: Ctx) {
    val s = c.s
    val user = s.library.presetsFor(s.ampId).filter { !it.factory }
    Sheet("Manage presets  ·  ${s.amp?.name ?: ""}", listOf("presets.manage.close"), c, width = 760.dp, fill = user.size > 4) {
        if (user.isEmpty()) {
            Note("No User presets for this amp yet. SAVE in the header stores the current sound as one.")
            return@Sheet
        }
        RowList(ActionPitch, if (user.size > 4) Modifier.weight(1f).fillMaxWidth() else Modifier.fillMaxWidth()) {
            user.forEach { p ->
                VoLumListRow(p.name, p.id == s.rig.presetId, { c.d(Action.LoadPreset(p.id)) }, Modifier.fillMaxWidth().height(ActionPitch).ids("presets.menu.user"),
                    subtitle = if (p.id == s.rig.presetId) "LOADED" else null) {
                    GlyphAction(VoLumGlyph.Store, "OVERWRITE", { c.d(Action.RequestOverwritePreset(p.id)) }, Modifier.ids("presets.manage.overwrite"))
                    GlyphAction(VoLumGlyph.Edit, "RENAME", { c.d(Action.RequestRenamePreset(p.id)) }, Modifier.ids("presets.manage.rename"))
                    GlyphAction(VoLumGlyph.Delete, "DELETE", { c.d(Action.RequestDeletePreset(p.id)) }, Modifier.ids("presets.manage.delete"), tint = VoLumColors.Danger)
                }
            }
        }
    }
}

private fun primaryLabel(c: Ctx, o: Overlay.NameDialog): String = when (o.purpose) {
    NamePurpose.SAVE_PRESET -> c.s.currentPreset?.takeIf { !it.factory && it.name.equals(o.text.trim(), ignoreCase = true) }?.let { "Update" } ?: "Save"
    NamePurpose.ADD_SOUND -> "Save and add"
    else -> "Rename"
}

private fun title(p: NamePurpose) = when (p) {
    NamePurpose.SAVE_PRESET -> "Save preset"
    NamePurpose.ADD_SOUND -> "Name this sound"
    NamePurpose.RENAME_PRESET -> "Rename preset"
    NamePurpose.RENAME_IR -> "Rename IR"
    NamePurpose.RENAME_PEDAL -> "Rename pedal"
}

private fun explain(c: Ctx, o: Overlay.NameDialog, primary: String): String = when (o.purpose) {
    NamePurpose.SAVE_PRESET -> if (primary == "Update") "Same name as the loaded preset: Update stores your changes in it." else "Saved as a User preset of ${c.s.amp?.name ?: "this amp"}."
    NamePurpose.ADD_SOUND -> "The sound is saved as a preset, then added to your Sounds."
    else -> "Names must be unique."
}

@Composable
internal fun NameDialog(c: Ctx, o: Overlay.NameDialog) {
    val primary = primaryLabel(c, o)
    Sheet(title(o.purpose), emptyList(), c, width = 520.dp, align = Alignment.TopCenter, fill = false) {
        Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
            VText("NAME", style = VoLumType.label(10.sp, VoLumColors.GoldDim))
            VoLumTextField(
                o.text, { c.d(Action.SetNameText(it)) }, Modifier.fillMaxWidth().ids("name.field"),
                placeholder = "Preset name",
                keyboardOptions = KeyboardOptions(imeAction = ImeAction.Done),
                keyboardActions = KeyboardActions(onDone = { c.d(Action.CommitName) }),
            )
            Hint(explain(c, o, primary))
            Row(Modifier.fillMaxWidth().padding(top = 4.dp), horizontalArrangement = Arrangement.spacedBy(10.dp, Alignment.End)) {
                VoLumButton("Cancel", { c.d(Action.Back) }, Modifier.ids("name.cancel"))
                val ok = o.text.isNotBlank()
                VoLumButton(primary, { c.d(Action.CommitName) }, Modifier.ids("name.save", "name.update", "name.rename").then(if (ok) Modifier else Modifier.alpha(0.45f)), primary = ok, enabled = ok)
            }
        }
    }
}
