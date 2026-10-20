package com.volum.ui.a

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.Amp
import com.volum.core.Cab
import com.volum.core.Overlay
import com.volum.core.Param
import com.volum.core.Preset
import com.volum.design.art.ArtRef
import com.volum.design.components.VoLumButton
import com.volum.design.components.VoLumChannelStepper
import com.volum.design.components.VoLumGlyph
import com.volum.design.components.VoLumListCaption
import com.volum.design.components.VoLumListRow
import com.volum.design.components.twoDigits
import com.volum.design.theme.VText
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumType
import kotlin.math.roundToInt

private val Pitch = 48.dp

/** Two side-by-side catalogs: bundled on the left, the user's own on the right, so neither hides the other. */
@Composable
internal fun ColumnScope.TwoColumns(left: @Composable ColumnScope.() -> Unit, right: @Composable ColumnScope.() -> Unit, rightWidth: Dp = 320.dp) {
    Row(Modifier.fillMaxWidth().weight(1f), horizontalArrangement = Arrangement.spacedBy(14.dp)) {
        Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(4.dp), content = left)
        Column(Modifier.width(rightWidth).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(4.dp), content = right)
    }
}

/** Rows that carry their own 48 dp actions are a little taller: the list row insets its content by a hair. */
private val ActionPitch = 50.dp

@Composable
private fun AmpRow(amp: Amp, selected: Boolean, id: String, onClick: () -> Unit, modifier: Modifier = Modifier, trailing: (@Composable androidx.compose.foundation.layout.RowScope.() -> Unit)? = null) =
    VoLumListRow(amp.name, selected, onClick, modifier.fillMaxWidth().height(if (trailing != null) ActionPitch else Pitch).ids(id), art = ArtRef.of(amp), subtitle = "${amp.channels} CHANNELS", trailing = trailing)

@Composable
internal fun AmpPicker(c: Ctx) {
    val s = c.s
    val bundled = s.library.amps.filter { !it.custom }
    val custom = s.library.amps.filter { it.custom }
    Sheet("Amps", emptyList(), c, width = 860.dp) {
        TwoColumns(
            left = {
                VoLumListCaption("BUNDLED  ·  ${bundled.size}")
                RowList(Pitch, Modifier.weight(1f).fillMaxWidth(), scrollToRow = bundled.indexOfFirst { it.id == s.ampId } - 2) {
                    bundled.forEach { a -> AmpRow(a, a.id == s.ampId, "amp.list.select", { c.d(Action.SelectAmp(a.id)) }) }
                }
            },
            right = {
                VoLumListCaption("CUSTOM  ·  ${custom.size}", if (custom.isNotEmpty()) Modifier.ids("amp.list.custom") else Modifier)
                VoLumButton("+  Build a custom amp", { c.d(Action.OpenBuilder(null)) }, Modifier.fillMaxWidth().ids("custom.amp.add"))
                RowList(ActionPitch, Modifier.weight(1f).fillMaxWidth()) {
                    custom.forEach { a ->
                        AmpRow(a, a.id == s.ampId, "amp.list.select_custom", { c.d(Action.SelectAmp(a.id)) }) {
                            GlyphAction(VoLumGlyph.Edit, "EDIT", { c.d(Action.OpenBuilder(a.id)) }, Modifier.ids("custom.amp.edit"))
                            GlyphAction(VoLumGlyph.Delete, "DELETE", { c.d(Action.RequestDeleteAmp(a.id)) }, Modifier.ids("custom.amp.delete"), tint = VoLumColors.Danger)
                        }
                    }
                    if (custom.isEmpty()) Note("Build an amp from your own .nam captures: one file per cab and channel.")
                }
            },
            rightWidth = 420.dp,
        )
    }
}

@Composable
internal fun IrMenu(c: Ctx) {
    val s = c.s
    val support = s.supportFocused
    val cab = if (support) s.rig.support.cab else s.rig.cab
    val playing = if (support) s.rig.support.irId.takeIf { cab == Cab.CUSTOM_IR } else s.rig.irId.takeIf { cab == Cab.CUSTOM_IR }
    Sheet(if (support) "Custom IR  ·  SUPPORT" else "Custom IR  ·  MAIN", listOf("ir.menu.close"), c, width = 560.dp) {
        RowList(Pitch, Modifier.weight(1f).fillMaxWidth(), scrollToRow = s.library.irs.indexOfFirst { it.id == playing }) {
            VoLumListRow("None  ·  amp only (No Cab)", cab == Cab.NONE, {
                c.d(if (support) Action.SelectSupportCab(Cab.NONE) else Action.SelectCab(Cab.NONE))
                c.d(Action.Back)
            }, Modifier.fillMaxWidth().height(Pitch).ids(if (support) "dual.support.cab" else "amp.cab.none"), subtitle = "THE DIRECT CAPTURE WITHOUT A CAB")
            s.library.irs.forEach { ir ->
                VoLumListRow(ir.name, ir.id == playing, { c.d(Action.SelectIr(ir.id)) }, Modifier.fillMaxWidth().height(Pitch).ids("ir.menu.select", "dual.support.ir.select"),
                    subtitle = if (ir.id == playing) "PLAYING" else "IR")
            }
        }
        if (s.library.irs.isEmpty()) Note("No IRs yet. Import WAV impulse responses in Manage.")
        Row(Modifier.fillMaxWidth().padding(top = 8.dp), horizontalArrangement = Arrangement.End) {
            VoLumButton("Manage IRs", { c.d(Action.Open(Overlay.IrManage)) }, Modifier.ids("ir.menu.manage"))
        }
    }
}

/**
 * The capture menu grouped like the desktop's: one column per family (Klon,
 * TS / Boost, Distortion, Fuzz) with the captures' full names, Custom on the
 * right with "Manage custom pedals" under it, Empty across the top.
 */
@Composable
internal fun CaptureMenu(c: Ctx, slot: Int) {
    val s = c.s
    val current = s.param(if (slot == 1) Param.NAM1_CAPTURE else Param.NAM2_CAPTURE).roundToInt()
    val bundled = s.library.pedals.filter { !it.custom && it.index != 0 }
    val custom = s.library.pedals.filter { it.custom }
    Sheet("NAM $slot  ·  capture", listOf("pre.capture.close"), c, width = 880.dp) {
        VoLumListRow("Empty  ·  no pedal in this slot", current == 0, { c.d(Action.SelectCapture(slot, 0)) }, Modifier.width(320.dp).height(Pitch).ids("pre.capture.empty"))
        Gap(h = 6.dp)
        Row(Modifier.fillMaxWidth().weight(1f), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            CaptureGroup.entries.forEach { g ->
                val group = bundled.filter { captureInfo(it)?.group == g }.sortedBy { captureInfo(it)?.rank }
                Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(2.dp)) {
                    VoLumListCaption(g.label)
                    group.forEach { p ->
                        VoLumListRow(captureInfo(p)?.full ?: p.name, p.index == current, { c.d(Action.SelectCapture(slot, p.index)) },
                            Modifier.fillMaxWidth().height(Pitch).ids("pre.capture.select"), subtitle = p.name.uppercase())
                    }
                }
            }
            Column(Modifier.weight(1.3f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(2.dp)) {
                VoLumListCaption("CUSTOM  ·  ${custom.size} / 64", if (custom.isNotEmpty()) Modifier.ids("pre.capture.custom") else Modifier)
                RowList(Pitch, Modifier.weight(1f).fillMaxWidth(), scrollToRow = custom.indexOfFirst { it.index == current }) {
                    custom.forEach { p ->
                        VoLumListRow(p.name, p.index == current, { c.d(Action.SelectCapture(slot, p.index)) }, Modifier.fillMaxWidth().height(Pitch).ids("pre.capture.select_custom"), subtitle = "YOUR CAPTURE")
                    }
                    if (custom.isEmpty()) Note("Import your own pedal captures in Manage.")
                }
                VoLumButton("Manage pedals", { c.d(Action.Open(Overlay.PedalManage)) }, Modifier.fillMaxWidth().ids("pre.capture.manage"))
            }
        }
    }
}

@Composable
internal fun SupportAmpMenu(c: Ctx) {
    val s = c.s
    val current = s.rig.support.ampId
    val bundled = s.library.amps.filter { !it.custom }
    Sheet("SUPPORT amp", listOf("dual.support.close"), c, width = 820.dp) {
        TwoColumns(
            left = {
                VoLumListCaption("BUNDLED")
                RowList(Pitch, Modifier.weight(1f).fillMaxWidth(), scrollToRow = bundled.indexOfFirst { it.id == current } - 2) {
                    bundled.forEach { a -> AmpRow(a, a.id == current, "dual.support.select", { c.d(Action.SelectSupportAmp(a.id)) }) }
                }
            },
            right = {
                VoLumListCaption("CUSTOM")
                RowList(Pitch, Modifier.weight(1f).fillMaxWidth()) {
                    s.library.amps.filter { it.custom }.forEach { a -> AmpRow(a, a.id == current, "dual.support.select_custom", { c.d(Action.SelectSupportAmp(a.id)) }) }
                }
                VoLumListRow("No SUPPORT amp", current == null, { c.d(Action.SelectSupportAmp(null)) }, Modifier.fillMaxWidth().height(Pitch).ids("dual.support.none"), subtitle = "MAIN PLAYS ALONE")
            },
        )
    }
}

@Composable
private fun PresetRow(c: Ctx, p: Preset, id: String, onClick: () -> Unit) {
    val amp = c.s.library.amp(p.ampId)
    VoLumListRow(p.name, false, onClick, Modifier.fillMaxWidth().height(Pitch).ids(id), art = amp?.let { ArtRef.of(it) }, subtitle = amp?.name?.uppercase())
}

/** Assign a Sound: the program number at the top says whether that slot is free or what it replaces. */
@Composable
internal fun SoundPicker(c: Ctx, o: Overlay.SoundPicker) {
    val s = c.s
    val pick = { id: String -> c.d(Action.AssignSound(o.program, id)) }
    val step = { d: Int -> c.d(Action.SetPickerProgram(Math.floorMod(o.program + d, 128))) }
    val taken = s.library.sounds[o.program]?.let { s.library.preset(it.presetId)?.name ?: "an invalid slot" }
    Sheet(if (o.replace) "Replace a Sound" else "Add a Sound", listOf("play.picker.close"), c, actions = {
        VText("PROGRAM", style = VoLumType.label(10.sp, VoLumColors.GoldDim))
        VoLumChannelStepper(twoDigits(o.program), { step(-1) }, { step(1) }, prevModifier = Modifier.ids("play.picker.program").desc("Previous program"), nextModifier = Modifier.ids("play.picker.program").desc("Next program"))
        VText(if (taken == null) "free" else "replaces $taken", Modifier.width(170.dp), style = VoLumType.label(11.sp, if (taken == null) VoLumColors.Teal else VoLumColors.Amber))
    }) {
        TwoColumns(
            left = {
                VoLumListCaption("FACTORY")
                RowList(Pitch, Modifier.weight(1f).fillMaxWidth()) {
                    s.library.presets.filter { it.factory }.forEach { p -> PresetRow(c, p, "play.picker.factory") { pick(p.id) } }
                }
            },
            right = {
                VoLumListCaption("USER")
                RowList(Pitch, Modifier.weight(1f).fillMaxWidth()) {
                    s.library.presets.filter { !it.factory }.forEach { p -> PresetRow(c, p, "play.picker.user") { pick(p.id) } }
                }
            },
            rightWidth = 400.dp,
        )
    }
}
