package com.lum.volum.ui.prototype

import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue

enum class PrototypeRoute { Setup, Workspace, Browser, Tuner, Loading, Error }
enum class SetupStep { Permission, Usb }
enum class RigSection { Pre, Amp, Post }

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
    MainIr("Main cabinet IR"),
    SupportIr("Support cabinet IR"),
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
    BrowserKind.MainIr to listOf(
        BrowserItem("v30", "4×12 V30 · 57 edge", "Factory IR", "Close dynamic microphone, edge of dust cap.", "FACTORY"),
        BrowserItem("green", "4×12 Green · 121", "Factory IR", "Ribbon microphone with a warm upper midrange.", "FACTORY"),
        BrowserItem("open", "2×12 Open · 67", "Factory IR", "Open-back cabinet with room and top-end detail.", "FACTORY"),
    ),
    BrowserKind.SupportIr to listOf(
        BrowserItem("cream", "2×12 Cream · Blend", "Factory IR", "Balanced dynamic and ribbon blend.", "FACTORY"),
        BrowserItem("modern", "4×12 Modern · 421", "Factory IR", "Focused close microphone for dense mixes.", "FACTORY"),
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
    OctaveShift("Shift"),
    OctaveMix("Mix"),
    CompThreshold("Threshold"),
    CompRatio("Ratio"),
    CompAttack("Attack"),
    CompRelease("Release"),
    NamOneLevel("Level"),
    NamOneMix("Mix"),
    NamTwoLevel("Level"),
    NamTwoMix("Mix"),
    Drive("Drive"),
    Bass("Bass"),
    Mid("Mid"),
    Treble("Treble"),
    Level("Level"),
    GateThreshold("Gate"),
    GateRelease("Release"),
    MainPan("Pan"),
    SupportPan("Pan"),
    IrLevel("IR Level"),
    DelayTime("Time"),
    DelayFeedback("Feedback"),
    DelayMix("Mix"),
    ReverbSize("Size"),
    ReverbDecay("Decay"),
    ReverbMix("Mix"),
    ReverbTone("Tone"),
    TremoloRate("Rate"),
    TremoloDepth("Depth"),
    TremoloShape("Shape"),
}

private val defaultParameters = RigParameter.entries.associateWith {
    when (it) {
        RigParameter.Drive -> .61f
        RigParameter.Bass -> .48f
        RigParameter.Mid -> .56f
        RigParameter.Treble -> .63f
        RigParameter.Level -> .58f
        RigParameter.GateThreshold -> .34f
        RigParameter.DelayMix -> .28f
        RigParameter.ReverbMix -> .34f
        RigParameter.TremoloDepth -> .42f
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
    val modelLoaded: Boolean = false,
    val supportEnabled: Boolean = false,
    val preLocked: Boolean = false,
    val postLocked: Boolean = false,
    val selectedSection: RigSection? = null,
    val selectedBlock: RigBlock? = null,
    val selectedDevice: String = "Audient EVO 4",
    val selectedAmp: String = "Blackbird 30 · Edge",
    val selectedSupportAmp: String = "California Lead",
    val selectedPreset: String = "Edge of breakup",
    val selectedNamOne: String = "Green Drive · Hot",
    val selectedNamTwo: String = "Gold Drive · Edge",
    val selectedMainIr: String = "4×12 V30 · 57 edge",
    val selectedSupportIr: String = "2×12 Cream · Blend",
    val blocks: Map<RigBlock, Boolean> = defaultBlocks,
    val parameters: Map<RigParameter, Float> = defaultParameters,
    val modes: Map<RigBlock, Int> = mapOf(
        RigBlock.Delay to 1,
        RigBlock.Reverb to 0,
        RigBlock.Tremolo to 0,
    ),
    val browserKind: BrowserKind? = null,
    val browserSelectedId: String? = null,
    val browserApply: BrowserApply = BrowserApply.ReturnWorkspace,
    val browserBack: PrototypeRoute = PrototypeRoute.Workspace,
    val loadingLabel: String = "Loading amp model",
    val errorMessage: String? = null,
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
    ) : PrototypeAction
    data class SelectBrowserItem(val id: String) : PrototypeAction
    data object ApplyBrowserItem : PrototypeAction
    data object CancelBrowser : PrototypeAction
    data object FinishLoading : PrototypeAction
    data object RetryLoading : PrototypeAction
    data object RecoverToWorkspace : PrototypeAction
    data class SelectSection(val section: RigSection) : PrototypeAction
    data class SelectBlock(val block: RigBlock) : PrototypeAction
    data object ClearSelection : PrototypeAction
    data class ToggleBlock(val block: RigBlock) : PrototypeAction
    data object ToggleSupport : PrototypeAction
    data class ToggleSectionLock(val section: RigSection) : PrototypeAction
    data object TogglePower : PrototypeAction
    data object ToggleBypass : PrototypeAction
    data object OpenTuner : PrototypeAction
    data object CloseTuner : PrototypeAction
    data class BeginParameterEdit(val parameter: RigParameter) : PrototypeAction
    data class SetParameter(val parameter: RigParameter, val value: Float) : PrototypeAction
    data object UndoParameterEdit : PrototypeAction
    data class SetMode(val block: RigBlock, val mode: Int) : PrototypeAction
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
    is PrototypeAction.OpenBrowser -> state.openBrowser(action.kind, action.apply, action.back)
    is PrototypeAction.SelectBrowserItem -> state.copy(browserSelectedId = action.id)
    PrototypeAction.ApplyBrowserItem -> applyBrowserItem(state)
    PrototypeAction.CancelBrowser -> state.copy(
        route = state.browserBack,
        browserKind = null,
        browserSelectedId = null,
    )
    PrototypeAction.FinishLoading -> state.copy(
        route = PrototypeRoute.Workspace,
        modelLoaded = true,
        powered = true,
        errorMessage = null,
    )
    PrototypeAction.RetryLoading -> state.copy(route = PrototypeRoute.Loading, errorMessage = null)
    PrototypeAction.RecoverToWorkspace -> state.copy(route = PrototypeRoute.Workspace, errorMessage = null)
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
    PrototypeAction.ClearSelection -> state.copy(
        selectedSection = null,
        selectedBlock = null,
        undoEdit = null,
    )
    is PrototypeAction.ToggleBlock -> {
        if (action.block in setOf(RigBlock.SupportAmp, RigBlock.SupportIr) && !state.supportEnabled) state
        else state.copy(blocks = state.blocks + (action.block to !(state.blocks[action.block] ?: false)))
    }
    PrototypeAction.ToggleSupport -> {
        val enabled = !state.supportEnabled
        state.copy(
            supportEnabled = enabled,
            blocks = state.blocks +
                (RigBlock.SupportAmp to enabled) +
                (RigBlock.SupportIr to enabled),
        )
    }
    is PrototypeAction.ToggleSectionLock -> when (action.section) {
        RigSection.Pre -> state.copy(preLocked = !state.preLocked)
        RigSection.Post -> state.copy(postLocked = !state.postLocked)
        RigSection.Amp -> state
    }
    PrototypeAction.TogglePower -> state.copy(powered = !state.powered)
    PrototypeAction.ToggleBypass -> state.copy(bypassed = !state.bypassed)
    PrototypeAction.OpenTuner -> state.copy(route = PrototypeRoute.Tuner)
    PrototypeAction.CloseTuner -> state.copy(route = PrototypeRoute.Workspace)
    is PrototypeAction.BeginParameterEdit -> state.copy(
        undoEdit = UndoEdit(action.parameter, state.parameters.getValue(action.parameter)),
    )
    is PrototypeAction.SetParameter -> state.copy(
        parameters = state.parameters + (action.parameter to action.value.coerceIn(0f, 1f)),
    )
    PrototypeAction.UndoParameterEdit -> state.undoEdit?.let {
        state.copy(parameters = state.parameters + (it.parameter to it.value), undoEdit = null)
    } ?: state
    is PrototypeAction.SetMode -> state.copy(modes = state.modes + (action.block to action.mode))
}

private fun PrototypeState.openBrowser(
    kind: BrowserKind,
    apply: BrowserApply,
    back: PrototypeRoute,
): PrototypeState {
    val items = browserCatalogs.getValue(kind)
    return copy(
        route = PrototypeRoute.Browser,
        browserKind = kind,
        browserSelectedId = items.first().id,
        browserApply = apply,
        browserBack = back,
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
        BrowserKind.Amp -> state.copy(selectedAmp = item.name)
        BrowserKind.SupportAmp -> state.copy(selectedSupportAmp = item.name)
        BrowserKind.NamOne -> state.copy(selectedNamOne = item.name)
        BrowserKind.NamTwo -> state.copy(selectedNamTwo = item.name)
        BrowserKind.MainIr -> state.copy(selectedMainIr = item.name)
        BrowserKind.SupportIr -> state.copy(selectedSupportIr = item.name)
        BrowserKind.Preset -> state.copy(selectedPreset = item.name)
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
                selected.copy(route = PrototypeRoute.Workspace, browserKind = null)
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

class PrototypeController(initial: PrototypeState) {
    var state by mutableStateOf(initial)
        private set

    fun dispatch(action: PrototypeAction) {
        state = reducePrototype(state, action)
    }
}
