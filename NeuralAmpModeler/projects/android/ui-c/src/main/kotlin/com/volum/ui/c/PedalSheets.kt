package com.volum.ui.c

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.getValue
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.Density
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.DIVISIONS
import com.volum.core.KnobRows
import com.volum.core.Overlay
import com.volum.core.Param
import com.volum.core.PostBlock
import com.volum.core.PreBlock
import com.volum.core.Reducer
import com.volum.core.Section
import com.volum.design.art.Motif
import com.volum.design.components.PedalCard
import com.volum.design.components.VoLumBlockHeader
import com.volum.design.components.VoLumGlyph
import com.volum.design.components.VoLumIcon
import com.volum.design.components.VoLumSelectionStyle
import com.volum.design.components.hairlineFrame
import com.volum.design.components.volumGlyph
import com.volum.design.components.volumSelection
import com.volum.design.theme.VFitText
import com.volum.design.theme.VText
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumType

internal val OriginRow = 56.dp
private val ChipW = 140.dp
private val PostSlotW = 150.dp

/** One pedal of a sheet: the `:design` card focuses it, its LED is its power. */
private class Card(val motif: Motif, val title: String, val power: Param, val focusId: String, val powerId: String, val focused: Boolean, val focus: Action)

/**
 * The sheet's first row: four pedal cards of one width, then [fixed] dp of other
 * controls in [others] more children. Every card title shares the size at which
 * the longest one fits its footer.
 */
@Composable
private fun OriginCards(ui: Ui, cards: List<Card>, fixed: Dp, others: Int, trailing: @Composable RowScope.() -> Unit) {
    BoxWithConstraints(Modifier.fillMaxWidth().height(OriginRow)) {
        val card = (maxWidth - fixed - CardGap * (cards.size + others - 1)) / cards.size
        val k = sharedFit(cards.map { it.title to (card - 34.dp) }, VoLumType.label(11.5.sp))
        val d = LocalDensity.current
        Row(Modifier.fillMaxSize(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(CardGap)) {
            cards.forEach { c ->
                val on = ui.s.on(c.power)
                CompositionLocalProvider(LocalDensity provides Density(d.density, d.fontScale * k)) {
                    PedalCard(
                        c.motif,
                        c.title,
                        on,
                        onClick = { ui.d(c.focus) },
                        onToggle = { ui.d(Action.ToggleParam(c.power)) },
                        modifier = Modifier.width(card).fillMaxHeight().ids(c.focusId),
                        focused = c.focused,
                        ledModifier = Modifier.semantics { contentDescription = c.title + " power" }.ids(c.powerId),
                    )
                }
            }
            trailing()
        }
    }
}

private val CardGap = 6.dp
private val HeaderW = 96.dp
private val StoreW = 96.dp

private fun headerWidth(ui: Ui, section: Section): Dp {
    val locked = (if (section == Section.PRE) ui.s.locks.pre else ui.s.locks.post) != null
    return HeaderW + if (locked && Reducer.lockDiffers(ui.s, section)) StoreW else 0.dp
}

/**
 * The block header of the sheet: title and padlock, then, while the locked block
 * differs from what this amp stores, a labelled STORE TO AMP in full teal.
 */
@Composable
private fun LockHeader(ui: Ui, section: Section) {
    val locked = (if (section == Section.PRE) ui.s.locks.pre else ui.s.locks.post) != null
    val pre = section == Section.PRE
    val title = if (pre) "PRE" else "POST"
    Row(verticalAlignment = Alignment.CenterVertically) {
        VoLumBlockHeader(title, locked, { ui.d(Action.SetLock(section, !locked)) }, Modifier.width(HeaderW), lockModifier = Modifier.ids(if (pre) "pre.lock" else "post.lock"))
        if (locked && Reducer.lockDiffers(ui.s, section)) {
            Tappable(
                { ui.d(Action.StoreLock(section)) },
                Modifier.size(StoreW, Metrics.target).semantics { contentDescription = "Store $title to this amp" },
                listOf(if (pre) "pre.store" else "post.store"),
            ) { pressed ->
                Row(
                    Modifier.fillMaxSize().padding(start = 6.dp, top = 8.dp, bottom = 8.dp).volumSelection(false, pressed, VoLumSelectionStyle.Brass, radius = 3.dp, inset = 0.dp)
                        .hairlineFrame(if (pressed) VoLumColors.Teal else VoLumColors.TealDim, radius = 3.dp).padding(horizontal = 6.dp),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(6.dp, Alignment.CenterHorizontally),
                ) {
                    VoLumIcon(VoLumGlyph.Store, color = VoLumColors.Teal)
                    Column(Modifier.weight(1f, fill = false)) {
                        VFitText("STORE", style = VoLumType.label(9.sp, VoLumColors.Teal), minSize = 6.sp)
                        VFitText("TO AMP", style = VoLumType.label(9.sp, VoLumColors.Teal), minSize = 6.sp)
                    }
                }
            }
        }
    }
}

/** A capture chooser on the PRE row: the slot and its capture; amber while its menu is open. */
@Composable
private fun CaptureChip(ui: Ui, slot: Int) {
    val p = if (slot == 1) Param.NAM1_CAPTURE else Param.NAM2_CAPTURE
    val overlay = Overlay.CaptureMenu(slot)
    val open = ui.s.ui.overlays.contains(overlay)
    Tappable(
        { ui.d(if (open) Action.Back else Action.Open(overlay)) },
        Modifier.width(ChipW).height(Metrics.target).semantics { contentDescription = "Change NAM $slot capture" },
        listOf(if (slot == 1) "pre.nam1.capture" else "pre.nam2.capture"),
    ) { pressed ->
        Row(
            Modifier.fillMaxHeight().volumSelection(open, pressed, VoLumSelectionStyle.ListTeal, radius = 4.dp, inset = 0.dp)
                .hairlineFrame(if (open) VoLumColors.Amber else if (pressed) VoLumColors.GoldDim else VoLumColors.BtnOffBorder, if (open) 1.4.dp else 1.dp, 4.dp)
                .padding(horizontal = 8.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(6.dp),
        ) {
            Column(Modifier.weight(1f)) {
                VText("NAM $slot", style = VoLumType.label(8.sp, VoLumColors.CreamDim))
                VFitText(captureName(ui, p), style = VoLumType.label(12.sp, VoLumColors.Gold), minSize = 8.sp)
            }
            val ink by rememberUpdatedState(if (open) VoLumColors.Amber else VoLumColors.GoldDim)
            Spacer(Modifier.size(10.dp, 8.dp).volumGlyph(if (open) VoLumGlyph.ChevronRight else VoLumGlyph.ChevronDown, { ink }))
        }
    }
}

@Composable
private fun Mode(ui: Ui, title: String, p: Param, ids: List<String?>, width: Dp = 230.dp) {
    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        VFitText(title.uppercase(), Modifier.width(50.dp), style = VoLumType.label(9.sp, VoLumColors.GoldDim), minSize = 6.sp)
        Pills(p.options, ui.s.param(p).toInt(), { ids.getOrNull(it) }, { ui.d(Action.SetParam(p, it.toDouble())) }, Modifier.width(width))
    }
}

@Composable
private fun RowScope.Knobs(ui: Ui, params: List<Param>, ids: (Param) -> List<String>, label: (Param) -> String = { it.label }, width: Dp = 84.dp) {
    Row(Modifier.weight(1f), horizontalArrangement = Arrangement.SpaceEvenly, verticalAlignment = Alignment.CenterVertically) {
        params.forEach { p ->
            Knob(
                label(p), ui.s.param(p), p.min, p.max, ids(p), { Fmt.of(p, it) }, { Fmt.parse(p, it) },
                { ui.d(Action.SetParam(p, it)) }, { ui.d(Action.ResetParam(p)) }, width = width,
            )
        }
    }
}

/** The tempo-sync divisions as a Brass grid, with the tempo it follows. */
@Composable
private fun Division(ui: Ui, p: Param, id: String) {
    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
        Column(Modifier.width(54.dp)) {
            VText("DIVISION", style = VoLumType.label(8.sp, VoLumColors.GoldDim))
            VText("%.0f".format(ui.s.metronome.bpm), style = VoLumType.value(13.sp, VoLumColors.Gold))
            VText("BPM", style = VoLumType.label(8.sp, VoLumColors.CreamDim))
        }
        Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
            DIVISIONS.chunked(4).forEachIndexed { row, items ->
                Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                    items.forEachIndexed { col, label ->
                        val i = row * 4 + col
                        Brass(label, ui.s.param(p).toInt() == i, listOf(id), { ui.d(Action.SetParam(p, i.toDouble())) }, minWidth = 52.dp)
                    }
                }
            }
        }
    }
}

private val preKnobIds = mapOf(
    Param.PITCH_SEMI to "pre.pitch.semi", Param.PITCH_MIX to "pre.pitch.mix",
    Param.PITCH_OCT_DOWN to "pre.pitch.octdown", Param.PITCH_OCT_UP to "pre.pitch.octup", Param.PITCH_DRY to "pre.pitch.dry",
    Param.COMP_AMOUNT to "pre.comp.input", Param.COMP_ATTACK to "pre.comp.attack", Param.COMP_RELEASE to "pre.comp.release", Param.COMP_LEVEL to "pre.comp.output",
    Param.NAM1_GAIN to "pre.nam1.gain", Param.NAM1_BASS to "pre.nam1.bass", Param.NAM1_MID to "pre.nam1.mid", Param.NAM1_MID_HZ to "pre.nam1.midhz", Param.NAM1_TREBLE to "pre.nam1.treble", Param.NAM1_LEVEL to "pre.nam1.level",
    Param.NAM2_GAIN to "pre.nam2.gain", Param.NAM2_BASS to "pre.nam2.bass", Param.NAM2_MID to "pre.nam2.mid", Param.NAM2_MID_HZ to "pre.nam2.midhz", Param.NAM2_TREBLE to "pre.nam2.treble", Param.NAM2_LEVEL to "pre.nam2.level",
)

/** PRE sheet. Its first row (pedals and both capture choosers) is where the capture menu opens from. */
@Composable
internal fun PreSheet(ui: Ui) {
    val s = ui.s
    val focus = s.ui.preFocus
    val transpose = s.param(Param.PITCH_MODE) == 0.0
    OriginCards(
        ui,
        listOf(
            Card(Motif.PITCH, if (transpose) "%+d st".format(s.param(Param.PITCH_SEMI).toInt()) else "Octaver", Param.PITCH_ON, "pre.card.pitch", "pre.pitch.on", focus == PreBlock.PITCH, Action.FocusPre(PreBlock.PITCH)),
            Card(Motif.COMP, "Compressor", Param.COMP_ON, "pre.card.comp", "pre.comp.on", focus == PreBlock.COMP, Action.FocusPre(PreBlock.COMP)),
            Card(Motif.NAM1, captureName(ui, Param.NAM1_CAPTURE), Param.NAM1_ON, "pre.card.nam1", "pre.nam1.on", focus == PreBlock.NAM1, Action.FocusPre(PreBlock.NAM1)),
            Card(Motif.NAM2, captureName(ui, Param.NAM2_CAPTURE), Param.NAM2_ON, "pre.card.nam2", "pre.nam2.on", focus == PreBlock.NAM2, Action.FocusPre(PreBlock.NAM2)),
        ),
        fixed = ChipW * 2 + headerWidth(ui, Section.PRE),
        others = 3,
    ) {
        CaptureChip(ui, 1)
        CaptureChip(ui, 2)
        LockHeader(ui, Section.PRE)
    }
    Row(Modifier.fillMaxWidth().height(110.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(12.dp)) {
        when (focus) {
            PreBlock.PITCH -> Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Mode(ui, "Mode", Param.PITCH_MODE, listOf("pre.pitch.mode.transpose", "pre.pitch.mode.octaver"))
                if (transpose) Mode(ui, "Character", Param.PITCH_CHAR, listOf(null, "pre.pitch.char.instant", "pre.pitch.char.poly"))
                else Mode(ui, "Voicing", Param.PITCH_VOICING, listOf("pre.pitch.voicing.vintage", "pre.pitch.voicing.modern"))
            }
            PreBlock.COMP -> Note("Evens out your picking before the amp. INPUT sets how hard it squeezes.", Modifier.width(220.dp))
            PreBlock.NAM1, PreBlock.NAM2 -> Column(Modifier.width(200.dp)) {
                VText(if (focus == PreBlock.NAM1) "NAM 1" else "NAM 2", style = VoLumType.label(10.sp, VoLumColors.GoldDim))
                VFitText(captureName(ui, if (focus == PreBlock.NAM1) Param.NAM1_CAPTURE else Param.NAM2_CAPTURE), style = VoLumType.label(21.sp, VoLumColors.Gold), minSize = 12.sp)
                Note("Change the capture with its chooser above.", maxLines = 2, size = 10.sp)
            }
        }
        Knobs(ui, KnobRows.forPre(focus, s), { p ->
            if (p == Param.PITCH_LEVEL) listOf(if (transpose) "pre.pitch.level" else "pre.pitch.oct_level") else listOfNotNull(preKnobIds[p])
        })
    }
}

private val postKnobIds = mapOf(
    Param.CHORUS_RATE to "post.chorus.rate", Param.CHORUS_DEPTH to "post.chorus.depth", Param.CHORUS_TONE to "post.chorus.tone", Param.CHORUS_WIDTH to "post.chorus.width", Param.CHORUS_MIX to "post.chorus.mix",
    Param.DELAY_TIME to "post.delay.time", Param.DELAY_FEEDBACK to "post.delay.feedback", Param.DELAY_MIX to "post.delay.mix", Param.DELAY_TONE to "post.delay.tone",
    Param.REVERB_MIX to "post.reverb.mix", Param.REVERB_DECAY to "post.reverb.decay", Param.REVERB_TONE to "post.reverb.tone", Param.REVERB_PRE_DELAY to "post.reverb.predelay", Param.REVERB_SHIMMER to "post.reverb.intensity",
    Param.TREM_RATE to "post.trem.rate", Param.TREM_DEPTH to "post.trem.depth", Param.TREM_SHAPE to "post.trem.shape", Param.TREM_MIX to "post.trem.mix", Param.TREM_XOVER to "post.trem.xover",
)

@Composable
internal fun PostSheet(ui: Ui) {
    val s = ui.s
    val focus = s.ui.postFocus
    val synced = (focus == PostBlock.DELAY && s.on(Param.DELAY_SYNC)) || (focus == PostBlock.TREM && s.on(Param.TREM_SYNC))
    fun title(base: String, p: Param) = base + "  " + p.options[s.param(p).toInt()].lowercase().replaceFirstChar { it.uppercase() }
    OriginCards(
        ui,
        listOf(
            Card(Motif.CHORUS, title("Chorus", Param.CHORUS_MODE), Param.CHORUS_ON, "post.card.chorus", "post.chorus.on", focus == PostBlock.CHORUS, Action.FocusPost(PostBlock.CHORUS)),
            Card(Motif.DELAY, title("Delay", Param.DELAY_MODE), Param.DELAY_ON, "post.card.delay", "post.delay.on", focus == PostBlock.DELAY, Action.FocusPost(PostBlock.DELAY)),
            Card(Motif.REVERB, title("Reverb", Param.REVERB_MODE), Param.REVERB_ON, "post.card.reverb", "post.reverb.on", focus == PostBlock.REVERB, Action.FocusPost(PostBlock.REVERB)),
            Card(Motif.TREM, title("Trem", Param.TREM_MODE), Param.TREM_ON, "post.card.trem", "post.trem.on", focus == PostBlock.TREM, Action.FocusPost(PostBlock.TREM)),
        ),
        fixed = PostSlotW + headerWidth(ui, Section.POST),
        others = 2,
    ) {
        Row(Modifier.width(PostSlotW), horizontalArrangement = Arrangement.spacedBy(6.dp, Alignment.End)) {
            when (focus) {
                PostBlock.DELAY -> {
                    CompactToggle("Ping-Pong", s.on(Param.DELAY_PING_PONG), listOf("post.delay.pingpong"), { ui.d(Action.ToggleParam(Param.DELAY_PING_PONG)) })
                    CompactToggle("Tempo Sync", s.on(Param.DELAY_SYNC), listOf("post.delay.sync"), { ui.d(Action.ToggleParam(Param.DELAY_SYNC)) })
                }
                PostBlock.TREM -> CompactToggle("Tempo Sync", s.on(Param.TREM_SYNC), listOf("post.trem.sync"), { ui.d(Action.ToggleParam(Param.TREM_SYNC)) })
                else -> Unit
            }
        }
        LockHeader(ui, Section.POST)
    }
    Row(Modifier.fillMaxWidth().height(110.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(12.dp)) {
        when (focus) {
            PostBlock.CHORUS -> Mode(ui, "Mode", Param.CHORUS_MODE, listOf("post.chorus.mode.classic", "post.chorus.mode.warped", "post.chorus.mode.clear", "post.chorus.mode.ensemble"), 290.dp)
            PostBlock.DELAY -> {
                Mode(ui, "Mode", Param.DELAY_MODE, listOf("post.delay.mode.digital", "post.delay.mode.analog", "post.delay.mode.reverse"), if (synced) 170.dp else 220.dp)
                if (synced) Division(ui, Param.DELAY_DIVISION, "post.delay.division")
            }
            PostBlock.REVERB -> Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Mode(ui, "Mode", Param.REVERB_MODE, listOf("post.reverb.mode.hall", "post.reverb.mode.plate", "post.reverb.mode.oktaverb"))
                if (s.param(Param.REVERB_MODE) == 2.0) Mode(ui, "Voice", Param.REVERB_SUB, listOf("post.reverb.sub.halo", "post.reverb.sub.shimmer", "post.reverb.sub.bloom"))
            }
            PostBlock.TREM -> {
                Mode(ui, "Mode", Param.TREM_MODE, listOf("post.trem.mode.optical", "post.trem.mode.bias", "post.trem.mode.harmonic"), if (synced) 170.dp else 220.dp)
                if (synced) Division(ui, Param.TREM_DIVISION, "post.trem.division")
            }
        }
        val delayChar = KnobRows.delayCharacterLabel(s)
        Knobs(
            ui,
            KnobRows.forPost(focus, s),
            { p -> if (p == Param.DELAY_AGE) listOf("post.delay." + delayChar.lowercase()) else listOfNotNull(postKnobIds[p]) },
            { p -> if (p == Param.DELAY_AGE) delayChar else p.label },
            if (synced) 72.dp else 84.dp,
        )
    }
}
