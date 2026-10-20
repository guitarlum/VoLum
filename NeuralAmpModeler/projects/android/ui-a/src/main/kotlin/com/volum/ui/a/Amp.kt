package com.volum.ui.a

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.Amp
import com.volum.core.Cab
import com.volum.core.KnobRows
import com.volum.core.Lane
import com.volum.core.Overlay
import com.volum.core.Param
import com.volum.design.art.ArtRef
import com.volum.design.components.HeroAmp
import com.volum.design.components.VoLumBrassButton
import com.volum.design.components.VoLumButton
import com.volum.design.components.VoLumChannelStepper
import com.volum.design.components.VoLumGlyph
import com.volum.design.components.VoLumHero
import com.volum.design.components.VoLumIcon
import com.volum.design.components.VoLumIrCabButton
import com.volum.design.components.VoLumToggle
import com.volum.design.theme.VoLumColors

internal fun cabLabel(amp: Amp?, cab: Cab, irName: String? = null): String = when {
    cab == Cab.CUSTOM_IR -> irName ?: cab.label
    amp?.custom == true -> amp.cabNames.getOrElse(cab.ordinal) { cab.label }
    else -> cab.label
}

/**
 * AMP focus. Left, the desktop hero: the static art with the Dual Amp chip, or
 * MAIN and SUPPORT lanes side by side (tap a lane to edit it), then the lane's amp
 * name, which opens its amp list. Right: channel and cab, the knob row, then the
 * gate / EQ switches with the Dual Amp extras. With SUPPORT focused every row acts
 * on SUPPORT and its knobs turn teal, like the desktop's lane focus.
 */
@Composable
internal fun AmpEditor(c: Ctx) {
    val s = c.s
    val lane = if (s.dual) s.ui.lane else Lane.MAIN
    val support = lane == Lane.SUPPORT
    val supportAmp = s.library.amp(s.rig.support.ampId)
    val laneAmp = if (support) supportAmp else s.amp
    Editor(
        left = {
            VoLumHero(
                HeroAmp(s.amp?.name ?: "", s.amp?.let { ArtRef.of(it) }),
                Modifier.weight(1f).fillMaxWidth().ids("amp.hero"),
                dual = s.dual,
                support = supportAmp?.let { HeroAmp(it.name, ArtRef.of(it)) },
                supportFocused = support,
                onDualChip = { c.d(Action.ToggleParam(Param.DUAL_ON)) },
                chipModifier = Modifier.ids("dual.toggle").desc(if (s.dual) "Dual Amp on" else "Dual Amp off"),
                onLane = { sup -> c.d(Action.FocusLane(if (sup) Lane.SUPPORT else Lane.MAIN)) },
                laneModifier = { sup -> Modifier.ids(if (sup) "dual.focus.support" else "dual.focus.main") },
            )
            val open = { c.d(Action.Open(if (support) Overlay.SupportAmpMenu else Overlay.AmpPicker)) }
            Row(
                Modifier.fillMaxWidth().heightIn(min = 48.dp).ids(if (support) "dual.support.choose" else "amp.name")
                    .clickable(role = Role.Button, onClickLabel = if (support) "Choose SUPPORT amp" else "Choose amp", onClick = open),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(6.dp, Alignment.CenterHorizontally),
            ) {
                BlockTitle(laneAmp?.name ?: "Choose SUPPORT amp", Modifier.weight(1f, fill = false))
                VoLumIcon(VoLumGlyph.ChevronDown, color = if (support) VoLumColors.Teal else VoLumColors.Gold, size = 12.dp)
            }
        },
        right = {
            if (support && laneAmp == null) {
                Gap(h = 8.dp)
                Note("SUPPORT has no amp yet. Pick one to blend it under MAIN; each lane keeps its own channel, cab and knobs.")
                VoLumButton("Choose SUPPORT amp", { c.d(Action.Open(Overlay.SupportAmpMenu)) }, Modifier.ids("dual.support.choose"), primary = true)
                return@Editor
            }
            CabRow(c, support, laneAmp)
            KnobRow(c, KnobRows.forAmp(lane, s.dual), support = support)
            Row(Modifier.fillMaxWidth().height(48.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Channel(c, support, laneAmp)
                val gate = if (support) Param.SUPPORT_GATE_ON else Param.GATE_ON
                val eq = if (support) Param.SUPPORT_EQ_ON else Param.EQ_ON
                VoLumToggle(s.on(gate), { c.d(Action.ToggleParam(gate)) }, Modifier.ids(if (support) "dual.support.gate.toggle" else "amp.gate.toggle"), label = "NOISE GATE")
                VoLumToggle(s.on(eq), { c.d(Action.ToggleParam(eq)) }, Modifier.ids(if (support) "dual.support.eq.toggle" else "amp.eq.toggle"), label = "EQ")
                Fill()
                if (support) {
                    VoLumToggle(s.on(Param.SUPPORT_POLARITY), { c.d(Action.ToggleParam(Param.SUPPORT_POLARITY)) }, Modifier.ids("dual.polarity").desc("Invert SUPPORT phase"), label = "PHASE")
                } else if (s.dual) {
                    VoLumButton(supportAmp?.name ?: "Add SUPPORT amp", { c.d(Action.Open(Overlay.SupportAmpMenu)) }, Modifier.ids("dual.support.choose").desc("Choose SUPPORT amp"))
                }
            }
        },
        leftWidth = if (s.dual) 300.dp else 262.dp,
    )
}

/** Channel stepper, then the cab row of the focused lane (`VoLumSpeakerRowControl`): No Cab, the three cabs, Custom IR. */
@Composable
private fun CabRow(c: Ctx, support: Boolean, amp: Amp?) {
    val s = c.s
    Row(Modifier.fillMaxWidth().height(48.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp, Alignment.CenterHorizontally)) {
        Caption("CAB", color = if (support) VoLumColors.Teal else VoLumColors.GoldDim)
        val cab = if (support) s.rig.support.cab else s.rig.cab
        listOf(Cab.NONE, Cab.G12, Cab.G65, Cab.V30).forEach { k ->
            val id = if (support) "dual.support.cab" else "amp.cab.${k.name.lowercase()}"
            VoLumBrassButton(cabLabel(amp, k), cab == k, { c.d(if (support) Action.SelectSupportCab(k) else Action.SelectCab(k)) }, Modifier.ids(id), minWidth = if (k == Cab.NONE) 64.dp else 52.dp)
        }
        val channel = if (support) s.rig.support.channel else s.rig.channel
        val irAllowed = channel in (amp?.noCabChannels ?: emptySet())
        val irIds = when {
            !irAllowed -> if (support) "dual.support.cab.custom_ir_disabled" else "amp.cab.custom_ir_disabled"
            support -> "dual.support.cab.custom_ir"
            else -> "amp.cab.custom_ir"
        }
        val irId = if (support) s.rig.support.irId else s.rig.irId
        VoLumIrCabButton(cab == Cab.CUSTOM_IR, { c.d(Action.Open(Overlay.IrMenu)) }, Modifier.ids(irIds), irName = s.library.ir(irId)?.name, enabled = irAllowed)
    }
}

@Composable
private fun RowScope.Channel(c: Ctx, support: Boolean, amp: Amp?) {
    val s = c.s
    val channels = amp?.channels ?: 1
    val channel = if (support) s.rig.support.channel else s.rig.channel
    val step = { d: Int ->
        val next = Math.floorMod(channel + d, channels)
        c.d(if (support) Action.SelectSupportChannel(next) else Action.SelectChannel(next))
    }
    val ids = if (support) "dual.support.channel" else "amp.channel"
    Caption("CH", color = if (support) VoLumColors.Teal else VoLumColors.GoldDim)
    VoLumChannelStepper(
        "${channel + 1}", { step(-1) }, { step(1) },
        Modifier.width(124.dp),
        prevModifier = Modifier.ids(ids).desc("Previous channel"),
        nextModifier = Modifier.ids(ids).desc("Next channel"),
    )
}
