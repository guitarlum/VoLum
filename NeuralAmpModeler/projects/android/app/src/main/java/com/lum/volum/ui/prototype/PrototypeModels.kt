package com.lum.volum.ui.prototype

import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue

enum class PrototypeRoute { Setup, Workspace, Browser, ContentEditor, Settings, Tuner, Metronome, Loading, Error }
enum class SetupStep { Permission, Usb }
enum class RigSection { Pre, Amp, Post }
enum class AmpLane { Main, Support }
enum class LaneFeature { Gate, Eq, Ir }
enum class SettingsTab { Audio, Performance, Libraries, About }

enum class RigBlock(
    val section: RigSection,
    val shortLabel: String,
    val title: String,
) {
    Octave(RigSection.Pre, "OCT", "Octave / Transpose"),
    Compressor(RigSection.Pre, "COMP", "Compressor"),
    NamOne(RigSection.Pre, "NAM 1", "NAM Pedal 1"),
    NamTwo(RigSection.Pre, "NAM 2", "NAM Pedal 2"),
    MainAmp(RigSection.Amp, "AMP", "Main Amp"),
    MainIr(RigSection.Amp, "IR", "Main IR"),
    SupportAmp(RigSection.Amp, "S AMP", "Support Amp"),
    SupportIr(RigSection.Amp, "S IR", "Support IR"),
    Delay(RigSection.Post, "DLY", "Delay"),
    Reverb(RigSection.Post, "REV", "Reverb"),
    Tremolo(RigSection.Post, "TREM", "Tremolo"),
}

enum class BrowserKind(val title: String) {
    Device("Input device"),
    Amp("Amp model"),
    SupportAmp("Support amp model"),
    NamOne("NAM Pedal 1 capture"),
    NamTwo("NAM Pedal 2 capture"),
    PreLibrary("PRE capture library"),
    MainIr("Main cabinet IR"),
    SupportIr("Support cabinet IR"),
    CabLibrary("Cabinet / IR library"),
    Preset("Rig preset"),
}

enum class BrowserApply {
    SelectInitialDevice,
    FinishInitialRig,
    ReturnWorkspace,
}

data class BrowserItem(
    val id: String,
    val name: String,
    val subtitle: String,
    val detail: String,
    val tag: String,
)

val browserCatalogs: Map<BrowserKind, List<BrowserItem>> = mapOf(
    BrowserKind.Device to listOf(
        BrowserItem("evo4", "Audient EVO 4", "USB · 2 in / 2 out", "Instrument input 1 · 48 kHz · 64 samples", "RECOMMENDED"),
        BrowserItem("motu", "MOTU M2", "USB · 2 in / 2 out", "Instrument input 1 · 48 kHz · 64 samples", "USB"),
        BrowserItem("scarlett", "Scarlett Solo", "USB · 2 in / 2 out", "Instrument input · 48 kHz · 128 samples", "USB"),
        BrowserItem("phone", "Phone microphone", "Built-in · mono", "Fallback input. Output latency will be higher.", "FALLBACK"),
    ),
    BrowserKind.Amp to listOf(
        BrowserItem("blackbird", "Blackbird 30 · Edge", "Factory amp · Channel 2", "Open British crunch with a firm low end and touch-sensitive breakup.", "FACTORY"),
        BrowserItem("california", "California Lead", "Factory amp · Channel 3", "Saturated lead voice with a focused midrange.", "FACTORY"),
        BrowserItem("clean", "Silver Bell Clean", "Factory amp · Channel 1", "Wide clean headroom with a bright American top.", "FACTORY"),
        BrowserItem("rectified", "Rectified Modern", "Factory amp · Channel 4", "Tight modern gain with a controlled bass response.", "FACTORY"),
        BrowserItem("broken", "Unverified model", "Recovery demo", "This item intentionally demonstrates safe load failure and retry.", "TEST ERROR"),
    ),
    BrowserKind.SupportAmp to listOf(
        BrowserItem("none", "None · Lane muted", "Support lane", "Keeps Dual Amp available while removing the support model.", "NONE"),
        BrowserItem("california", "California Lead", "Factory amp · Channel 3", "Saturated lead voice with a focused midrange.", "FACTORY"),
        BrowserItem("blackbird", "Blackbird 30 · Edge", "Factory amp · Channel 2", "Open British crunch with a firm low end and touch-sensitive breakup.", "FACTORY"),
        BrowserItem("clean", "Silver Bell Clean", "Factory amp · Channel 1", "Wide clean headroom with a bright American top.", "FACTORY"),
        BrowserItem("rectified", "Rectified Modern", "Factory amp · Channel 4", "Tight modern gain with a controlled bass response.", "FACTORY"),
    ),
    BrowserKind.NamOne to listOf(
        BrowserItem("808", "Green Drive · Hot", "PRE capture", "Mid-forward drive capture for tightening high gain amps.", "BUNDLED"),
        BrowserItem("boost", "Clean Boost · 12 dB", "PRE capture", "Transparent level boost with a slight upper-mid lift.", "BUNDLED"),
        BrowserItem("fuzz", "Velvet Fuzz", "PRE capture", "Compressed vintage fuzz with a rounded attack.", "BUNDLED"),
    ),
    BrowserKind.NamTwo to listOf(
        BrowserItem("klon", "Gold Drive · Edge", "PRE capture", "Low-gain boost with broad harmonic lift.", "BUNDLED"),
        BrowserItem("dist", "Red Distortion", "PRE capture", "Dense distortion for sustain and harmonic focus.", "BUNDLED"),
        BrowserItem("custom-pre", "My pedal capture", "Custom PRE", "Imported user capture.", "CUSTOM"),
    ),
    BrowserKind.PreLibrary to listOf(
        BrowserItem("808", "Green Drive · Hot", "NAM Pedal 1", "Bundled PRE capture.", "BUNDLED"),
        BrowserItem("klon", "Gold Drive · Edge", "NAM Pedal 2", "Bundled PRE capture.", "BUNDLED"),
        BrowserItem("custom-pre", "My pedal capture", "Custom PRE", "Imported user capture.", "CUSTOM"),
    ),
    BrowserKind.MainIr to listOf(
        BrowserItem("direct", "DIRECT · No cabinet", "Amp output", "Bypasses cabinet and custom IR processing.", "DIRECT"),
        BrowserItem("v30", "4×12 V30 · 57 edge", "Factory IR", "Close dynamic microphone, edge of dust cap.", "FACTORY"),
        BrowserItem("green", "4×12 Green · 121", "Factory IR", "Ribbon microphone with a warm upper midrange.", "FACTORY"),
        BrowserItem("open", "2×12 Open · 67", "Factory IR", "Open-back cabinet with room and top-end detail.", "FACTORY"),
        BrowserItem("custom-main", "My Studio IR", "Custom IR", "Imported user impulse response.", "CUSTOM"),
    ),
    BrowserKind.SupportIr to listOf(
        BrowserItem("direct", "DIRECT · No cabinet", "Amp output", "Bypasses cabinet and custom IR processing.", "DIRECT"),
        BrowserItem("cream", "2×12 Cream · Blend", "Factory IR", "Balanced dynamic and ribbon blend.", "FACTORY"),
        BrowserItem("modern", "4×12 Modern · 421", "Factory IR", "Focused close microphone for dense mixes.", "FACTORY"),
        BrowserItem("custom-ir", "Studio Room A", "Custom IR", "Imported user impulse response.", "CUSTOM"),
    ),
    BrowserKind.CabLibrary to listOf(
        BrowserItem("v30", "4×12 V30 · 57 edge", "Main / support", "Factory cabinet IR.", "FACTORY"),
        BrowserItem("cream", "2×12 Cream · Blend", "Main / support", "Factory cabinet IR.", "FACTORY"),
        BrowserItem("custom-ir", "Studio Room A", "Custom IR", "Imported user impulse response.", "CUSTOM"),
    ),
    BrowserKind.Preset to listOf(
        BrowserItem("edge", "Edge of breakup", "Preset 01", "Main amp with subtle compression, delay, and plate reverb.", "FAVORITE"),
        BrowserItem("rhythm", "Dry rhythm", "Preset 02", "Tight gate, dual amp off, short room.", "RIG"),
        BrowserItem("lead", "Wide lead", "Preset 03", "Support amp, analog delay, and hall reverb.", "RIG"),
        BrowserItem("ambient", "Oktaverb sky", "Preset 04", "Octave pre voice, long delay, and Oktaverb.", "RIG"),
    ),
)

enum class RigParameter(val label: String) {
    PitchSemitones("Semitones"),
    PitchMix("Mix"),
    PitchOctDown("Oct Down"),
    PitchOctUp("Oct Up"),
    PitchDry("Dry"),
    PitchLevel("Level"),
    CompAmount("Input"),
    CompRatio("Ratio"),
    CompAttack("Attack"),
    CompRelease("Release"),
    CompMix("Mix"),
    CompLevel("Output"),
    NamOneGain("Gain"),
    NamOneBass("Bass"),
    NamOneMid("Mid"),
    NamOneMidFreq("Mid Freq"),
    NamOneTreble("Treble"),
    NamOneLevel("Level"),
    NamTwoGain("Gain"),
    NamTwoBass("Bass"),
    NamTwoMid("Mid"),
    NamTwoMidFreq("Mid Freq"),
    NamTwoTreble("Treble"),
    NamTwoLevel("Level"),
    MainInput("Input"),
    MainGate("Threshold"),
    MainBass("Bass"),
    MainMid("Mid"),
    MainTreble("Treble"),
    MainOutput("Output"),
    MainPan("Pan"),
    SupportInput("Input"),
    SupportGate("Threshold"),
    SupportBass("Bass"),
    SupportMid("Mid"),
    SupportTreble("Treble"),
    SupportOutput("Output"),
    SupportPan("Pan"),
    DelayTime("Time"),
    DelayFeedback("Feedback"),
    DelayMix("Mix"),
    DelayTone("Tone"),
    DelayAge("Age"),
    ReverbDecay("Decay"),
    ReverbMix("Mix"),
    ReverbTone("Tone"),
    ReverbPreDelay("Pre-delay"),
    ReverbShimmer("Shimmer"),
    TremoloRate("Rate"),
    TremoloDepth("Depth"),
    TremoloShape("Shape"),
    TremoloMix("Mix"),
    TremoloCrossover("Crossover"),
    InputCalibration("Input dBu"),
    MetronomeVolume("Volume"),
}

private val defaultParameters = RigParameter.entries.associateWith {
    when (it) {
        RigParameter.MainInput -> .61f
        RigParameter.MainBass -> .48f
        RigParameter.MainMid -> .56f
        RigParameter.MainTreble -> .63f
        RigParameter.MainOutput -> .58f
        RigParameter.MainGate -> .34f
        RigParameter.SupportOutput -> .45f
        RigParameter.PitchOctDown -> .8f
        RigParameter.PitchDry -> 1f
        RigParameter.CompMix -> 1f
        RigParameter.DelayMix -> .28f
        RigParameter.ReverbMix -> .34f
        RigParameter.TremoloDepth -> .85f
        RigParameter.TremoloMix -> .6f
        RigParameter.InputCalibration -> .6f
        else -> .5f
    }
}

private val defaultBlocks = RigBlock.entries.associateWith {
    it !in setOf(RigBlock.Octave, RigBlock.NamTwo, RigBlock.SupportAmp, RigBlock.SupportIr, RigBlock.Tremolo)
}

data class UndoEdit(val parameter: RigParameter, val value: Float)

data class PrototypeState(
    val route: PrototypeRoute = PrototypeRoute.Setup,
    val setupStep: SetupStep = SetupStep.Permission,
    val microphoneGranted: Boolean = false,
    val usbConnected: Boolean = false,
    val powered: Boolean = false,
    val bypassed: Boolean = false,
    val presetDirty: Boolean = false,
    val modelLoaded: Boolean = false,
    val supportEnabled: Boolean = false,
    val preLocked: Boolean = false,
    val postLocked: Boolean = false,
    val preDirty: Boolean = false,
    val postDirty: Boolean = false,
    val selectedSection: RigSection? = null,
    val selectedBlock: RigBlock? = null,
    val selectedLane: AmpLane = AmpLane.Main,
    val settingsTab: SettingsTab = SettingsTab.Audio,
    val selectedDevice: String = "Audient EVO 4",
    val selectedAmp: String = "Blackbird 30 · Edge",
    val selectedSupportAmp: String = "California Lead",
    val selectedPreset: String = "Edge of breakup",
    val selectedNamOne: String = "Green Drive · Hot",
    val selectedNamTwo: String = "Gold Drive · Edge",
    val selectedMainIr: String = "4×12 V30 · 57 edge",
    val selectedSupportIr: String = "2×12 Cream · Blend",
    val mainChannel: Int = 1,
    val supportChannel: Int = 2,
    val mainGateEnabled: Boolean = true,
    val mainEqEnabled: Boolean = true,
    val mainIrEnabled: Boolean = false,
    val supportGateEnabled: Boolean = true,
    val supportEqEnabled: Boolean = true,
    val supportIrEnabled: Boolean = false,
    val supportPolarityInverted: Boolean = false,
    val pitchCharacter: Int = 0,
    val pitchVoicing: Int = 1,
    val delaySync: Boolean = false,
    val delayDivision: Int = 4,
    val delayPingPong: Boolean = false,
    val reverbSubMode: Int = 1,
    val tremoloSync: Boolean = false,
    val tremoloDivision: Int = 4,
    val sampleRate: Int = 48000,
    val bufferFrames: Int = 64,
    val calibrateInput: Boolean = false,
    val outputMode: Int = 1,
    val liteMode: Boolean = false,
    val metronomeActive: Boolean = false,
    val metronomeBpm: Int = 120,
    val metronomeTimeSig: Int = 3,
    val blocks: Map<RigBlock, Boolean> = defaultBlocks,
    val parameters: Map<RigParameter, Float> = defaultParameters,
    val modes: Map<RigBlock, Int> = mapOf(
        RigBlock.Octave to 0,
        RigBlock.Delay to 1,
        RigBlock.Reverb to 0,
        RigBlock.Tremolo to 0,
    ),
    val browserKind: BrowserKind? = null,
    val browserSelectedId: String? = null,
    val browserApply: BrowserApply = BrowserApply.ReturnWorkspace,
    val browserBack: PrototypeRoute = PrototypeRoute.Workspace,
    val browserManage: Boolean = false,
    val contentEditorKind: BrowserKind? = null,
    val contentEditorExisting: Boolean = false,
    val contentFilesStaged: Boolean = false,
    val loadingLabel: String = "Loading amp model",
    val errorMessage: String? = null,
    val feedbackMessage: String? = null,
    val undoEdit: UndoEdit? = null,
    val latencyMs: Int = 7,
    val xruns: Int = 0,
    val inputPeak: Float = .46f,
    val outputPeak: Float = .72f,
)

sealed interface PrototypeAction {
    data object GrantPermission : PrototypeAction
    data object DenyPermission : PrototypeAction
    data object ConnectUsb : PrototypeAction
    data class OpenBrowser(
        val kind: BrowserKind,
        val apply: BrowserApply = BrowserApply.ReturnWorkspace,
        val back: PrototypeRoute = PrototypeRoute.Workspace,
        val manage: Boolean = false,
    ) : PrototypeAction
    data class SelectBrowserItem(val id: String) : PrototypeAction
    data object ApplyBrowserItem : PrototypeAction
    data object CancelBrowser : PrototypeAction
    data object FinishLoading : PrototypeAction
    data object RetryLoading : PrototypeAction
    data object RecoverToWorkspace : PrototypeAction
    data object OpenSettings : PrototypeAction
    data object CloseSettings : PrototypeAction
    data class SelectSettingsTab(val tab: SettingsTab) : PrototypeAction
    data object OpenMetronome : PrototypeAction
    data object CloseMetronome : PrototypeAction
    data class SelectSection(val section: RigSection) : PrototypeAction
    data class SelectBlock(val block: RigBlock) : PrototypeAction
    data class SelectLane(val lane: AmpLane) : PrototypeAction
    data object ClearSelection : PrototypeAction
    data class ToggleBlock(val block: RigBlock) : PrototypeAction
    data object ToggleSupport : PrototypeAction
    data class ToggleSectionLock(val section: RigSection) : PrototypeAction
    data class StoreSection(val section: RigSection) : PrototypeAction
    data class StepChannel(val lane: AmpLane, val delta: Int) : PrototypeAction
    data class ToggleLaneFeature(val lane: AmpLane, val feature: LaneFeature) : PrototypeAction
    data object ToggleSupportPolarity : PrototypeAction
    data object TogglePower : PrototypeAction
    data object ToggleBypass : PrototypeAction
    data object OpenTuner : PrototypeAction
    data object CloseTuner : PrototypeAction
    data class BeginParameterEdit(val parameter: RigParameter) : PrototypeAction
    data class SetParameter(val parameter: RigParameter, val value: Float) : PrototypeAction
    data object UndoParameterEdit : PrototypeAction
    data class SetMode(val block: RigBlock, val mode: Int) : PrototypeAction
    data class SetPitchCharacter(val value: Int) : PrototypeAction
    data class SetPitchVoicing(val value: Int) : PrototypeAction
    data object ToggleDelaySync : PrototypeAction
    data class StepDelayDivision(val delta: Int) : PrototypeAction
    data object ToggleDelayPingPong : PrototypeAction
    data class SetReverbSubMode(val value: Int) : PrototypeAction
    data object ToggleTremoloSync : PrototypeAction
    data class StepTremoloDivision(val delta: Int) : PrototypeAction
    data class SetSampleRate(val value: Int) : PrototypeAction
    data class SetBufferFrames(val value: Int) : PrototypeAction
    data object ToggleInputCalibration : PrototypeAction
    data class SetOutputMode(val value: Int) : PrototypeAction
    data object ToggleLiteMode : PrototypeAction
    data object ToggleMetronome : PrototypeAction
    data class StepMetronomeBpm(val delta: Int) : PrototypeAction
    data class SetMetronomeTimeSig(val value: Int) : PrototypeAction
    data object SavePresetAsNew : PrototypeAction
    data object OverwritePreset : PrototypeAction
    data object AddLibraryItem : PrototypeAction
    data object EditLibraryItem : PrototypeAction
    data object DeleteLibraryItem : PrototypeAction
    data object CloseContentEditor : PrototypeAction
    data object StageContentFiles : PrototypeAction
    data object RenameContent : PrototypeAction
    data object SaveContentEditor : PrototypeAction
    data object ClearFeedback : PrototypeAction
}

fun reducePrototype(state: PrototypeState, action: PrototypeAction): PrototypeState = when (action) {
    PrototypeAction.GrantPermission -> state.copy(
        microphoneGranted = true,
        setupStep = SetupStep.Usb,
        errorMessage = null,
    )
    PrototypeAction.DenyPermission -> state.copy(
        microphoneGranted = false,
        errorMessage = "Microphone access is required for live guitar input. You can retry safely.",
    )
    PrototypeAction.ConnectUsb -> state.copy(usbConnected = true).openBrowser(
        BrowserKind.Device,
        BrowserApply.SelectInitialDevice,
        PrototypeRoute.Setup,
    )
    is PrototypeAction.OpenBrowser -> state.openBrowser(action.kind, action.apply, action.back, action.manage)
    is PrototypeAction.SelectBrowserItem -> state.copy(browserSelectedId = action.id)
    PrototypeAction.ApplyBrowserItem -> applyBrowserItem(state)
    PrototypeAction.CancelBrowser -> state.copy(
        route = state.browserBack,
        browserKind = null,
        browserSelectedId = null,
        browserManage = false,
    )
    PrototypeAction.FinishLoading -> state.copy(
        route = PrototypeRoute.Workspace,
        modelLoaded = true,
        powered = true,
        errorMessage = null,
    )
    PrototypeAction.RetryLoading -> state.copy(route = PrototypeRoute.Loading, errorMessage = null)
    PrototypeAction.RecoverToWorkspace -> state.copy(route = PrototypeRoute.Workspace, errorMessage = null)
    PrototypeAction.OpenSettings -> state.copy(route = PrototypeRoute.Settings, feedbackMessage = null)
    PrototypeAction.CloseSettings -> state.copy(route = PrototypeRoute.Workspace, feedbackMessage = null)
    is PrototypeAction.SelectSettingsTab -> state.copy(settingsTab = action.tab, feedbackMessage = null)
    PrototypeAction.OpenMetronome -> state.copy(route = PrototypeRoute.Metronome)
    PrototypeAction.CloseMetronome -> state.copy(route = PrototypeRoute.Workspace)
    is PrototypeAction.SelectSection -> state.copy(
        selectedSection = action.section,
        selectedBlock = null,
        undoEdit = null,
    )
    is PrototypeAction.SelectBlock -> state.copy(
        selectedSection = null,
        selectedBlock = action.block,
        undoEdit = null,
    )
    is PrototypeAction.SelectLane -> {
        if (action.lane == AmpLane.Support && !state.supportEnabled) state
        else state.copy(selectedLane = action.lane, selectedSection = RigSection.Amp, selectedBlock = null)
    }
    PrototypeAction.ClearSelection -> state.copy(
        selectedSection = null,
        selectedBlock = null,
        undoEdit = null,
    )
    is PrototypeAction.ToggleBlock -> {
        if (action.block in setOf(RigBlock.SupportAmp, RigBlock.SupportIr) && !state.supportEnabled) state
        else state.copy(blocks = state.blocks + (action.block to !(state.blocks[action.block] ?: false)))
            .markDirty(action.block.section)
    }
    PrototypeAction.ToggleSupport -> {
        val enabled = !state.supportEnabled
        state.copy(
            supportEnabled = enabled,
            blocks = state.blocks +
                (RigBlock.SupportAmp to enabled) +
                (RigBlock.SupportIr to enabled),
            selectedLane = if (enabled) state.selectedLane else AmpLane.Main,
        ).markDirty(RigSection.Amp)
    }
    is PrototypeAction.ToggleSectionLock -> when (action.section) {
        RigSection.Pre -> state.copy(preLocked = !state.preLocked, preDirty = false)
        RigSection.Post -> state.copy(postLocked = !state.postLocked, postDirty = false)
        RigSection.Amp -> state
    }
    is PrototypeAction.StoreSection -> when (action.section) {
        RigSection.Pre -> state.copy(preDirty = false, feedbackMessage = "PRE stored to ${state.selectedAmp}")
        RigSection.Post -> state.copy(postDirty = false, feedbackMessage = "POST stored to ${state.selectedAmp}")
        RigSection.Amp -> state
    }
    is PrototypeAction.StepChannel -> when (action.lane) {
        AmpLane.Main -> state.copy(mainChannel = (state.mainChannel + action.delta).floorMod(4))
        AmpLane.Support -> state.copy(supportChannel = (state.supportChannel + action.delta).floorMod(4))
    }.markDirty(RigSection.Amp)
    is PrototypeAction.ToggleLaneFeature -> toggleLaneFeature(state, action.lane, action.feature).markDirty(RigSection.Amp)
    PrototypeAction.ToggleSupportPolarity ->
        state.copy(supportPolarityInverted = !state.supportPolarityInverted).markDirty(RigSection.Amp)
    PrototypeAction.TogglePower -> state.copy(powered = !state.powered)
    PrototypeAction.ToggleBypass -> state.copy(bypassed = !state.bypassed)
    PrototypeAction.OpenTuner -> state.copy(route = PrototypeRoute.Tuner)
    PrototypeAction.CloseTuner -> state.copy(route = PrototypeRoute.Workspace)
    is PrototypeAction.BeginParameterEdit -> state.copy(
        undoEdit = UndoEdit(action.parameter, state.parameters.getValue(action.parameter)),
    )
    is PrototypeAction.SetParameter -> {
        val updated = state.copy(
            parameters = state.parameters + (action.parameter to action.value.coerceIn(0f, 1f)),
        )
        if (action.parameter in setOf(RigParameter.InputCalibration, RigParameter.MetronomeVolume)) updated
        else updated.markDirty(sectionFor(action.parameter))
    }
    PrototypeAction.UndoParameterEdit -> state.undoEdit?.let {
        state.copy(parameters = state.parameters + (it.parameter to it.value), undoEdit = null)
    } ?: state
    is PrototypeAction.SetMode -> state.copy(modes = state.modes + (action.block to action.mode))
        .markDirty(action.block.section)
    is PrototypeAction.SetPitchCharacter -> state.copy(pitchCharacter = action.value.coerceIn(0, 1)).markDirty(RigSection.Pre)
    is PrototypeAction.SetPitchVoicing -> state.copy(pitchVoicing = action.value.coerceIn(0, 1)).markDirty(RigSection.Pre)
    PrototypeAction.ToggleDelaySync -> state.copy(delaySync = !state.delaySync).markDirty(RigSection.Post)
    is PrototypeAction.StepDelayDivision ->
        state.copy(delayDivision = (state.delayDivision + action.delta).floorMod(8)).markDirty(RigSection.Post)
    PrototypeAction.ToggleDelayPingPong -> state.copy(delayPingPong = !state.delayPingPong).markDirty(RigSection.Post)
    is PrototypeAction.SetReverbSubMode ->
        state.copy(reverbSubMode = action.value.coerceIn(0, 2)).markDirty(RigSection.Post)
    PrototypeAction.ToggleTremoloSync -> state.copy(tremoloSync = !state.tremoloSync).markDirty(RigSection.Post)
    is PrototypeAction.StepTremoloDivision ->
        state.copy(tremoloDivision = (state.tremoloDivision + action.delta).floorMod(8)).markDirty(RigSection.Post)
    is PrototypeAction.SetSampleRate -> state.copy(sampleRate = action.value, feedbackMessage = "Audio will restart at ${action.value / 1000} kHz")
    is PrototypeAction.SetBufferFrames -> state.copy(bufferFrames = action.value, feedbackMessage = "Buffer set to ${action.value} samples")
    PrototypeAction.ToggleInputCalibration -> state.copy(calibrateInput = !state.calibrateInput)
    is PrototypeAction.SetOutputMode -> state.copy(outputMode = action.value.coerceIn(0, 2))
    PrototypeAction.ToggleLiteMode -> state.copy(liteMode = !state.liteMode)
    PrototypeAction.ToggleMetronome -> state.copy(metronomeActive = !state.metronomeActive)
    is PrototypeAction.StepMetronomeBpm ->
        state.copy(metronomeBpm = (state.metronomeBpm + action.delta).coerceIn(30, 240))
    is PrototypeAction.SetMetronomeTimeSig -> state.copy(metronomeTimeSig = action.value.coerceIn(0, 4))
    PrototypeAction.SavePresetAsNew -> state.copy(
        selectedPreset = "New rig ${browserCatalogs.getValue(BrowserKind.Preset).size + 1}",
        feedbackMessage = "Preset saved",
        presetDirty = false,
    )
    PrototypeAction.OverwritePreset -> state.copy(feedbackMessage = "${state.selectedPreset} overwritten", presetDirty = false)
    PrototypeAction.AddLibraryItem -> state.copy(
        route = PrototypeRoute.ContentEditor,
        contentEditorKind = state.browserKind,
        contentEditorExisting = false,
        contentFilesStaged = false,
        feedbackMessage = null,
    )
    PrototypeAction.EditLibraryItem -> state.copy(
        route = PrototypeRoute.ContentEditor,
        contentEditorKind = state.browserKind,
        contentEditorExisting = true,
        contentFilesStaged = true,
        feedbackMessage = null,
    )
    PrototypeAction.DeleteLibraryItem -> state.copy(feedbackMessage = "Delete confirmation opened")
    PrototypeAction.CloseContentEditor -> state.copy(
        route = PrototypeRoute.Browser,
        contentEditorKind = null,
        contentFilesStaged = false,
        feedbackMessage = null,
    )
    PrototypeAction.StageContentFiles -> state.copy(contentFilesStaged = true, feedbackMessage = "Source files staged")
    PrototypeAction.RenameContent -> state.copy(feedbackMessage = "Name field focused")
    PrototypeAction.SaveContentEditor -> state.copy(
        route = PrototypeRoute.Browser,
        contentEditorKind = null,
        contentFilesStaged = false,
        feedbackMessage = if (state.contentEditorExisting) "Library item updated" else "Library item imported",
    )
    PrototypeAction.ClearFeedback -> state.copy(feedbackMessage = null)
}

private fun PrototypeState.openBrowser(
    kind: BrowserKind,
    apply: BrowserApply,
    back: PrototypeRoute,
    manage: Boolean = false,
): PrototypeState {
    val items = browserCatalogs.getValue(kind)
    return copy(
        route = PrototypeRoute.Browser,
        browserKind = kind,
        browserSelectedId = items.first().id,
        browserApply = apply,
        browserBack = back,
        browserManage = manage,
        errorMessage = null,
    )
}

private fun applyBrowserItem(state: PrototypeState): PrototypeState {
    val kind = state.browserKind ?: return state
    val item = browserCatalogs.getValue(kind).firstOrNull { it.id == state.browserSelectedId } ?: return state
    if (item.id == "broken") {
        return state.copy(
            route = PrototypeRoute.Error,
            loadingLabel = "Loading ${item.name}",
            errorMessage = "The amp model could not be verified. Your live rig is unchanged.",
        )
    }
    val selected = when (kind) {
        BrowserKind.Device -> state.copy(selectedDevice = item.name)
        BrowserKind.Amp -> state.copy(selectedAmp = item.name, mainChannel = item.defaultChannel(), presetDirty = false)
        BrowserKind.SupportAmp -> if (item.id == "none") {
            state.copy(
                selectedSupportAmp = item.name,
                blocks = state.blocks + (RigBlock.SupportAmp to false) + (RigBlock.SupportIr to false),
                presetDirty = true,
            )
        } else {
            state.copy(
                selectedSupportAmp = item.name,
                supportChannel = item.defaultChannel(),
                blocks = state.blocks + (RigBlock.SupportAmp to true) + (RigBlock.SupportIr to true),
                presetDirty = true,
            )
        }
        BrowserKind.NamOne -> state.copy(selectedNamOne = item.name, presetDirty = true)
        BrowserKind.NamTwo -> state.copy(selectedNamTwo = item.name, presetDirty = true)
        BrowserKind.PreLibrary -> if (item.subtitle.contains("2")) {
            state.copy(selectedNamTwo = item.name)
        } else {
            state.copy(selectedNamOne = item.name)
        }
        BrowserKind.MainIr ->
            state.copy(selectedMainIr = item.name, mainIrEnabled = item.tag == "CUSTOM", presetDirty = true)
        BrowserKind.SupportIr ->
            state.copy(selectedSupportIr = item.name, supportIrEnabled = item.tag == "CUSTOM", presetDirty = true)
        BrowserKind.CabLibrary -> state.copy(selectedMainIr = item.name, mainIrEnabled = item.tag == "CUSTOM")
        BrowserKind.Preset -> state.copy(selectedPreset = item.name, presetDirty = false)
    }
    return when (state.browserApply) {
        BrowserApply.SelectInitialDevice -> selected.openBrowser(
            BrowserKind.Amp,
            BrowserApply.FinishInitialRig,
            PrototypeRoute.Setup,
        )
        BrowserApply.FinishInitialRig -> selected.copy(
            route = PrototypeRoute.Loading,
            loadingLabel = "Loading ${item.name}",
            browserKind = null,
        )
        BrowserApply.ReturnWorkspace -> {
            if (kind == BrowserKind.Device) {
                selected.copy(route = state.browserBack, browserKind = null, browserManage = false)
            } else {
                selected.copy(
                    route = PrototypeRoute.Loading,
                    loadingLabel = "Applying ${item.name}",
                    browserKind = null,
                )
            }
        }
    }
}

private fun PrototypeState.markDirty(section: RigSection): PrototypeState = when (section) {
    RigSection.Pre -> copy(presetDirty = true, preDirty = preDirty || preLocked)
    RigSection.Post -> copy(presetDirty = true, postDirty = postDirty || postLocked)
    RigSection.Amp -> copy(presetDirty = true)
}

private fun sectionFor(parameter: RigParameter): RigSection = when (parameter) {
    RigParameter.PitchSemitones,
    RigParameter.PitchMix,
    RigParameter.PitchOctDown,
    RigParameter.PitchOctUp,
    RigParameter.PitchDry,
    RigParameter.PitchLevel,
    RigParameter.CompAmount,
    RigParameter.CompRatio,
    RigParameter.CompAttack,
    RigParameter.CompRelease,
    RigParameter.CompMix,
    RigParameter.CompLevel,
    RigParameter.NamOneGain,
    RigParameter.NamOneBass,
    RigParameter.NamOneMid,
    RigParameter.NamOneMidFreq,
    RigParameter.NamOneTreble,
    RigParameter.NamOneLevel,
    RigParameter.NamTwoGain,
    RigParameter.NamTwoBass,
    RigParameter.NamTwoMid,
    RigParameter.NamTwoMidFreq,
    RigParameter.NamTwoTreble,
    RigParameter.NamTwoLevel -> RigSection.Pre

    RigParameter.MainInput,
    RigParameter.MainGate,
    RigParameter.MainBass,
    RigParameter.MainMid,
    RigParameter.MainTreble,
    RigParameter.MainOutput,
    RigParameter.MainPan,
    RigParameter.SupportInput,
    RigParameter.SupportGate,
    RigParameter.SupportBass,
    RigParameter.SupportMid,
    RigParameter.SupportTreble,
    RigParameter.SupportOutput,
    RigParameter.SupportPan,
    RigParameter.InputCalibration -> RigSection.Amp

    RigParameter.DelayTime,
    RigParameter.DelayFeedback,
    RigParameter.DelayMix,
    RigParameter.DelayTone,
    RigParameter.DelayAge,
    RigParameter.ReverbDecay,
    RigParameter.ReverbMix,
    RigParameter.ReverbTone,
    RigParameter.ReverbPreDelay,
    RigParameter.ReverbShimmer,
    RigParameter.TremoloRate,
    RigParameter.TremoloDepth,
    RigParameter.TremoloShape,
    RigParameter.TremoloMix,
    RigParameter.TremoloCrossover,
    RigParameter.MetronomeVolume -> RigSection.Post
}

private fun toggleLaneFeature(state: PrototypeState, lane: AmpLane, feature: LaneFeature): PrototypeState =
    when (lane) {
        AmpLane.Main -> when (feature) {
            LaneFeature.Gate -> state.copy(mainGateEnabled = !state.mainGateEnabled)
            LaneFeature.Eq -> state.copy(mainEqEnabled = !state.mainEqEnabled)
            LaneFeature.Ir -> state.copy(mainIrEnabled = !state.mainIrEnabled)
        }
        AmpLane.Support -> when (feature) {
            LaneFeature.Gate -> state.copy(supportGateEnabled = !state.supportGateEnabled)
            LaneFeature.Eq -> state.copy(supportEqEnabled = !state.supportEqEnabled)
            LaneFeature.Ir -> state.copy(supportIrEnabled = !state.supportIrEnabled)
        }
    }

private fun Int.floorMod(size: Int): Int = ((this % size) + size) % size

private fun BrowserItem.defaultChannel(): Int = when (id) {
    "clean" -> 0
    "blackbird" -> 1
    "california" -> 2
    "rectified" -> 3
    else -> 0
}

class PrototypeController(initial: PrototypeState) {
    var state by mutableStateOf(initial)
        private set

    fun dispatch(action: PrototypeAction) {
        state = reducePrototype(state, action)
    }
}
