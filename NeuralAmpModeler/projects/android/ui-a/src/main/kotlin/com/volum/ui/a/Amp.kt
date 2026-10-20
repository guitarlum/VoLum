package com.volum.ui.a

import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.Amp
import com.volum.core.Cab
import com.volum.core.KnobRows
import com.volum.core.Lane
import com.volum.core.Overlay
import com.volum.core.Param

internal fun cabLabel(amp: Amp?, cab: Cab): String =
    if (amp?.custom == true && cab != Cab.CUSTOM_IR) amp.cabNames.getOrElse(cab.ordinal) { cab.label } else cab.label

/**
 * AMP focus: the lane's art and amp on the left; channel and cab, Dual Amp, the knob
 * row and gate / EQ on the right. With Dual Amp on and SUPPORT focused every row acts
 * on SUPPORT, like the desktop's lane focus.
 */
@Composable
internal fun AmpEditor(c: Ctx) {
    val s = c.s
    val lane = if (s.dual) s.ui.lane else Lane.MAIN
    val support = lane == Lane.SUPPORT
    val laneAmp = if (support) s.library.amp(s.rig.support.ampId) else s.amp
    Editor(laneAmp?.name ?: "", c, com.volum.ui.a.Texture.AMP, artIds = listOf("amp.hero"),
        left = {
            val open = { c.d(Action.Open(if (support) Overlay.SupportAmpMenu else Overlay.AmpPicker)) }
            Tap(open, Modifier.fillMaxWidth().height(RowH), if (support) listOf("dual.support.choose") else emptyList()) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    T(laneAmp?.name ?: "Choose SUPPORT amp", Type.display(17.sp), Modifier.weight(1f, fill = false).then(if (!support) Modifier.ids("amp.name") else Modifier))
                    GlyphIcon(Glyph.DOWN, Modifier.height(20.dp))
                }
            }
        },
        right = {
            if (support && laneAmp == null) {
                ControlRow { DualRow(c, lane) }
                Notice("SUPPORT has no amp yet. Choose one to blend it with MAIN.")
                return@Editor
            }
            ControlRow { ChannelAndCabs(c, support, laneAmp) }
            ControlRow { DualRow(c, lane) }
            KnobRow(c, KnobRows.forAmp(lane, s.dual), Modifier.weight(1f))
            ControlRow {
                val gate = if (support) Param.SUPPORT_GATE_ON else Param.GATE_ON
                val eq = if (support) Param.SUPPORT_EQ_ON else Param.EQ_ON
                Switch("NOISE GATE", s.on(gate), { c.d(Action.ToggleParam(gate)) }, ids = listOf(if (support) "dual.support.gate.toggle" else "amp.gate.toggle"))
                Switch("EQ", s.on(eq), { c.d(Action.ToggleParam(eq)) }, ids = listOf(if (support) "dual.support.eq.toggle" else "amp.eq.toggle"))
                Spacer(Modifier.weight(1f))
                T(if (s.dual) (if (support) "EDITING SUPPORT" else "EDITING MAIN") else "", Type.label(10.sp))
            }
        },
    )
}

@Composable
private fun androidx.compose.foundation.layout.RowScope.ChannelAndCabs(c: Ctx, support: Boolean, amp: Amp?) {
    val s = c.s
    val channels = amp?.channels ?: 1
    val channel = if (support) s.rig.support.channel else s.rig.channel
    val step = { d: Int ->
        val next = Math.floorMod(channel + d, channels)
        c.d(if (support) Action.SelectSupportChannel(next) else Action.SelectChannel(next))
    }
    val chIds = listOf(if (support) "dual.support.channel" else "amp.channel")
    Stepper("CH", "${channel + 1}", { step(-1) }, { step(1) }, prevIds = chIds, nextIds = chIds, enabled = channels > 1, valueWidth = 32.dp)
    Spacer(Modifier.weight(0.1f))
    val cab = if (support) s.rig.support.cab else s.rig.cab
    listOf(Cab.NONE, Cab.G12, Cab.G65, Cab.V30).forEach { k ->
        val id = if (support) "dual.support.cab" else "amp.cab.${k.name.lowercase()}"
        Chip(cabLabel(amp, k), { c.d(if (support) Action.SelectSupportCab(k) else Action.SelectCab(k)) }, ids = listOf(id), selected = cab == k)
    }
    val irAllowed = channel in (amp?.noCabChannels ?: emptySet())
    val irIds = when {
        !irAllowed -> listOf(if (support) "dual.support.cab.custom_ir_disabled" else "amp.cab.custom_ir_disabled")
        support -> listOf("dual.support.cab.custom_ir")
        else -> listOf("amp.cab.custom_ir")
    }
    Chip("CUSTOM IR", { c.d(Action.Open(Overlay.IrMenu)) }, ids = irIds, selected = cab == Cab.CUSTOM_IR, enabled = irAllowed)
}

@Composable
private fun androidx.compose.foundation.layout.RowScope.DualRow(c: Ctx, lane: Lane) {
    val s = c.s
    Switch("DUAL AMP", s.dual, { c.d(Action.ToggleParam(Param.DUAL_ON)) }, ids = listOf("dual.toggle"))
    if (!s.dual) {
        T("Blend a second amp under this one.", Type.label(10.sp, Ink.faint))
        return
    }
    Chip("MAIN", { c.d(Action.FocusLane(Lane.MAIN)) }, ids = listOf("dual.focus.main"), selected = lane == Lane.MAIN)
    Chip("SUPPORT", { c.d(Action.FocusLane(Lane.SUPPORT)) }, ids = listOf("dual.focus.support"), selected = lane == Lane.SUPPORT)
    val supportAmp = s.library.amp(s.rig.support.ampId)
    Chip(supportAmp?.name?.let { "+ $it" } ?: "CHOOSE SUPPORT AMP", { c.d(Action.Open(Overlay.SupportAmpMenu)) }, Modifier.weight(1f), listOf("dual.support.choose"))
    if (lane == Lane.SUPPORT) {
        Switch("Ø", s.on(Param.SUPPORT_POLARITY), { c.d(Action.ToggleParam(Param.SUPPORT_POLARITY)) }, ids = listOf("dual.polarity"))
    }
}
