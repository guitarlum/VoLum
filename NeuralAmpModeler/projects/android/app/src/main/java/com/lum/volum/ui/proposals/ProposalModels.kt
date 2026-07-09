package com.lum.volum.ui.proposals

import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue

enum class ProposalId(val slug: String) {
    FractalStage("fractal-stage"),
    BlueFocus("blue-focus"),
    AuricPedalboard("auric-pedalboard"),
    SignalAtlas("signal-atlas"),
    ThumbDeck("thumb-deck"),
    BlackoutLive("blackout-live"),
    FlightRack("flight-rack"),
    OrbitCockpit("orbit-cockpit");

    companion object {
        fun fromSlug(slug: String?): ProposalId? = entries.firstOrNull { it.slug == slug }
    }
}

enum class ProposalOrientation { Portrait, Landscape }
enum class EntryPoint { Walkthrough, Live, Edit }
enum class JourneyStep { Permission, Usb, Device, Model, Loading, Live, Edit, Tuner, Error }
enum class DemoScenario { Ready, NoUsb, Loading, Error }

data class ProposalSpec(
    val id: ProposalId,
    val name: String,
    val number: String,
    val orientation: ProposalOrientation,
    val currentVibe: Boolean,
    val thesis: String,
    val guitarist: String,
    val navigation: String,
    val interaction: String,
    val strengths: String,
    val risks: String,
)

val proposalSpecs = listOf(
    ProposalSpec(
        ProposalId.FractalStage, "Fractal Stage", "01", ProposalOrientation.Portrait, true,
        "A performance home with atmosphere above and protected commands below.",
        "Players who want immediate status with occasional deliberate edits.",
        "Bottom command dock; modal setup and editing layers.",
        "Tap to select, then horizontal scrub. Live parameters stay locked.",
        "Strong VoLum identity; excellent glance hierarchy.",
        "Hero art consumes space on very small phones.",
    ),
    ProposalSpec(
        ProposalId.BlueFocus, "Blue Focus", "02", ProposalOrientation.Portrait, true,
        "One musical decision at a time, shown at instrument scale.",
        "Tone-shapers who dislike dense control surfaces.",
        "A linear focus stack with previous/next section stepping.",
        "Large focused dial plus precise minus/plus steps.",
        "Low cognitive load; safest detailed editing.",
        "Slower when changing several effects in sequence.",
    ),
    ProposalSpec(
        ProposalId.AuricPedalboard, "Auric Pedalboard", "03", ProposalOrientation.Landscape, true,
        "A familiar floorboard translated into a compact stage surface.",
        "Pedal users who think in stomps, order, and illuminated hardware.",
        "Pedals are home; tapping one raises its editor tray.",
        "Large stomp toggles; editor uses horizontal sliders.",
        "Immediate effect state recognition; playful and tactile.",
        "Physical metaphor can limit future routing complexity.",
    ),
    ProposalSpec(
        ProposalId.SignalAtlas, "Signal Atlas", "04", ProposalOrientation.Landscape, true,
        "The entire signal path is a readable map, never a hidden menu.",
        "Rig builders who reason about order and system state.",
        "Signal-chain nodes with a persistent inspector.",
        "Select a node, then use protected linear controls.",
        "Best scalability and clearest signal-flow model.",
        "More technical than performance-first alternatives.",
    ),
    ProposalSpec(
        ProposalId.ThumbDeck, "Thumb Deck", "05", ProposalOrientation.Portrait, false,
        "A card deck turns the rig into a short sequence of purposeful surfaces.",
        "Mobile-native users who prefer swiping through tasks.",
        "Horizontal deck with anchored Live and Setup cards.",
        "Swipe to section; tap a value before adjustment.",
        "Discoverable, calm, and easy to learn.",
        "Card order can hide distant functions.",
    ),
    ProposalSpec(
        ProposalId.BlackoutLive, "Blackout Live", "06", ProposalOrientation.Portrait, false,
        "During performance, almost nothing is editable and nothing competes.",
        "Players who set a rig once and need absolute confidence live.",
        "Immutable live face; hold Edit to enter a separate workspace.",
        "Hold-to-edit, steppers, and explicit Done.",
        "Best accidental-edit safety and dark-stage legibility.",
        "Setup is intentionally less immediate.",
    ),
    ProposalSpec(
        ProposalId.FlightRack, "Flight Rack", "07", ProposalOrientation.Landscape, false,
        "A rugged rack concentrates system health and module bays.",
        "Technical players managing larger rigs and reliability.",
        "Persistent left rail plus stacked rack bays.",
        "Section-first navigation and broad faders.",
        "Dense but ordered; strong status and expansion model.",
        "Industrial density may feel less musical.",
    ),
    ProposalSpec(
        ProposalId.OrbitCockpit, "Orbit Cockpit", "08", ProposalOrientation.Landscape, false,
        "Performance status forms a central instrument surrounded by quick sectors.",
        "Experimental performers who value spatial muscle memory.",
        "Central live core with surrounding functional sectors.",
        "Tap a sector; edit in a dedicated right-side console.",
        "Fast, memorable, and visually distinctive.",
        "Novel spatial model has the steepest learning curve.",
    ),
)

data class DemoSnapshot(
    val step: JourneyStep = JourneyStep.Permission,
    val microphoneGranted: Boolean = false,
    val usbConnected: Boolean = false,
    val selectedDevice: String = "Audient EVO 4 · USB",
    val selectedModel: String = "Blackbird 30 · Edge",
    val modelLoaded: Boolean = false,
    val powered: Boolean = false,
    val bypassed: Boolean = false,
    val tunerEnabled: Boolean = false,
    val busy: Boolean = false,
    val errorMessage: String? = null,
    val latencyMs: Int = 7,
    val xruns: Int = 0,
    val peak: Float = 0.72f,
    val drive: Float = 0.61f,
    val bass: Float = 0.48f,
    val mid: Float = 0.56f,
    val treble: Float = 0.63f,
    val level: Float = 0.58f,
    val gateEnabled: Boolean = true,
    val gateThreshold: Float = 0.34f,
    val delayEnabled: Boolean = true,
    val delayMix: Float = 0.28f,
    val delayMode: Int = 1,
    val reverbEnabled: Boolean = true,
    val reverbMix: Float = 0.34f,
    val reverbMode: Int = 0,
    val tremoloEnabled: Boolean = false,
    val tremoloDepth: Float = 0.42f,
    val tremoloMode: Int = 2,
)

sealed interface DemoAction {
    data object GrantPermission : DemoAction
    data object DenyPermission : DemoAction
    data object ConnectUsb : DemoAction
    data object SelectDevice : DemoAction
    data object SelectModel : DemoAction
    data object FinishLoading : DemoAction
    data object Retry : DemoAction
    data object OpenLive : DemoAction
    data object OpenEdit : DemoAction
    data object OpenTuner : DemoAction
    data object BackToLive : DemoAction
    data object TogglePower : DemoAction
    data object ToggleBypass : DemoAction
    data object ToggleGate : DemoAction
    data object ToggleDelay : DemoAction
    data object ToggleReverb : DemoAction
    data object ToggleTremolo : DemoAction
    data class SetValue(val parameter: Parameter, val value: Float) : DemoAction
    data class SetMode(val effect: Effect, val mode: Int) : DemoAction
    data class ApplyScenario(val scenario: DemoScenario) : DemoAction
    data class Enter(val entryPoint: EntryPoint, val permissionGranted: Boolean) : DemoAction
}

enum class Parameter { Drive, Bass, Mid, Treble, Level, Gate, Delay, Reverb, Tremolo }
enum class Effect { Delay, Reverb, Tremolo }

fun reduceDemo(state: DemoSnapshot, action: DemoAction): DemoSnapshot = when (action) {
    DemoAction.GrantPermission -> state.copy(microphoneGranted = true, step = JourneyStep.Usb)
    DemoAction.DenyPermission -> state.copy(
        microphoneGranted = false,
        step = JourneyStep.Permission,
        errorMessage = "Microphone access is still off. Live audio cannot start, but no other data is affected.",
    )
    DemoAction.ConnectUsb -> state.copy(usbConnected = true, step = JourneyStep.Device)
    DemoAction.SelectDevice -> state.copy(step = JourneyStep.Model)
    DemoAction.SelectModel -> state.copy(step = JourneyStep.Loading, busy = true, errorMessage = null)
    DemoAction.FinishLoading -> state.copy(step = JourneyStep.Live, busy = false, modelLoaded = true, powered = true)
    DemoAction.Retry -> state.copy(step = JourneyStep.Loading, busy = true, errorMessage = null)
    DemoAction.OpenLive, DemoAction.BackToLive -> state.copy(step = JourneyStep.Live, busy = false, errorMessage = null)
    DemoAction.OpenEdit -> state.copy(step = JourneyStep.Edit)
    DemoAction.OpenTuner -> state.copy(step = JourneyStep.Tuner, tunerEnabled = true)
    DemoAction.TogglePower -> state.copy(powered = !state.powered)
    DemoAction.ToggleBypass -> state.copy(bypassed = !state.bypassed)
    DemoAction.ToggleGate -> state.copy(gateEnabled = !state.gateEnabled)
    DemoAction.ToggleDelay -> state.copy(delayEnabled = !state.delayEnabled)
    DemoAction.ToggleReverb -> state.copy(reverbEnabled = !state.reverbEnabled)
    DemoAction.ToggleTremolo -> state.copy(tremoloEnabled = !state.tremoloEnabled)
    is DemoAction.SetValue -> when (action.parameter) {
        Parameter.Drive -> state.copy(drive = action.value)
        Parameter.Bass -> state.copy(bass = action.value)
        Parameter.Mid -> state.copy(mid = action.value)
        Parameter.Treble -> state.copy(treble = action.value)
        Parameter.Level -> state.copy(level = action.value)
        Parameter.Gate -> state.copy(gateThreshold = action.value)
        Parameter.Delay -> state.copy(delayMix = action.value)
        Parameter.Reverb -> state.copy(reverbMix = action.value)
        Parameter.Tremolo -> state.copy(tremoloDepth = action.value)
    }
    is DemoAction.SetMode -> when (action.effect) {
        Effect.Delay -> state.copy(delayMode = action.mode)
        Effect.Reverb -> state.copy(reverbMode = action.mode)
        Effect.Tremolo -> state.copy(tremoloMode = action.mode)
    }
    is DemoAction.ApplyScenario -> when (action.scenario) {
        DemoScenario.Ready -> DemoSnapshot(
            step = JourneyStep.Live,
            microphoneGranted = true,
            usbConnected = true,
            modelLoaded = true,
            powered = true,
        )
        DemoScenario.NoUsb -> DemoSnapshot(
            step = JourneyStep.Usb,
            microphoneGranted = true,
            usbConnected = false,
        )
        DemoScenario.Loading -> DemoSnapshot(
            step = JourneyStep.Loading,
            microphoneGranted = true,
            usbConnected = true,
            busy = true,
        )
        DemoScenario.Error -> DemoSnapshot(
            step = JourneyStep.Error,
            microphoneGranted = true,
            usbConnected = true,
            errorMessage = "Amp model could not be verified. Your live rig is unchanged.",
        )
    }
    is DemoAction.Enter -> when (action.entryPoint) {
        EntryPoint.Walkthrough -> DemoSnapshot(
            step = JourneyStep.Permission,
            microphoneGranted = action.permissionGranted,
        )
        EntryPoint.Live -> DemoSnapshot(
            step = JourneyStep.Live,
            microphoneGranted = action.permissionGranted,
            usbConnected = true,
            modelLoaded = true,
            powered = true,
        )
        EntryPoint.Edit -> DemoSnapshot(
            step = JourneyStep.Edit,
            microphoneGranted = action.permissionGranted,
            usbConnected = true,
            modelLoaded = true,
            powered = true,
        )
    }
}

class DemoController(initial: DemoSnapshot = DemoSnapshot()) {
    var state by mutableStateOf(initial)
        private set

    fun dispatch(action: DemoAction) {
        state = reduceDemo(state, action)
    }
}
