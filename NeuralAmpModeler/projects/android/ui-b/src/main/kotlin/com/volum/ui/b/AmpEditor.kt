package com.volum.ui.b

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
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
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.Cab
import com.volum.core.KnobRows
import com.volum.core.Lane
import com.volum.core.Overlay
import com.volum.core.Param
import com.volum.core.VoLumState
import com.volum.design.components.VoLumBrassButton
import com.volum.design.components.VoLumGlyph
import com.volum.design.components.VoLumIcon
import com.volum.design.components.VoLumIrCabButton
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumType
import com.volum.harness.featureId

private val stockCabs = listOf(Cab.NONE, Cab.G12, Cab.G65, Cab.V30)

/** AMP: lane (Dual Amp), amp and cab row on top; channel, the knob row and the switches of the focused lane below. */
@Composable
internal fun AmpEditor(s: VoLumState) {
    val st = LocalStage.current
    val support = s.supportFocused
    val lane = if (support) Lane.SUPPORT else Lane.MAIN
    val laneAmp = if (support) s.library.amp(s.rig.support.ampId) else s.amp
    val channel = if (support) s.rig.support.channel else s.rig.channel
    Column(Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
        Row(Modifier.fillMaxWidth().height(48.dp), horizontalArrangement = Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.CenterVertically) {
            if (s.dual) LaneChoice(s, Modifier.width(210.dp).fillMaxHeight())
            Pad({ st.go(Action.Open(if (support) Overlay.SupportAmpMenu else Overlay.AmpPicker)) }, Modifier.width(172.dp).fillMaxHeight(),
                if (support) arrayOf("dual.support.choose") else emptyArray(), description = "Choose the ${if (support) "SUPPORT" else ""} amp") { _ ->
                Row(Modifier.fillMaxSize().padding(horizontal = 10.dp), verticalAlignment = Alignment.CenterVertically) {
                    Column(Modifier.weight(1f)) {
                        FitText(if (support) "SUPPORT AMP" else "AMP", VoLumType.label(9.sp, if (support) VoLumColors.Teal else VoLumColors.GoldDim), Modifier.fillMaxWidth().height(13.dp), align = TextAlign.Start)
                        FitText(laneAmp?.name ?: "choose an amp", VoLumType.label(13.sp, VoLumColors.Gold), Modifier.fillMaxWidth().height(20.dp), align = TextAlign.Start)
                    }
                    VoLumIcon(VoLumGlyph.ChevronDown, color = VoLumColors.Gold)
                }
            }
            CabRow(s, support, Modifier.weight(1f).fillMaxHeight())
        }
        Row(Modifier.fillMaxWidth().weight(1f), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
            val n = laneAmp?.channels ?: 1
            val chIds = if (support) arrayOf("dual.support.channel") else arrayOf("amp.channel")
            val pick = { ch: Int -> st.go(if (support) Action.SelectSupportChannel(ch) else Action.SelectChannel(ch)) }
            Column(Modifier.width(112.dp).fillMaxHeight(), verticalArrangement = Arrangement.Center) {
                FitText("CHANNEL", VoLumType.KnobLabel, Modifier.fillMaxWidth().height(18.dp))
                Stepper("${channel + 1}", { pick((channel - 1 + n) % n) }, { pick((channel + 1) % n) }, Modifier.fillMaxWidth().height(52.dp),
                    chIds, chIds, prevEnabled = laneAmp != null && n > 1, nextEnabled = laneAmp != null && n > 1, description = "channel")
                FitText("of $n", VoLumType.label(9.sp, VoLumColors.CreamDim), Modifier.fillMaxWidth().height(14.dp))
            }
            KnobRows.forAmp(lane, s.dual).forEach { p ->
                Knob(p, s.param(p), { st.go(Action.SetParam(p, it)) }, Modifier.weight(1f).fillMaxHeight(), Ids.ampKnob(p, lane),
                    support = support || p == Param.SUPPORT_PAN)
            }
            AmpSwitches(s, support, Modifier.width(176.dp).fillMaxHeight())
        }
    }
}

/** MAIN / SUPPORT lane focus, AmberPicker like every exclusive choice, each naming its amp. */
@Composable
private fun LaneChoice(s: VoLumState, modifier: Modifier) {
    val st = LocalStage.current
    val supportAmp = s.library.amp(s.rig.support.ampId)
    Row(modifier, horizontalArrangement = Arrangement.spacedBy(4.dp)) {
        Pad({ st.go(Action.FocusLane(Lane.MAIN)) }, Modifier.weight(1f).fillMaxHeight(), arrayOf("dual.focus.main"), selected = s.ui.lane == Lane.MAIN,
            style = Sel.AmberPicker, role = Role.Tab) { ink -> LaneLabel("MAIN", s.amp?.name ?: "", ink) }
        if (supportAmp == null) {
            Pad({ st.go(Action.Open(Overlay.SupportAmpMenu)) }, Modifier.weight(1f).fillMaxHeight(), arrayOf("dual.support.choose"), style = Sel.AmberPicker) { ink ->
                LaneLabel("SUPPORT", "+ choose amp", ink)
            }
        } else {
            Pad({ st.go(Action.FocusLane(Lane.SUPPORT)) }, Modifier.weight(1f).fillMaxHeight(), arrayOf("dual.focus.support"), selected = s.ui.lane == Lane.SUPPORT,
                style = Sel.AmberPicker, role = Role.Tab) { ink -> LaneLabel("SUPPORT", supportAmp.name, ink) }
        }
    }
}

@Composable
private fun LaneLabel(lane: String, amp: String, ink: androidx.compose.ui.graphics.Color) {
    Column(Modifier.fillMaxSize().padding(horizontal = 8.dp), verticalArrangement = Arrangement.Center) {
        FitText(lane, VoLumType.label(11.sp, ink), Modifier.fillMaxWidth().height(16.dp))
        FitText(amp, VoLumType.label(10.sp, if (ink == VoLumColors.AmberInk) ink else VoLumColors.Teal), Modifier.fillMaxWidth().height(16.dp))
    }
}

/** The desktop speaker row: No Cab, three cabs (Brass dialect), then Custom IR or its greyed state with the reason. */
@Composable
private fun CabRow(s: VoLumState, support: Boolean, modifier: Modifier) {
    val st = LocalStage.current
    val laneAmp = if (support) s.library.amp(s.rig.support.ampId) else s.amp
    val channel = if (support) s.rig.support.channel else s.rig.channel
    val cab = if (support) s.rig.support.cab else s.rig.cab
    val irId = if (support) s.rig.support.irId else s.rig.irId
    val irAllowed = laneAmp != null && channel in laneAmp.noCabChannels
    Row(modifier, horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
        stockCabs.forEachIndexed { i, c ->
            val label = if (laneAmp?.custom == true) laneAmp.cabNames.getOrElse(i) { c.label } else c.label
            val ids = if (support) arrayOf("dual.support.cab") else arrayOf("amp.cab.${c.name.lowercase()}")
            VoLumBrassButton(label, cab == c, { st.go(if (support) Action.SelectSupportCab(c) else Action.SelectCab(c)) },
                Modifier.height(48.dp).ids(ids), enabled = laneAmp != null, minWidth = if (i == 0) 64.dp else 52.dp)
        }
        val irKey = if (support) "dual.support.cab.custom_ir" else "amp.cab.custom_ir"
        VoLumIrCabButton(cab == Cab.CUSTOM_IR, { st.go(Action.Open(Overlay.IrMenu)) },
            Modifier.height(48.dp).featureId(if (irAllowed) irKey else "${irKey}_disabled"), irName = s.library.ir(irId)?.name, enabled = irAllowed, minWidth = 112.dp)
        if (!irAllowed && laneAmp != null) {
            FitText("needs a No Cab capture on this channel", VoLumType.body(10.sp, VoLumColors.CreamDim), Modifier.weight(1f), align = TextAlign.Start)
        }
    }
}

@Composable
private fun AmpSwitches(s: VoLumState, support: Boolean, modifier: Modifier) {
    val st = LocalStage.current
    val gate = if (support) Param.SUPPORT_GATE_ON else Param.GATE_ON
    val eq = if (support) Param.SUPPORT_EQ_ON else Param.EQ_ON
    val gateSwitch: @Composable (Modifier) -> Unit = { m ->
        Switch("NOISE GATE", s.on(gate), { st.go(Action.ToggleParam(gate)) }, m, arrayOf(if (support) "dual.support.gate.toggle" else "amp.gate.toggle"))
    }
    val eqSwitch: @Composable (Modifier) -> Unit = { m ->
        Switch("EQ", s.on(eq), { st.go(Action.ToggleParam(eq)) }, m, arrayOf(if (support) "dual.support.eq.toggle" else "amp.eq.toggle"))
    }
    BoxWithConstraints(modifier) {
        val compact = maxHeight < 100.dp
        Column(Modifier.fillMaxSize(), verticalArrangement = Arrangement.Center) {
            if (compact) {
                Row(Modifier.fillMaxWidth().height(48.dp)) {
                    gateSwitch(Modifier.weight(1.3f).fillMaxHeight())
                    eqSwitch(Modifier.weight(1f).fillMaxHeight())
                }
            } else {
                gateSwitch(Modifier.fillMaxWidth().height(48.dp))
                Row(Modifier.fillMaxWidth().height(48.dp)) {
                    eqSwitch(Modifier.weight(1f).fillMaxHeight())
                    if (s.dual) {
                        Switch("Ø PHASE", s.on(Param.SUPPORT_POLARITY), { st.go(Action.ToggleParam(Param.SUPPORT_POLARITY)) }, Modifier.weight(1.2f).fillMaxHeight(),
                            arrayOf("dual.polarity"))
                    }
                }
            }
            if (s.outputSafety) {
                Box(Modifier.fillMaxWidth().featureId("status.output_safety")) {
                    Status("OUTPUT SAFETY · limiting", Modifier.fillMaxWidth().height(20.dp), VoLumColors.Danger)
                }
            }
        }
    }
}
