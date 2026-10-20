package com.volum.design.gallery

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.volum.core.VoLumStore
import com.volum.design.art.Motif
import com.volum.design.art.rememberArtFrames
import com.volum.design.components.HeaderToolFace
import com.volum.design.components.IrCabFace
import com.volum.design.components.LockFace
import com.volum.design.components.ModeToggleFace
import com.volum.design.components.PresetBarFace
import com.volum.design.components.PresetBarPart
import com.volum.design.components.QuietSlotRow
import com.volum.design.components.SegmentSwitch
import com.volum.design.components.VoLumBlockHeader
import com.volum.design.components.VoLumBrassButton
import com.volum.design.components.VoLumBuildTip
import com.volum.design.components.VoLumGlyph
import com.volum.design.components.VoLumHeaderCluster
import com.volum.design.components.VoLumIcon
import com.volum.design.components.VoLumIrCabButton
import com.volum.design.components.VoLumLed
import com.volum.design.components.VoLumLogo
import com.volum.design.components.VoLumMeter
import com.volum.design.components.VoLumPresetBar
import com.volum.design.components.VoLumQuietBlock
import com.volum.design.components.VoLumSegmentSwitch
import com.volum.design.components.VoLumSwitchTrack
import com.volum.design.components.VoLumTextField
import com.volum.design.components.volumHeaderPlate
import com.volum.design.components.volumSectionFrame
import com.volum.design.theme.VoLumColors

// ---- Header: the two header bars, preset bar states, glyph set, pressed faces ----

@Composable
internal fun HeaderPage() {
    var inPlayA by remember { mutableStateOf(false) }
    var metronome by remember { mutableStateOf(true) }
    Column(Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(3.dp)) {
        Caption("BUILD HEADER  ·  logo, preset bar, PLAY / BUILD toggle, tuner, metronome, gear")
        HeaderBar {
            Box(Modifier.weight(1f), contentAlignment = Alignment.Center) {
                VoLumPresetBar("Sunset Crunch", dirty = true, onOpen = {}, onPrev = {}, onNext = {}, modifier = Modifier.width(300.dp))
            }
            VoLumHeaderCluster(inPlayA, { inPlayA = !inPlayA }, {}, {}, {})
        }
        Caption("PLAY HEADER  ·  first-run tip, metronome clicking, update badge on the gear")
        HeaderBar {
            Spacer(Modifier.weight(1f))
            VoLumBuildTip({})
            VoLumHeaderCluster(true, {}, {}, { metronome = !metronome }, {}, metronomeOn = metronome, updateBadge = true)
        }
        Spacer(Modifier.height(3.dp))
        Row(Modifier.fillMaxWidth().weight(1f), horizontalArrangement = Arrangement.spacedBy(16.dp)) {
            Section("PRESET BAR  ·  clean, unsaved, no preset, empty bank, pressed", Modifier.weight(1f)) {
                Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                    PresetBarFace("Sunset Crunch", false, Modifier.weight(1f).height(48.dp))
                    PresetBarFace("Sunset Crunch", true, Modifier.weight(1f).height(48.dp))
                }
                Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                    PresetBarFace(null, false, Modifier.weight(1f).height(48.dp))
                    PresetBarFace(null, true, Modifier.weight(1f).height(48.dp), canStep = false)
                }
                Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                    PresetBarFace("Marshall JMP 2203 Plexi Lead Stack", false, Modifier.weight(1f).height(48.dp), pressed = PresetBarPart.NAME)
                    PresetBarFace("Lead Boost", true, Modifier.weight(1f).height(48.dp), pressed = PresetBarPart.NEXT)
                }
            }
            Section("GLYPHS  ·  rest and pressed", Modifier.width(320.dp)) {
                VoLumGlyph.entries.chunked(9).forEach { row ->
                    Row {
                        row.forEach { g -> Box(Modifier.size(34.dp), contentAlignment = Alignment.Center) { VoLumIcon(g) } }
                    }
                }
                Row(verticalAlignment = Alignment.CenterVertically) {
                    ModeToggleFace(inPlay = false, pressed = true, modifier = Modifier.size(56.dp, 48.dp))
                    ModeToggleFace(inPlay = true, pressed = true, modifier = Modifier.size(56.dp, 48.dp))
                    HeaderToolFace(VoLumGlyph.Tuner, pressed = true, modifier = Modifier.size(48.dp))
                    HeaderToolFace(VoLumGlyph.Metronome, pressed = false, modifier = Modifier.size(48.dp), active = false)
                    HeaderToolFace(VoLumGlyph.Metronome, pressed = true, modifier = Modifier.size(48.dp), active = false)
                    HeaderToolFace(VoLumGlyph.Gear, pressed = true, modifier = Modifier.size(48.dp), badge = true)
                }
            }
        }
    }
}

@Composable
private fun HeaderBar(content: @Composable androidx.compose.foundation.layout.RowScope.() -> Unit) {
    Row(Modifier.fillMaxWidth().height(52.dp).volumHeaderPlate().padding(start = 10.dp, end = 6.dp), verticalAlignment = Alignment.CenterVertically) {
        VoLumLogo(Modifier.width(120.dp), compact = true)
        content()
    }
}

// ---- Blocks: PRE / POST quiet blocks, block headers, cab row, inputs, lamps ------

@Composable
internal fun BlocksPage(store: VoLumStore) {
    val frames = rememberArtFrames()
    var preLocked by remember { mutableStateOf(false) }
    var postLocked by remember { mutableStateOf(true) }
    var preOn by remember { mutableStateOf(setOf(2, 3)) }
    var postOn by remember { mutableStateOf(setOf(2)) }
    var lite by remember { mutableStateOf(false) }
    var cab by remember { mutableIntStateOf(4) }
    var name by remember { mutableStateOf("Sunset Crunch") }
    var empty by remember { mutableStateOf("") }
    Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
        val pre = listOf(Motif.PITCH to "PITCH", Motif.COMP to "COMP", Motif.NAM1 to "KLON", Motif.NAM2 to "TS")
        VoLumQuietBlock("PRE", preLocked, { preLocked = !preLocked }, {}, Modifier.width(150.dp).fillMaxHeight()) {
            pre.forEachIndexed { i, (m, label) ->
                QuietSlotRow(m, label, i in preOn, {}, { v -> preOn = if (v) preOn + i else preOn - i }, Modifier.fillMaxWidth().weight(1f), 0, Modifier, previewPillPressed = i == 1)
            }
        }
        val post = listOf(Motif.CHORUS to "CHORUS", Motif.DELAY to "DELAY", Motif.REVERB to "REVRB", Motif.TREM to "TREM")
        VoLumQuietBlock("POST", postLocked, { postLocked = !postLocked }, {}, Modifier.width(150.dp).fillMaxHeight(), dirty = true, onStore = {}, storeFirst = true) {
            post.forEachIndexed { i, (m, label) ->
                QuietSlotRow(m, label, i in postOn, {}, { v -> postOn = if (v) postOn + i else postOn - i }, Modifier.fillMaxWidth().weight(1f), 0, Modifier, previewPillPressed = false)
            }
        }
        Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(4.dp)) {
            Caption("CAB ROW  ·  V30, Custom IR (rest, active copper, pressed, no DIRECT capture)")
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
                VoLumBrassButton("V30", cab == 3, { cab = 3 })
                VoLumIrCabButton(false, { cab = 4 }, minWidth = 100.dp)
                VoLumIrCabButton(cab == 4, { cab = 4 }, irName = "Royer 4x12", minWidth = 120.dp)
                IrCabFace(active = false, hot = true, enabled = true, irName = null, modifier = Modifier.height(48.dp).width(100.dp))
                VoLumIrCabButton(false, {}, enabled = false, minWidth = 100.dp)
            }
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                Section("EXPANDED BLOCK HEADERS  ·  open, locked + unsaved", Modifier.weight(1f)) {
                    Box(Modifier.fillMaxWidth().volumSectionFrame(expanded = true)) { VoLumBlockHeader("PRE", false, {}) }
                    Box(Modifier.fillMaxWidth().volumSectionFrame(locked = true, expanded = true)) { VoLumBlockHeader("POST", true, {}, dirty = true, onStore = {}, storeFirst = true) }
                }
                Section("NAME FIELD  ·  empty, filled", Modifier.weight(1f)) {
                    VoLumTextField(empty, { empty = it }, Modifier.fillMaxWidth(), placeholder = "Preset name")
                    VoLumTextField(name, { name = it }, Modifier.fillMaxWidth())
                }
            }
            Caption("PRESSED  ·  segment half, lock open / closed, switch track    LAMPS  ·  off, on, clip")
            Row(horizontalArrangement = Arrangement.spacedBy(10.dp), verticalAlignment = Alignment.CenterVertically) {
                VoLumSegmentSwitch("FULL", "LITE", lite, { lite = it }, Modifier.width(120.dp))
                SegmentSwitch("FULL", "LITE", lite, {}, Modifier.width(120.dp), { Modifier }, previewPressed = !lite)
                LockFace(locked = false, hot = true, modifier = Modifier.size(40.dp))
                LockFace(locked = true, hot = true, modifier = Modifier.size(40.dp))
                VoLumSwitchTrack(true, pressed = true)
                VoLumSwitchTrack(false, pressed = true)
                VoLumLed(false)
                VoLumLed(true)
                VoLumLed(true, color = VoLumColors.Danger)
            }
            Caption("METER  ·  horizontal IN / OUT ladders")
            VoLumMeter({ store.telemetry.value.meter(store.telemetry.value.inDb).toFloat() }, "IN", Modifier.fillMaxWidth(), frame = { frames.longValue }, horizontal = true)
            VoLumMeter({ store.telemetry.value.meter(store.telemetry.value.outDb).toFloat() }, "OUT", Modifier.fillMaxWidth(), out = true, frame = { frames.longValue }, horizontal = true)
        }
    }
}
