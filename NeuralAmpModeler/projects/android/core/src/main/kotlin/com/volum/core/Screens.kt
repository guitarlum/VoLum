package com.volum.core

enum class Scope { CANDIDATE, DESIGN }

/** Every surface a candidate renders. Each candidate marks its surfaces with `Modifier.screen(...)` from `:harness`. */
enum class ScreenId(val id: String) {
    PLAY("play"),
    PLAY_PICKER("play.picker"),
    BUILD_AMP("build.amp"),
    BUILD_PRE("build.pre"),
    BUILD_POST("build.post"),
    AMP_PICKER("amp.picker"),
    CAB_IR_MENU("cab.ir_menu"),
    PRE_CAPTURE_MENU("pre.capture_menu"),
    SUPPORT_AMP_MENU("support.amp_menu"),
    PRESETS_MENU("presets.menu"),
    PRESETS_MANAGE("presets.manage"),
    DIALOG_NAME("dialog.name"),
    CUSTOM_AMP_BUILDER("custom.amp_builder"),
    CUSTOM_IR_MANAGE("custom.ir_manage"),
    CUSTOM_PEDAL_MANAGE("custom.pedal_manage"),
    TUNER("tuner"),
    METRONOME("metronome"),
    SETTINGS_SIGNAL("settings.signal"),
    SETTINGS_MIDI("settings.midi"),
    SETTINGS_SYSTEM("settings.system"),
    SETTINGS_AUDIO("settings.audio"),
    AUDIO_STATUS("audio.status"),
    PACK_EXPORT("pack.export"),
    PACK_IMPORT("pack.import"),
    DESIGN_GALLERY("design-gallery"),
    ;

    companion object {
        private val byId = entries.associateBy { it.id }
        fun of(id: String): ScreenId = byId[id] ?: error("unknown screen id '$id'")
        fun ofOrNull(id: String): ScreenId? = byId[id]
    }
}

/**
 * [fixture] is the state the screen is reached from (reachability starts at
 * home in this fixture) and its first capture; [captures] are every fixture the
 * box captures it in.
 */
data class ScreenSpec(
    val screen: ScreenId,
    val fixture: String,
    val captures: List<String>,
    val scope: Scope = Scope.CANDIDATE,
)

object ScreenRegistry {
    val HOME = ScreenId.PLAY

    val specs: List<ScreenSpec> = listOf(
        ScreenSpec(ScreenId.PLAY, "default", listOf("default", "first_launch", "invalid_slot", "update_available", "mic_denied")),
        ScreenSpec(ScreenId.PLAY_PICKER, "default", listOf("default")),
        ScreenSpec(ScreenId.BUILD_AMP, "default", listOf("default", "dual_amp", "dual_amp_support", "dual_support_ir", "output_safety", "load_failed", "custom_amp_no_direct")),
        ScreenSpec(ScreenId.BUILD_PRE, "default", listOf("pre_pitch", "pre_pitch_transpose", "pre_comp", "pre_nam1", "pre_locked_differs")),
        ScreenSpec(ScreenId.BUILD_POST, "default", listOf("post_chorus", "post_delay", "post_delay_sync", "post_reverb_okt", "post_trem_harmonic")),
        ScreenSpec(ScreenId.AMP_PICKER, "default", listOf("default")),
        ScreenSpec(ScreenId.CAB_IR_MENU, "default", listOf("default")),
        ScreenSpec(ScreenId.PRE_CAPTURE_MENU, "default", listOf("default")),
        ScreenSpec(ScreenId.SUPPORT_AMP_MENU, "dual_amp_empty_support", listOf("dual_amp_empty_support")),
        ScreenSpec(ScreenId.PRESETS_MENU, "default", listOf("default", "preset_dirty")),
        ScreenSpec(ScreenId.PRESETS_MANAGE, "default", listOf("default", "confirm_delete_preset")),
        ScreenSpec(ScreenId.DIALOG_NAME, "default", listOf("default", "on_factory")),
        ScreenSpec(ScreenId.CUSTOM_AMP_BUILDER, "default", listOf("default", "builder_files")),
        ScreenSpec(ScreenId.CUSTOM_IR_MANAGE, "default", listOf("ir_config", "import_failed")),
        ScreenSpec(ScreenId.CUSTOM_PEDAL_MANAGE, "default", listOf("default")),
        ScreenSpec(ScreenId.TUNER, "default", listOf("default")),
        ScreenSpec(ScreenId.METRONOME, "default", listOf("metronome_on")),
        ScreenSpec(ScreenId.SETTINGS_SIGNAL, "default", listOf("default")),
        ScreenSpec(ScreenId.SETTINGS_MIDI, "default", listOf("default", "midi_one_channel")),
        ScreenSpec(ScreenId.SETTINGS_SYSTEM, "default", listOf("default", "update_available")),
        ScreenSpec(ScreenId.SETTINGS_AUDIO, "default", listOf("default", "midi_device_lost")),
        ScreenSpec(ScreenId.AUDIO_STATUS, "default", listOf("mic_denied", "mic_denied_permanent", "no_interface", "interface_unplugged")),
        ScreenSpec(ScreenId.PACK_EXPORT, "default", listOf("export_sounds", "export_sounds_support")),
        ScreenSpec(ScreenId.PACK_IMPORT, "default", listOf("pack_preview", "pack_conflict", "pack_damaged", "pack_refused")),
        ScreenSpec(ScreenId.DESIGN_GALLERY, "default", listOf("default", "pre_nam1", "gallery_tokens", "gallery_controls", "gallery_dialogs", "gallery_arts", "gallery_motifs", "gallery_header", "gallery_blocks"), Scope.DESIGN),
    )

    private val byScreen = specs.associateBy { it.screen }
    fun spec(screen: ScreenId): ScreenSpec = byScreen.getValue(screen)
    fun inScope(scope: Scope): List<ScreenSpec> = specs.filter { it.scope == scope }

    fun screenOf(s: VoLumState): ScreenId = when (val o = s.ui.top) {
        is Overlay.AmpPicker -> ScreenId.AMP_PICKER
        is Overlay.IrMenu -> ScreenId.CAB_IR_MENU
        is Overlay.CaptureMenu -> ScreenId.PRE_CAPTURE_MENU
        is Overlay.SupportAmpMenu -> ScreenId.SUPPORT_AMP_MENU
        is Overlay.PresetMenu -> ScreenId.PRESETS_MENU
        is Overlay.PresetManage -> ScreenId.PRESETS_MANAGE
        is Overlay.NameDialog -> ScreenId.DIALOG_NAME
        is Overlay.CustomAmpBuilder -> ScreenId.CUSTOM_AMP_BUILDER
        is Overlay.IrManage -> ScreenId.CUSTOM_IR_MANAGE
        is Overlay.PedalManage -> ScreenId.CUSTOM_PEDAL_MANAGE
        is Overlay.Tuner -> ScreenId.TUNER
        is Overlay.Metronome -> ScreenId.METRONOME
        is Overlay.Settings -> when (o.tab) {
            SettingsTab.SIGNAL -> ScreenId.SETTINGS_SIGNAL
            SettingsTab.MIDI -> ScreenId.SETTINGS_MIDI
            SettingsTab.SYSTEM -> ScreenId.SETTINGS_SYSTEM
        }
        is Overlay.AudioDevices -> ScreenId.SETTINGS_AUDIO
        is Overlay.AudioStatus -> ScreenId.AUDIO_STATUS
        is Overlay.PackExport -> ScreenId.PACK_EXPORT
        is Overlay.PackImport -> ScreenId.PACK_IMPORT
        is Overlay.SoundPicker -> ScreenId.PLAY_PICKER
        null -> when (s.ui.view) {
            View.PLAY -> ScreenId.PLAY
            View.BUILD -> when (s.ui.section) {
                Section.PRE -> ScreenId.BUILD_PRE
                Section.AMP -> ScreenId.BUILD_AMP
                Section.POST -> ScreenId.BUILD_POST
            }
        }
    }

    /** Puts a fixture state on [screen] the way a user would have navigated there. */
    fun place(s: VoLumState, screen: ScreenId): VoLumState {
        if (screen == ScreenId.DESIGN_GALLERY || screenOf(s) == screen) return s
        fun at(view: View, section: Section = s.ui.section, vararg overlays: Overlay) =
            s.copy(ui = s.ui.copy(view = view, section = section, overlays = overlays.toList(), buildTipSeen = s.ui.buildTipSeen || view == View.BUILD))
        val play = s.ui.view == View.PLAY
        val view = s.ui.view
        return when (screen) {
            ScreenId.PLAY -> at(View.PLAY)
            ScreenId.PLAY_PICKER -> at(View.PLAY, s.ui.section, Overlay.SoundPicker(s.library.firstFreeProgram() ?: 0, replace = false))
            ScreenId.BUILD_AMP -> at(View.BUILD, Section.AMP)
            ScreenId.BUILD_PRE -> at(View.BUILD, Section.PRE)
            ScreenId.BUILD_POST -> at(View.BUILD, Section.POST)
            ScreenId.AMP_PICKER -> at(View.BUILD, Section.AMP, Overlay.AmpPicker)
            ScreenId.CAB_IR_MENU -> at(View.BUILD, Section.AMP, Overlay.IrMenu)
            ScreenId.PRE_CAPTURE_MENU -> at(View.BUILD, Section.PRE, Overlay.CaptureMenu(1)).let { it.copy(ui = it.ui.copy(preFocus = PreBlock.NAM1)) }
            ScreenId.SUPPORT_AMP_MENU -> at(View.BUILD, Section.AMP, Overlay.SupportAmpMenu)
            ScreenId.PRESETS_MENU -> at(View.BUILD, Section.AMP, Overlay.PresetMenu)
            ScreenId.PRESETS_MANAGE -> at(View.BUILD, Section.AMP, Overlay.PresetManage)
            ScreenId.DIALOG_NAME -> at(if (play) View.PLAY else view, s.ui.section, Overlay.NameDialog(NamePurpose.SAVE_PRESET, Reducer.nameSuggestion(s)))
            ScreenId.CUSTOM_AMP_BUILDER -> at(View.BUILD, Section.AMP, Overlay.AmpPicker, Overlay.CustomAmpBuilder)
            ScreenId.CUSTOM_IR_MANAGE -> at(View.BUILD, Section.AMP, Overlay.IrMenu, Overlay.IrManage)
            ScreenId.CUSTOM_PEDAL_MANAGE -> at(View.BUILD, Section.PRE, Overlay.CaptureMenu(1), Overlay.PedalManage)
            ScreenId.TUNER -> at(view, s.ui.section, Overlay.Tuner)
            ScreenId.METRONOME -> at(view, s.ui.section, Overlay.Metronome)
            ScreenId.SETTINGS_SIGNAL -> at(view, s.ui.section, Overlay.Settings(SettingsTab.SIGNAL))
            ScreenId.SETTINGS_MIDI -> at(view, s.ui.section, Overlay.Settings(SettingsTab.MIDI))
            ScreenId.SETTINGS_SYSTEM -> at(view, s.ui.section, Overlay.Settings(SettingsTab.SYSTEM))
            ScreenId.SETTINGS_AUDIO -> at(view, s.ui.section, Overlay.Settings(SettingsTab.SIGNAL), Overlay.AudioDevices)
            ScreenId.AUDIO_STATUS -> at(view, s.ui.section, Overlay.AudioStatus)
            ScreenId.PACK_EXPORT -> at(view, s.ui.section, Overlay.Settings(SettingsTab.SYSTEM), Overlay.PackExport)
            ScreenId.PACK_IMPORT -> at(view, s.ui.section, Overlay.Settings(SettingsTab.SYSTEM), Overlay.PackImport).let {
                if (it.packImport != null) it
                else it.copy(packImport = FakeEngine.packPreview(it, FakeEngine.answerPick(it, PickKind.PACK_OPEN).first()))
            }
            ScreenId.DESIGN_GALLERY -> s
        }
    }
}
