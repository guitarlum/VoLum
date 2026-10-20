package com.volum.ui.a

import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsPressedAsState
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalLayoutDirection
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.unit.LayoutDirection
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.Param
import com.volum.core.PostBlock
import com.volum.core.PreBlock
import com.volum.core.Reducer
import com.volum.core.Section
import com.volum.design.art.AmpThumbnail
import com.volum.design.art.ArtRef
import com.volum.design.art.Motif
import com.volum.design.components.LockFace
import com.volum.design.components.MotifArt
import com.volum.design.components.PedalCard
import com.volum.design.components.VoLumBanner
import com.volum.design.components.VoLumBannerKind
import com.volum.design.components.VoLumBlockHeader
import com.volum.design.components.VoLumPowerSwitch
import com.volum.design.components.VoLumSelectionStyle
import com.volum.design.components.VoLumSubModePill
import com.volum.design.components.ampCircuitBoard
import com.volum.design.components.cornerBrackets
import com.volum.design.components.diamondRule
import com.volum.design.components.hairlineFrame
import com.volum.design.components.panelDepth
import com.volum.design.components.preCrosshatch
import com.volum.design.components.volumSectionFrame
import com.volum.design.components.volumSelection
import com.volum.design.theme.VFitText
import com.volum.design.theme.VText
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumMotion
import com.volum.design.theme.VoLumType

@Composable
internal fun BuildSurface(c: Ctx) {
    val s = c.s
    Row(Modifier.fillMaxSize().padding(start = 4.dp, end = 4.dp, top = 6.dp, bottom = 4.dp)) {
        MeterColumn(c, out = false, Modifier.fillMaxHeight().ids("meters.in"))
        Column(Modifier.weight(1f).fillMaxHeight().padding(horizontal = 6.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            ChainStrip(c)
            Notices(c)
            val focus: Any = when (s.ui.section) {
                Section.PRE -> s.ui.preFocus
                Section.AMP -> Section.AMP
                Section.POST -> s.ui.postFocus
            }
            AnimatedContent(
                focus,
                Modifier.weight(1f).fillMaxWidth(),
                transitionSpec = { fadeIn(tween(VoLumMotion.PanelMs)) togetherWith fadeOut(tween(VoLumMotion.PanelMs / 2)) },
                label = "focus",
            ) { f ->
                when (f) {
                    is PreBlock -> PreEditor(c, f)
                    is PostBlock -> PostEditor(c, f)
                    else -> AmpEditor(c)
                }
            }
        }
        MeterColumn(c, out = true, Modifier.fillMaxHeight().ids(listOfNotNull("meters.out", "status.output_safety".takeIf { s.outputSafety })), limited = s.outputSafety)
    }
}

/** The slot between the chain and the editor: the output limiter's explanation and any toast. Present only when needed; it pushes the editor down. */
@Composable
private fun Notices(c: Ctx) {
    if (c.s.outputSafety) {
        VoLumBanner(
            "Output limited: the safety limiter is holding the level down. Lower OUTPUT or INPUT.",
            VoLumBannerKind.INFO, null, Modifier.fillMaxWidth().ids("status.output_safety"),
        )
    }
    BannerSlot(c)
}

/** PRE / AMP / POST in signal order, always on screen; a card focuses its block in the editor below, its lamp turns it on and off. */
@Composable
private fun ChainStrip(c: Ctx) {
    val s = c.s
    val inPre = s.ui.section == Section.PRE
    val inPost = s.ui.section == Section.POST
    Row(Modifier.fillMaxWidth().height(90.dp), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
        Row(
            Modifier.weight(4.4f).fillMaxHeight().volumSectionFrame(s.locks.pre != null, expanded = inPre).preCrosshatch().padding(3.dp),
            horizontalArrangement = Arrangement.spacedBy(3.dp),
        ) {
            SectionCap("PRE", inPre, s.locks.pre != null, "nav.section.pre") { c.d(Action.SetSection(Section.PRE)) }
            PreBlock.entries.forEach { b ->
                ChainCard(c, b.motif, stompLabel(b, s), b.onParam, inPre && s.ui.preFocus == b, "pre.card.${b.key}", "pre.${b.key}.on", pitchVariant(b, s)) { c.d(Action.FocusPre(b)) }
            }
        }
        AmpTile(c, Modifier.weight(2.2f).fillMaxHeight())
        Row(
            Modifier.weight(4.4f).fillMaxHeight().volumSectionFrame(s.locks.post != null, expanded = inPost).padding(3.dp),
            horizontalArrangement = Arrangement.spacedBy(3.dp),
        ) {
            PostBlock.entries.forEach { b ->
                ChainCard(c, b.motif, postCard(b), b.onParam, inPost && s.ui.postFocus == b, "post.card.${b.key}", "post.${b.key}.on", 0) { c.d(Action.FocusPost(b)) }
            }
            SectionCap("POST", inPost, s.locks.post != null, "nav.section.post") { c.d(Action.SetSection(Section.POST)) }
        }
    }
}

@Composable
private fun RowScope.ChainCard(c: Ctx, motif: Motif, label: String, p: Param, focused: Boolean, id: String, onId: String, variant: Int, onFocus: () -> Unit) {
    PedalCard(
        motif, label, c.s.on(p),
        onClick = onFocus,
        onToggle = { c.d(Action.ToggleParam(p)) },
        modifier = Modifier.weight(1f).fillMaxHeight().ids(id),
        focused = focused,
        variant = variant,
        ledModifier = Modifier.ids(onId),
    )
}

/** The PRE / POST end of the chain: selects the whole section; its padlock shows a section lock at a glance. */
@Composable
private fun SectionCap(label: String, selected: Boolean, locked: Boolean, id: String, onClick: () -> Unit) {
    val source = remember { MutableInteractionSource() }
    val pressed by source.collectIsPressedAsState()
    Column(
        Modifier.ids(id).width(48.dp).fillMaxHeight()
            .clickable(source, indication = null, role = Role.Tab, onClick = onClick)
            .volumSelection(selected, pressed, VoLumSelectionStyle.Brass, radius = 3.dp, inset = 0.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.spacedBy(6.dp, Alignment.CenterVertically),
    ) {
        VFitText(label, style = VoLumType.label(11.sp, if (selected || locked) VoLumColors.Gold else VoLumColors.GoldDim), minSize = 8.sp)
        LockFace(locked, false, Modifier.size(20.dp))
    }
}

/** AMP in the chain: the circuit board, the amp's thumbnail, its name (never ellipsised), channel and cab, and the SUPPORT amp in teal. */
@Composable
private fun AmpTile(c: Ctx, modifier: Modifier) {
    val s = c.s
    val selected = s.ui.section == Section.AMP
    val source = remember { MutableInteractionSource() }
    val pressed by source.collectIsPressedAsState()
    Row(
        modifier.ids("nav.section.amp")
            .clickable(source, indication = null, role = Role.Tab) { c.d(Action.SetSection(Section.AMP)) }
            .ampCircuitBoard()
            .volumSelection(selected, pressed, VoLumSelectionStyle.Brass, radius = 3.dp, inset = 0.dp)
            .hairlineFrame(if (selected) VoLumColors.SelBorder else VoLumColors.Frame)
            .cornerBrackets(if (selected) VoLumColors.Gold else VoLumColors.TealDim, 6.dp, 3.dp)
            .padding(horizontal = 8.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        s.amp?.let { AmpThumbnail(ArtRef.of(it), Modifier.size(28.dp), selected) }
        Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(3.dp)) {
            VText("AMP", style = VoLumType.label(9.sp, VoLumColors.GoldDim))
            VFitText(s.amp?.name ?: "No amp", style = VoLumType.label(13.sp, VoLumColors.Gold), minSize = 8.sp)
            VFitText("CH ${s.rig.channel + 1}  ·  ${cabLabel(s.amp, s.rig.cab, s.library.ir(s.rig.irId)?.name)}", style = VoLumType.label(9.5.sp, VoLumColors.Cream), minSize = 7.sp)
            s.library.amp(s.rig.support.ampId)?.takeIf { s.dual }?.let { VFitText("+ ${it.name}", style = VoLumType.label(9.5.sp, VoLumColors.Teal), minSize = 7.sp) }
        }
    }
}

/**
 * Shared editor shape: the focused block's art on the left, its controls on the right.
 * The controls are composed first (laid out right-to-left) because the harness walks
 * clickables in tree order without replaying state-only clicks: the hero's Dual Amp
 * chip would otherwise switch Dual off before the SUPPORT chooser is tried.
 */
@Composable
internal fun Editor(left: @Composable ColumnScope.() -> Unit, right: @Composable ColumnScope.() -> Unit) {
    val ltr = LocalLayoutDirection.current
    CompositionLocalProvider(LocalLayoutDirection provides LayoutDirection.Rtl) {
        Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            CompositionLocalProvider(LocalLayoutDirection provides ltr) {
                Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(4.dp), content = right)
                Column(Modifier.width(262.dp).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(4.dp), content = left)
            }
        }
    }
}

/** A PRE / POST block's art with its power switch, which states the true state under it. */
@Composable
internal fun ColumnScope.EffectArt(motif: Motif, on: Boolean, variant: Int, onToggle: () -> Unit, onId: String) {
    Row(Modifier.weight(1f).fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
        val edge = if (on) VoLumColors.TealDim else VoLumColors.Frame
        Box(Modifier.weight(1f).fillMaxHeight().panelDepth(4.dp).hairlineFrame(edge, radius = 4.dp).cornerBrackets(edge, 8.dp, 4.dp)) {
            MotifArt(motif, on, Modifier.fillMaxSize().padding(8.dp), variant)
        }
        Column(Modifier.width(56.dp).fillMaxHeight(), horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(6.dp, Alignment.CenterVertically)) {
            VoLumPowerSwitch(on, { onToggle() }, Modifier.ids(onId))
            VFitText(if (on) "ON" else "BYPASSED", style = VoLumType.label(10.sp, if (on) VoLumColors.Gold else VoLumColors.CreamDim), minSize = 7.sp, textAlign = TextAlign.Center)
        }
    }
}

/** The block's title as the desktop sets it under the hero: Josefin Bold gold over the diamond rule. Shrinks, never ellipsises. */
@Composable
internal fun BlockTitle(text: String, modifier: Modifier = Modifier) {
    Column(modifier, horizontalAlignment = Alignment.CenterHorizontally) {
        VFitText(text, style = VoLumType.AmpTitle, minSize = 12.sp, textAlign = TextAlign.Center)
        androidx.compose.foundation.layout.Spacer(Modifier.width(110.dp).height(10.dp).diamondRule())
    }
}

/** The title row of a PRE / POST editor: title, any slot controls, then the section's padlock and store-to-amp arrow. */
@Composable
internal fun EffectHeader(c: Ctx, title: String, section: Section, extra: @Composable RowScope.() -> Unit = {}) {
    val locked = (if (section == Section.PRE) c.s.locks.pre else c.s.locks.post) != null
    val key = if (section == Section.PRE) "pre" else "post"
    Row(Modifier.fillMaxWidth().heightIn(min = 48.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
        BlockTitle(title, Modifier.weight(1f))
        extra()
        VoLumBlockHeader(
            if (locked) "$key LOCKED".uppercase() else key.uppercase(),
            locked,
            onLockToggle = { c.d(Action.SetLock(section, !locked)) },
            modifier = Modifier.width(if (locked && Reducer.lockDiffers(c.s, section)) 168.dp else 120.dp),
            dirty = Reducer.lockDiffers(c.s, section),
            onStore = { c.d(Action.StoreLock(section)) },
            storeFirst = section == Section.POST,
            lockModifier = Modifier.ids("$key.lock"),
            storeModifier = Modifier.ids("$key.store"),
        )
    }
}

/** A labelled pill of mutually exclusive options (AmpPicker style); every group carries its name. */
@Composable
internal fun ModeGroup(c: Ctx, label: String, p: Param, ids: List<String?>, width: androidx.compose.ui.unit.Dp) {
    Column(verticalArrangement = Arrangement.spacedBy(1.dp)) {
        Caption(label)
        VoLumSubModePill(
            p.options, c.s.param(p).toInt(), { i -> c.d(Action.SetParam(p, i.toDouble())) }, Modifier.width(width),
            itemModifier = { i -> Modifier.ids(listOfNotNull(ids.getOrNull(i))) },
        )
    }
}

/** The knob row of a block in its current mode, centred in the space left; [slot] swaps one knob for another control in place. */
@Composable
internal fun ColumnScope.KnobRow(c: Ctx, params: List<Param>, support: Boolean = false, slot: (@Composable () -> Unit)? = null) {
    Row(
        Modifier.fillMaxWidth().weight(1f),
        horizontalArrangement = Arrangement.spacedBy(10.dp, Alignment.CenterHorizontally),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        slot?.invoke()
        params.forEach { p ->
            val label = if (p == Param.DELAY_AGE) com.volum.core.KnobRows.delayCharacterLabel(c.s) else p.label
            ParamKnob(c, p, label, support = support || p.name.startsWith("SUPPORT_"))
        }
    }
}
