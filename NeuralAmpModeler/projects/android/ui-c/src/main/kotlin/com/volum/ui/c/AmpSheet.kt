package com.volum.ui.c

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.text.BasicText
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.volum.core.Action
import com.volum.core.Cab
import com.volum.core.KnobRows
import com.volum.core.Lane
import com.volum.core.Overlay
import com.volum.core.Param

private val mainKnobIds = mapOf(
    Param.INPUT to listOf("amp.knob.input"),
    Param.GATE to listOf("amp.knob.gate"),
    Param.BASS to listOf("amp.knob.bass", "amp.knob.reset", "amp.knob.exact"),
    Param.MID to listOf("amp.knob.mid"),
    Param.TREBLE to listOf("amp.knob.treble"),
    Param.OUTPUT to listOf("amp.knob.output"),
    Param.MAIN_PAN to listOf("dual.pan.main"),
    Param.SUPPORT_INPUT to listOf("dual.support.knob.input"),
    Param.SUPPORT_GATE to listOf("dual.support.knob.gate"),
    Param.SUPPORT_BASS to listOf("dual.support.knob.bass"),
    Param.SUPPORT_MID to listOf("dual.support.knob.mid"),
    Param.SUPPORT_TREBLE to listOf("dual.support.knob.treble"),
    Param.SUPPORT_OUTPUT to listOf("dual.support.knob.output"),
    Param.SUPPORT_PAN to listOf("dual.pan.support"),
)

/** AMP sheet. With Dual Amp on, the lane switch retargets every row below it to MAIN or SUPPORT. */
@Composable
internal fun AmpSheet(ui: Ui, modifier: Modifier) {
    val s = ui.s
    val support = s.supportFocused
    val supportAmp = s.library.amp(s.rig.support.ampId)
    SectionSheet(
        if (support) "AMP · SUPPORT" else if (s.dual) "AMP · MAIN" else "AMP",
        modifier,
        actions = {
            if (s.dual) {
                Choice("MAIN", s.ui.lane == Lane.MAIN, listOf("dual.focus.main"), { ui.d(Action.FocusLane(Lane.MAIN)) }, sub = s.amp?.name)
                Choice("SUPPORT", support, listOf("dual.focus.support"), { ui.d(Action.FocusLane(Lane.SUPPORT)) }, sub = supportAmp?.name ?: "empty")
                if (supportAmp == null) Btn("+ CHOOSE SUPPORT AMP", listOf("dual.support.choose"), { ui.d(Action.Open(Overlay.SupportAmpMenu)) }, strong = true)
            }
            if (support) Switch("Ø PHASE", s.on(Param.SUPPORT_POLARITY), listOf("dual.polarity"), { ui.d(Action.ToggleParam(Param.SUPPORT_POLARITY)) })
            Switch("DUAL AMP", s.dual, listOf("dual.toggle"), { ui.d(Action.ToggleParam(Param.DUAL_ON)) })
        },
    ) {
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            if (support) {
                Btn(
                    (supportAmp?.name ?: "Choose support amp") + "  ▾",
                    listOf("dual.support.choose"),
                    { ui.d(Action.Open(Overlay.SupportAmpMenu)) },
                    sub = if (supportAmp?.custom == true) "CUSTOM · SUPPORT" else "SUPPORT AMP",
                    modifier = Modifier.width(230.dp),
                )
                val channels = supportAmp?.channels ?: 0
                (0 until channels).forEach { ch ->
                    Choice("CH ${ch + 1}", s.rig.support.channel == ch, listOf("dual.support.channel"), { ui.d(Action.SelectSupportChannel(ch)) })
                }
            } else {
                Btn(
                    (s.amp?.name ?: "Choose amp") + "  ▾",
                    emptyList(),
                    { ui.d(Action.Open(Overlay.AmpPicker)) },
                    sub = if (s.amp?.custom == true) "CUSTOM AMP · CHANGE" else "AMP · CHANGE",
                    modifier = Modifier.width(230.dp),
                )
                (0 until (s.amp?.channels ?: 0)).forEach { ch ->
                    Choice("CH ${ch + 1}", s.rig.channel == ch, listOf("amp.channel"), { ui.d(Action.SelectChannel(ch)) })
                }
            }
            Fill()
            val gate = if (support) Param.SUPPORT_GATE_ON else Param.GATE_ON
            val eq = if (support) Param.SUPPORT_EQ_ON else Param.EQ_ON
            Switch("NOISE GATE", s.on(gate), listOf(if (support) "dual.support.gate.toggle" else "amp.gate.toggle"), { ui.d(Action.ToggleParam(gate)) })
            Switch("EQ", s.on(eq), listOf(if (support) "dual.support.eq.toggle" else "amp.eq.toggle"), { ui.d(Action.ToggleParam(eq)) })
        }
        Gap(h = 6.dp)
        CabRow(ui, support)
        Gap(h = 6.dp)
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceEvenly) {
            KnobRows.forAmp(s.ui.lane.takeIf { support } ?: Lane.MAIN, s.dual).forEach { p ->
                ParamKnob(ui, p, mainKnobIds[p].orEmpty())
            }
        }
    }
}

@Composable
private fun CabRow(ui: Ui, support: Boolean) {
    val s = ui.s
    val amp = if (support) s.library.amp(s.rig.support.ampId) else s.amp
    val cab = if (support) s.rig.support.cab else s.rig.cab
    val channel = if (support) s.rig.support.channel else s.rig.channel
    val irName = s.library.ir(if (support) s.rig.support.irId else s.rig.irId)?.name
    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
        BasicText("CAB", style = labelStyle(10f), modifier = Modifier.width(36.dp))
        listOf(Cab.NONE to "amp.cab.none", Cab.G12 to "amp.cab.g12", Cab.G65 to "amp.cab.g65", Cab.V30 to "amp.cab.v30").forEach { (c, id) ->
            Choice(
                cabLabel(s, c, amp),
                cab == c,
                listOf(if (support) "dual.support.cab" else id),
                { ui.d(if (support) Action.SelectSupportCab(c) else Action.SelectCab(c)) },
                minWidth = 64.dp,
                enabled = amp != null,
            )
        }
        val irAllowed = amp != null && channel in amp.noCabChannels
        Choice(
            "CUSTOM IR  ▸",
            cab == Cab.CUSTOM_IR,
            listOf(
                when {
                    support && irAllowed -> "dual.support.cab.custom_ir"
                    support -> "dual.support.cab.custom_ir_disabled"
                    irAllowed -> "amp.cab.custom_ir"
                    else -> "amp.cab.custom_ir_disabled"
                },
            ),
            { ui.d(Action.Open(Overlay.IrMenu)) },
            sub = if (!irAllowed) "needs a No Cab capture on this channel" else irName ?: "pick or import",
            enabled = irAllowed,
            minWidth = 150.dp,
        )
    }
}
