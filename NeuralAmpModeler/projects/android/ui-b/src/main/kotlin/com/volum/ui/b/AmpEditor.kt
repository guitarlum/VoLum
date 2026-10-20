package com.volum.ui.b

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.Cab
import com.volum.core.KnobRows
import com.volum.core.Lane
import com.volum.core.Overlay
import com.volum.core.Param
import com.volum.core.VoLumState
import com.volum.harness.featureId

/** AMP: amp and channel, cab row, the knob row and switches of the focused lane (MAIN, or SUPPORT with Dual Amp on). */
@Composable
internal fun AmpEditor(s: VoLumState) {
    val st = LocalStage.current
    val support = s.supportFocused
    val lane = if (support) Lane.SUPPORT else Lane.MAIN
    val laneAmp = if (support) s.library.amp(s.rig.support.ampId) else s.amp
    val channel = if (support) s.rig.support.channel else s.rig.channel
    Column(Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Row(Modifier.fillMaxWidth().height(56.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            if (s.dual) LaneChoice(s, Modifier.width(210.dp).fillMaxHeight())
            Pad({ st.go(Action.Open(if (support) Overlay.SupportAmpMenu else Overlay.AmpPicker)) }, Modifier.width(140.dp).fillMaxHeight(),
                if (support) arrayOf("dual.support.choose") else emptyArray(), description = "Choose amp") {
                Column(Modifier.fillMaxSize().padding(horizontal = 10.dp), verticalArrangement = Arrangement.Center) {
                    Caption(if (support) "SUPPORT AMP ▾" else "AMP ▾", Modifier.fillMaxWidth())
                    FitText(laneAmp?.name ?: "none", Type.value(14.sp), Modifier.fillMaxWidth().height(22.dp), align = androidx.compose.ui.text.style.TextAlign.Start)
                }
            }
            val n = laneAmp?.channels ?: 1
            val chIds = if (support) arrayOf("dual.support.channel") else arrayOf("amp.channel")
            val pick = { ch: Int -> st.go(if (support) Action.SelectSupportChannel(ch) else Action.SelectChannel(ch)) }
            Column(Modifier.width(140.dp).fillMaxHeight()) {
                Stepper("CH ${channel + 1}", { pick((channel - 1 + n) % n) }, { pick((channel + 1) % n) }, Modifier.fillMaxSize(),
                    chIds, chIds, prevEnabled = laneAmp != null && n > 1, nextEnabled = laneAmp != null && n > 1, description = "channel")
            }
            CabRow(s, support, Modifier.weight(1f).fillMaxHeight())
        }
        Row(Modifier.fillMaxWidth().weight(1f), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
            KnobRows.forAmp(lane, s.dual).forEach { p ->
                Knob(p.label, s.param(p), p.min, p.max, p.default, p.unit, { st.go(Action.SetParam(p, it)) }, { st.go(Action.ResetParam(p)) },
                    Modifier.weight(1f).fillMaxHeight(), Ids.ampKnob(p, lane))
            }
            AmpSwitches(s, support, Modifier.width(200.dp).fillMaxHeight())
        }
    }
}

@Composable
private fun LaneChoice(s: VoLumState, modifier: Modifier) {
    val st = LocalStage.current
    val supportAmp = s.library.amp(s.rig.support.ampId)
    Row(modifier, horizontalArrangement = Arrangement.spacedBy(4.dp)) {
        Pad({ st.go(Action.FocusLane(Lane.MAIN)) }, Modifier.weight(1f).fillMaxHeight(), arrayOf("dual.focus.main"), selected = s.ui.lane == Lane.MAIN,
            sel = Sel.AMBER_PICKER, role = Role.Tab) {
            LaneLabel("MAIN", s.amp?.name ?: "")
        }
        if (supportAmp == null) {
            Pad({ st.go(Action.Open(Overlay.SupportAmpMenu)) }, Modifier.weight(1f).fillMaxHeight(), arrayOf("dual.support.choose"), sel = Sel.AMBER_PICKER) {
                LaneLabel("SUPPORT", "+ choose amp")
            }
        } else {
            Pad({ st.go(Action.FocusLane(Lane.SUPPORT)) }, Modifier.weight(1f).fillMaxHeight(), arrayOf("dual.focus.support"), selected = s.ui.lane == Lane.SUPPORT,
                sel = Sel.AMBER_PICKER, role = Role.Tab) {
                LaneLabel("SUPPORT", supportAmp.name)
            }
        }
    }
}

@Composable
private fun LaneLabel(lane: String, amp: String) {
    Column(Modifier.fillMaxSize().padding(horizontal = 8.dp), verticalArrangement = Arrangement.Center) {
        FitText(lane, Type.label(11.sp, Ink.text), Modifier.fillMaxWidth().height(16.dp))
        FitText(amp, Type.value(12.sp, Ink.dim), Modifier.fillMaxWidth().height(18.dp))
    }
}

private val stockCabs = listOf(Cab.NONE, Cab.G12, Cab.G65, Cab.V30)

@Composable
private fun CabRow(s: VoLumState, support: Boolean, modifier: Modifier) {
    val st = LocalStage.current
    val laneAmp = if (support) s.library.amp(s.rig.support.ampId) else s.amp
    val channel = if (support) s.rig.support.channel else s.rig.channel
    val cab = if (support) s.rig.support.cab else s.rig.cab
    val irId = if (support) s.rig.support.irId else s.rig.irId
    val irAllowed = laneAmp != null && channel in laneAmp.noCabChannels
    Row(modifier, horizontalArrangement = Arrangement.spacedBy(4.dp)) {
        stockCabs.forEachIndexed { i, c ->
            val label = if (laneAmp?.custom == true) laneAmp.cabNames.getOrElse(i) { c.label } else c.label
            val ids = if (support) arrayOf("dual.support.cab") else arrayOf("amp.cab.${c.name.lowercase()}")
            TextPad(label, { st.go(if (support) Action.SelectSupportCab(c) else Action.SelectCab(c)) }, Modifier.weight(1f).fillMaxHeight(),
                ids, selected = cab == c, enabled = laneAmp != null, sel = Sel.AMBER_PICKER, role = Role.Tab)
        }
        val irIds = if (support) "dual.support.cab.custom_ir" else "amp.cab.custom_ir"
        if (irAllowed) {
            Pad({ st.go(Action.Open(Overlay.IrMenu)) }, Modifier.weight(1.7f).fillMaxHeight(), arrayOf(irIds), selected = cab == Cab.CUSTOM_IR,
                sel = Sel.AMBER_PICKER, description = "Custom IR") {
                Column(Modifier.fillMaxSize().padding(horizontal = 6.dp), verticalArrangement = Arrangement.Center) {
                    FitText("Custom IR ▾", Type.label(12.sp, Ink.text), Modifier.fillMaxWidth().height(18.dp))
                    FitText(s.library.ir(irId)?.name ?: "choose", Type.value(10.sp, Ink.dim), Modifier.fillMaxWidth().height(14.dp))
                }
            }
        } else {
            Box(Modifier.weight(1.7f).fillMaxHeight().clip(RoundedCornerShape(6.dp)).background(Ink.panel).featureId("${irIds}_disabled"),
                contentAlignment = Alignment.Center) {
                Column(Modifier.fillMaxSize().padding(horizontal = 6.dp), verticalArrangement = Arrangement.Center) {
                    FitText("Custom IR", Type.label(12.sp, Ink.faint), Modifier.fillMaxWidth().height(18.dp))
                    FitText("needs a No Cab capture", Type.value(9.sp, Ink.faint), Modifier.fillMaxWidth().height(14.dp))
                }
            }
        }
    }
}

@Composable
private fun AmpSwitches(s: VoLumState, support: Boolean, modifier: Modifier) {
    val st = LocalStage.current
    val gate = if (support) Param.SUPPORT_GATE_ON else Param.GATE_ON
    val eq = if (support) Param.SUPPORT_EQ_ON else Param.EQ_ON
    Column(modifier, verticalArrangement = Arrangement.spacedBy(6.dp)) {
        Switch("NOISE GATE", s.on(gate), { st.go(Action.ToggleParam(gate)) }, Modifier.fillMaxWidth().height(48.dp),
            arrayOf(if (support) "dual.support.gate.toggle" else "amp.gate.toggle"))
        Row(Modifier.fillMaxWidth().height(48.dp), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            Switch("EQ", s.on(eq), { st.go(Action.ToggleParam(eq)) }, Modifier.weight(1f).fillMaxHeight(),
                arrayOf(if (support) "dual.support.eq.toggle" else "amp.eq.toggle"))
            if (s.dual) {
                Switch("Ø", s.on(Param.SUPPORT_POLARITY), { st.go(Action.ToggleParam(Param.SUPPORT_POLARITY)) }, Modifier.weight(1f).fillMaxHeight(),
                    arrayOf("dual.polarity"))
            }
        }
        if (s.outputSafety) SafetyNotice(Modifier.fillMaxWidth().weight(1f))
    }
}

@Composable
private fun SafetyNotice(modifier: Modifier) {
    Box(modifier.clip(RoundedCornerShape(6.dp)).background(Ink.alert.copy(alpha = 0.3f)).featureId("status.output_safety").padding(horizontal = 8.dp),
        contentAlignment = Alignment.Center) {
        FitText("OUTPUT SAFETY · limiting", Type.label(10.sp, Ink.hot), Modifier.fillMaxWidth())
    }
}