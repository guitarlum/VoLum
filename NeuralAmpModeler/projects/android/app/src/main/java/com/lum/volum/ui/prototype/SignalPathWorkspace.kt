package com.lum.volum.ui.prototype

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

@Composable
fun SignalPathWorkspace(
    state: PrototypeState,
    dispatch: (PrototypeAction) -> Unit,
) {
    Column(
        Modifier
            .fillMaxSize()
            .background(
                Brush.verticalGradient(
                    listOf(PrototypeTheme.canvas, PrototypeTheme.panel, PrototypeTheme.canvas),
                ),
            ),
    ) {
        PerformanceRail(state, dispatch)
        SignalPath(state, dispatch)
        FocusPane(state, dispatch, Modifier.weight(1f))
    }
}

@Composable
private fun PerformanceRail(
    state: PrototypeState,
    dispatch: (PrototypeAction) -> Unit,
) {
    Row(
        Modifier
            .fillMaxWidth()
            .height(56.dp)
            .background(PrototypeTheme.panel)
            .border(1.dp, PrototypeTheme.line)
            .padding(horizontal = 6.dp, vertical = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        Column(Modifier.width(88.dp).padding(start = 6.dp)) {
            Text(
                "VOLUM",
                color = PrototypeTheme.text,
                fontFamily = PrototypeTheme.display,
                fontSize = 11.sp,
                letterSpacing = 1.5.sp,
                maxLines = 1,
            )
            Text("SIGNAL PATH", color = PrototypeTheme.amber, fontFamily = PrototypeTheme.body, fontSize = 8.sp)
        }
        PrototypeButton(
            label = if (state.presetDirty) "• ${state.selectedPreset}" else state.selectedPreset,
            onClick = {
                dispatch(
                    PrototypeAction.OpenBrowser(
                        BrowserKind.Preset,
                        BrowserApply.ReturnWorkspace,
                        PrototypeRoute.Workspace,
                    ),
                )
            },
            modifier = Modifier.weight(1.25f),
        )
        Column(Modifier.width(104.dp), verticalArrangement = Arrangement.spacedBy(1.dp)) {
            Text(
                "${state.latencyMs} ms · ${state.xruns} xr",
                color = if (state.xruns == 0) PrototypeTheme.teal else PrototypeTheme.red,
                fontFamily = PrototypeTheme.display,
                fontSize = 7.sp,
            )
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                Text("IN", color = PrototypeTheme.muted, fontFamily = PrototypeTheme.body, fontSize = 7.sp)
                LevelMeter(state.inputPeak, Modifier.weight(1f).height(5.dp))
            }
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                Text("OUT", color = PrototypeTheme.muted, fontFamily = PrototypeTheme.body, fontSize = 7.sp)
                LevelMeter(state.outputPeak, Modifier.weight(1f).height(5.dp))
                if (state.supportEnabled) {
                    LevelMeter((state.outputPeak * .82f).coerceIn(0f, 1f), Modifier.weight(1f).height(5.dp))
                }
            }
        }
        HoldPowerButton(
            powered = state.powered,
            onLongPress = { dispatch(PrototypeAction.TogglePower) },
            modifier = Modifier.width(60.dp),
        )
        PrototypeButton(
            label = "Bypass",
            onClick = { dispatch(PrototypeAction.ToggleBypass) },
            modifier = Modifier.width(72.dp),
            active = state.bypassed,
        )
        PrototypeButton(
            label = "Tuner",
            onClick = { dispatch(PrototypeAction.OpenTuner) },
            modifier = Modifier.width(64.dp),
        )
        PrototypeButton(
            label = "Click",
            onClick = { dispatch(PrototypeAction.OpenMetronome) },
            modifier = Modifier.width(64.dp),
            active = state.metronomeActive,
        )
        PrototypeButton(
            label = "Set",
            onClick = { dispatch(PrototypeAction.OpenSettings) },
            modifier = Modifier.width(52.dp),
        )
    }
}

@Composable
private fun SignalPath(
    state: PrototypeState,
    dispatch: (PrototypeAction) -> Unit,
) {
    Row(
        Modifier
            .fillMaxWidth()
            .height(148.dp)
            .background(PrototypeTheme.inset)
            .padding(horizontal = 6.dp, vertical = 4.dp),
        verticalAlignment = Alignment.Bottom,
        horizontalArrangement = Arrangement.spacedBy(5.dp),
    ) {
        Column(Modifier.width(38.dp)) {
            Spacer(Modifier.height(48.dp))
            EndpointTile("IN", state.inputPeak, Modifier.fillMaxWidth())
        }
        ChainSection(
            section = RigSection.Pre,
            weight = 4f,
            state = state,
            dispatch = dispatch,
        )
        HairlineConnector(Modifier.width(6.dp).padding(bottom = 45.dp))
        AmpChainSection(state, dispatch, Modifier.weight(4f))
        HairlineConnector(Modifier.width(6.dp).padding(bottom = 45.dp))
        ChainSection(
            section = RigSection.Post,
            weight = 3f,
            state = state,
            dispatch = dispatch,
        )
        Column(Modifier.width(38.dp)) {
            Spacer(Modifier.height(48.dp))
            EndpointTile("OUT", state.outputPeak, Modifier.fillMaxWidth())
        }
    }
}

@Composable
private fun RowScope.RowScopeChainBlocks(
    blocks: List<RigBlock>,
    state: PrototypeState,
    dispatch: (PrototypeAction) -> Unit,
    accent: androidx.compose.ui.graphics.Color? = null,
) {
    blocks.forEach { block ->
        BlockTile(
            block = block,
            enabled = state.blocks[block] == true,
            selected = state.selectedBlock == block,
            onSelect = { dispatch(PrototypeAction.SelectBlock(block)) },
            onToggle = { dispatch(PrototypeAction.ToggleBlock(block)) },
            modifier = Modifier.weight(1f),
            accent = accent ?: sectionAccent(block.section),
        )
    }
}

@Composable
private fun RowScope.ChainSection(
    section: RigSection,
    weight: Float,
    state: PrototypeState,
    dispatch: (PrototypeAction) -> Unit,
) {
    val blocks = when (section) {
        RigSection.Pre -> listOf(RigBlock.Octave, RigBlock.Compressor, RigBlock.NamOne, RigBlock.NamTwo)
        RigSection.Post -> listOf(RigBlock.Delay, RigBlock.Reverb, RigBlock.Tremolo)
        RigSection.Amp -> emptyList()
    }
    val accent = sectionAccent(section)
    Column(
        Modifier
            .weight(weight)
            .fillMaxHeight()
            .border(
                1.dp,
                if (state.selectedSection == section) accent else accent.copy(alpha = .32f),
                RoundedCornerShape(8.dp),
            ),
    ) {
        SectionHeading(
            section,
            selected = state.selectedSection == section,
            onSelect = { dispatch(PrototypeAction.SelectSection(section)) },
            modifier = Modifier.fillMaxWidth(),
            trailing = if (section == RigSection.Pre) "4 BLOCKS" else "3 BLOCKS",
        )
        Row(
            Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(4.dp),
        ) {
            RowScopeChainBlocks(blocks, state, dispatch)
        }
    }
}

@Composable
private fun AmpChainSection(
    state: PrototypeState,
    dispatch: (PrototypeAction) -> Unit,
    modifier: Modifier = Modifier,
) {
    Column(
        modifier
            .fillMaxHeight()
            .border(
                1.dp,
                if (state.selectedSection == RigSection.Amp) PrototypeTheme.amber else PrototypeTheme.amber.copy(.32f),
                RoundedCornerShape(8.dp),
            ),
    ) {
        SectionHeading(
            RigSection.Amp,
            selected = state.selectedSection == RigSection.Amp,
            onSelect = { dispatch(PrototypeAction.SelectSection(RigSection.Amp)) },
            modifier = Modifier.fillMaxWidth(),
            trailing = "MAIN + SUPPORT",
        )
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
            RowScopeChainBlocks(listOf(RigBlock.MainAmp, RigBlock.MainIr), state, dispatch, PrototypeTheme.amber)
            if (state.supportEnabled) {
                RowScopeChainBlocks(
                    listOf(RigBlock.SupportAmp, RigBlock.SupportIr),
                    state,
                    dispatch,
                    PrototypeTheme.support,
                )
            } else {
                Box(
                    Modifier
                        .weight(2f)
                        .height(92.dp)
                        .background(PrototypeTheme.support.copy(.06f), RoundedCornerShape(8.dp))
                        .border(1.dp, PrototypeTheme.support.copy(.35f), RoundedCornerShape(8.dp))
                        .padding(horizontal = 7.dp),
                ) {
                    PrototypeButton(
                        "Dual +",
                        onClick = { dispatch(PrototypeAction.ToggleSupport) },
                        modifier = Modifier.align(Alignment.Center).fillMaxWidth(),
                    )
                }
            }
        }
    }
}

@Composable
private fun FocusPane(
    state: PrototypeState,
    dispatch: (PrototypeAction) -> Unit,
    modifier: Modifier = Modifier,
) {
    Box(
        modifier
            .fillMaxWidth()
            .background(PrototypeTheme.panel.copy(alpha = .92f))
            .padding(8.dp),
    ) {
        when {
            state.selectedBlock != null -> BlockEditor(state.selectedBlock, state, dispatch)
            state.selectedSection != null -> SectionSummary(state.selectedSection, state, dispatch)
            else -> PerformanceSummary(state, dispatch)
        }
    }
}

@Composable
private fun PerformanceSummary(
    state: PrototypeState,
    dispatch: (PrototypeAction) -> Unit,
) {
    Row(
        Modifier.fillMaxSize(),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        Column(Modifier.weight(1.5f)) {
            Text("CURRENT RIG", color = PrototypeTheme.amber, fontFamily = PrototypeTheme.body, fontSize = 9.sp)
            Text(
                state.selectedAmp,
                color = PrototypeTheme.text,
                fontFamily = PrototypeTheme.display,
                fontSize = 12.sp,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
            Text(
                "${state.selectedMainIr} · ${if (state.supportEnabled) "DUAL AMP" else "MAIN LANE"}",
                color = PrototypeTheme.muted,
                fontFamily = PrototypeTheme.body,
                fontSize = 9.sp,
                maxLines = 1,
            )
        }
        ValueBadge(
            if (state.supportEnabled) "Channels" else "Channel",
            if (state.supportEnabled) "${state.mainChannel + 1} / ${state.supportChannel + 1}"
            else "CH ${state.mainChannel + 1}",
            Modifier.weight(.8f),
            PrototypeTheme.amber,
        )
        ValueBadge("DSP", if (state.powered) "LIVE" else "OFF", Modifier.weight(.7f))
        Column(Modifier.weight(1f)) {
            Text(
                "Select a block to edit",
                color = PrototypeTheme.text,
                fontFamily = PrototypeTheme.body,
                fontWeight = FontWeight.Bold,
                fontSize = 12.sp,
            )
            Text(
                "Power icons toggle only.",
                color = PrototypeTheme.muted,
                fontFamily = PrototypeTheme.body,
                fontSize = 9.sp,
                maxLines = 1,
            )
        }
        PrototypeButton(
            "Amp library",
            onClick = {
                dispatch(
                    PrototypeAction.OpenBrowser(
                        BrowserKind.Amp,
                        BrowserApply.ReturnWorkspace,
                        PrototypeRoute.Workspace,
                    ),
                )
            },
            modifier = Modifier.width(112.dp),
            primary = true,
        )
    }
}

@Composable
private fun SectionSummary(
    section: RigSection,
    state: PrototypeState,
    dispatch: (PrototypeAction) -> Unit,
) {
    Row(
        Modifier.fillMaxSize(),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        FocusTitle(section.name, "SECTION SUMMARY", state, dispatch, sectionAccent(section))
        when (section) {
            RigSection.Pre -> {
                SectionSceneControls(
                    section = section,
                    locked = state.preLocked,
                    dirty = state.preDirty,
                    active = activeCount(state, section),
                    total = 4,
                    state = state,
                    dispatch = dispatch,
                )
            }
            RigSection.Amp -> AmpSectionSummary(state, dispatch, Modifier.weight(1f))
            RigSection.Post -> {
                SectionSceneControls(
                    section = section,
                    locked = state.postLocked,
                    dirty = state.postDirty,
                    active = activeCount(state, section),
                    total = 3,
                    state = state,
                    dispatch = dispatch,
                )
            }
        }
    }
}

@Composable
private fun RowScope.SectionSceneControls(
    section: RigSection,
    locked: Boolean,
    dirty: Boolean,
    active: Int,
    total: Int,
    state: PrototypeState,
    dispatch: (PrototypeAction) -> Unit,
) {
    val accent = sectionAccent(section)
    ValueBadge("Active", "$active / $total", Modifier.weight(.65f), accent)
    ValueBadge(
        "Scene",
        if (locked) if (dirty) "LOCKED · EDITED" else "LOCKED" else "PER AMP",
        Modifier.weight(1.1f),
        accent,
    )
    FeatureToggle(
        "Lock ${section.name}",
        locked,
        { dispatch(PrototypeAction.ToggleSectionLock(section)) },
        Modifier.weight(1f),
        accent,
    )
    if (locked && dirty) {
        PrototypeButton(
            "Store to ${state.selectedAmp.substringBefore(" · ")}",
            { dispatch(PrototypeAction.StoreSection(section)) },
            Modifier.weight(1.25f),
            primary = true,
        )
    } else {
        ValueBadge(
            if (locked) "Carry" else "Recall",
            if (locked) "ACROSS AMPS" else "WITH AMP",
            Modifier.weight(1.25f),
            accent,
        )
    }
}

@Composable
private fun AmpSectionSummary(
    state: PrototypeState,
    dispatch: (PrototypeAction) -> Unit,
    modifier: Modifier = Modifier,
) {
    val support = state.selectedLane == AmpLane.Support
    val accent = if (support) PrototypeTheme.support else PrototypeTheme.amber
    val amp = if (support) state.selectedSupportAmp else state.selectedAmp
    val cab = if (support) state.selectedSupportIr else state.selectedMainIr
    val channel = if (support) state.supportChannel else state.mainChannel
    val ampKind = if (support) BrowserKind.SupportAmp else BrowserKind.Amp
    val cabKind = if (support) BrowserKind.SupportIr else BrowserKind.MainIr
    val gate = if (support) state.supportGateEnabled else state.mainGateEnabled
    val eq = if (support) state.supportEqEnabled else state.mainEqEnabled
    val ir = if (support) state.supportIrEnabled else state.mainIrEnabled

    Column(modifier.fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            ModeSelector(
                listOf("Main", "Support"),
                if (support) 1 else 0,
                { if (it == 0 || state.supportEnabled) dispatch(PrototypeAction.SelectLane(AmpLane.entries[it])) },
                Modifier.weight(1.05f),
            )
            OptionStepper(
                "Channel",
                channelLabel(amp, channel),
                { dispatch(PrototypeAction.StepChannel(state.selectedLane, -1)) },
                { dispatch(PrototypeAction.StepChannel(state.selectedLane, 1)) },
                Modifier.weight(1.05f),
                accent,
            )
            FeatureToggle(
                "Dual",
                state.supportEnabled,
                { dispatch(PrototypeAction.ToggleSupport) },
                Modifier.weight(.75f),
                PrototypeTheme.support,
            )
            if (support) {
                FeatureToggle(
                    "Polarity Ø",
                    state.supportPolarityInverted,
                    { dispatch(PrototypeAction.ToggleSupportPolarity) },
                    Modifier.weight(.85f),
                    PrototypeTheme.support,
                )
            } else {
                ValueBadge("Lane", "MAIN", Modifier.weight(.85f), PrototypeTheme.amber)
            }
        }
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            PrototypeButton(
                "AMP · ${amp.substringBefore(" · ")}",
                {
                    dispatch(
                        PrototypeAction.OpenBrowser(
                            ampKind,
                            BrowserApply.ReturnWorkspace,
                            PrototypeRoute.Workspace,
                        ),
                    )
                },
                Modifier.weight(1.35f),
            )
            PrototypeButton(
                "CAB · ${cab.substringBefore(" · ")}",
                {
                    dispatch(
                        PrototypeAction.OpenBrowser(
                            cabKind,
                            BrowserApply.ReturnWorkspace,
                            PrototypeRoute.Workspace,
                        ),
                    )
                },
                Modifier.weight(1.35f),
            )
            FeatureToggle(
                "Gate",
                gate,
                { dispatch(PrototypeAction.ToggleLaneFeature(state.selectedLane, LaneFeature.Gate)) },
                Modifier.weight(.7f),
                accent,
            )
            FeatureToggle(
                "EQ",
                eq,
                { dispatch(PrototypeAction.ToggleLaneFeature(state.selectedLane, LaneFeature.Eq)) },
                Modifier.weight(.65f),
                accent,
            )
            ValueBadge(
                "Src",
                when {
                    cab.startsWith("DIRECT") -> "DIRECT"
                    ir -> "CUSTOM"
                    else -> "CAB"
                },
                Modifier.weight(.8f),
                accent,
            )
        }
    }
}

@Composable
private fun EffectSummary(
    block: RigBlock,
    state: PrototypeState,
    dispatch: (PrototypeAction) -> Unit,
    modifier: Modifier = Modifier,
) {
    Column(
        modifier
            .fillMaxHeight()
            .background(PrototypeTheme.inset, RoundedCornerShape(9.dp))
            .border(1.dp, PrototypeTheme.line, RoundedCornerShape(9.dp))
            .padding(10.dp),
        verticalArrangement = Arrangement.Center,
    ) {
        Text(block.title.uppercase(), color = PrototypeTheme.text, fontFamily = PrototypeTheme.display, fontSize = 9.sp)
        Text(
            if (state.blocks[block] == true) "ON" else "OFF",
            color = if (state.blocks[block] == true) PrototypeTheme.teal else PrototypeTheme.muted,
            fontFamily = PrototypeTheme.body,
            fontSize = 9.sp,
        )
        PrototypeButton("Open", { dispatch(PrototypeAction.SelectBlock(block)) }, Modifier.fillMaxWidth())
    }
}

@Composable
private fun BlockEditor(
    block: RigBlock,
    state: PrototypeState,
    dispatch: (PrototypeAction) -> Unit,
) {
    Row(
        Modifier.fillMaxSize(),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        FocusTitle(
            block.title,
            block.section.name,
            state,
            dispatch,
            if (block in setOf(RigBlock.SupportAmp, RigBlock.SupportIr)) PrototypeTheme.support
            else sectionAccent(block.section),
            width = if (block in setOf(RigBlock.MainAmp, RigBlock.SupportAmp)) 100.dp else 118.dp,
        )
        BlockMetaControls(block, state, dispatch)
        val parameters = parametersFor(block, state)
        Row(
            Modifier.weight(1f).fillMaxHeight(),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceEvenly,
        ) {
            parameters.forEach { parameter ->
                TouchKnob(
                    parameter = parameter,
                    value = state.parameters.getValue(parameter),
                    onBegin = { dispatch(PrototypeAction.BeginParameterEdit(parameter)) },
                    onChange = { dispatch(PrototypeAction.SetParameter(parameter, it)) },
                    modifier = Modifier.weight(1f),
                    diameter = when {
                        parameters.size >= 7 -> 46.dp
                        parameters.size >= 5 -> 50.dp
                        else -> 62.dp
                    },
                    displayLabel = parameterLabel(parameter, block, state),
                )
            }
        }
    }
}

@Composable
private fun BlockMetaControls(
    block: RigBlock,
    state: PrototypeState,
    dispatch: (PrototypeAction) -> Unit,
) {
    when (block) {
        RigBlock.Octave -> {
            val transpose = (state.modes[block] ?: 0) == 0
            Column(Modifier.width(220.dp), verticalArrangement = Arrangement.spacedBy(5.dp)) {
                ModeSelector(
                    listOf("Transpose", "Octaver"),
                    state.modes[block] ?: 0,
                    { dispatch(PrototypeAction.SetMode(block, it)) },
                    Modifier.fillMaxWidth(),
                )
                if (transpose) {
                    ModeSelector(
                        listOf("Instant", "Poly"),
                        state.pitchCharacter,
                        { dispatch(PrototypeAction.SetPitchCharacter(it)) },
                        Modifier.fillMaxWidth(),
                    )
                } else {
                    ModeSelector(
                        listOf("Vintage", "Modern"),
                        state.pitchVoicing,
                        { dispatch(PrototypeAction.SetPitchVoicing(it)) },
                        Modifier.fillMaxWidth(),
                    )
                }
            }
        }
        RigBlock.NamOne -> BrowserMetaButton(BrowserKind.NamOne, state.selectedNamOne, dispatch)
        RigBlock.NamTwo -> BrowserMetaButton(BrowserKind.NamTwo, state.selectedNamTwo, dispatch)
        RigBlock.MainAmp, RigBlock.SupportAmp -> {
            val support = block == RigBlock.SupportAmp
            val lane = if (support) AmpLane.Support else AmpLane.Main
            val kind = if (support) BrowserKind.SupportAmp else BrowserKind.Amp
            val amp = if (support) state.selectedSupportAmp else state.selectedAmp
            val channel = if (support) state.supportChannel else state.mainChannel
            val accent = if (support) PrototypeTheme.support else PrototypeTheme.amber
            val gate = if (support) state.supportGateEnabled else state.mainGateEnabled
            val eq = if (support) state.supportEqEnabled else state.mainEqEnabled
            Column(Modifier.width(180.dp), verticalArrangement = Arrangement.spacedBy(5.dp)) {
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                    PrototypeButton(
                        "AMP · ${amp.substringBefore(" · ")}",
                        {
                            dispatch(
                                PrototypeAction.OpenBrowser(
                                    kind,
                                    BrowserApply.ReturnWorkspace,
                                    PrototypeRoute.Workspace,
                                ),
                            )
                        },
                        Modifier.weight(1.55f),
                    )
                    FeatureToggle(
                        "Gate",
                        gate,
                        { dispatch(PrototypeAction.ToggleLaneFeature(lane, LaneFeature.Gate)) },
                        Modifier.weight(1f),
                        accent,
                    )
                    FeatureToggle(
                        "EQ",
                        eq,
                        { dispatch(PrototypeAction.ToggleLaneFeature(lane, LaneFeature.Eq)) },
                        Modifier.weight(1f),
                        accent,
                    )
                }
                OptionStepper(
                    "Channel",
                    channelLabel(amp, channel),
                    { dispatch(PrototypeAction.StepChannel(lane, -1)) },
                    { dispatch(PrototypeAction.StepChannel(lane, 1)) },
                    Modifier.fillMaxWidth(),
                    accent,
                )
            }
        }
        RigBlock.MainIr -> CabinetMeta(
            BrowserKind.MainIr,
            state.selectedMainIr,
            state.mainIrEnabled,
            AmpLane.Main,
            dispatch,
        )
        RigBlock.SupportIr -> CabinetMeta(
            BrowserKind.SupportIr,
            state.selectedSupportIr,
            state.supportIrEnabled,
            AmpLane.Support,
            dispatch,
        )
        RigBlock.Delay -> EffectModeMeta(
            block,
            listOf("Digital", "Analog", "Reverse"),
            state,
            dispatch,
        )
        RigBlock.Reverb -> EffectModeMeta(
            block,
            listOf("Hall", "Plate", "Oktaverb"),
            state,
            dispatch,
        )
        RigBlock.Tremolo -> EffectModeMeta(
            block,
            listOf("Optical", "Bias", "Harmonic"),
            state,
            dispatch,
        )
        RigBlock.Compressor -> Unit
    }
}

@Composable
private fun BrowserMetaButton(
    kind: BrowserKind,
    selected: String,
    dispatch: (PrototypeAction) -> Unit,
) {
    PrototypeButton(
        selected,
        {
            dispatch(
                PrototypeAction.OpenBrowser(
                    kind,
                    BrowserApply.ReturnWorkspace,
                    PrototypeRoute.Workspace,
                ),
            )
        },
        Modifier.width(126.dp),
    )
}

@Composable
private fun CabinetMeta(
    kind: BrowserKind,
    selected: String,
    enabled: Boolean,
    lane: AmpLane,
    dispatch: (PrototypeAction) -> Unit,
) {
    val accent = if (lane == AmpLane.Support) PrototypeTheme.support else PrototypeTheme.amber
    Column(Modifier.width(220.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
        PrototypeButton(
            selected,
            {
                dispatch(
                    PrototypeAction.OpenBrowser(
                        kind,
                        BrowserApply.ReturnWorkspace,
                        PrototypeRoute.Workspace,
                    ),
                )
            },
            Modifier.fillMaxWidth(),
            primary = true,
        )
        ValueBadge(
            "Src",
            when {
                selected.startsWith("DIRECT") -> "DIRECT"
                enabled -> "CUSTOM IR"
                else -> "FACTORY CAB"
            },
            Modifier.fillMaxWidth(),
            accent,
        )
    }
}

@Composable
private fun EffectModeMeta(
    block: RigBlock,
    labels: List<String>,
    state: PrototypeState,
    dispatch: (PrototypeAction) -> Unit,
) {
    val divisions = listOf("1/2", "1/4", "1/4.", "1/4T", "1/8", "1/8.", "1/8T", "1/16")
    Column(Modifier.width(198.dp), verticalArrangement = Arrangement.spacedBy(5.dp)) {
        ModeSelector(
            labels,
            state.modes[block] ?: 0,
            { dispatch(PrototypeAction.SetMode(block, it)) },
            Modifier.fillMaxWidth(),
        )
        when (block) {
            RigBlock.Delay -> Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                FeatureToggle(
                    if (state.delaySync) "Sync ${divisions[state.delayDivision]}" else "Sync",
                    state.delaySync,
                    { dispatch(PrototypeAction.ToggleDelaySync) },
                    Modifier.weight(1f),
                    PrototypeTheme.post,
                )
                if (state.delaySync) {
                    PrototypeButton(
                        divisions[state.delayDivision],
                        { dispatch(PrototypeAction.StepDelayDivision(1)) },
                        Modifier.weight(.7f),
                        active = true,
                    )
                }
                if ((state.modes[block] ?: 0) != 2) {
                    FeatureToggle(
                        "Ping",
                        state.delayPingPong,
                        { dispatch(PrototypeAction.ToggleDelayPingPong) },
                        Modifier.weight(.8f),
                        PrototypeTheme.post,
                    )
                }
            }
            RigBlock.Reverb -> {
                if ((state.modes[block] ?: 0) == 2) {
                    ModeSelector(
                        listOf("Halo", "Shimmer", "Bloom"),
                        state.reverbSubMode,
                        { dispatch(PrototypeAction.SetReverbSubMode(it)) },
                        Modifier.fillMaxWidth(),
                    )
                } else {
                    ValueBadge("Voice", labels[state.modes[block] ?: 0], Modifier.fillMaxWidth(), PrototypeTheme.post)
                }
            }
            RigBlock.Tremolo -> Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                FeatureToggle(
                    if (state.tremoloSync) "Sync ${divisions[state.tremoloDivision]}" else "Sync",
                    state.tremoloSync,
                    { dispatch(PrototypeAction.ToggleTremoloSync) },
                    Modifier.weight(1f),
                    PrototypeTheme.post,
                )
                if (state.tremoloSync) {
                    PrototypeButton(
                        divisions[state.tremoloDivision],
                        { dispatch(PrototypeAction.StepTremoloDivision(1)) },
                        Modifier.weight(.72f),
                        active = true,
                    )
                }
            }
            else -> Unit
        }
    }
}

@Composable
private fun FocusTitle(
    title: String,
    eyebrow: String,
    state: PrototypeState,
    dispatch: (PrototypeAction) -> Unit,
    accent: androidx.compose.ui.graphics.Color = PrototypeTheme.amber,
    width: androidx.compose.ui.unit.Dp = 118.dp,
) {
    Column(Modifier.width(width), verticalArrangement = Arrangement.spacedBy(5.dp)) {
        Text(eyebrow.uppercase(), color = accent, fontFamily = PrototypeTheme.body, fontSize = 8.sp)
        Text(
            title.uppercase(),
            color = PrototypeTheme.text,
            fontFamily = PrototypeTheme.display,
            fontSize = if (title == "Compressor") 9.sp else 11.sp,
            maxLines = 2,
            overflow = TextOverflow.Ellipsis,
        )
        Row(horizontalArrangement = Arrangement.spacedBy(5.dp)) {
            PrototypeButton("Done", { dispatch(PrototypeAction.ClearSelection) }, Modifier.weight(1f))
            if (state.undoEdit != null) {
                PrototypeButton("Undo", { dispatch(PrototypeAction.UndoParameterEdit) }, Modifier.weight(1f))
            }
        }
    }
}

private fun activeCount(state: PrototypeState, section: RigSection): Int =
    RigBlock.entries.count { it.section == section && state.blocks[it] == true }

private fun parametersFor(block: RigBlock, state: PrototypeState): List<RigParameter> = when (block) {
    RigBlock.Octave -> if ((state.modes[block] ?: 0) == 0) {
        listOf(RigParameter.PitchSemitones, RigParameter.PitchMix, RigParameter.PitchLevel)
    } else {
        listOf(
            RigParameter.PitchOctDown,
            RigParameter.PitchOctUp,
            RigParameter.PitchDry,
            RigParameter.PitchLevel,
        )
    }
    RigBlock.Compressor -> listOf(
        RigParameter.CompAmount,
        RigParameter.CompAttack,
        RigParameter.CompRelease,
        RigParameter.CompLevel,
    )
    RigBlock.NamOne -> listOf(
        RigParameter.NamOneGain,
        RigParameter.NamOneBass,
        RigParameter.NamOneMid,
        RigParameter.NamOneMidFreq,
        RigParameter.NamOneTreble,
        RigParameter.NamOneLevel,
    )
    RigBlock.NamTwo -> listOf(
        RigParameter.NamTwoGain,
        RigParameter.NamTwoBass,
        RigParameter.NamTwoMid,
        RigParameter.NamTwoMidFreq,
        RigParameter.NamTwoTreble,
        RigParameter.NamTwoLevel,
    )
    RigBlock.MainAmp -> buildList {
        add(RigParameter.MainInput)
        add(RigParameter.MainGate)
        add(RigParameter.MainBass)
        add(RigParameter.MainMid)
        add(RigParameter.MainTreble)
        add(RigParameter.MainOutput)
        if (state.supportEnabled) add(RigParameter.MainPan)
    }
    RigBlock.SupportAmp -> listOf(
        RigParameter.SupportInput,
        RigParameter.SupportGate,
        RigParameter.SupportBass,
        RigParameter.SupportMid,
        RigParameter.SupportTreble,
        RigParameter.SupportOutput,
        RigParameter.SupportPan,
    )
    RigBlock.MainIr, RigBlock.SupportIr -> emptyList()
    RigBlock.Delay -> buildList {
        if (!state.delaySync) add(RigParameter.DelayTime)
        add(RigParameter.DelayFeedback)
        add(RigParameter.DelayMix)
        add(RigParameter.DelayTone)
        add(RigParameter.DelayAge)
    }
    RigBlock.Reverb -> buildList {
        add(RigParameter.ReverbMix)
        add(RigParameter.ReverbDecay)
        add(RigParameter.ReverbTone)
        add(RigParameter.ReverbPreDelay)
        if ((state.modes[block] ?: 0) == 2) add(RigParameter.ReverbShimmer)
    }
    RigBlock.Tremolo -> buildList {
        if (!state.tremoloSync) add(RigParameter.TremoloRate)
        add(RigParameter.TremoloDepth)
        add(RigParameter.TremoloShape)
        add(RigParameter.TremoloMix)
        if ((state.modes[block] ?: 0) == 2) add(RigParameter.TremoloCrossover)
    }
}

private fun parameterLabel(parameter: RigParameter, block: RigBlock, state: PrototypeState): String {
    if (block == RigBlock.Delay && parameter == RigParameter.DelayAge) {
        return listOf("Grit", "Wear", "Bloom")[state.modes[block] ?: 0]
    }
    return parameter.label
}

private fun channelLabel(amp: String, channel: Int): String {
    val labels = when {
        amp.contains("Blackbird", ignoreCase = true) -> listOf("Clean", "Edge", "Drive", "Lead")
        amp.contains("California", ignoreCase = true) -> listOf("Clean", "Crunch", "Lead", "Solo")
        amp.contains("Silver", ignoreCase = true) -> listOf("Normal", "Bright", "Push", "Boost")
        amp.contains("Rectified", ignoreCase = true) -> listOf("Clean", "Raw", "Vintage", "Modern")
        else -> listOf("Ch 1", "Ch 2", "Ch 3", "Ch 4")
    }
    return labels[channel.coerceIn(0, labels.lastIndex)]
}
