package com.volum.ui.a

import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsPressedAsState
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.Cab
import com.volum.core.PickKind
import com.volum.design.art.AmpArt
import com.volum.design.art.ArtRef
import com.volum.design.components.CrossButton
import com.volum.design.components.VoLumButton
import com.volum.design.components.VoLumLed
import com.volum.design.components.VoLumSelectionStyle
import com.volum.design.components.VoLumTextField
import com.volum.design.components.hairlineFrame
import com.volum.design.components.insetWell
import com.volum.design.components.volumSelection
import com.volum.design.theme.VFitText
import com.volum.design.theme.VText
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumType

private val FilePitch = 50.dp

/**
 * The custom amp builder, laid out like the desktop's: name, Add .nam files and
 * one row per file (speaker and channel) on the left; the six art styles and the
 * coverage grid (cab x channel, cab names editable) on the right.
 */
@Composable
internal fun AmpBuilder(c: Ctx) {
    val b = c.s.builder
    val canSave = b.name.isNotBlank() && b.files.isNotEmpty()
    Sheet(if (b.editAmpId != null) "Edit custom amp" else "Custom amp", listOf("builder.close"), c, actions = {
        if (!canSave) VText(if (b.name.isBlank()) "Name it to save" else "Add a capture to save", style = VoLumType.label(10.sp, VoLumColors.CreamDim))
        VoLumButton("Save amp", { c.d(Action.BuilderSave) }, Modifier.ids("builder.save").alpha(if (canSave) 1f else 0.45f), primary = canSave, enabled = canSave)
    }) {
        Row(Modifier.fillMaxWidth().weight(1f), horizontalArrangement = Arrangement.spacedBy(18.dp)) {
            Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                Caption("PROFILE NAME")
                VoLumTextField(b.name, { c.d(Action.BuilderSetName(it)) }, Modifier.fillMaxWidth().ids("builder.name"), placeholder = "Name your amp, e.g. Monomyth Skeleton Key")
                VoLumButton("+  Add .nam files", { c.d(Action.PickFiles(PickKind.NAM_AMP_FILES)) }, Modifier.fillMaxWidth().ids("builder.add_files"))
                if (b.files.isEmpty()) {
                    Note("Add the amp's .nam captures. Named CAB-NAME-CHANNEL.nam, they fill in their speaker and channel.", Modifier.fillMaxWidth())
                    Fill()
                } else {
                    RowList(FilePitch, Modifier.weight(1f).fillMaxWidth()) {
                        b.files.forEachIndexed { i, file ->
                            Row(Modifier.fillMaxWidth().height(FilePitch).padding(start = 8.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                                VFitText(file.name, Modifier.weight(1f), style = VoLumType.label(12.sp, VoLumColors.TextMed), minSize = 8.sp)
                                VoLumButton(b.cabNames.getOrElse(file.cabSlot) { Cab.entries[file.cabSlot].label }, { c.d(Action.BuilderSetFileCab(i, (file.cabSlot + 1) % 4)) },
                                    Modifier.ids("builder.file.cab").desc("Speaker of ${file.name}: tap for the next"))
                                VoLumButton("Ch ${file.channel}", { c.d(Action.BuilderSetFileChannel(i, if (file.channel >= 6) 1 else file.channel + 1)) },
                                    Modifier.ids("builder.file.channel").desc("Channel of ${file.name}: tap for the next"))
                                CrossButton({ c.d(Action.BuilderRemoveFile(i)) }, Modifier.ids("builder.file.remove").desc("Remove ${file.name}"), VoLumColors.Danger)
                            }
                        }
                    }
                }
                Hint("Tap a speaker or channel to step it. DIRECT is the amp alone: pair it with a custom IR. Your files are never renamed.")
            }
            Column(Modifier.width(390.dp).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                Caption("ART")
                Row(Modifier.fillMaxWidth().height(52.dp), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    (0..5).forEach { a -> ArtTile(a, b.art == a, { c.d(Action.BuilderSetArt(a)) }, Modifier.weight(1f).fillMaxHeight()) }
                }
                Gap(h = 4.dp)
                Coverage(c)
            }
        }
    }
}

@Composable
private fun ArtTile(style: Int, selected: Boolean, onClick: () -> Unit, modifier: Modifier) {
    val source = remember { MutableInteractionSource() }
    val pressed by source.collectIsPressedAsState()
    Box(
        modifier.ids("builder.art").desc("Art style ${style + 1}")
            .clickable(source, indication = null, role = Role.RadioButton) { onClick() }
            .insetWell(3.dp)
            .volumSelection(selected, pressed, VoLumSelectionStyle.Brass, radius = 3.dp, inset = 0.dp, glow = selected)
            .hairlineFrame(if (selected) VoLumColors.SelBorder else VoLumColors.BtnOffBorder, radius = 3.dp),
    ) {
        AmpArt(ArtRef.custom(style), Modifier.fillMaxSize().padding(5.dp))
        if (selected) VoLumLed(true, Modifier.align(Alignment.TopEnd), color = VoLumColors.Gold)
    }
}

/** The desktop's coverage grid: which channel has a capture through which cab; the three cab names are fields to rename. */
@Composable
private fun Coverage(c: Ctx) {
    val b = c.s.builder
    val channels = maxOf(2, b.files.maxOfOrNull { it.channel } ?: 0)
    Row(verticalAlignment = Alignment.CenterVertically) {
        Caption("COVERAGE", Modifier.weight(1f))
        VText("tap a cab name to rename it", style = VoLumType.label(9.sp, VoLumColors.CreamDim))
    }
    Row(Modifier.fillMaxWidth().height(16.dp)) {
        Gap(w = 132.dp)
        (1..channels).forEach { ch -> VText("$ch", Modifier.width(34.dp), style = VoLumType.value(9.sp, VoLumColors.GoldDim), textAlign = TextAlign.Center) }
    }
    b.cabNames.forEachIndexed { slot, name ->
        Row(Modifier.fillMaxWidth().height(48.dp), verticalAlignment = Alignment.CenterVertically) {
            if (slot == 0) {
                VText(name, Modifier.width(126.dp).padding(start = 8.dp), style = VoLumType.label(12.sp, VoLumColors.Gold))
            } else {
                CabNameField(c, slot, name, Modifier.width(126.dp))
            }
            Gap(w = 6.dp)
            (1..channels).forEach { ch ->
                val covered = b.files.any { it.cabSlot == slot && it.channel == ch }
                Box(Modifier.size(30.dp).padding(2.dp).insetWell(2.dp), contentAlignment = Alignment.Center) {
                    if (covered) VoLumLed(true, color = VoLumColors.Gold, animate = false)
                }
                Gap(w = 4.dp)
            }
        }
    }
}

@Composable
private fun CabNameField(c: Ctx, slot: Int, name: String, modifier: Modifier) {
    var text by remember(name) { mutableStateOf(name) }
    VoLumTextField(text, { text = it; if (it.isNotBlank()) c.d(Action.BuilderSetCabName(slot, it)) }, modifier.ids("builder.cab_name"), placeholder = "Cab $slot")
}
