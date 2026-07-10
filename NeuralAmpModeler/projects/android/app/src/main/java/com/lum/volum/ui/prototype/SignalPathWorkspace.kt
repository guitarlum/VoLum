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
            label = state.selectedPreset,
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
        PrototypeButton(
            label = "IN · ${state.selectedDevice.replace("Audient ", "")}",
            onClick = {
                dispatch(
                    PrototypeAction.OpenBrowser(
                        BrowserKind.Device,
                        BrowserApply.ReturnWorkspace,
                        PrototypeRoute.Workspace,
                    ),
                )
            },
            modifier = Modifier.weight(1.05f),
        )
        Column(Modifier.width(96.dp)) {
            Text(
                "${state.latencyMs} ms · ${state.xruns} xr",
                color = if (state.xruns == 0) PrototypeTheme.teal else PrototypeTheme.red,
                fontFamily = PrototypeTheme.display,
                fontSize = 8.sp,
            )
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(5.dp)) {
                Text("OUT", color = PrototypeTheme.muted, fontFamily = PrototypeTheme.body, fontSize = 8.sp)
                LevelMeter(state.outputPeak, Modifier.weight(1f).height(7.dp))
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
            modifier = Modifier.width(72.dp),
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
) {
    blocks.forEach { block ->
        BlockTile(
            block = block,
            enabled = state.blocks[block] == true,
            selected = state.selectedBlock == block,
            onSelect = { dispatch(PrototypeAction.SelectBlock(block)) },
            onToggle = { dispatch(PrototypeAction.ToggleBlock(block)) },
            modifier = Modifier.weight(1f),
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
    Column(
        Modifier
            .weight(weight)
            .fillMaxHeight()
            .border(
                1.dp,
                if (state.selectedSection == section) PrototypeTheme.amber else PrototypeTheme.line,
                RoundedCornerShape(8.dp),
            ),
    ) {
        SectionHeading(
            section,
            selected = state.selectedSection == section,
            onSelect = { dispatch(PrototypeAction.SelectSection(section)) },
            modifier = Modifier.fillMaxWidth(),
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
                if (state.selectedSection == RigSection.Amp) PrototypeTheme.amber else PrototypeTheme.line,
                RoundedCornerShape(8.dp),
            ),
    ) {
        SectionHeading(
            RigSection.Amp,
            selected = state.selectedSection == RigSection.Amp,
            onSelect = { dispatch(PrototypeAction.SelectSection(RigSection.Amp)) },
            modifier = Modifier.fillMaxWidth(),
        )
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
            RowScopeChainBlocks(listOf(RigBlock.MainAmp, RigBlock.MainIr), state, dispatch)
            if (state.supportEnabled) {
                RowScopeChainBlocks(listOf(RigBlock.SupportAmp, RigBlock.SupportIr), state, dispatch)
            } else {
                Box(
                    Modifier
                        .weight(2f)
                        .height(92.dp)
                        .background(PrototypeTheme.panel, RoundedCornerShape(8.dp))
                        .border(1.dp, PrototypeTheme.line, RoundedCornerShape(8.dp))
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
                fontSize = 14.sp,
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
        ValueBadge("USB", if (state.usbConnected) "EVO 4" else "MISSING", Modifier.weight(.75f))
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
        horizontalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        FocusTitle(section.name, "SECTION SUMMARY", state, dispatch)
        when (section) {
            RigSection.Pre -> {
                ValueBadge("Active", "${activeCount(state, RigSection.Pre)} / 4", Modifier.weight(.7f))
                ValueBadge("NAM 1", state.selectedNamOne, Modifier.weight(1.1f))
                ValueBadge("NAM 2", state.selectedNamTwo, Modifier.weight(1.1f))
                PrototypeButton(
                    if (state.preLocked) "PRE locked" else "Lock PRE",
                    onClick = { dispatch(PrototypeAction.ToggleSectionLock(RigSection.Pre)) },
                    modifier = Modifier.width(96.dp),
                    active = state.preLocked,
                )
            }
            RigSection.Amp -> {
                ValueBadge("Main", state.selectedAmp, Modifier.weight(1.15f))
                ValueBadge("Main IR", state.selectedMainIr, Modifier.weight(1.15f))
                PrototypeButton(
                    if (state.supportEnabled) "Support on" else "Support off",
                    onClick = { dispatch(PrototypeAction.ToggleSupport) },
                    modifier = Modifier.width(106.dp),
                    active = state.supportEnabled,
                )
                if (state.supportEnabled) {
                    ValueBadge("Support", state.selectedSupportAmp, Modifier.weight(1f))
                }
            }
            RigSection.Post -> {
                EffectSummary(RigBlock.Delay, state, dispatch, Modifier.weight(1f))
                EffectSummary(RigBlock.Reverb, state, dispatch, Modifier.weight(1f))
                EffectSummary(RigBlock.Tremolo, state, dispatch, Modifier.weight(1f))
            }
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
        FocusTitle(block.title, block.section.name, state, dispatch)
        val parameters = parametersFor(block)
        if (block in setOf(RigBlock.MainAmp, RigBlock.SupportAmp)) {
            PrototypeButton(
                if (block == RigBlock.MainAmp) state.selectedAmp else state.selectedSupportAmp,
                onClick = {
                    dispatch(
                        PrototypeAction.OpenBrowser(
                            if (block == RigBlock.MainAmp) BrowserKind.Amp else BrowserKind.SupportAmp,
                            BrowserApply.ReturnWorkspace,
                            PrototypeRoute.Workspace,
                        ),
                    )
                },
                modifier = Modifier.width(126.dp),
            )
        }
        if (block in setOf(RigBlock.NamOne, RigBlock.NamTwo, RigBlock.MainIr, RigBlock.SupportIr)) {
            val kind = when (block) {
                RigBlock.NamOne -> BrowserKind.NamOne
                RigBlock.NamTwo -> BrowserKind.NamTwo
                RigBlock.MainIr -> BrowserKind.MainIr
                else -> BrowserKind.SupportIr
            }
            val selected = when (kind) {
                BrowserKind.NamOne -> state.selectedNamOne
                BrowserKind.NamTwo -> state.selectedNamTwo
                BrowserKind.MainIr -> state.selectedMainIr
                BrowserKind.SupportIr -> state.selectedSupportIr
                else -> ""
            }
            PrototypeButton(
                selected,
                onClick = {
                    dispatch(
                        PrototypeAction.OpenBrowser(
                            kind,
                            BrowserApply.ReturnWorkspace,
                            PrototypeRoute.Workspace,
                        ),
                    )
                },
                modifier = Modifier.width(144.dp),
            )
        }
        if (block in setOf(RigBlock.Delay, RigBlock.Reverb, RigBlock.Tremolo)) {
            val labels = when (block) {
                RigBlock.Delay -> listOf("Digital", "Analog", "Reverse")
                RigBlock.Reverb -> listOf("Hall", "Plate", "Oktaverb")
                else -> listOf("Optical", "Bias", "Harmonic")
            }
            ModeSelector(
                labels,
                state.modes[block] ?: 0,
                { dispatch(PrototypeAction.SetMode(block, it)) },
                Modifier.width(240.dp),
            )
        }
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
                    diameter = if (parameters.size > 5) 50.dp else 62.dp,
                )
            }
        }
    }
}

@Composable
private fun FocusTitle(
    title: String,
    eyebrow: String,
    state: PrototypeState,
    dispatch: (PrototypeAction) -> Unit,
) {
    Column(Modifier.width(118.dp), verticalArrangement = Arrangement.spacedBy(5.dp)) {
        Text(eyebrow.uppercase(), color = PrototypeTheme.amber, fontFamily = PrototypeTheme.body, fontSize = 8.sp)
        Text(
            title.uppercase(),
            color = PrototypeTheme.text,
            fontFamily = PrototypeTheme.display,
            fontSize = 11.sp,
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

private fun parametersFor(block: RigBlock): List<RigParameter> = when (block) {
    RigBlock.Octave -> listOf(RigParameter.OctaveShift, RigParameter.OctaveMix)
    RigBlock.Compressor -> listOf(
        RigParameter.CompThreshold,
        RigParameter.CompRatio,
        RigParameter.CompAttack,
        RigParameter.CompRelease,
    )
    RigBlock.NamOne -> listOf(RigParameter.NamOneLevel, RigParameter.NamOneMix)
    RigBlock.NamTwo -> listOf(RigParameter.NamTwoLevel, RigParameter.NamTwoMix)
    RigBlock.MainAmp -> listOf(
        RigParameter.Drive,
        RigParameter.Bass,
        RigParameter.Mid,
        RigParameter.Treble,
        RigParameter.Level,
        RigParameter.GateThreshold,
        RigParameter.GateRelease,
    )
    RigBlock.SupportAmp -> listOf(
        RigParameter.Drive,
        RigParameter.Bass,
        RigParameter.Mid,
        RigParameter.Treble,
        RigParameter.Level,
        RigParameter.GateThreshold,
        RigParameter.SupportPan,
    )
    RigBlock.MainIr, RigBlock.SupportIr -> listOf(RigParameter.IrLevel)
    RigBlock.Delay -> listOf(RigParameter.DelayTime, RigParameter.DelayFeedback, RigParameter.DelayMix)
    RigBlock.Reverb -> listOf(
        RigParameter.ReverbSize,
        RigParameter.ReverbDecay,
        RigParameter.ReverbMix,
        RigParameter.ReverbTone,
    )
    RigBlock.Tremolo -> listOf(RigParameter.TremoloRate, RigParameter.TremoloDepth, RigParameter.TremoloShape)
}
