package com.volum.core

enum class View { PLAY, BUILD }
enum class Section { PRE, AMP, POST }
enum class PreBlock { PITCH, COMP, NAM1, NAM2 }
enum class PostBlock { CHORUS, DELAY, REVERB, TREM }
enum class Lane { MAIN, SUPPORT }
enum class SettingsTab { SIGNAL, MIDI, SYSTEM }

/** Desktop cab row order: No Cab, G12, G65, V30, then Custom IR. */
enum class Cab(val label: String) { NONE("No Cab"), G12("G12"), G65("G65"), V30("V30"), CUSTOM_IR("Custom IR") }

enum class NamePurpose { SAVE_PRESET, ADD_SOUND, RENAME_PRESET, RENAME_IR, RENAME_PEDAL }

sealed interface Overlay {
    data object AmpPicker : Overlay
    data object IrMenu : Overlay
    data class CaptureMenu(val slot: Int) : Overlay
    data object SupportAmpMenu : Overlay
    data object PresetMenu : Overlay
    data object PresetManage : Overlay
    data class NameDialog(val purpose: NamePurpose, val text: String, val targetId: String? = null) : Overlay
    data object CustomAmpBuilder : Overlay
    data object IrManage : Overlay
    data object PedalManage : Overlay
    data object Tuner : Overlay
    data object Metronome : Overlay
    data class Settings(val tab: SettingsTab) : Overlay
    data object AudioDevices : Overlay
    data object AudioStatus : Overlay
    data object PackExport : Overlay
    data object PackImport : Overlay
    data class SoundPicker(val program: Int, val replace: Boolean) : Overlay
}

enum class ConfirmKind { DELETE_PRESET, OVERWRITE_PRESET, DELETE_AMP, DELETE_IR, DELETE_PEDAL }

data class Confirm(val kind: ConfirmKind, val targetId: String, val message: String)

enum class BannerKind { ERROR, INFO }

data class Banner(val kind: BannerKind, val message: String)

data class UiState(
    val view: View = View.PLAY,
    val section: Section = Section.AMP,
    val preFocus: PreBlock = PreBlock.PITCH,
    val postFocus: PostBlock = PostBlock.DELAY,
    val lane: Lane = Lane.MAIN,
    val overlays: List<Overlay> = emptyList(),
    val confirm: Confirm? = null,
    val banner: Banner? = null,
    val buildTipSeen: Boolean = true,
    val irConfigOpen: String? = null,
) {
    val top: Overlay? get() = overlays.lastOrNull()
}

data class SupportLane(
    val ampId: String? = null,
    val channel: Int = 0,
    val cab: Cab = Cab.V30,
    val irId: String? = null,
)

data class Rig(
    val params: Map<Param, Double> = Param.defaults,
    val channel: Int = 0,
    val cab: Cab = Cab.G12,
    val irId: String? = null,
    val support: SupportLane = SupportLane(),
    val presetId: String? = null,
    val dirty: Boolean = false,
    val modeMemory: Map<String, Map<Param, Double>> = emptyMap(),
    val dualPannedOnce: Boolean = false,
) {
    /** What a preset stores and Default resets: knobs, pedals, cabs, Dual Amp. */
    fun sound(): Rig = copy(presetId = null, dirty = false)
}

data class AmpFile(val name: String, val cabSlot: Int, val channel: Int)

data class Amp(
    val id: String,
    val name: String,
    val channels: Int,
    val custom: Boolean = false,
    val art: Int = 0,
    /** Channels (0-based) that have a No Cab capture, so Custom IR is allowed. */
    val noCabChannels: Set<Int> = (0 until channels).toSet(),
    val files: List<AmpFile> = emptyList(),
    val cabNames: List<String> = listOf("DIRECT", "CAB 1", "CAB 2", "CAB 3"),
    val inputLevelDbu: Double? = null,
    val outputLevelDbu: Double? = null,
)

data class Preset(
    val id: String,
    val ampId: String,
    val name: String,
    val factory: Boolean,
    val rig: Rig,
)

data class Ir(
    val id: String,
    val name: String,
    val levelDb: Double = 0.0,
    val lowCutHz: Double = 20.0,
    val highCutHz: Double = 20000.0,
)

data class Pedal(val index: Int, val name: String, val custom: Boolean)

data class Sound(val ampId: String, val presetId: String)

data class Library(
    val amps: List<Amp> = emptyList(),
    val presets: List<Preset> = emptyList(),
    val irs: List<Ir> = emptyList(),
    val pedals: List<Pedal> = emptyList(),
    val sounds: Map<Int, Sound> = emptyMap(),
    val live: Int? = null,
    val packBackups: Int = 0,
) {
    fun amp(id: String?): Amp? = amps.firstOrNull { it.id == id }
    fun preset(id: String?): Preset? = presets.firstOrNull { it.id == id }
    fun presetsFor(ampId: String): List<Preset> = presets.filter { it.ampId == ampId }.sortedWith(compareBy({ !it.factory }, { it.name.lowercase() }))
    fun ir(id: String?): Ir? = irs.firstOrNull { it.id == id }
    fun soundValid(program: Int): Boolean = sounds[program]?.let { preset(it.presetId) != null && amp(it.ampId) != null } ?: false
    fun firstFreeProgram(): Int? = (0..127).firstOrNull { it !in sounds }
}

data class Locks(
    val pre: Map<Param, Double>? = null,
    val post: Map<Param, Double>? = null,
)

data class SignalSettings(
    val calibrationDbu: Double = 12.0,
    val calibrate: Boolean = false,
    val outputMode: Int = 1,
    val lite: Boolean = false,
    val animateArt: Boolean = true,
)

data class MidiSettings(
    /** 0 = all channels, 1..16 = one channel. */
    val channel: Int = 0,
    val lastOneChannel: Int = 1,
    val recallCc: Int = 102,
    val bank: Int = 0,
)

enum class Permission { GRANTED, DENIED, DENIED_PERMANENT }
enum class InterfaceStatus { OK, NO_INTERFACE, UNPLUGGED }

data class AudioDevice(val id: String, val name: String, val usb: Boolean, val channels: Int)
data class MidiDevice(val id: String, val name: String, val ble: Boolean)

data class AudioState(
    val inputs: List<AudioDevice> = emptyList(),
    val outputs: List<AudioDevice> = emptyList(),
    val inputId: String? = null,
    val outputId: String? = null,
    val inputChannel: Int = 0,
    val sampleRate: Int = 48000,
    val sampleRates: List<Int> = listOf(44100, 48000, 96000),
    val bufferFrames: Int = 128,
    val buffers: List<Int> = listOf(64, 128, 256, 512),
    val permission: Permission = Permission.GRANTED,
    val status: InterfaceStatus = InterfaceStatus.OK,
    val midiDevices: List<MidiDevice> = emptyList(),
    val midiDeviceId: String? = null,
    val bleScanning: Boolean = false,
    val midiLost: Boolean = false,
)

enum class TimeSig(val label: String, val beats: Int) { S1_4("1/4", 1), S2_4("2/4", 2), S3_4("3/4", 3), S4_4("4/4", 4), S6_8("6/8", 6) }

data class MetronomeState(
    val on: Boolean = false,
    val bpm: Double = 120.0,
    val volume: Double = 0.7,
    val sig: TimeSig = TimeSig.S4_4,
)

data class UpdateState(
    val auto: Boolean = true,
    val available: String? = null,
    val checks: Int = 0,
)

enum class ExportScope { EVERYTHING, SOUNDS, AMP }
enum class MergeMode { OVERWRITE, ADD, RESET }
enum class PackOutcome(val label: String) { ADD("Add"), REPLACE("Replace"), KEEP_MINE("Keep mine"), RELOADS("Reloads"), SKIP("Skip") }

data class PackExportState(
    val scope: ExportScope = ExportScope.EVERYTHING,
    val ticked: Set<String> = emptySet(),
    val ampId: String? = null,
    val savedAs: String? = null,
) {
    fun alsoIncluding(lib: Library): List<String> = when (scope) {
        ExportScope.EVERYTHING -> emptyList()
        ExportScope.SOUNDS -> ticked.mapNotNull { lib.preset(it) }.flatMap { p ->
            listOfNotNull(lib.ir(p.rig.irId)?.name) +
                listOf(p.rig.params[Param.NAM1_CAPTURE], p.rig.params[Param.NAM2_CAPTURE]).mapNotNull { idx ->
                    lib.pedals.firstOrNull { it.custom && it.index.toDouble() == idx }?.name
                } + listOfNotNull(lib.amp(p.ampId)?.takeIf { it.custom }?.name)
        }.distinct()
        ExportScope.AMP -> lib.presets.filter { it.ampId == ampId }.mapNotNull { lib.ir(it.rig.irId)?.name }.distinct()
    }
}

data class PackRow(val id: String, val name: String, val kind: String, val outcome: PackOutcome, val ticked: Boolean = true)

data class PackImportState(
    val name: String = "",
    val everything: Boolean = true,
    val rows: List<PackRow> = emptyList(),
    val merge: MergeMode = MergeMode.OVERWRITE,
    val restoreMachine: Boolean = false,
    val damaged: Boolean = false,
    val refused: Boolean = false,
    val imported: Boolean = false,
) {
    val allTicked: Boolean get() = rows.all { it.ticked }
}

data class BuilderFile(val name: String, val cabSlot: Int, val channel: Int)

data class BuilderState(
    val editAmpId: String? = null,
    val name: String = "",
    val files: List<BuilderFile> = emptyList(),
    val cabNames: List<String> = listOf("DIRECT", "CAB 1", "CAB 2", "CAB 3"),
    val art: Int = 0,
)

/** Knobs of the deterministic fake host: how the fake engine answers requests in this fixture. */
data class FakeHostConfig(
    val nextPickInvalid: Boolean = false,
    val permissionGrantsOnRequest: Boolean = true,
    val rescanFindsInterface: Boolean = true,
    val bleFinds: Boolean = true,
    val packDamaged: Boolean = false,
    val packConflict: Boolean = false,
    val packOverCapacity: Boolean = false,
    val updateOnCheck: String? = "1.4.0",
    val loadFailOnAmp: String? = null,
)

data class VoLumState(
    val library: Library = Library(),
    val ampId: String = "",
    val rigs: Map<String, Rig> = emptyMap(),
    val locks: Locks = Locks(),
    val ui: UiState = UiState(),
    val signal: SignalSettings = SignalSettings(),
    val midi: MidiSettings = MidiSettings(),
    val audio: AudioState = AudioState(),
    val metronome: MetronomeState = MetronomeState(),
    val update: UpdateState = UpdateState(),
    val packExport: PackExportState = PackExportState(),
    val packImport: PackImportState? = null,
    val builder: BuilderState = BuilderState(),
    val host: FakeHostConfig = FakeHostConfig(),
    val outputSafety: Boolean = false,
    /** External effects the fake host recorded instead of performing (links, system settings, share sheet, files written). */
    val hostLog: List<String> = emptyList(),
) {
    val amp: Amp? get() = library.amp(ampId)
    val rig: Rig get() = rigs[ampId] ?: Rig()
    val dual: Boolean get() = param(Param.DUAL_ON) == 1.0

    fun param(p: Param): Double {
        val lock = when (p.group) {
            ParamGroup.PRE -> locks.pre
            ParamGroup.POST -> locks.post
            else -> null
        }
        return lock?.get(p) ?: rig.params[p] ?: p.default
    }

    fun on(p: Param): Boolean = param(p) >= 0.5

    val presetName: String? get() = library.preset(rig.presetId)?.name
    val tunerOpen: Boolean get() = ui.overlays.any { it is Overlay.Tuner }
    val currentPreset: Preset? get() = library.preset(rig.presetId)

    fun screen(): ScreenId = ScreenRegistry.screenOf(this)
}
