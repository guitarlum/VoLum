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
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.NamePurpose
import com.volum.core.Overlay
import com.volum.core.ScreenId
import com.volum.core.VoLumState
import com.volum.design.components.VoLumButton
import com.volum.design.components.VoLumGlyph
import com.volum.design.components.cornerBrackets
import com.volum.design.components.hairlineFrame
import com.volum.design.components.panelDepth
import com.volum.design.components.presetBarInk
import com.volum.design.components.presetBarLabel
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumType
import com.volum.design.theme.withOpacity
import com.volum.harness.screen

@Composable
internal fun PresetMenu(s: VoLumState) {
    val st = LocalStage.current
    val current = s.currentPreset
    val user = current?.takeIf { !it.factory }
    val presets = s.library.presetsFor(s.ampId)
    val factory = presets.filter { it.factory }
    val users = presets.filter { !it.factory }
    Panel(s, ScreenId.PRESETS_MENU, "Presets", arrayOf("presets.menu.close"), subtitle = "on ${s.amp?.name ?: "this amp"}") {
        Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(14.dp)) {
            Column(Modifier.width(236.dp).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                SectionTitle("PLAYING", Modifier.fillMaxWidth())
                FitText(presetBarLabel(current?.name, s.rig.dirty), VoLumType.label(14.sp, presetBarInk(current?.name, s.rig.dirty)), Modifier.fillMaxWidth().height(22.dp), align = TextAlign.Start)
                TextPad("SAVE AS NEW PRESET…", { st.go(Action.Open(Overlay.NameDialog(NamePurpose.SAVE_PRESET, "New Preset"))) }, Modifier.fillMaxWidth().height(52.dp),
                    arrayOf("presets.menu.save_new"), tone = Tone.PRIMARY)
                TextPad("RENAME / DELETE…", { st.go(Action.Open(Overlay.PresetManage)) }, Modifier.fillMaxWidth().height(52.dp), arrayOf("presets.menu.manage"))
                TextPad(if (user != null) "OVERWRITE \"${user.name}\"" else "OVERWRITE (load a User preset)", { user?.let { st.go(Action.RequestOverwritePreset(it.id)) } },
                    Modifier.fillMaxWidth().height(52.dp), arrayOf("presets.menu.overwrite"), enabled = user != null)
            }
            Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                SectionTitle("DEFAULT", Modifier.fillMaxWidth())
                Box(Modifier.fillMaxWidth().height(52.dp)) {
                    ListRow("Default", current == null, { st.go(Action.ResetToDefault) }, arrayOf("presets.menu.default"), subtitle = "factory settings, no preset")
                }
                SectionTitle("FACTORY", Modifier.fillMaxWidth())
                FitGrid(factory, 1, 52.dp, Modifier.fillMaxWidth().weight(1f), gap = 4.dp, empty = "No Factory presets for this amp.",
                    reveal = factory.indexOfFirst { it.id == current?.id }) { p ->
                    PresetRow(s, p, current?.id == p.id, arrayOf("presets.menu.factory")) { st.go(Action.LoadPreset(p.id)) }
                }
            }
            Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                SectionTitle("USER", Modifier.fillMaxWidth())
                FitGrid(users, 1, 52.dp, Modifier.fillMaxWidth().weight(1f), gap = 4.dp, empty = "SAVE AS NEW PRESET keeps this rig as a User preset.",
                    reveal = users.indexOfFirst { it.id == current?.id }) { p ->
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
    Panel(s, ScreenId.PRESETS_MANAGE, "Manage presets", arrayOf("presets.manage.close"), subtitle = "User presets on ${s.amp?.name ?: "this amp"}",
        bodyHeight = (users.size.coerceIn(2, 5) * 64 + 8).dp) {
        FitGrid(users, 1, 58.dp, Modifier.fillMaxSize(), empty = "No User presets on this amp yet. SAVE PRESET in the header keeps one.") { p ->
            val group = LocalGridGroup.current
            val tools = rememberFitGroup()
            Row(Modifier.fillMaxSize().panelDepth(4.dp).hairlineFrame(VoLumColors.Frame, radius = 4.dp).padding(start = 14.dp, end = 4.dp, top = 2.dp, bottom = 2.dp),
                verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                Column(Modifier.weight(1f)) {
                    FitText(p.name, VoLumType.display(19.sp, VoLumColors.Cream), Modifier.fillMaxWidth().height(26.dp), align = TextAlign.Start, group = group)
                    FitText(s.library.amp(p.ampId)?.name ?: "", VoLumType.label(10.sp, VoLumColors.TealDim), Modifier.fillMaxWidth().height(14.dp), align = TextAlign.Start)
                }
                GlyphPad(VoLumGlyph.Store, "OVERWRITE", { st.go(Action.RequestOverwritePreset(p.id)) }, arrayOf("presets.manage.overwrite"), width = 96.dp, group = tools)
                GlyphPad(VoLumGlyph.Edit, "RENAME", { st.go(Action.RequestRenamePreset(p.id)) }, arrayOf("presets.manage.rename"), width = 88.dp, group = tools)
                GlyphPad(VoLumGlyph.Delete, "DELETE", { st.go(Action.RequestDeletePreset(p.id)) }, arrayOf("presets.manage.delete"), danger = true, width = 88.dp, group = tools)
            }
        }
    }
}

/** Name box for saving, adding a Sound and renaming (the desktop name dialog); the commit button says what it will do. */
@Composable
internal fun NameDialog(s: VoLumState, o: Overlay.NameDialog) {
    val st = LocalStage.current
    val user = s.currentPreset?.takeIf { !it.factory }
    val updates = o.purpose == NamePurpose.SAVE_PRESET && user != null && user.name.equals(o.text.trim(), ignoreCase = true)
    val (title, verb, ids) = when (o.purpose) {
        NamePurpose.SAVE_PRESET -> Triple("Save preset", if (updates) "Update" else "Save", arrayOf(if (updates) "name.update" else "name.save"))
        NamePurpose.ADD_SOUND -> Triple("Name this Sound", "Add Sound", arrayOf("name.save"))
        NamePurpose.RENAME_PRESET -> Triple("Rename preset", "Rename", arrayOf("name.rename"))
        NamePurpose.RENAME_IR -> Triple("Rename IR", "Rename", arrayOf("name.rename"))
        NamePurpose.RENAME_PEDAL -> Triple("Rename pedal", "Rename", arrayOf("name.rename"))
    }
    val note = when {
        updates -> "Same name as the loaded preset: it is updated in place."
        o.purpose == NamePurpose.SAVE_PRESET -> "Saved as a new User preset on ${s.amp?.name ?: "this amp"}."
        o.purpose == NamePurpose.ADD_SOUND -> "Saved as a preset and added to the next free program."
        else -> "Names stay unique; a number is added if needed."
    }
    Box(Modifier.fillMaxSize().background(VoLumColors.Scrim).swallowTouches().screen(ScreenId.DIALOG_NAME), contentAlignment = Alignment.TopCenter) {
        Column(
            Modifier.padding(top = 20.dp).width(520.dp).panelDepth(6.dp).hairlineFrame(VoLumColors.GoldDim.withOpacity(0.6f), 1.6.dp, 6.dp)
                .cornerBrackets(VoLumColors.Corner, 12.dp, 5.dp).padding(horizontal = 18.dp, vertical = 14.dp),
            verticalArrangement = Arrangement.spacedBy(10.dp),
        ) {
            FitText(title, VoLumType.display(24.sp, VoLumColors.Gold), Modifier.fillMaxWidth().height(30.dp), align = TextAlign.Start)
            Field(o.text, { st.go(Action.SetNameText(it)) }, Modifier.fillMaxWidth().height(52.dp), arrayOf("name.field"), placeholder = "Name",
                onDone = { st.go(Action.CommitName) })
            FitText(note, hint(), Modifier.fillMaxWidth().height(18.dp), align = TextAlign.Start)
            Row(Modifier.fillMaxWidth().height(56.dp), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                VoLumButton("Cancel", { st.go(Action.Back) }, Modifier.weight(1f).fillMaxHeight().ids(arrayOf("name.cancel")))
                VoLumButton(verb, { st.go(Action.CommitName) }, Modifier.weight(1f).fillMaxHeight().ids(ids), primary = true, enabled = o.text.isNotBlank())
            }
        }
    }
}
