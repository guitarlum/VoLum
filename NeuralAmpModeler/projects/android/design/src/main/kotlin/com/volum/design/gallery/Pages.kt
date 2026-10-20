package com.volum.design.gallery

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableDoubleStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.Catalog
import com.volum.core.Param
import com.volum.core.PreBlock
import com.volum.core.Section
import com.volum.core.View
import com.volum.core.VoLumState
import com.volum.core.VoLumStore
import com.volum.design.GalleryGap
import com.volum.design.art.AmpArt
import com.volum.design.art.AmpThumbnail
import com.volum.design.art.ArtInput
import com.volum.design.art.ArtRef
import com.volum.design.art.Motif
import com.volum.design.art.amps.FactoryArt
import com.volum.design.art.rememberArtFrames
import com.volum.design.components.AmpTitle
import com.volum.design.components.Corners
import com.volum.design.components.HeroAmp
import com.volum.design.components.PedalCard
import com.volum.design.components.PlayBanner
import com.volum.design.components.PlayStagePanel
import com.volum.design.components.QuietSlot
import com.volum.design.components.VoLumBanner
import com.volum.design.components.VoLumBannerKind
import com.volum.design.components.VoLumBrassButton
import com.volum.design.components.VoLumButton
import com.volum.design.components.VoLumChannelStepper
import com.volum.design.components.VoLumConfirmDialog
import com.volum.design.components.VoLumHero
import com.volum.design.components.VoLumKnob
import com.volum.design.components.VoLumListRow
import com.volum.design.components.VoLumMeter
import com.volum.design.components.VoLumModePicker
import com.volum.design.components.VoLumPowerSwitch
import com.volum.design.components.VoLumSegmentSwitch
import com.volum.design.components.VoLumSelectionStyle
import com.volum.design.components.VoLumSubModePill
import com.volum.design.components.VoLumToggle
import com.volum.design.components.ampCircuitBoard
import com.volum.design.components.cornerBrackets
import com.volum.design.components.diamondRule
import com.volum.design.components.hairlineFrame
import com.volum.design.components.insetWell
import com.volum.design.components.panelDepth
import com.volum.design.components.preCrosshatch
import com.volum.design.components.selectionInk
import com.volum.design.components.volumScrollbar
import com.volum.design.components.volumSelection
import com.volum.design.theme.VText
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumType
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.map
import kotlin.math.roundToInt

@Composable
private fun Caption(text: String, modifier: Modifier = Modifier) = VText(text, modifier, style = VoLumType.label(9.sp, VoLumColors.CreamDim))

@Composable
private fun Section(title: String, modifier: Modifier = Modifier, content: @Composable ColumnScope.() -> Unit) {
    Column(modifier, verticalArrangement = Arrangement.spacedBy(4.dp)) {
        VText(title, style = VoLumType.SectionHeader)
        content()
    }
}

private fun storeInput(store: VoLumStore) = ArtInput { store.telemetry.value }

private fun pedalName(s: VoLumState, p: Param, fallback: String): String =
    s.library.pedals.firstOrNull { it.index == s.param(p).toInt() }?.name ?: fallback

// ---- Stage: the PLAY art, the BUILD hero and the NAM pedal cards ---------------

@Composable
internal fun StagePage(store: VoLumStore, s: VoLumState) {
    val input = remember(store) { storeInput(store) }
    val amp = s.amp ?: Catalog.amps.last()
    val art = ArtRef.of(amp)
    val energy by remember(store) { store.telemetry.map { (it.energy * 100).roundToInt() }.distinctUntilChanged() }.collectAsState((store.telemetry.value.energy * 100).roundToInt())
    val frames = rememberArtFrames()
    Row(Modifier.fillMaxSize(), horizontalArrangement = GalleryGap) {
        VoLumMeter({ store.telemetry.value.meter(store.telemetry.value.inDb).toFloat() }, "IN", Modifier.fillMaxHeight().padding(vertical = 6.dp), frame = { frames.longValue })
        Column(Modifier.weight(1.55f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Caption("PLAY  ·  ${amp.name}  ·  moving art", Modifier.weight(1f))
                VText("ENERGY " + "%d.%02d".format(energy / 100, energy % 100), style = VoLumType.value(10.sp, VoLumColors.Gold))
            }
            PlayStagePanel(art, input, Modifier.fillMaxWidth().weight(1f).padding(3.dp), picks = false, animate = s.signal.animateArt)
            PlayBanner(s.presetName ?: amp.name, Modifier.fillMaxWidth().height(50.dp), program = s.library.live, secondary = amp.name, dirty = s.rig.dirty)
        }
        Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Caption("BUILD  ·  hero (static, as on the desktop)")
            VoLumHero(HeroAmp(amp.name, art), Modifier.fillMaxWidth().height(132.dp), onDualChip = {})
            AmpTitle(amp.name, Modifier.fillMaxWidth())
            Caption("PRE  ·  NAM pedal cards")
            Row(Modifier.fillMaxWidth().weight(1f), horizontalArrangement = Arrangement.spacedBy(0.dp)) {
                val preFocus = s.ui.view == View.BUILD && s.ui.section == Section.PRE
                PedalCard(Motif.NAM1, pedalName(s, Param.NAM1_CAPTURE, "NAM 1"), s.on(Param.NAM1_ON), { store.dispatch(Action.FocusPre(PreBlock.NAM1)) }, { store.dispatch(Action.ToggleParam(Param.NAM1_ON)) },
                    Modifier.weight(1f).fillMaxHeight(), focused = preFocus && s.ui.preFocus == PreBlock.NAM1)
                PedalCard(Motif.NAM2, pedalName(s, Param.NAM2_CAPTURE, "NAM 2"), s.on(Param.NAM2_ON), { store.dispatch(Action.FocusPre(PreBlock.NAM2)) }, { store.dispatch(Action.ToggleParam(Param.NAM2_ON)) },
                    Modifier.weight(1f).fillMaxHeight(), focused = preFocus && s.ui.preFocus == PreBlock.NAM2)
            }
        }
        VoLumMeter({ store.telemetry.value.meter(store.telemetry.value.outDb).toFloat() }, "OUT", Modifier.fillMaxHeight().padding(vertical = 6.dp), out = true, frame = { frames.longValue })
    }
}

// ---- Tokens: colour, type, selection, surfaces ---------------------------------

private val swatches = listOf(
    "Gold" to VoLumColors.Gold, "GoldDim" to VoLumColors.GoldDim, "Corner" to VoLumColors.Corner, "Amber" to VoLumColors.Amber,
    "Cream" to VoLumColors.Cream, "CreamDim" to VoLumColors.CreamDim, "TextBright" to VoLumColors.TextBright, "TextMed" to VoLumColors.TextMed,
    "Teal" to VoLumColors.Teal, "TealDim" to VoLumColors.TealDim, "GridCyan" to VoLumColors.GridCyan, "Copper IR" to VoLumColors.BtnIrOnBorder,
    "Danger" to VoLumColors.Danger, "SelBorder" to VoLumColors.SelBorder, "Bg" to VoLumColors.Bg, "PanelTop" to VoLumColors.PanelTop,
    "PanelBot" to VoLumColors.PanelBot, "HeroBg" to VoLumColors.HeroBg, "WellDark" to VoLumColors.WellDark, "Frame" to VoLumColors.Frame,
)

@Composable
internal fun TokensPage() {
    Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(16.dp)) {
        Section("COLOUR  ·  VoLumColorHelpers.h", Modifier.width(250.dp)) {
            swatches.chunked(4).forEach { row ->
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    row.forEach { (name, c) ->
                        Column(Modifier.width(56.dp)) {
                            Box(Modifier.fillMaxWidth().height(28.dp).clip(RoundedCornerShape(3.dp)).background(c).hairlineFrame(VoLumColors.Frame, radius = 3.dp))
                            VText(name, style = VoLumType.label(8.sp, VoLumColors.CreamDim))
                        }
                    }
                }
            }
        }
        Section("TYPE  ·  Poiret One / Josefin Sans / Michroma", Modifier.weight(1f)) {
            VText("VoLum", style = VoLumType.Logo)
            VText("Sunset Crunch", style = VoLumType.SoundTitle)
            VText("THC Sunset", style = VoLumType.AmpTitle)
            Row(horizontalArrangement = Arrangement.spacedBy(14.dp), verticalAlignment = Alignment.Bottom) {
                VText("NOISE GATE", style = VoLumType.KnobLabel)
                VText("-80.0 dB", style = VoLumType.KnobValue)
                VText("05  127", style = VoLumType.ProgramNumber)
                VText("CH 3", style = VoLumType.value(13.sp))
            }
            VText("V30-THC-3.nam   ·   Josefin Sans body", style = VoLumType.Footer)
            Spacer(Modifier.height(4.dp))
            VText("SELECTION  ·  DrawVoLumSelection", style = VoLumType.SectionHeader)
            Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                VoLumSelectionStyle.entries.forEach { st ->
                    Column(verticalArrangement = Arrangement.spacedBy(3.dp)) {
                        listOf("active" to (true to false), "pressed" to (false to true), "rest" to (false to false)).forEach { (name, flags) ->
                            Box(Modifier.width(96.dp).height(24.dp).volumSelection(flags.first, flags.second, st), contentAlignment = Alignment.Center) {
                                VText("${st.name.take(5)} $name", style = VoLumType.label(10.sp, selectionInk(st, flags.first)))
                            }
                        }
                    }
                }
            }
        }
        Section("SURFACES", Modifier.width(200.dp)) {
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Box(Modifier.size(96.dp, 120.dp).preCrosshatch().hairlineFrame(VoLumColors.Frame).cornerBrackets(VoLumColors.TealDim.copy(alpha = 0.45f), 6.dp, 3.dp), contentAlignment = Alignment.Center) { VText("PRE", style = VoLumType.label(11.sp)) }
                Box(Modifier.size(96.dp, 120.dp).ampCircuitBoard().hairlineFrame(VoLumColors.Frame).cornerBrackets(VoLumColors.TealDim, 6.dp, 3.dp), contentAlignment = Alignment.Center) { VText("AMP", style = VoLumType.label(11.sp)) }
            }
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Box(Modifier.size(96.dp, 64.dp).panelDepth(4.dp).hairlineFrame(VoLumColors.HeroBorder).cornerBrackets(VoLumColors.HeroCorner, 12.dp, 6.dp), contentAlignment = Alignment.Center) { Caption("panel + brackets") }
                Box(Modifier.size(96.dp, 64.dp).insetWell(), contentAlignment = Alignment.Center) { Caption("inset well") }
            }
            Spacer(Modifier.width(110.dp).height(12.dp).diamondRule())
            Caption("spacing 4 / 8 / 12 / 18 / 28  ·  radius 3 / 4 / 8")
        }
    }
}

// ---- Controls ---------------------------------------------------------------------

@Composable
internal fun ControlsPage(store: VoLumStore) {
    var input by remember { mutableDoubleStateOf(1.5) }
    var gate by remember { mutableDoubleStateOf(-80.0) }
    var bass by remember { mutableDoubleStateOf(6.0) }
    var pan by remember { mutableDoubleStateOf(0.5) }
    var gateOn by remember { mutableStateOf(true) }
    var eq by remember { mutableStateOf(false) }
    var power by remember { mutableStateOf(true) }
    var mode by remember { mutableIntStateOf(0) }
    var sub by remember { mutableIntStateOf(1) }
    var cab by remember { mutableIntStateOf(3) }
    var channel by remember { mutableIntStateOf(3) }
    var lite by remember { mutableStateOf(false) }
    var amp by remember { mutableIntStateOf(14) }
    val frames = rememberArtFrames()
    Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(14.dp)) {
        Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(2.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                VoLumMeter({ store.telemetry.value.meter(store.telemetry.value.inDb).toFloat() }, "IN", Modifier.height(112.dp), frame = { frames.longValue })
                VoLumKnob(input, { input = it }, -20.0..20.0, 0.0, "INPUT", "%.1f dB".format(input), Modifier.padding(horizontal = 4.dp), parseEntry = { it.toDoubleOrNull() })
                VoLumKnob(gate, { gate = it }, -100.0..0.0, -80.0, "GATE", "%.1f dB".format(gate))
                VoLumKnob(bass, { bass = it }, 0.0..10.0, 5.0, "BASS", "%.1f".format(bass))
                VoLumKnob(pan, { pan = it }, -1.0..1.0, 0.0, "PAN", "%.2f".format(pan), support = true)
                VoLumKnob(5.0, {}, 0.0..10.0, 5.0, "MID", "5.0", enabled = false)
                VoLumMeter({ store.telemetry.value.meter(store.telemetry.value.outDb).toFloat() }, "OUT", Modifier.height(112.dp), out = true, frame = { frames.longValue })
            }
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                VoLumChannelStepper("$channel", { channel = if (channel <= 1) 5 else channel - 1 }, { channel = if (channel >= 5) 1 else channel + 1 })
                VoLumToggle(gateOn, { gateOn = it }, label = "NOISE GATE")
                VoLumToggle(eq, { eq = it }, label = "EQ")
            }
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
                listOf("No Cab", "G12", "G65", "V30", "Custom IR").forEachIndexed { i, label ->
                    VoLumBrassButton(label, cab == i, { cab = i }, enabled = i != 4, minWidth = if (i == 0) 64.dp else 52.dp)
                }
            }
            Row(horizontalArrangement = Arrangement.spacedBy(10.dp), verticalAlignment = Alignment.CenterVertically) {
                VoLumSubModePill(listOf("DARK", "SHIMMER", "BLOOM"), sub, { sub = it }, Modifier.width(220.dp))
                VoLumSegmentSwitch("FULL", "LITE", lite, { lite = it }, Modifier.width(130.dp))
            }
        }
        Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
            Caption("MODE PICKER")
            VoLumModePicker(listOf("DIGITAL", "ANALOG", "REVERSE"), mode, { mode = it })
            Caption("POWER")
            VoLumPowerSwitch(power, { power = it })
        }
        Column(Modifier.width(230.dp)) {
            Caption("LIST  ·  ListTeal + thumbnails + scrollbar")
            val list = rememberLazyListState()
            LazyColumn(Modifier.fillMaxWidth().weight(1f).insetWell().volumScrollbar(list), state = list) {
                itemsIndexed(Catalog.amps) { i, a ->
                    VoLumListRow(a.name, amp == i, { amp = i }, Modifier.fillMaxWidth().padding(end = 6.dp), art = ArtRef.of(a))
                }
            }
        }
    }
}

// ---- Dialogs, banners, buttons, Dual Amp hero ------------------------------------

@Composable
internal fun DialogsPage(store: VoLumStore, s: VoLumState) {
    val thc = Catalog.amps[14]
    val slo = Catalog.amps[13]
    Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(14.dp)) {
        Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Caption("CONFIRM  ·  destructive (red)")
            Box(Modifier.fillMaxWidth().height(150.dp)) {
                val c = s.ui.confirm
                VoLumConfirmDialog("Delete preset", c?.message ?: "Delete \"Sunset Crunch\"?", "Delete", { store.dispatch(Action.AcceptConfirm) }, { store.dispatch(Action.CancelConfirm) })
            }
            Caption("BANNER  ·  error / info")
            VoLumBanner(s.ui.banner?.message ?: "File is not a WAV file: renamed-junk.wav", VoLumBannerKind.ERROR, { store.dispatch(Action.DismissBanner) }, Modifier.fillMaxWidth())
            VoLumBanner("Pack imported: 3 items added", VoLumBannerKind.INFO, {}, Modifier.fillMaxWidth())
        }
        Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Caption("CONFIRM  ·  caution (amber)")
            Box(Modifier.fillMaxWidth().height(150.dp)) {
                VoLumConfirmDialog("Overwrite preset", "\"Sunset Crunch\" already exists. Replace it with the current sound?", "Overwrite", {}, {})
            }
            Caption("DUAL AMP HERO  ·  SUPPORT focused")
            VoLumHero(HeroAmp(thc.name, ArtRef.of(thc)), Modifier.fillMaxWidth().weight(1f), dual = true, support = HeroAmp(slo.name, ArtRef.of(slo)), supportFocused = true, onDualChip = {}, onLane = {})
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                VoLumButton("Cancel", {})
                VoLumButton("Save", {}, primary = true)
                VoLumButton("Remove", {}, primary = true, destructive = true)
                VoLumButton("Disabled", {}, enabled = false)
            }
        }
    }
}

// ---- Every factory hero art --------------------------------------------------------

@Composable
internal fun ArtsPage() {
    Column(Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
        FactoryArt.specs.chunked(5).forEach { row ->
            Row(Modifier.fillMaxWidth().weight(1f), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                row.forEach { spec ->
                    Column(Modifier.weight(1f).fillMaxHeight()) {
                        Box(Modifier.fillMaxWidth().weight(1f).panelDepth().hairlineFrame(VoLumColors.HeroBorder).cornerBrackets(VoLumColors.HeroCorner, 10.dp, 4.dp)) {
                            AmpArt(ArtRef.factory(spec.case), Modifier.fillMaxSize().padding(4.dp))
                        }
                        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(5.dp)) {
                            AmpThumbnail(ArtRef.factory(spec.case), Modifier.size(14.dp))
                            VText("${spec.amp}  ·  ${spec.motif}", style = VoLumType.label(8.5.sp, VoLumColors.CreamDim))
                        }
                    }
                }
            }
        }
    }
}

// ---- Motifs, quiet slots, custom arts, thumbnails ----------------------------------

@Composable
internal fun MotifsPage() {
    val labels = mapOf(Motif.PITCH to "+0 st", Motif.COMP to "Compressor", Motif.NAM1 to "Klon", Motif.NAM2 to "TS", Motif.CHORUS to "Ensemble . 50 %", Motif.DELAY to "Digital . 320 ms", Motif.REVERB to "Hall . 20 %", Motif.TREM to "Bias . 5.0 Hz")
    Column(Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
        Row(Modifier.fillMaxWidth().weight(1.25f), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            Motif.entries.forEachIndexed { i, m ->
                PedalCard(m, labels.getValue(m), on = i % 3 != 1, onClick = {}, onToggle = {}, modifier = Modifier.weight(1f).fillMaxHeight(), focused = i == 2)
            }
        }
        Row(Modifier.fillMaxWidth().weight(1f), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            QuietBlock("PRE", listOf(Motif.PITCH to "PITCH", Motif.COMP to "COMP", Motif.NAM1 to "KLON", Motif.NAM2 to "TS"), Modifier.width(128.dp))
            QuietBlock("POST", listOf(Motif.CHORUS to "CHORUS", Motif.DELAY to "DELAY", Motif.REVERB to "REVRB", Motif.TREM to "TREM"), Modifier.width(128.dp))
            Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                Caption("CUSTOM AMP ART  ·  six styles")
                Row(Modifier.fillMaxWidth().weight(1f), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    (0 until 6).forEach { st ->
                        Box(Modifier.weight(1f).fillMaxHeight().background(VoLumColors.HeroBg).hairlineFrame(VoLumColors.HeroBorder)) {
                            AmpArt(ArtRef.custom(st), Modifier.fillMaxSize().padding(4.dp))
                        }
                    }
                }
                Caption("AMP LIST THUMBNAILS")
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    FactoryArt.specs.forEach { AmpThumbnail(ArtRef.factory(it.case), Modifier.size(22.dp)) }
                    (0 until 6).forEach { AmpThumbnail(ArtRef.custom(it), Modifier.size(22.dp)) }
                }
            }
        }
    }
}

@Composable
private fun QuietBlock(title: String, slots: List<Pair<Motif, String>>, modifier: Modifier) {
    var on by remember { mutableStateOf(setOf(1, 2)) }
    Column(modifier.fillMaxHeight().panelDepth().hairlineFrame(VoLumColors.Frame).cornerBrackets(VoLumColors.TealDim, 6.dp, 3.dp, Corners.ALL).padding(2.dp)) {
        VText(title, Modifier.padding(start = 6.dp, top = 3.dp), style = VoLumType.SectionHeader)
        slots.forEachIndexed { i, (m, label) ->
            QuietSlot(m, label, i in on, {}, { v -> on = if (v) on + i else on - i }, Modifier.fillMaxWidth().weight(1f))
        }
    }
}

@Suppress("unused")
private val unusedStyle = TextStyle(color = Color.Unspecified)
