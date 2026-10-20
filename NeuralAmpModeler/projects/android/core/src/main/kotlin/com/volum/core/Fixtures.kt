package com.volum.core

data class FixtureSpec(
    val id: String,
    val description: String,
    /** Fixed art energy for captures; null lets the fake engine's clock run. */
    val energy: Double? = 0.5,
    val build: () -> VoLumState,
)

/**
 * Deterministic starting states. `default` mirrors the desktop screenshot seed
 * (`docs/screenshot-recipes.md`): THC Sunset on its User preset Sunset Crunch
 * with NAM 1 Klon + NAM 2 TS and Hall reverb, the five starting Sounds plus
 * Sunset Crunch on program 5 (LIVE), and the custom library (Monomyth Skeleton
 * Key, 5000$ Klon, Marshall 4x12 _ Royer).
 */
object Fixtures {
    private const val THC = "amp:14"
    private const val SOLDANO = "amp:13"
    private const val CUSTOM_AMP = "c:1"

    fun seed(): VoLumState {
        val customAmp = Amp(
            id = CUSTOM_AMP, name = "Monomyth Skeleton Key", channels = 2, custom = true, art = 3,
            noCabChannels = setOf(0),
            files = listOf(AmpFile("DIRECT-Monomyth-1.nam", 0, 1), AmpFile("V30-Monomyth-1.nam", 3, 1), AmpFile("G12-Monomyth-2.nam", 1, 2)),
            inputLevelDbu = 9.5, outputLevelDbu = 14.0,
        )
        val sunsetParams = Param.defaults + mapOf(
            Param.NAM1_ON to 1.0, Param.NAM1_CAPTURE to 6.0,
            Param.NAM2_ON to 1.0, Param.NAM2_CAPTURE to 7.0,
            Param.REVERB_ON to 1.0, Param.INPUT to 1.5, Param.BASS to 6.0, Param.TREBLE to 6.5,
        )
        val sunsetRig = Rig(params = sunsetParams, channel = 2, cab = Cab.V30)
        val userPresets = listOf(
            Preset("u:1", THC, "Sunset Crunch", factory = false, rig = sunsetRig),
            Preset("u:2", SOLDANO, "SLO Rhythm", factory = false, rig = Rig(params = Param.defaults + mapOf(Param.COMP_ON to 1.0, Param.NAM1_ON to 1.0, Param.NAM1_CAPTURE to 6.0), channel = 1, cab = Cab.V30)),
            Preset("u:3", CUSTOM_AMP, "Skeleton Drive", factory = false, rig = Rig(channel = 0, cab = Cab.V30)),
        )
        val library = Library(
            amps = Catalog.amps + customAmp,
            presets = Catalog.factoryPresets + userPresets,
            irs = listOf(Ir("ir:royer", "Marshall 4x12 _ Royer")),
            pedals = Catalog.pedals + Pedal(64, "5000\$ Klon", custom = true),
            sounds = Catalog.startingSounds + (5 to Sound(THC, "u:1")),
            live = 5,
        )
        return VoLumState(
            library = library,
            ampId = THC,
            rigs = mapOf(THC to sunsetRig.copy(presetId = "u:1")),
            ui = UiState(view = View.PLAY, buildTipSeen = true),
            audio = AudioState(
                inputs = listOf(FakeEngine.usbInterface, FakeEngine.builtinMic),
                outputs = listOf(FakeEngine.usbInterface, FakeEngine.builtinSpeaker),
                inputId = FakeEngine.usbInterface.id,
                outputId = FakeEngine.usbInterface.id,
                midiDevices = listOf(FakeEngine.usbController),
                midiDeviceId = FakeEngine.usbController.id,
            ),
        )
    }

    private fun VoLumState.rig(f: (Rig) -> Rig) = copy(rigs = rigs + (ampId to f(rig)))
    private fun VoLumState.params(vararg kv: Pair<Param, Double>) = rig { it.copy(params = it.params + kv) }
    private fun VoLumState.ui(f: (UiState) -> UiState) = copy(ui = f(ui))
    private fun VoLumState.pre(block: PreBlock) = ui { it.copy(view = View.BUILD, section = Section.PRE, preFocus = block) }
    private fun VoLumState.post(block: PostBlock) = ui { it.copy(view = View.BUILD, section = Section.POST, postFocus = block) }
    private fun VoLumState.dual(supportFocus: Boolean, support: String? = SOLDANO) =
        params(Param.DUAL_ON to 1.0, Param.MAIN_PAN to -0.5, Param.SUPPORT_PAN to 0.5)
            .rig { it.copy(support = SupportLane(ampId = support, channel = 1, cab = Cab.G12), dualPannedOnce = true) }
            .ui { it.copy(view = View.BUILD, section = Section.AMP, lane = if (supportFocus) Lane.SUPPORT else Lane.MAIN) }
    private fun VoLumState.host(f: (FakeHostConfig) -> FakeHostConfig) = copy(host = f(host))

    private fun f(id: String, description: String, energy: Double? = 0.5, build: () -> VoLumState) = FixtureSpec(id, description, energy, build)

    val all: List<FixtureSpec> = listOf(
        f("default", "desktop screenshot seed: THC Sunset, Sunset Crunch, six Sounds, custom library") { seed() },
        f("first_launch", "five starting Sounds, nothing LIVE, Tweak your sound note pending", energy = 0.0) {
            seed().let { it.copy(library = it.library.copy(sounds = Catalog.startingSounds, live = null)) }.ui { it.copy(buildTipSeen = false) }
        },
        f("unnamed_rig", "rig changed from no preset: + Add this sound needs a name") { seed().rig { it.copy(presetId = null, dirty = true) } },
        f("play_full", "all 128 programs hold a Sound") {
            seed().let { s -> s.copy(library = s.library.copy(sounds = (0..127).associateWith { s.library.sounds[it] ?: Sound(THC, "u:1") })) }
        },
        f("invalid_slot", "program 6 points at a deleted preset") {
            seed().let { it.copy(library = it.library.copy(sounds = it.library.sounds + (6 to Sound(THC, "u:deleted")))) }
        },
        f("update_available", "update check found 1.4.0") { seed().copy(update = UpdateState(auto = true, available = "1.4.0", checks = 1)) },
        f("knobs_moved", "AMP BASS away from its default") { seed().params(Param.BASS to 8.5) },
        f("preset_dirty", "Sunset Crunch changed since loading (unsaved)") { seed().params(Param.BASS to 7.0).rig { it.copy(dirty = true) } },
        f("on_factory", "THC Sunset on its Factory preset Sunset Clean") {
            seed().let { s -> s.rig { Catalog.factoryPresets.first { it.id == Catalog.factoryId(14, 1) }.rig.copy(presetId = Catalog.factoryId(14, 1)) } }
        },
        f("cab_g12", "cab on G12") { seed().rig { it.copy(cab = Cab.G12) } },
        f("output_safety", "output safety limiter engaged", energy = 1.0) { seed().copy(outputSafety = true) },
        f("load_failed", "a model failed to load") {
            seed().ui { it.copy(view = View.BUILD, banner = Banner(BannerKind.ERROR, "Could not load Diezel Herbert Mk1. The previous amp keeps playing.")) }
        },
        f("custom_amp_no_direct", "custom amp on a channel without a No Cab capture: Custom IR greyed out") {
            seed().let { s ->
                s.copy(ampId = CUSTOM_AMP, rigs = s.rigs + (CUSTOM_AMP to Rig(channel = 1, cab = Cab.G12, irId = "ir:royer", presetId = "u:3")))
            }
        },
        f("custom_levels", "custom amp whose capture stores input and output level") {
            seed().let { s -> s.copy(ampId = CUSTOM_AMP, rigs = s.rigs + (CUSTOM_AMP to Rig(channel = 0, cab = Cab.NONE, presetId = "u:3"))) }
        },
        f("dual_amp", "Dual Amp on, SUPPORT Soldano SLO100, editing MAIN") { seed().dual(supportFocus = false) },
        f("dual_amp_support", "Dual Amp on, editing SUPPORT") { seed().dual(supportFocus = true) },
        f("dual_amp_empty_support", "Dual Amp on, SUPPORT empty") { seed().dual(supportFocus = false, support = null) },
        f("dual_support_ir", "Dual Amp on, editing SUPPORT, which plays the Royer custom IR") {
            seed().dual(supportFocus = true).rig { it.copy(support = it.support.copy(cab = Cab.CUSTOM_IR, irId = "ir:royer")) }
        },
        f("dual_custom_support_no_direct", "Dual Amp on, editing a custom SUPPORT amp on a channel without a No Cab capture: Custom IR greyed out") {
            seed().dual(supportFocus = true, support = CUSTOM_AMP)
        },
        f("pre_pitch", "PRE, Pitch focused (OCTAVER)") { seed().pre(PreBlock.PITCH) },
        f("pre_pitch_transpose", "PRE, Pitch in TRANSPOSE (INSTANT)") { seed().params(Param.PITCH_MODE to 0.0, Param.PITCH_ON to 1.0).pre(PreBlock.PITCH) },
        f("pre_pitch_transpose_poly", "PRE, Pitch in TRANSPOSE POLY") { seed().params(Param.PITCH_MODE to 0.0, Param.PITCH_CHAR to 2.0).pre(PreBlock.PITCH) },
        f("pre_pitch_vintage", "PRE, Pitch OCTAVER VINTAGE") { seed().params(Param.PITCH_VOICING to 0.0).pre(PreBlock.PITCH) },
        f("pre_comp", "PRE, Comp focused") { seed().pre(PreBlock.COMP) },
        f("pre_nam1", "PRE, NAM 1 (Klon) focused") { seed().pre(PreBlock.NAM1) },
        f("pre_nam2", "PRE, NAM 2 (TS) focused") { seed().pre(PreBlock.NAM2) },
        f("pre_locked_differs", "PRE locked with Pitch on, which this amp has not saved: Store shows") {
            seed().pre(PreBlock.PITCH).let { s ->
                s.copy(locks = s.locks.copy(pre = Param.entries.filter { it.group == ParamGroup.PRE }.associateWith { s.param(it) } + (Param.PITCH_ON to 1.0)))
            }
        },
        f("post_locked_differs", "POST locked with Delay on, which this amp has not saved: Store shows") {
            seed().post(PostBlock.DELAY).let { s ->
                s.copy(locks = s.locks.copy(post = Param.entries.filter { it.group == ParamGroup.POST }.associateWith { s.param(it) } + (Param.DELAY_ON to 1.0)))
            }
        },
        f("post_chorus", "POST, Chorus focused (ENSEMBLE)") { seed().params(Param.CHORUS_ON to 1.0).post(PostBlock.CHORUS) },
        f("post_chorus_classic", "POST, Chorus CLASSIC") { seed().params(Param.CHORUS_MODE to 0.0).post(PostBlock.CHORUS) },
        f("post_delay", "POST, Delay focused (DIGITAL)") { seed().params(Param.DELAY_ON to 1.0).post(PostBlock.DELAY) },
        f("post_delay_analog", "POST, Delay ANALOG") { seed().params(Param.DELAY_MODE to 1.0).post(PostBlock.DELAY) },
        f("post_delay_reverse", "POST, Delay REVERSE") { seed().params(Param.DELAY_MODE to 2.0).post(PostBlock.DELAY) },
        f("post_delay_sync", "POST, Delay TEMPO SYNC on") { seed().params(Param.DELAY_ON to 1.0, Param.DELAY_SYNC to 1.0).post(PostBlock.DELAY) },
        f("post_reverb", "POST, Reverb focused (HALL)") { seed().post(PostBlock.REVERB) },
        f("post_reverb_plate", "POST, Reverb PLATE") { seed().params(Param.REVERB_MODE to 1.0).post(PostBlock.REVERB) },
        f("post_reverb_okt", "POST, Reverb OKTAVERB SHIMMER") { seed().params(Param.REVERB_MODE to 2.0).post(PostBlock.REVERB) },
        f("post_reverb_okt_halo", "POST, Reverb OKTAVERB HALO") { seed().params(Param.REVERB_MODE to 2.0, Param.REVERB_SUB to 0.0).post(PostBlock.REVERB) },
        f("post_trem", "POST, Tremolo focused (OPTICAL)") { seed().post(PostBlock.TREM) },
        f("post_trem_bias", "POST, Tremolo BIAS") { seed().params(Param.TREM_MODE to 1.0).post(PostBlock.TREM) },
        f("post_trem_harmonic", "POST, Tremolo HARMONIC") { seed().params(Param.TREM_MODE to 2.0, Param.TREM_ON to 1.0).post(PostBlock.TREM) },
        f("post_trem_sync", "POST, Tremolo TEMPO SYNC on") { seed().params(Param.TREM_SYNC to 1.0).post(PostBlock.TREM) },
        f("save_dialog_dirty", "name box on the changed User preset: the button reads Update") {
            seed().params(Param.BASS to 7.0).rig { it.copy(dirty = true) }
                .ui { it.copy(view = View.BUILD, overlays = listOf(Overlay.NameDialog(NamePurpose.SAVE_PRESET, "Sunset Crunch"))) }
        },
        f("rename_dialog", "renaming Sunset Crunch to Sunset Crunch Live") {
            seed().ui { it.copy(view = View.BUILD, overlays = listOf(Overlay.PresetManage, Overlay.NameDialog(NamePurpose.RENAME_PRESET, "Sunset Crunch Live", "u:1"))) }
        },
        f("confirm_delete_preset", "Manage presets asking before deleting Sunset Crunch") {
            seed().ui { it.copy(view = View.BUILD, overlays = listOf(Overlay.PresetManage), confirm = Confirm(ConfirmKind.DELETE_PRESET, "u:1", "Delete \"Sunset Crunch\"?")) }
        },
        f("metronome_on", "metronome running at 96 BPM in 4/4") { seed().copy(metronome = MetronomeState(on = true, bpm = 96.0)) },
        f("metronome_3_4", "metronome in 3/4") { seed().copy(metronome = MetronomeState(sig = TimeSig.S3_4)) },
        f("midi_one_channel", "MIDI listens on channel 3 only") { seed().copy(midi = MidiSettings(channel = 3, lastOneChannel = 3)) },
        f("midi_bank_2", "MIDI tab on bank 3") { seed().copy(midi = MidiSettings(bank = 2)) },
        f("midi_ble_found", "a BLE controller was found next to the USB one") {
            seed().let { it.copy(audio = it.audio.copy(midiDevices = listOf(FakeEngine.usbController, FakeEngine.bleController))) }
        },
        f("midi_device_lost", "the MIDI controller was disconnected") { seed().let { it.copy(audio = it.audio.copy(midiLost = true)) } },
        f("output_raw", "Output mode Raw") { seed().copy(signal = SignalSettings(outputMode = 0)) },
        f("lite", "Performance LITE") { seed().copy(signal = SignalSettings(lite = true)) },
        f("mic_denied", "microphone permission denied; asking again grants it", energy = 0.0) {
            seed().let { it.copy(audio = it.audio.copy(permission = Permission.DENIED)) }
        },
        f("mic_denied_permanent", "microphone permission denied for good: only system settings can grant it", energy = 0.0) {
            seed().let { it.copy(audio = it.audio.copy(permission = Permission.DENIED_PERMANENT)) }.host { it.copy(permissionGrantsOnRequest = false) }
        },
        f("no_interface", "started without a USB audio interface", energy = 0.0) {
            seed().let {
                it.copy(audio = it.audio.copy(inputs = listOf(FakeEngine.builtinMic), outputs = listOf(FakeEngine.builtinSpeaker),
                    inputId = null, outputId = FakeEngine.builtinSpeaker.id, status = InterfaceStatus.NO_INTERFACE))
            }
        },
        f("interface_unplugged", "the USB interface was unplugged mid-session", energy = 0.0) {
            seed().let {
                it.copy(audio = it.audio.copy(inputs = listOf(FakeEngine.builtinMic), outputs = listOf(FakeEngine.builtinSpeaker),
                    inputId = null, outputId = FakeEngine.builtinSpeaker.id, status = InterfaceStatus.UNPLUGGED))
            }
        },
        f("import_invalid", "the next file pick returns files that are not valid captures / WAVs") { seed().host { it.copy(nextPickInvalid = true) } },
        f("import_failed", "an IR import was just rejected") {
            seed().ui { it.copy(banner = Banner(BannerKind.ERROR, "File is not a WAV file: renamed-junk.wav")) }
        },
        f("ir_config", "IR management with Level / Low cut / High cut open on Royer") { seed().ui { it.copy(irConfigOpen = "ir:royer") } },
        f("export_sounds", "Pack export, Sounds scope, Sunset Crunch and Skeleton Drive (needs its custom amp) ticked") {
            seed().copy(packExport = PackExportState(scope = ExportScope.SOUNDS, ticked = setOf("u:1", "u:3")))
        },
        f("export_sounds_support", "Pack export, Sounds scope, a Dual Amp preset whose SUPPORT is the custom amp on the Royer custom IR ticked") {
            seed().let { s ->
                val dualRig = s.rig.copy(
                    params = s.rig.params + (Param.DUAL_ON to 1.0),
                    support = SupportLane(ampId = CUSTOM_AMP, channel = 0, cab = Cab.CUSTOM_IR, irId = "ir:royer"),
                    presetId = null,
                )
                s.copy(
                    library = s.library.copy(presets = s.library.presets + Preset("u:4", THC, "Sunset Stack", factory = false, rig = dualRig)),
                    packExport = PackExportState(scope = ExportScope.SOUNDS, ticked = setOf("u:4")),
                )
            }
        },
        f("export_amp", "Pack export, whole-amp scope, no amp picked yet") { seed().copy(packExport = PackExportState(scope = ExportScope.AMP)) },
        f("pack_preview", "an Everything Pack with three new items") { seed().let { it.copy(packImport = FakeEngine.packPreview(it, PickedFile("seed.volumpack", 48_000))) } },
        f("pack_merge_add", "Pack preview in Add merge mode") {
            seed().let { it.copy(packImport = FakeEngine.packPreview(it, PickedFile("seed.volumpack", 48_000)).copy(merge = MergeMode.ADD)) }
        },
        f("pack_conflict", "a Pack that holds items you already have (Replace / Keep mine / Reloads / Skip)") {
            seed().host { it.copy(packConflict = true) }.let { it.copy(packImport = FakeEngine.packPreview(it, PickedFile("backup.volumpack", 48_000))) }
        },
        f("pack_damaged", "a damaged Pack") {
            seed().host { it.copy(packDamaged = true) }.let { it.copy(packImport = FakeEngine.packPreview(it, PickedFile("broken.volumpack", 10, valid = false))) }
        },
        f("pack_refused", "a Pack that needs more custom pedal slots than are left") {
            seed().host { it.copy(packOverCapacity = true) }.let { it.copy(packImport = FakeEngine.packPreview(it, PickedFile("huge.volumpack", 900_000))) }
        },
        f("builder_files", "custom amp builder with a name and two files") {
            seed().copy(builder = BuilderState(name = "Skeleton Key II", files = listOf(FakeEngine.parseAmpFile("DI-Skel-1.nam"), FakeEngine.parseAmpFile("V30-Skel-2.nam"))))
        },
        f("energy_0", "PLAY art at energy 0", energy = 0.0) { seed() },
        f("energy_half", "PLAY art at energy 0.5", energy = 0.5) { seed() },
        f("energy_full", "PLAY art at energy 1", energy = 1.0) { seed() },
    )

    private val byId = all.associateBy { it.id }
    fun spec(id: String): FixtureSpec = byId[id] ?: error("unknown fixture '$id'")
    fun build(id: String): VoLumState = spec(id).build()
}
