package com.volum.ui.b

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
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.NamePurpose
import com.volum.core.Overlay
import com.volum.core.ScreenId
import com.volum.core.VoLumState
import com.volum.harness.screen

@Composable
internal fun PresetMenu(s: VoLumState) {
    val st = LocalStage.current
    val current = s.currentPreset
    val user = current?.takeIf { !it.factory }
    val presets = s.library.presetsFor(s.ampId)
    Panel(ScreenId.PRESETS_MENU, "Presets", arrayOf("presets.menu.close"),
        subtitle = "${s.amp?.name ?: ""} · playing ${current?.name ?: "Default"}${if (s.rig.dirty) " (unsaved)" else ""}") {
        Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            Column(Modifier.width(230.dp).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                TextPad("DEFAULT", { st.go(Action.ResetToDefault) }, Modifier.fillMaxWidth().height(56.dp), arrayOf("presets.menu.default"),
                    selected = current == null, sel = Sel.LIST_TEAL)
                TextPad("SAVE AS NEW…", { st.go(Action.Open(Overlay.NameDialog(NamePurpose.SAVE_PRESET, "New Preset"))) }, Modifier.fillMaxWidth().height(56.dp),
                    arrayOf("presets.menu.save_new"))
                TextPad("MANAGE…", { st.go(Action.Open(Overlay.PresetManage)) }, Modifier.fillMaxWidth().height(56.dp), arrayOf("presets.menu.manage"))
                TextPad(if (user != null) "OVERWRITE ${user.name}" else "OVERWRITE", { user?.let { st.go(Action.RequestOverwritePreset(it.id)) } },
                    Modifier.fillMaxWidth().height(56.dp), arrayOf("presets.menu.overwrite"), enabled = user != null)
            }
            Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                Caption("FACTORY", Modifier.fillMaxWidth())
                FitGrid(presets.filter { it.factory }, 1, 52.dp, Modifier.fillMaxWidth().weight(1f), empty = "No Factory presets for this amp.") { p ->
                    PresetRow(s, p, current?.id == p.id, arrayOf("presets.menu.factory")) { st.go(Action.LoadPreset(p.id)) }
                }
            }
            Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                Caption("USER", Modifier.fillMaxWidth())
                FitGrid(presets.filter { !it.factory }, 1, 52.dp, Modifier.fillMaxWidth().weight(1f), empty = "SAVE AS NEW keeps this rig as a User preset.") { p ->
                    PresetRow(s, p, current?.id == p.id, arrayOf("presets.menu.user")) { st.go(Action.LoadPreset(p.id)) }
                }
            }
        }
    }
}

@Composable
internal fun PresetManage(s: VoLumState) {
    val st = LocalStage.current
    val users = s.library.presetsFor(s.ampId).filter { !it.factory }
    Panel(ScreenId.PRESETS_MANAGE, "Manage presets", arrayOf("presets.manage.close"), subtitle = "User presets on ${s.amp?.name ?: ""}") {
        FitGrid(users, 1, 56.dp, Modifier.fillMaxSize(), empty = "No User presets on this amp yet.") { p ->
            Row(Modifier.fillMaxSize().clip(RoundedCornerShape(6.dp)).background(Ink.panel).padding(start = 12.dp, end = 4.dp, top = 4.dp, bottom = 4.dp),
                verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                FitText(p.name, Type.display(18.sp), Modifier.weight(1f), align = TextAlign.Start)
                TextPad("OVERWRITE", { st.go(Action.RequestOverwritePreset(p.id)) }, Modifier.width(120.dp).fillMaxHeight(), arrayOf("presets.manage.overwrite"))
                TextPad("RENAME", { st.go(Action.RequestRenamePreset(p.id)) }, Modifier.width(100.dp).fillMaxHeight(), arrayOf("presets.manage.rename"))
                TextPad("DELETE", { st.go(Action.RequestDeletePreset(p.id)) }, Modifier.width(100.dp).fillMaxHeight(), arrayOf("presets.manage.delete"),
                    style = Type.label(13.sp, Ink.alert))
            }
        }
    }
}

/** Name box for saving, adding a Sound and renaming; the commit button says what it will do. */
@Composable
internal fun NameDialog(s: VoLumState, o: Overlay.NameDialog) {
    val st = LocalStage.current
    val user = s.currentPreset?.takeIf { !it.factory }
    val updates = o.purpose == NamePurpose.SAVE_PRESET && user != null && user.name.equals(o.text.trim(), ignoreCase = true)
    val (title, verb, ids) = when (o.purpose) {
        NamePurpose.SAVE_PRESET -> Triple("Save preset", if (updates) "UPDATE" else "SAVE", arrayOf(if (updates) "name.update" else "name.save"))
        NamePurpose.ADD_SOUND -> Triple("Name this Sound", "ADD SOUND", arrayOf("name.save"))
        NamePurpose.RENAME_PRESET -> Triple("Rename preset", "RENAME", arrayOf("name.rename"))
        NamePurpose.RENAME_IR -> Triple("Rename IR", "RENAME", arrayOf("name.rename"))
        NamePurpose.RENAME_PEDAL -> Triple("Rename pedal", "RENAME", arrayOf("name.rename"))
    }
    val hint = when {
        updates -> "Same name as the loaded preset: it is updated in place."
        o.purpose == NamePurpose.SAVE_PRESET -> "Saved as a new User preset on ${s.amp?.name ?: "this amp"}."
        o.purpose == NamePurpose.ADD_SOUND -> "Saved as a preset and added to the next free program."
        else -> "Names stay unique; a number is added if needed."
    }
    Box(Modifier.fillMaxSize().background(Ink.scrim).swallowTouches().screen(ScreenId.DIALOG_NAME), contentAlignment = Alignment.TopCenter) {
        Column(
            Modifier.padding(top = 24.dp).widthIn(max = 560.dp).fillMaxWidth(0.62f).clip(RoundedCornerShape(12.dp)).background(Ink.panel).brackets().padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(10.dp),
        ) {
            FitText(title, Type.display(24.sp), Modifier.fillMaxWidth().height(32.dp), align = TextAlign.Start)
            Field(o.text, { st.go(Action.SetNameText(it)) }, Modifier.fillMaxWidth().height(56.dp), arrayOf("name.field"), placeholder = "Name",
                onDone = { st.go(Action.CommitName) })
            FitText(hint, Type.value(12.sp, Ink.dim), Modifier.fillMaxWidth().height(18.dp), align = TextAlign.Start)
            Row(Modifier.fillMaxWidth().height(56.dp), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                TextPad("CANCEL", { st.go(Action.Back) }, Modifier.weight(1f).fillMaxHeight(), arrayOf("name.cancel"))
                TextPad(verb, { st.go(Action.CommitName) }, Modifier.weight(1f).fillMaxHeight(), ids, enabled = o.text.isNotBlank(), selected = true,
                    style = Type.label(14.sp, Ink.hot))
            }
        }
    }
}
