package com.volum.ui.a

import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsPressedAsState
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
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
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.semantics.CustomAccessibilityAction
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.customActions
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.Overlay
import com.volum.core.PickKind
import com.volum.core.SettingsTab
import com.volum.design.art.AmpThumbnail
import com.volum.design.art.ArtRef
import com.volum.design.components.CrossButton
import com.volum.design.components.VoLumButton
import com.volum.design.components.VoLumChannelStepper
import com.volum.design.components.VoLumGlyph
import com.volum.design.components.VoLumIcon
import com.volum.design.components.VoLumSegmentSwitch
import com.volum.design.components.VoLumSelectionStyle
import com.volum.design.components.VoLumTextField
import com.volum.design.components.VoLumToggle
import com.volum.design.components.hairlineFrame
import com.volum.design.components.insetWell
import com.volum.design.components.panelDepth
import com.volum.design.components.twoDigits
import com.volum.design.components.volumSelection
import com.volum.design.theme.VFitText
import com.volum.design.theme.VText
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumType
import com.volum.design.theme.withOpacity

/** A settings group (`VoLumSettingsGroupFrameControl`): panel depth, brass hairline, the title in gold. */
@Composable
internal fun Card(title: String, modifier: Modifier = Modifier, content: @Composable ColumnScope.() -> Unit) {
    Column(
        modifier.panelDepth(4.dp).hairlineFrame(VoLumColors.Frame, radius = 4.dp).padding(horizontal = 12.dp, vertical = 8.dp),
        verticalArrangement = Arrangement.spacedBy(4.dp),
    ) {
        VText(title, style = VoLumType.label(13.sp, VoLumColors.Gold))
        content()
    }
}

private fun tabHint(t: SettingsTab) = when (t) {
    SettingsTab.SIGNAL -> "How audio gets in and out of VoLum"
    SettingsTab.MIDI -> "Which MIDI channel and recall CC this VoLum hears, and which Sound each program number plays"
    SettingsTab.SYSTEM -> "This build, your library, updates"
}

@Composable
internal fun Settings(c: Ctx, tab: SettingsTab) {
    Sheet("Settings", listOf("settings.close"), c, actions = {
        Row(Modifier.width(330.dp).hairlineFrame(VoLumColors.GoldDim.withOpacity(0.6f), radius = 3.dp)) {
            SettingsTab.entries.forEach { t ->
                Choice(t.name, t == tab, { c.d(Action.SetSettingsTab(t)) }, Modifier.weight(1f).ids("settings.tab.${t.name.lowercase()}"), VoLumSelectionStyle.AmberPicker, size = 12.sp)
            }
        }
    }) {
        Hint(tabHint(tab), Modifier.padding(bottom = 6.dp), lines = 1)
        Row(Modifier.fillMaxWidth().weight(1f), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            when (tab) {
                SettingsTab.SIGNAL -> SignalTab(c)
                SettingsTab.MIDI -> MidiTab(c)
                SettingsTab.SYSTEM -> SystemTab(c)
            }
        }
    }
}

@Composable
private fun RowScope.SignalTab(c: Ctx) {
    val s = c.s
    val sig = s.signal
    Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Row(Modifier.fillMaxWidth().weight(1f), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            Card("Input calibration", Modifier.weight(1f).fillMaxHeight()) {
                VoLumToggle(sig.calibrate, { c.d(Action.SetCalibrate(it)) }, Modifier.ids("signal.calibration.on"), label = "Calibrate input")
                var text by remember(sig.calibrationDbu) { mutableStateOf(f("%.1f", sig.calibrationDbu)) }
                Row(Modifier.alpha(if (sig.calibrate) 1f else 0.4f), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    VoLumTextField(
                        text, { text = it }, Modifier.width(110.dp).ids("signal.calibration.level"),
                        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal, imeAction = ImeAction.Done),
                        keyboardActions = KeyboardActions(onDone = { parseNumber(text)?.let { c.d(Action.SetCalibrationDbu(it)) } }),
                    )
                    VText("dBu at full scale", style = VoLumType.label(11.sp, VoLumColors.CreamDim))
                }
                val level = s.amp?.inputLevelDbu
                if (level == null) Hint("${s.amp?.name ?: "This model"} has no capture level, so calibration cannot apply.", Modifier.ids("signal.calibration.unavailable"))
                else Hint("This capture was made at ${f("%.1f", level)} dBu.")
            }
            Card("Output mode", Modifier.weight(1f).fillMaxHeight()) {
                val hasOut = s.amp?.outputLevelDbu != null
                listOf("Raw", "Normalized", "Calibrated").forEachIndexed { i, label ->
                    val enabled = i != 2 || hasOut
                    Choice(if (enabled) label else "$label (n/a)", sig.outputMode == i, { c.d(Action.SetOutputMode(i)) },
                        Modifier.fillMaxWidth().ids("signal.output." + label.lowercase()), VoLumSelectionStyle.Brass, enabled = enabled, size = 12.sp)
                }
            }
            Card("Performance", Modifier.weight(1f).fillMaxHeight()) {
                VoLumSegmentSwitch("FULL", "LITE", sig.lite, { c.d(Action.SetLite(it)) }, Modifier.fillMaxWidth(),
                    segmentModifier = { lite -> Modifier.ids(if (lite) "signal.perf.lite" else "signal.perf.full") })
                Hint(if (sig.lite) "Smaller A2 slice, lower CPU." else "The full A2 model.", lines = 1)
                Caption("ANIMATE ART IN PLAY")
                VoLumSegmentSwitch("OFF", "ON", sig.animateArt, { on -> if (on != sig.animateArt) c.d(Action.SetAnimateArt(on)) }, Modifier.fillMaxWidth(),
                    segmentModifier = { Modifier.ids("signal.animate_art") })
            }
        }
        VoLumButton("Audio & MIDI devices", { c.d(Action.Open(Overlay.AudioDevices)) }, Modifier.align(Alignment.CenterHorizontally).width(300.dp).ids("signal.audio_devices"))
    }
}

@Composable
private fun RowScope.MidiTab(c: Ctx) {
    val s = c.s
    val m = s.midi
    Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Card("What this VoLum listens to", Modifier.fillMaxWidth()) {
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                VoLumSegmentSwitch("All channels", "One channel", m.channel != 0, { one -> c.d(Action.SetMidiChannel(if (one) m.lastOneChannel else 0)) }, Modifier.width(250.dp),
                    segmentModifier = { one -> Modifier.ids(if (one) "midi.channel.one" else "midi.channel.all") })
                val ch = if (m.channel == 0) m.lastOneChannel else m.channel
                val chIds = Modifier.ids("midi.channel.number")
                Row(Modifier.alpha(if (m.channel == 0) 0.4f else 1f), verticalAlignment = Alignment.CenterVertically) {
                    Caption("CH")
                    VoLumChannelStepper(twoDigits(ch), { c.d(Action.SetMidiChannel(if (ch <= 1) 16 else ch - 1)) }, { c.d(Action.SetMidiChannel(if (ch >= 16) 1 else ch + 1)) },
                        prevModifier = chIds.desc("Previous MIDI channel"), nextModifier = chIds.desc("Next MIDI channel"))
                }
                Fill()
                Caption("RECALL CC")
                val ccIds = Modifier.ids("midi.recall_cc")
                VoLumChannelStepper("${m.recallCc}", { c.d(Action.SetRecallCc(if (m.recallCc <= 0) 119 else m.recallCc - 1)) }, { c.d(Action.SetRecallCc(if (m.recallCc >= 119) 0 else m.recallCc + 1)) },
                    prevModifier = ccIds.desc("Recall CC down"), nextModifier = ccIds.desc("Recall CC up"))
            }
            Row(horizontalArrangement = Arrangement.spacedBy(20.dp)) {
                Hint("MIDI calls All channels Omni. One guitarist, one board: leave it on All. Two VoLums on one cable: give each its own channel.", Modifier.weight(1f))
                Hint("Recall CC: the value is the program number. Use it when Program Change never arrives. Tap a slot below to choose its Sound.", Modifier.width(300.dp))
            }
        }
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            VText("What each program number plays", style = VoLumType.label(13.sp, VoLumColors.Gold))
            GlyphButton(VoLumGlyph.Prev, { c.d(Action.SetMidiBank(m.bank - 1)) }, "Previous bank", Modifier.ids("midi.bank.prev"), enabled = m.bank > 0, tint = VoLumColors.Gold)
            VText("BANK ${twoDigits(m.bank + 1)}", style = VoLumType.value(11.sp))
            GlyphButton(VoLumGlyph.Next, { c.d(Action.SetMidiBank(m.bank + 1)) }, "Next bank", Modifier.ids("midi.bank.next"), enabled = m.bank < 15, tint = VoLumColors.Gold)
            BankDots(m.bank)
            Fill()
            VText("Programs ${m.bank * 8}-${m.bank * 8 + 7}", style = VoLumType.label(10.sp, VoLumColors.CreamDim))
        }
        (0 until 8).chunked(4).forEach { row ->
            Row(Modifier.fillMaxWidth().weight(1f), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                row.forEach { i -> MidiSlot(c, m.bank * 8 + i, Modifier.weight(1f).fillMaxHeight()) }
            }
        }
    }
}

/** Sixteen banks of eight programs; the current bank gold (`VoLumMidiFootswitch` page dots). */
@Composable
private fun BankDots(bank: Int) {
    Spacer(Modifier.width(150.dp).height(10.dp).drawBehind {
        val step = size.width / 16
        for (i in 0 until 16) drawCircle(if (i == bank) VoLumColors.Gold else VoLumColors.Frame, if (i == bank) 3.5.dp.toPx() else 2.5.dp.toPx(), Offset(step * i + step / 2, size.height / 2))
    })
}

/**
 * One program switch: its number, LIVE when it is the playing Sound, the Sound and its
 * amp, or a "+" when empty. The whole slot chooses its Sound; clearing is set apart
 * behind a hairline on the right so a live tap does not hit it by accident.
 */
@Composable
private fun MidiSlot(c: Ctx, p: Int, modifier: Modifier) {
    val s = c.s
    val sound = s.library.sounds[p]
    val valid = s.library.soundValid(p)
    val live = s.library.live == p
    val amp = sound?.let { s.library.amp(it.ampId) }
    val move = Modifier.semantics { customActions = listOf(CustomAccessibilityAction("Move down") { c.d(Action.MoveSound(p, 1)); true }) }
    val source = remember { MutableInteractionSource() }
    val pressed by source.collectIsPressedAsState()
    val edge = when {
        sound != null && !valid -> VoLumColors.Danger.withOpacity(0.7f)
        live -> VoLumColors.SelBorder
        else -> VoLumColors.Frame
    }
    Row(modifier.panelDepth(4.dp).hairlineFrame(edge, radius = 4.dp)) {
        Row(
            Modifier.ids("midi.slot.assign", "midi.slot.move").then(move).weight(1f).fillMaxHeight()
                .clickable(source, indication = null, role = Role.Button, onClickLabel = "Choose the Sound of program $p") { c.d(Action.Open(Overlay.SoundPicker(p, replace = true))) }
                .volumSelection(live, pressed, VoLumSelectionStyle.Brass, radius = 4.dp, inset = 0.dp)
                .padding(horizontal = 8.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            when {
                sound == null -> VoLumIcon(VoLumGlyph.Plus, color = VoLumColors.GoldDim, size = 18.dp)
                !valid -> VText("!", Modifier.width(18.dp), style = VoLumType.label(18.sp, VoLumColors.Danger))
                amp != null -> AmpThumbnail(ArtRef.of(amp), Modifier.size(26.dp), live)
            }
            Column(Modifier.weight(1f)) {
                Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    VText(p.toString().padStart(3, '0'), style = VoLumType.value(10.sp, if (live) VoLumColors.Gold else VoLumColors.GoldDim))
                    if (live) VText("LIVE", style = VoLumType.label(9.sp, VoLumColors.Gold))
                }
                when {
                    sound == null -> VText("Empty", style = VoLumType.label(12.sp, VoLumColors.CreamDim))
                    !valid -> {
                        VText("Invalid slot", style = VoLumType.label(12.sp, VoLumColors.Danger))
                        VText("Its Sound is gone", style = VoLumType.label(9.sp, VoLumColors.Danger.withOpacity(0.8f)))
                    }
                    else -> {
                        VFitText(s.library.preset(sound.presetId)?.name ?: "", style = VoLumType.display(15.sp, VoLumColors.Cream), minSize = 10.sp)
                        VText(amp?.name ?: "", style = VoLumType.label(9.sp, VoLumColors.TealDim))
                    }
                }
            }
        }
        if (sound != null) {
            Box(Modifier.fillMaxHeight().width(1.dp).padding(vertical = 10.dp).drawBehind { drawRect(VoLumColors.Frame) })
            Box(Modifier.fillMaxHeight(), contentAlignment = Alignment.TopCenter) {
                CrossButton({ c.d(Action.ClearSound(p)) }, Modifier.ids("midi.slot.clear").desc("Clear program $p"), VoLumColors.CreamDim)
            }
        }
    }
}

@Composable
private fun RowScope.SystemTab(c: Ctx) {
    val s = c.s
    val a = s.audio
    Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(10.dp)) {
        Card("Model information", Modifier.fillMaxWidth().weight(1f).ids("system.model_info")) {
            val latency = a.bufferFrames * 1000.0 / a.sampleRate
            Hint("Playing ${s.amp?.name ?: "-"}  ·  ${if (s.signal.lite) "A2 Lite" else "A2 Full"} model", color = VoLumColors.Cream, lines = 1)
            Hint("Model rate: ${a.sampleRate} Hz", color = VoLumColors.Cream, lines = 1)
            Hint("Buffer: ${a.bufferFrames} samples  ·  ${f("%.1f", latency)} ms per buffer; the real round trip is higher", color = VoLumColors.Cream, lines = 2)
        }
        Card("Back up your library", Modifier.fillMaxWidth().weight(1f)) {
            Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                VoLumButton("Export Pack", { c.d(Action.Open(Overlay.PackExport)) }, Modifier.weight(1f).ids("pack.export.open"))
                VoLumButton("Import Pack", { c.d(Action.PickFiles(PickKind.PACK_OPEN)) }, Modifier.weight(1f).ids("pack.import.open"))
            }
            Hint("Move your custom amps, presets, IRs and pedals to another phone, or share part of your library.")
        }
    }
    Card("VoLum  ·  By Lum", Modifier.weight(1f).fillMaxHeight()) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            VText("Version 1.3  ·  Android preview", Modifier.weight(1f).ids("system.about.version"), style = VoLumType.body(12.sp, VoLumColors.Cream))
        }
        Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            VoLumButton("Read the manual", { c.d(Action.OpenLink("https://github.com/guitarlum/VoLum/blob/main/docs/user-guide.en.md")) }, Modifier.weight(1f).ids("system.about.manual"))
            VoLumButton("Share log", { c.d(Action.ShareLog) }, Modifier.weight(1f).ids("system.share_log"))
        }
        VoLumToggle(s.update.auto, { c.d(Action.SetAutoUpdate(it)) }, Modifier.ids("system.update.auto"), label = "Check for updates automatically")
        val available = s.update.available
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            VText(
                when {
                    available != null -> "VoLum $available is available. You have 1.3."
                    s.update.checks > 0 -> "You have the latest version."
                    else -> "Not checked yet."
                },
                Modifier.weight(1f), style = VoLumType.label(11.sp, if (available != null) VoLumColors.Amber else VoLumColors.CreamDim), maxLines = 2,
            )
            VoLumButton("Check now", { c.d(Action.CheckUpdatesNow) }, Modifier.ids("system.update.check_now"))
        }
        if (available != null) {
            VoLumButton("Get VoLum $available", { c.d(Action.OpenLink("https://github.com/guitarlum/VoLum/releases/latest")) }, Modifier.fillMaxWidth().ids("system.update.link"), primary = true)
        }
    }
}
