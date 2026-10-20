package com.volum.ui.c

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.Cab
import com.volum.core.KnobRows
import com.volum.core.Lane
import com.volum.core.Overlay
import com.volum.core.Param
import com.volum.design.art.AmpThumbnail
import com.volum.design.components.VoLumChannelStepper
import com.volum.design.components.VoLumGlyph
import com.volum.design.components.VoLumIrCabButton
import com.volum.design.components.VoLumSelectionStyle
import com.volum.design.components.hairlineFrame
import com.volum.design.components.volumGlyph
import com.volum.design.components.volumSelection
import com.volum.design.theme.VFitText
import com.volum.design.theme.VText
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumType

private val knobIds = mapOf(
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

/**
 * AMP sheet. Its first row is the amp selector, channel and cab row: the row the amp
 * list and the Custom IR menu open from, and the row the sheet folds down to while
 * they are open. With Dual Amp on, the lane picked on the art retargets every control.
 */
@Composable
internal fun AmpSheet(ui: Ui) {
    val s = ui.s
    val support = s.supportFocused
    val amp = if (support) s.library.amp(s.rig.support.ampId) else s.amp
    Row(Modifier.fillMaxWidth().height(OriginRow), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(10.dp)) {
        AmpSelector(ui, support)
        val channels = amp?.channels ?: 0
        val ch = if (support) s.rig.support.channel else s.rig.channel
        Caption("CH", color = VoLumColors.CreamDim)
        val chId = listOf(if (support) "dual.support.channel" else "amp.channel")
        val pick: (Int) -> Unit = { c -> ui.d(if (support) Action.SelectSupportChannel(c) else Action.SelectChannel(c)) }
        VoLumChannelStepper(
            if (channels == 0) "-" else "${ch + 1}",
            onPrev = { if (channels > 0) pick((ch - 1 + channels) % channels) },
            onNext = { if (channels > 0) pick((ch + 1) % channels) },
            prevModifier = Modifier.semantics { contentDescription = "Previous channel" }.ids(chId),
            nextModifier = Modifier.semantics { contentDescription = "Next channel" }.ids(chId),
        )
        Gap(w = 4.dp)
        CabRow(ui, support)
    }
    Row(Modifier.fillMaxWidth().height(110.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        val gate = if (support) Param.SUPPORT_GATE_ON else Param.GATE_ON
        val eq = if (support) Param.SUPPORT_EQ_ON else Param.EQ_ON
        Column(Modifier.width(150.dp).fillMaxHeight(), verticalArrangement = Arrangement.SpaceEvenly) {
            Toggle("Noise Gate", s.on(gate), listOf(if (support) "dual.support.gate.toggle" else "amp.gate.toggle"), { ui.d(Action.ToggleParam(gate)) })
            Toggle("EQ", s.on(eq), listOf(if (support) "dual.support.eq.toggle" else "amp.eq.toggle"), { ui.d(Action.ToggleParam(eq)) })
        }
        Column(Modifier.width(150.dp).fillMaxHeight(), verticalArrangement = Arrangement.SpaceEvenly) {
            Toggle("Dual Amp", s.dual, listOf("dual.toggle"), { ui.d(Action.ToggleParam(Param.DUAL_ON)) })
            val irAllowed = amp != null && (if (support) s.rig.support.channel else s.rig.channel) in amp.noCabChannels
            when {
                support -> Toggle("Ø Phase", s.on(Param.SUPPORT_POLARITY), listOf("dual.polarity"), { ui.d(Action.ToggleParam(Param.SUPPORT_POLARITY)) })
                amp != null && !irAllowed -> Note("Custom IR needs a No Cab capture on this channel.", color = VoLumColors.CreamDim, maxLines = 3, size = 10.sp)
                else -> Gap(h = Metrics.target)
            }
        }
        Row(Modifier.weight(1f), horizontalArrangement = Arrangement.SpaceEvenly, verticalAlignment = Alignment.CenterVertically) {
            KnobRows.forAmp(if (support) Lane.SUPPORT else Lane.MAIN, s.dual).forEach { p ->
                Knob(
                    p.label, s.param(p), p.min, p.max, knobIds[p].orEmpty(), { Fmt.of(p, it) }, { Fmt.parse(p, it) },
                    { ui.d(Action.SetParam(p, it)) }, { ui.d(Action.ResetParam(p)) },
                    support = support || p == Param.SUPPORT_PAN, width = 76.dp,
                )
            }
        }
    }
}

/** The amp selector: art thumbnail, name, chevron. Amber while its list is open; a second tap closes the list. */
@Composable
private fun AmpSelector(ui: Ui, support: Boolean) {
    val s = ui.s
    val amp = if (support) s.library.amp(s.rig.support.ampId) else s.amp
    val overlay: Overlay = if (support) Overlay.SupportAmpMenu else Overlay.AmpPicker
    val open = s.ui.overlays.contains(overlay)
    Tappable(
        { ui.d(if (open) Action.Back else Action.Open(overlay)) },
        Modifier.height(Metrics.target).widthIn(min = 170.dp, max = 220.dp).semantics { contentDescription = if (support) "Choose support amp" else "Choose amp" },
        listOf(if (support) "dual.support.choose" else "amp.name"),
    ) { pressed ->
        Row(
            Modifier.fillMaxHeight().padding(vertical = 4.dp)
                .volumSelection(open, pressed, VoLumSelectionStyle.ListTeal, radius = 4.dp, inset = 0.dp)
                .hairlineFrame(if (open) VoLumColors.Amber else if (pressed) VoLumColors.GoldDim else VoLumColors.BtnOffBorder, if (open) 1.4.dp else 1.dp, 4.dp)
                .padding(horizontal = 8.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            if (amp != null) AmpThumbnail(artOf(amp), Modifier.size(26.dp), true)
            Column(Modifier.weight(1f)) {
                VFitText(amp?.name ?: "Choose support amp", style = VoLumType.label(13.sp, if (support) VoLumColors.Teal else VoLumColors.Gold), minSize = 9.sp)
                VText(if (support) "SUPPORT AMP" else if (amp?.custom == true) "CUSTOM AMP" else "AMP", style = VoLumType.label(8.sp, VoLumColors.CreamDim))
            }
            val ink by rememberUpdatedState(if (open) VoLumColors.Amber else VoLumColors.GoldDim)
            androidx.compose.foundation.layout.Spacer(Modifier.size(10.dp, 8.dp).volumGlyph(if (open) VoLumGlyph.ChevronRight else VoLumGlyph.ChevronDown, { ink }))
        }
    }
}

/** The desktop cab row: Brass buttons, then the Custom IR button that opens its menu (greyed with its reason when the channel has no No Cab capture). */
@Composable
private fun CabRow(ui: Ui, support: Boolean) {
    val s = ui.s
    val amp = if (support) s.library.amp(s.rig.support.ampId) else s.amp
    val cab = if (support) s.rig.support.cab else s.rig.cab
    val channel = if (support) s.rig.support.channel else s.rig.channel
    val irName = s.library.ir(if (support) s.rig.support.irId else s.rig.irId)?.name
    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(4.dp)) {
        listOf(Cab.NONE to "amp.cab.none", Cab.G12 to "amp.cab.g12", Cab.G65 to "amp.cab.g65", Cab.V30 to "amp.cab.v30").forEach { (c, id) ->
            Brass(cabLabel(c, amp), cab == c, listOf(if (support) "dual.support.cab" else id), { ui.d(if (support) Action.SelectSupportCab(c) else Action.SelectCab(c)) }, enabled = amp != null)
        }
        val irAllowed = amp != null && channel in amp.noCabChannels
        val open = s.ui.overlays.contains(Overlay.IrMenu)
        VoLumIrCabButton(
            cab == Cab.CUSTOM_IR || open,
            { ui.d(if (open) Action.Back else Action.Open(Overlay.IrMenu)) },
            Modifier.ids(
                when {
                    support && irAllowed -> "dual.support.cab.custom_ir"
                    support -> "dual.support.cab.custom_ir_disabled"
                    irAllowed -> "amp.cab.custom_ir"
                    else -> "amp.cab.custom_ir_disabled"
                },
            ),
            irName = if (cab == Cab.CUSTOM_IR) irName else null,
            enabled = irAllowed,
        )
    }
}
