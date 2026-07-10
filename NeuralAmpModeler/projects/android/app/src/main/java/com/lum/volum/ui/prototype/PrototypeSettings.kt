package com.lum.volum.ui.prototype

import androidx.compose.foundation.background
import androidx.compose.foundation.border
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
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

@Composable
fun SettingsExperience(
    state: PrototypeState,
    dispatch: (PrototypeAction) -> Unit,
) {
    Column(
        Modifier
            .fillMaxSize()
            .background(Brush.verticalGradient(listOf(PrototypeTheme.canvas, PrototypeTheme.panel))),
    ) {
        SettingsHeader("SETTINGS", "Audio, performance, and content live here", "Done") {
            dispatch(PrototypeAction.CloseSettings)
        }
        Row(Modifier.fillMaxSize().padding(8.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Column(
                Modifier
                    .width(136.dp)
                    .fillMaxHeight()
                    .background(PrototypeTheme.inset, RoundedCornerShape(10.dp))
                    .border(1.dp, PrototypeTheme.line, RoundedCornerShape(10.dp))
                    .padding(6.dp),
                verticalArrangement = Arrangement.spacedBy(6.dp),
            ) {
                SettingsTab.entries.forEach { tab ->
                    PrototypeButton(
                        tab.name,
                        { dispatch(PrototypeAction.SelectSettingsTab(tab)) },
                        Modifier.fillMaxWidth(),
                        active = state.settingsTab == tab,
                    )
                }
            }
            Box(
                Modifier
                    .weight(1f)
                    .fillMaxHeight()
                    .background(PrototypeTheme.panel, RoundedCornerShape(10.dp))
                    .border(1.dp, PrototypeTheme.line, RoundedCornerShape(10.dp))
                    .padding(10.dp),
            ) {
                when (state.settingsTab) {
                    SettingsTab.Audio -> AudioSettings(state, dispatch)
                    SettingsTab.Performance -> PerformanceSettings(state, dispatch)
                    SettingsTab.Libraries -> LibrarySettings(state, dispatch)
                    SettingsTab.About -> AboutSettings(state)
                }
            }
        }
    }
}

@Composable
private fun SettingsHeader(
    title: String,
    subtitle: String,
    action: String,
    onAction: () -> Unit,
) {
    Row(
        Modifier
            .fillMaxWidth()
            .height(54.dp)
            .background(PrototypeTheme.panel)
            .border(1.dp, PrototypeTheme.line)
            .padding(horizontal = 12.dp, vertical = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        Column(Modifier.weight(1f)) {
            Text(title, color = PrototypeTheme.text, fontFamily = PrototypeTheme.display, fontSize = 14.sp)
            Text(
                subtitle,
                color = PrototypeTheme.muted,
                fontFamily = PrototypeTheme.body,
                fontSize = 9.sp,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
        }
        PrototypeButton(action, onAction, Modifier.width(82.dp), primary = true)
    }
}

@Composable
private fun AudioSettings(
    state: PrototypeState,
    dispatch: (PrototypeAction) -> Unit,
) {
    Column(Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        SettingsTitle("AUDIO", "Device changes are deliberate and restart the stream.")
        Row(Modifier.fillMaxWidth().weight(1.12f), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            SettingCard("INPUT DEVICE", Modifier.weight(1.35f)) {
                PrototypeButton(
                    "Change · ${state.selectedDevice.replace("Audient ", "")}",
                    {
                        dispatch(
                            PrototypeAction.OpenBrowser(
                                BrowserKind.Device,
                                BrowserApply.ReturnWorkspace,
                                PrototypeRoute.Settings,
                            ),
                        )
                    },
                    Modifier.fillMaxWidth(),
                    primary = true,
                )
            }
            SettingCard("SAMPLE RATE", Modifier.weight(1f)) {
                val rates = listOf(44100, 48000, 96000)
                ModeSelector(
                    listOf("44", "48", "96"),
                    rates.indexOf(state.sampleRate).coerceAtLeast(0),
                    { dispatch(PrototypeAction.SetSampleRate(rates[it])) },
                    Modifier.fillMaxWidth(),
                )
                Text("kHz", color = PrototypeTheme.muted, fontFamily = PrototypeTheme.body, fontSize = 8.sp)
            }
            SettingCard("BUFFER", Modifier.weight(1.1f)) {
                val buffers = listOf(32, 64, 128, 256)
                ModeSelector(
                    buffers.map(Int::toString),
                    buffers.indexOf(state.bufferFrames).coerceAtLeast(0),
                    { dispatch(PrototypeAction.SetBufferFrames(buffers[it])) },
                    Modifier.fillMaxWidth(),
                )
                Text("samples", color = PrototypeTheme.muted, fontFamily = PrototypeTheme.body, fontSize = 8.sp)
            }
        }
        Row(Modifier.fillMaxWidth().weight(.88f), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            SettingCard("INPUT CALIBRATION", Modifier.weight(1.25f)) {
                val levels = listOf(.4167f, .5333f, .6f)
                val current = state.parameters.getValue(RigParameter.InputCalibration)
                val index = levels.indices.minByOrNull { kotlin.math.abs(levels[it] - current) } ?: 2
                Row(
                    Modifier.fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                ) {
                    FeatureToggle(
                        "Calibrate",
                        state.calibrateInput,
                        { dispatch(PrototypeAction.ToggleInputCalibration) },
                        Modifier.weight(1f),
                        PrototypeTheme.pre,
                    )
                    PrototypeButton(
                        "${listOf(-10, 4, 12)[index]} dBu",
                        {
                            dispatch(
                                PrototypeAction.SetParameter(
                                    RigParameter.InputCalibration,
                                    levels[(index + 1) % levels.size],
                                ),
                            )
                        },
                        Modifier.weight(1f),
                    )
                }
            }
            SettingCard("STREAM HEALTH", Modifier.weight(1f)) {
                ValueBadge(
                    "Stream",
                    "${state.latencyMs} ms · ${state.xruns} xr",
                    Modifier.fillMaxWidth(),
                    if (state.xruns == 0) PrototypeTheme.post else PrototypeTheme.red,
                )
            }
            SettingCard("CONNECTION", Modifier.weight(1f)) {
                ValueBadge(
                    "USB",
                    if (state.usbConnected) "EVO 4 · IN 1" else "MISSING",
                    Modifier.fillMaxWidth(),
                    PrototypeTheme.pre,
                )
            }
        }
        FeedbackLine(state.feedbackMessage)
    }
}

@Composable
private fun PerformanceSettings(
    state: PrototypeState,
    dispatch: (PrototypeAction) -> Unit,
) {
    Column(Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(10.dp)) {
        SettingsTitle("PERFORMANCE", "Signal behavior and CPU policy.")
        Row(Modifier.fillMaxWidth().weight(1f), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            SettingCard("OUTPUT MODE", Modifier.weight(1.4f)) {
                ModeSelector(
                    listOf("Raw", "Normalized", "Calibrated"),
                    state.outputMode,
                    { dispatch(PrototypeAction.SetOutputMode(it)) },
                    Modifier.fillMaxWidth(),
                )
                Text(
                    listOf("No gain adjustment.", "Consistent model loudness.", "Matches input calibration.")[state.outputMode],
                    color = PrototypeTheme.muted,
                    fontFamily = PrototypeTheme.body,
                    fontSize = 9.sp,
                    textAlign = TextAlign.Center,
                )
            }
            SettingCard("CPU MODE", Modifier.weight(1f)) {
                FeatureToggle(
                    "Lite",
                    state.liteMode,
                    { dispatch(PrototypeAction.ToggleLiteMode) },
                    Modifier.fillMaxWidth(),
                    PrototypeTheme.amber,
                )
                Text(
                    "Smaller A2 slice · lower CPU",
                    color = PrototypeTheme.muted,
                    fontFamily = PrototypeTheme.body,
                    fontSize = 9.sp,
                    textAlign = TextAlign.Center,
                )
            }
            SettingCard("LIVE STATUS", Modifier.weight(1f)) {
                ValueBadge("DSP", if (state.powered) "LIVE" else "OFF", Modifier.fillMaxWidth())
                ValueBadge("Bypass", if (state.bypassed) "ON" else "OFF", Modifier.fillMaxWidth(), PrototypeTheme.amber)
            }
        }
        FeedbackLine(state.feedbackMessage)
    }
}

@Composable
private fun LibrarySettings(
    state: PrototypeState,
    dispatch: (PrototypeAction) -> Unit,
) {
    Column(Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        SettingsTitle("LIBRARIES", "Import, edit, and remove content away from the live stage.")
        Row(Modifier.fillMaxWidth().weight(1f), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            LibraryCard("AMPS", "Factory + custom models", BrowserKind.Amp, PrototypeTheme.amber, dispatch)
            LibraryCard("PRE CAPTURES", "NAM 1 + NAM 2 captures", BrowserKind.PreLibrary, PrototypeTheme.pre, dispatch)
            LibraryCard("CABINET IRS", "Main + support IRs", BrowserKind.CabLibrary, PrototypeTheme.support, dispatch)
            LibraryCard("PRESETS", state.selectedPreset, BrowserKind.Preset, PrototypeTheme.post, dispatch)
        }
        FeedbackLine(state.feedbackMessage)
    }
}

@Composable
private fun RowScope.LibraryCard(
    title: String,
    detail: String,
    kind: BrowserKind,
    accent: androidx.compose.ui.graphics.Color,
    dispatch: (PrototypeAction) -> Unit,
) {
    Column(
        Modifier
            .weight(1f)
            .fillMaxHeight()
            .background(accent.copy(.07f), RoundedCornerShape(10.dp))
            .border(1.dp, accent.copy(.55f), RoundedCornerShape(10.dp))
            .padding(10.dp),
        verticalArrangement = Arrangement.SpaceBetween,
    ) {
        Column {
            Text(title, color = accent, fontFamily = PrototypeTheme.display, fontSize = 10.sp)
            Text(detail, color = PrototypeTheme.muted, fontFamily = PrototypeTheme.body, fontSize = 9.sp)
        }
        PrototypeButton(
            "Manage",
            {
                dispatch(
                    PrototypeAction.OpenBrowser(
                        kind,
                        BrowserApply.ReturnWorkspace,
                        PrototypeRoute.Settings,
                        manage = true,
                    ),
                )
            },
            Modifier.fillMaxWidth(),
        )
    }
}

@Composable
private fun AboutSettings(state: PrototypeState) {
    Column(
        Modifier.fillMaxSize(),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.Center,
    ) {
        Text("VOLUM", color = PrototypeTheme.text, fontFamily = PrototypeTheme.display, fontSize = 24.sp)
        Text("Android UX prototype", color = PrototypeTheme.amber, fontFamily = PrototypeTheme.body, fontSize = 12.sp)
        Text(
            "Fixed PRE → AMP/CAB → POST signal path",
            color = PrototypeTheme.muted,
            fontFamily = PrototypeTheme.body,
            fontSize = 10.sp,
        )
        Text(
            "${state.selectedDevice} · ${state.sampleRate / 1000} kHz · ${state.bufferFrames} samples",
            color = PrototypeTheme.teal,
            fontFamily = PrototypeTheme.body,
            fontSize = 10.sp,
        )
    }
}

@Composable
private fun SettingCard(
    title: String,
    modifier: Modifier = Modifier,
    content: @Composable () -> Unit,
) {
    Column(
        modifier
            .fillMaxHeight()
            .background(PrototypeTheme.inset, RoundedCornerShape(10.dp))
            .border(1.dp, PrototypeTheme.line, RoundedCornerShape(10.dp))
            .padding(10.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.SpaceEvenly,
    ) {
        Text(title, color = PrototypeTheme.amber, fontFamily = PrototypeTheme.display, fontSize = 9.sp)
        content()
    }
}

@Composable
private fun SettingsTitle(title: String, subtitle: String) {
    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        Text(title, color = PrototypeTheme.text, fontFamily = PrototypeTheme.display, fontSize = 13.sp)
        Text(
            "  ·  $subtitle",
            color = PrototypeTheme.muted,
            fontFamily = PrototypeTheme.body,
            fontSize = 9.sp,
            maxLines = 1,
        )
    }
}

@Composable
private fun FeedbackLine(message: String?) {
    Text(
        message ?: "Changes are kept in deterministic prototype state.",
        color = if (message == null) PrototypeTheme.muted else PrototypeTheme.teal,
        fontFamily = PrototypeTheme.body,
        fontSize = 9.sp,
        maxLines = 1,
        modifier = Modifier.height(14.dp),
    )
}

@Composable
fun MetronomeExperience(
    state: PrototypeState,
    dispatch: (PrototypeAction) -> Unit,
) {
    Column(
        Modifier
            .fillMaxSize()
            .background(
                Brush.radialGradient(
                    listOf(PrototypeTheme.post.copy(.12f), PrototypeTheme.canvas),
                ),
            ),
    ) {
        SettingsHeader("METRONOME", "Stage click · delay and tremolo sync use this tempo", "Done") {
            dispatch(PrototypeAction.CloseMetronome)
        }
        Row(
            Modifier.fillMaxSize().padding(10.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            FeatureToggle(
                "Click",
                state.metronomeActive,
                { dispatch(PrototypeAction.ToggleMetronome) },
                Modifier.width(94.dp),
                PrototypeTheme.post,
            )
            OptionStepper(
                "Tempo",
                "${state.metronomeBpm} BPM",
                { dispatch(PrototypeAction.StepMetronomeBpm(-1)) },
                { dispatch(PrototypeAction.StepMetronomeBpm(1)) },
                Modifier.width(172.dp),
                PrototypeTheme.post,
            )
            Column(Modifier.width(166.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                Text("TIME SIGNATURE", color = PrototypeTheme.muted, fontFamily = PrototypeTheme.body, fontSize = 8.sp)
                ModeSelector(
                    listOf("1/4", "2/4", "3/4", "4/4", "6/8"),
                    state.metronomeTimeSig,
                    { dispatch(PrototypeAction.SetMetronomeTimeSig(it)) },
                    Modifier.fillMaxWidth(),
                )
            }
            TouchKnob(
                RigParameter.MetronomeVolume,
                state.parameters.getValue(RigParameter.MetronomeVolume),
                { dispatch(PrototypeAction.BeginParameterEdit(RigParameter.MetronomeVolume)) },
                { dispatch(PrototypeAction.SetParameter(RigParameter.MetronomeVolume, it)) },
                Modifier.width(72.dp),
                diameter = 64.dp,
            )
            Column(Modifier.width(92.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                ValueBadge("Beat", if (state.metronomeActive) "1 / 4" else "STOPPED", Modifier.fillMaxWidth(), PrototypeTheme.post)
                Text(
                    "Tap ± for single BPM steps. Hold knob, then drag for fine volume.",
                    color = PrototypeTheme.muted,
                    fontFamily = PrototypeTheme.body,
                    fontSize = 8.sp,
                    lineHeight = 11.sp,
                )
            }
        }
    }
}

@Composable
fun ContentEditorExperience(
    state: PrototypeState,
    dispatch: (PrototypeAction) -> Unit,
) {
    val kind = state.contentEditorKind ?: return
    val noun = when (kind) {
        BrowserKind.Amp, BrowserKind.SupportAmp -> "CUSTOM AMP"
        BrowserKind.NamOne, BrowserKind.NamTwo, BrowserKind.PreLibrary -> "PRE CAPTURE"
        BrowserKind.MainIr, BrowserKind.SupportIr, BrowserKind.CabLibrary -> "CABINET IR"
        BrowserKind.Preset -> "PRESET"
        BrowserKind.Device -> "AUDIO DEVICE"
    }
    Column(Modifier.fillMaxSize().background(PrototypeTheme.canvas)) {
        SettingsHeader(
            if (state.contentEditorExisting) "EDIT $noun" else "IMPORT $noun",
            "Changes remain staged until Save",
            "Cancel",
        ) { dispatch(PrototypeAction.CloseContentEditor) }
        Row(
            Modifier.fillMaxSize().padding(10.dp),
            horizontalArrangement = Arrangement.spacedBy(10.dp),
        ) {
            SettingCard("IDENTITY", Modifier.weight(1f)) {
                ValueBadge(
                    "Name",
                    if (state.contentEditorExisting) "Selected item" else "New ${noun.lowercase()}",
                    Modifier.fillMaxWidth(),
                )
                PrototypeButton(
                    "Rename",
                    { dispatch(PrototypeAction.RenameContent) },
                    Modifier.fillMaxWidth(),
                )
            }
            SettingCard("SOURCE FILES", Modifier.weight(1.15f)) {
                ValueBadge(
                    "Files",
                    if (state.contentFilesStaged) "MAPPED" else "NONE",
                    Modifier.fillMaxWidth(),
                    PrototypeTheme.pre,
                )
                PrototypeButton(
                    when (kind) {
                        BrowserKind.Preset -> "Capture live rig"
                        BrowserKind.MainIr, BrowserKind.SupportIr, BrowserKind.CabLibrary -> "Choose .wav"
                        else -> "Add .nam files"
                    },
                    { dispatch(PrototypeAction.StageContentFiles) },
                    Modifier.fillMaxWidth(),
                    primary = true,
                )
            }
            SettingCard("ASSIGNMENT", Modifier.weight(1.35f)) {
                when (kind) {
                    BrowserKind.Amp, BrowserKind.SupportAmp -> {
                        ModeSelector(listOf("Ch 1", "Ch 2", "Ch 3", "Ch 4"), 1, {}, Modifier.fillMaxWidth())
                        ModeSelector(listOf("Direct", "Cab A", "Cab B"), 1, {}, Modifier.fillMaxWidth())
                    }
                    BrowserKind.NamOne, BrowserKind.NamTwo, BrowserKind.PreLibrary ->
                        ModeSelector(listOf("NAM 1", "NAM 2"), 0, {}, Modifier.fillMaxWidth())
                    BrowserKind.MainIr, BrowserKind.SupportIr, BrowserKind.CabLibrary ->
                        ModeSelector(listOf("Main", "Support"), 0, {}, Modifier.fillMaxWidth())
                    BrowserKind.Preset ->
                        ValueBadge("Owner", state.selectedAmp.substringBefore(" · "), Modifier.fillMaxWidth())
                    BrowserKind.Device -> ValueBadge("Type", "USB AUDIO", Modifier.fillMaxWidth())
                }
            }
            Column(
                Modifier.width(112.dp).fillMaxHeight(),
                verticalArrangement = Arrangement.spacedBy(8.dp, Alignment.Bottom),
            ) {
                if (state.contentEditorExisting) {
                    PrototypeButton(
                        "Delete",
                        { dispatch(PrototypeAction.DeleteLibraryItem) },
                        Modifier.fillMaxWidth(),
                        danger = true,
                    )
                }
                PrototypeButton(
                    "Save",
                    { dispatch(PrototypeAction.SaveContentEditor) },
                    Modifier.fillMaxWidth(),
                    primary = true,
                )
                state.feedbackMessage?.let {
                    Text(
                        it,
                        color = PrototypeTheme.teal,
                        fontFamily = PrototypeTheme.body,
                        fontSize = 8.sp,
                        maxLines = 2,
                    )
                }
            }
        }
    }
}
