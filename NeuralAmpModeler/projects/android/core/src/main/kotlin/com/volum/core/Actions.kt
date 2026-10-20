package com.volum.core

enum class PickKind { NAM_AMP_FILES, IR_FILES, PEDAL_FILES, PACK_OPEN, PACK_SAVE }

data class PickedFile(val name: String, val bytes: Int, val valid: Boolean = true)

sealed interface MidiEvent {
    val channel: Int
    data class ProgramChange(override val channel: Int, val program: Int) : MidiEvent
    data class ControlChange(override val channel: Int, val cc: Int, val value: Int) : MidiEvent
}

sealed interface Action {
    data class SetView(val view: View) : Action
    data class SetSection(val section: Section) : Action
    data class FocusPre(val block: PreBlock) : Action
    data class FocusPost(val block: PostBlock) : Action
    data class FocusLane(val lane: Lane) : Action
    data class Open(val overlay: Overlay) : Action
    data object Back : Action
    data class CloseOverlay(val overlay: Overlay) : Action

    data class SetParam(val param: Param, val value: Double) : Action
    data class ToggleParam(val param: Param) : Action
    data class ResetParam(val param: Param) : Action

    data class SelectAmp(val ampId: String) : Action
    data class SelectChannel(val channel: Int) : Action
    data class SelectCab(val cab: Cab) : Action
    data class SelectIr(val irId: String) : Action
    data class SelectSupportAmp(val ampId: String?) : Action
    data class SelectSupportChannel(val channel: Int) : Action
    data class SelectSupportCab(val cab: Cab) : Action
    data class SetLock(val section: Section, val locked: Boolean) : Action
    data class StoreLock(val section: Section) : Action
    data class SelectCapture(val slot: Int, val index: Int) : Action

    data class StepPreset(val delta: Int) : Action
    data class LoadPreset(val presetId: String) : Action
    data object ResetToDefault : Action
    data class RequestOverwritePreset(val presetId: String) : Action
    data class RequestDeletePreset(val presetId: String) : Action
    data class SetNameText(val text: String) : Action
    data object CommitName : Action
    data object AcceptConfirm : Action
    data object CancelConfirm : Action
    data object DismissBanner : Action

    data class RecallSound(val program: Int) : Action
    data class StepSound(val delta: Int) : Action
    data object AddThisSound : Action
    data class SetPickerProgram(val program: Int) : Action
    data class AssignSound(val program: Int, val presetId: String) : Action
    data class ClearSound(val program: Int) : Action
    data class MoveSound(val program: Int, val delta: Int) : Action
    data class EditStomp(val param: Param) : Action

    data class SetMetronomeOn(val on: Boolean) : Action
    data class SetBpm(val bpm: Double) : Action
    data class SetMetronomeVolume(val volume: Double) : Action
    data class SetTimeSig(val sig: TimeSig) : Action

    data class SetSettingsTab(val tab: SettingsTab) : Action
    data class SetCalibrationDbu(val dbu: Double) : Action
    data class SetCalibrate(val on: Boolean) : Action
    data class SetOutputMode(val mode: Int) : Action
    data class SetLite(val lite: Boolean) : Action
    data class SetAnimateArt(val on: Boolean) : Action

    data class SetMidiChannel(val channel: Int) : Action
    data class SetRecallCc(val cc: Int) : Action
    data class SetMidiBank(val bank: Int) : Action
    data class SelectMidiDevice(val deviceId: String) : Action
    data object ScanBleMidi : Action
    data class Midi(val event: MidiEvent) : Action

    data class SelectInput(val deviceId: String) : Action
    data class SelectInputChannel(val channel: Int) : Action
    data class SelectOutput(val deviceId: String) : Action
    data class SetSampleRate(val rate: Int) : Action
    data class SetBuffer(val frames: Int) : Action
    data object RequestMicPermission : Action
    data object OpenAppSettings : Action
    data object RescanAudio : Action
    data object UseBuiltinInput : Action
    data object ReconnectInterface : Action

    data class SetAutoUpdate(val on: Boolean) : Action
    data object CheckUpdatesNow : Action
    data class OpenLink(val url: String) : Action
    data object ShareLog : Action

    data class OpenBuilder(val editAmpId: String?) : Action
    data class BuilderSetName(val name: String) : Action
    data class BuilderSetFileCab(val index: Int, val cabSlot: Int) : Action
    data class BuilderSetFileChannel(val index: Int, val channel: Int) : Action
    data class BuilderRemoveFile(val index: Int) : Action
    data class BuilderSetCabName(val slot: Int, val name: String) : Action
    data class BuilderSetArt(val art: Int) : Action
    data object BuilderSave : Action
    data class RequestDeleteAmp(val ampId: String) : Action

    data class OpenIrConfig(val irId: String?) : Action
    data class SetIrLevel(val irId: String, val db: Double) : Action
    data class SetIrLowCut(val irId: String, val hz: Double) : Action
    data class SetIrHighCut(val irId: String, val hz: Double) : Action
    data class RequestRenameIr(val irId: String) : Action
    data class RequestDeleteIr(val irId: String) : Action
    data class RequestRenamePreset(val presetId: String) : Action
    data class RequestRenamePedal(val index: Int) : Action
    data class RequestDeletePedal(val index: Int) : Action

    data class SetExportScope(val scope: ExportScope) : Action
    data class ToggleExportSound(val presetId: String) : Action
    data class SetExportAmp(val ampId: String) : Action
    data class ToggleImportRow(val rowId: String) : Action
    data class SetMerge(val mode: MergeMode) : Action
    data class SetRestoreMachine(val on: Boolean) : Action
    data object ConfirmImport : Action

    /** Opens a Storage Access Framework pick; the fake engine answers with [FilesPicked]. */
    data class PickFiles(val kind: PickKind) : Action
    data class FilesPicked(val kind: PickKind, val files: List<PickedFile>) : Action
}
