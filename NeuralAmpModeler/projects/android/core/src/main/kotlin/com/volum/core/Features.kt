package com.volum.core

typealias StateCheck = (before: VoLumState, after: VoLumState) -> Boolean

/**
 * One row of `.scratch/android-shootout/features.md`.
 *
 * UI features carry the [check] the harness applies after operating the node
 * through semantics, and the [canonical] action a correct UI dispatches (used
 * only by `:core` unit tests to prove fixture + reducer satisfy [check]).
 */
data class FeatureSpec(
    val id: String,
    val screen: ScreenId?,
    /** `tap`, `toggle`, `select`, `drag`, `text`, `file`, `action:<label>`, `view`, `engine`, or `-` for N/A. */
    val interaction: String,
    val fixture: String = "default",
    val naReason: String? = null,
    val text: String? = null,
    val param: Param? = null,
    val check: StateCheck = { _, _ -> true },
    val canonical: ((VoLumState) -> Action)? = null,
    val precondition: (VoLumState) -> Boolean = { true },
) {
    val isNa: Boolean get() = naReason != null
    val isEngine: Boolean get() = interaction == "engine"
    val isUi: Boolean get() = !isNa && !isEngine
    val isView: Boolean get() = interaction == "view"
    val actionLabel: String? get() = interaction.takeIf { it.startsWith("action:") }?.removePrefix("action:")
}

object FeatureRegistry {
    private val list = mutableListOf<FeatureSpec>()

    private fun add(f: FeatureSpec) {
        list += f
    }

    private fun tap(id: String, screen: ScreenId, fixture: String = "default", interaction: String = "tap", text: String? = null, canonical: (VoLumState) -> Action, check: StateCheck) =
        add(FeatureSpec(id, screen, interaction, fixture, text = text, check = check, canonical = canonical))

    private fun toggle(id: String, screen: ScreenId, p: Param, fixture: String = "default") =
        add(FeatureSpec(id, screen, "toggle", fixture, param = p, check = { b, a -> a.on(p) != b.on(p) }, canonical = { Action.ToggleParam(p) }))

    private fun drag(id: String, screen: ScreenId, p: Param, fixture: String = "default") =
        add(FeatureSpec(id, screen, "drag", fixture, param = p, check = { b, a -> a.param(p) != b.param(p) }, canonical = { s -> Action.SetParam(p, dragTarget(p, s.param(p))) }))

    private fun select(id: String, screen: ScreenId, p: Param, value: Double, fixture: String = "default") =
        add(FeatureSpec(id, screen, "select", fixture, param = p, check = { b, a -> b.param(p) != value && a.param(p) == value }, canonical = { Action.SetParam(p, value) }))

    private fun view(id: String, screen: ScreenId, fixture: String = "default", precondition: (VoLumState) -> Boolean = { true }) =
        add(FeatureSpec(id, screen, "view", fixture, precondition = precondition))

    private fun engine(id: String) = add(FeatureSpec(id, null, "engine"))

    private fun na(id: String, reason: String) = add(FeatureSpec(id, null, "-", naReason = reason))

    /** The value a canonical drag moves to: 85 % of the range, or 15 % when already in the upper half. */
    fun dragTarget(p: Param, current: Double): Double = p.fromNormalized(if (p.normalized(current) < 0.5) 0.85 else 0.15)

    private inline fun <reified T : Overlay> opened(): StateCheck = { b, a -> b.ui.top !is T && a.ui.top is T }
    private inline fun <reified T : Overlay> closed(): StateCheck = { b, a -> b.ui.overlays.any { it is T } && a.ui.overlays.none { it is T } }
    private fun <T> changed(f: (VoLumState) -> T): StateCheck = { b, a -> f(a) != f(b) }

    private val play = ScreenId.PLAY
    private val buildAmp = ScreenId.BUILD_AMP
    private val buildPre = ScreenId.BUILD_PRE
    private val buildPost = ScreenId.BUILD_POST

    init {
        // Navigation and header
        tap("nav.view.build", play, canonical = { Action.SetView(View.BUILD) }) { b, a -> b.ui.view == View.PLAY && a.ui.view == View.BUILD }
        tap("nav.view.play", buildAmp, canonical = { Action.SetView(View.PLAY) }) { b, a -> b.ui.view == View.BUILD && a.ui.view == View.PLAY }
        tap("nav.section.pre", buildAmp, interaction = "select", canonical = { Action.SetSection(Section.PRE) }) { _, a -> a.ui.view == View.BUILD && a.ui.section == Section.PRE }
        tap("nav.section.amp", buildPre, interaction = "select", canonical = { Action.SetSection(Section.AMP) }) { _, a -> a.ui.view == View.BUILD && a.ui.section == Section.AMP }
        tap("nav.section.post", buildAmp, interaction = "select", canonical = { Action.SetSection(Section.POST) }) { _, a -> a.ui.view == View.BUILD && a.ui.section == Section.POST }
        tap("nav.tuner.open", play, canonical = { Action.Open(Overlay.Tuner) }, check = opened<Overlay.Tuner>())
        tap("nav.metronome.open", play, canonical = { Action.Open(Overlay.Metronome) }, check = opened<Overlay.Metronome>())
        tap("nav.settings.open", play, canonical = { Action.Open(Overlay.Settings(SettingsTab.SIGNAL)) }, check = opened<Overlay.Settings>())
        tap("nav.audio_status.open", play, canonical = { Action.Open(Overlay.AudioStatus) }, check = opened<Overlay.AudioStatus>())
        na("nav.back", "Esc is a keyboard key; Android Back is a system gesture the harness cannot operate through semantics, and every panel's close control is its own feature id")
        view("header.preset_name", buildAmp) { it.presetName != null }
        tap("header.preset.prev", buildAmp, canonical = { Action.StepPreset(-1) }, check = changed { it.rig.presetId })
        tap("header.preset.next", buildAmp, canonical = { Action.StepPreset(1) }, check = changed { it.rig.presetId })
        tap("header.preset.menu", buildAmp, canonical = { Action.Open(Overlay.PresetMenu) }, check = opened<Overlay.PresetMenu>())
        view("header.unsaved", buildAmp, "preset_dirty") { it.rig.dirty }
        view("meters.in", buildAmp)
        view("meters.out", buildAmp)
        view("status.output_safety", buildAmp, "output_safety") { it.outputSafety }
        view("banner.error", buildAmp, "load_failed") { it.ui.banner?.kind == BannerKind.ERROR }
        tap("banner.dismiss", buildAmp, "load_failed", canonical = { Action.DismissBanner }) { b, a -> b.ui.banner != null && a.ui.banner == null }
        view("tip.build", play, "first_launch") { !it.ui.buildTipSeen }
        view("dialog.confirm.message", ScreenId.PRESETS_MANAGE, "confirm_delete_preset") { it.ui.confirm != null }
        tap("dialog.confirm.accept", ScreenId.PRESETS_MANAGE, "confirm_delete_preset", canonical = { Action.AcceptConfirm }) { b, a ->
            b.ui.confirm != null && a.ui.confirm == null && a.library.preset("u:1") == null
        }
        tap("dialog.confirm.cancel", ScreenId.PRESETS_MANAGE, "confirm_delete_preset", canonical = { Action.CancelConfirm }) { b, a ->
            b.ui.confirm != null && a.ui.confirm == null && a.library.preset("u:1") != null
        }

        // PLAY
        tap("play.sound.recall", play, canonical = { Action.RecallSound(1) }, check = changed { it.library.live })
        tap("play.sound.next", play, canonical = { Action.StepSound(1) }, check = changed { it.library.live })
        tap("play.sound.prev", play, canonical = { Action.StepSound(-1) }, check = changed { it.library.live })
        view("play.sound.live", play) { it.library.live != null }
        view("play.sound.name", play)
        view("play.sound.program", play)
        view("play.sound.invalid", play, "invalid_slot") { s -> s.library.sounds.keys.any { !s.library.soundValid(it) } }
        tap("play.sound.replace", play, canonical = { Action.Open(Overlay.SoundPicker(0, replace = true)) }) { b, a ->
            b.ui.top !is Overlay.SoundPicker && (a.ui.top as? Overlay.SoundPicker)?.let { it.replace && it.program in b.library.sounds } == true
        }
        tap("play.sound.clear", play, canonical = { Action.ClearSound(0) }) { b, a -> a.library.sounds.size == b.library.sounds.size - 1 }
        tap("play.sound.reorder", play, interaction = "action:Move down", canonical = { Action.MoveSound(0, 1) }) { b, a ->
            a.library.sounds.size == b.library.sounds.size && a.library.sounds != b.library.sounds
        }
        tap("play.add_this", play, canonical = { Action.AddThisSound }) { b, a -> a.library.sounds.size == b.library.sounds.size + 1 }
        tap("play.add_this.unnamed", play, "unnamed_rig", canonical = { Action.AddThisSound }) { _, a -> (a.ui.top as? Overlay.NameDialog)?.purpose == NamePurpose.ADD_SOUND }
        tap("play.add_this.full", play, "play_full", canonical = { Action.AddThisSound }, check = opened<Overlay.SoundPicker>())
        tap("play.add_sound", play, canonical = { s -> Action.Open(Overlay.SoundPicker(s.library.firstFreeProgram() ?: 0, replace = false)) }) { b, a ->
            b.ui.top !is Overlay.SoundPicker && (a.ui.top as? Overlay.SoundPicker)?.replace == false
        }
        tap("play.save", play, canonical = { s -> Action.Open(Overlay.NameDialog(NamePurpose.SAVE_PRESET, Reducer.nameSuggestion(s))) }) { _, a ->
            (a.ui.top as? Overlay.NameDialog)?.purpose == NamePurpose.SAVE_PRESET
        }
        toggle("play.stomp.pitch", play, Param.PITCH_ON)
        toggle("play.stomp.comp", play, Param.COMP_ON)
        toggle("play.stomp.nam1", play, Param.NAM1_ON)
        toggle("play.stomp.nam2", play, Param.NAM2_ON)
        toggle("play.stomp.chorus", play, Param.CHORUS_ON)
        toggle("play.stomp.delay", play, Param.DELAY_ON)
        toggle("play.stomp.reverb", play, Param.REVERB_ON)
        toggle("play.stomp.trem", play, Param.TREM_ON)
        tap("play.stomp.edit", play, canonical = { Action.EditStomp(Param.DELAY_ON) }) { b, a ->
            b.ui.view == View.PLAY && a.ui.view == View.BUILD && a.ui.section != Section.AMP
        }
        view("play.art", play)
        view("play.meters", play)
        tap("play.picker.program", ScreenId.PLAY_PICKER, interaction = "select", canonical = { s -> Action.SetPickerProgram(((s.ui.top as Overlay.SoundPicker).program + 1) % 128) }) { b, a ->
            (b.ui.top as? Overlay.SoundPicker)?.program != (a.ui.top as? Overlay.SoundPicker)?.program && a.ui.top is Overlay.SoundPicker
        }
        tap("play.picker.factory", ScreenId.PLAY_PICKER, canonical = { s -> Action.AssignSound((s.ui.top as Overlay.SoundPicker).program, Catalog.factoryId(0, 0)) }) { b, a ->
            val program = (b.ui.top as? Overlay.SoundPicker)?.program
            program != null && a.library.sounds[program] != b.library.sounds[program] && a.library.preset(a.library.sounds[program]?.presetId)?.factory == true
        }
        tap("play.picker.user", ScreenId.PLAY_PICKER, canonical = { s -> Action.AssignSound((s.ui.top as Overlay.SoundPicker).program, "u:2") }) { b, a ->
            val program = (b.ui.top as? Overlay.SoundPicker)?.program
            program != null && a.library.sounds[program] != b.library.sounds[program] && a.library.preset(a.library.sounds[program]?.presetId)?.factory == false
        }
        tap("play.picker.close", ScreenId.PLAY_PICKER, canonical = { Action.Back }, check = closed<Overlay.SoundPicker>())
        engine("play.starting_sounds")
        engine("play.view_memory")

        // BUILD amps and cabs
        tap("amp.list.select", ScreenId.AMP_PICKER, canonical = { Action.SelectAmp(Catalog.ampId(0)) }, check = changed { it.ampId })
        view("amp.list.custom", ScreenId.AMP_PICKER) { s -> s.library.amps.any { it.custom } }
        view("amp.name", buildAmp)
        view("amp.hero", buildAmp)
        tap("amp.channel", buildAmp, interaction = "select", canonical = { Action.SelectChannel(0) }, check = changed { it.rig.channel })
        tap("amp.cab.none", buildAmp, interaction = "select", canonical = { Action.SelectCab(Cab.NONE) }) { b, a -> b.rig.cab != Cab.NONE && a.rig.cab == Cab.NONE }
        tap("amp.cab.g12", buildAmp, interaction = "select", canonical = { Action.SelectCab(Cab.G12) }) { b, a -> b.rig.cab != Cab.G12 && a.rig.cab == Cab.G12 }
        tap("amp.cab.g65", buildAmp, interaction = "select", canonical = { Action.SelectCab(Cab.G65) }) { b, a -> b.rig.cab != Cab.G65 && a.rig.cab == Cab.G65 }
        tap("amp.cab.v30", buildAmp, "cab_g12", interaction = "select", canonical = { Action.SelectCab(Cab.V30) }) { b, a -> b.rig.cab != Cab.V30 && a.rig.cab == Cab.V30 }
        tap("amp.cab.custom_ir", buildAmp, canonical = { Action.Open(Overlay.IrMenu) }, check = opened<Overlay.IrMenu>())
        view("amp.cab.custom_ir_disabled", buildAmp, "custom_amp_no_direct") { s -> s.rig.channel !in (s.amp?.noCabChannels ?: emptySet()) }
        drag("amp.knob.input", buildAmp, Param.INPUT)
        drag("amp.knob.gate", buildAmp, Param.GATE)
        drag("amp.knob.bass", buildAmp, Param.BASS)
        drag("amp.knob.mid", buildAmp, Param.MID)
        drag("amp.knob.treble", buildAmp, Param.TREBLE)
        drag("amp.knob.output", buildAmp, Param.OUTPUT)
        tap("amp.knob.reset", buildAmp, "knobs_moved", interaction = "action:Reset", canonical = { Action.ResetParam(Param.BASS) }) { b, a ->
            b.param(Param.BASS) != Param.BASS.default && a.param(Param.BASS) == Param.BASS.default
        }
        tap("amp.knob.exact", buildAmp, interaction = "text", text = "7.5", canonical = { Action.SetParam(Param.BASS, 7.5) }) { _, a -> a.param(Param.BASS) == 7.5 }
        toggle("amp.gate.toggle", buildAmp, Param.GATE_ON)
        toggle("amp.eq.toggle", buildAmp, Param.EQ_ON)
        engine("amp.rig_memory")
        engine("amp.lite_note")

        // Dual Amp
        toggle("dual.toggle", buildAmp, Param.DUAL_ON)
        tap("dual.focus.main", buildAmp, "dual_amp_support", interaction = "select", canonical = { Action.FocusLane(Lane.MAIN) }) { b, a -> b.ui.lane == Lane.SUPPORT && a.ui.lane == Lane.MAIN }
        tap("dual.focus.support", buildAmp, "dual_amp", interaction = "select", canonical = { Action.FocusLane(Lane.SUPPORT) }) { b, a -> b.ui.lane == Lane.MAIN && a.ui.lane == Lane.SUPPORT }
        tap("dual.support.choose", buildAmp, "dual_amp_empty_support", canonical = { Action.Open(Overlay.SupportAmpMenu) }, check = opened<Overlay.SupportAmpMenu>())
        tap("dual.support.select", ScreenId.SUPPORT_AMP_MENU, "dual_amp_empty_support", canonical = { Action.SelectSupportAmp(Catalog.ampId(0)) }) { b, a ->
            a.rig.support.ampId != null && a.rig.support.ampId != b.rig.support.ampId
        }
        tap("dual.support.none", ScreenId.SUPPORT_AMP_MENU, "dual_amp", canonical = { Action.SelectSupportAmp(null) }) { b, a -> b.rig.support.ampId != null && a.rig.support.ampId == null }
        tap("dual.support.close", ScreenId.SUPPORT_AMP_MENU, "dual_amp_empty_support", canonical = { Action.Back }, check = closed<Overlay.SupportAmpMenu>())
        drag("dual.pan.main", buildAmp, Param.MAIN_PAN, "dual_amp")
        drag("dual.pan.support", buildAmp, Param.SUPPORT_PAN, "dual_amp_support")
        toggle("dual.polarity", buildAmp, Param.SUPPORT_POLARITY, "dual_amp_support")
        tap("dual.support.channel", buildAmp, "dual_amp_support", interaction = "select", canonical = { Action.SelectSupportChannel(0) }, check = changed { it.rig.support.channel })
        tap("dual.support.cab", buildAmp, "dual_amp_support", interaction = "select", canonical = { Action.SelectSupportCab(Cab.V30) }, check = changed { it.rig.support.cab })
        drag("dual.support.knob.input", buildAmp, Param.SUPPORT_INPUT, "dual_amp_support")
        drag("dual.support.knob.gate", buildAmp, Param.SUPPORT_GATE, "dual_amp_support")
        drag("dual.support.knob.bass", buildAmp, Param.SUPPORT_BASS, "dual_amp_support")
        drag("dual.support.knob.mid", buildAmp, Param.SUPPORT_MID, "dual_amp_support")
        drag("dual.support.knob.treble", buildAmp, Param.SUPPORT_TREBLE, "dual_amp_support")
        drag("dual.support.knob.output", buildAmp, Param.SUPPORT_OUTPUT, "dual_amp_support")
        toggle("dual.support.gate.toggle", buildAmp, Param.SUPPORT_GATE_ON, "dual_amp_support")
        toggle("dual.support.eq.toggle", buildAmp, Param.SUPPORT_EQ_ON, "dual_amp_support")
        na("dual.route", "the desktop UI no longer surfaces STACK / L-R; the enum only keeps old chunks loading")

        // PRE
        tap("pre.lock", buildPre, interaction = "toggle", canonical = { s -> Action.SetLock(Section.PRE, s.locks.pre == null) }) { b, a -> (b.locks.pre == null) != (a.locks.pre == null) }
        tap("pre.store", buildPre, "pre_locked_differs", canonical = { Action.StoreLock(Section.PRE) }) { b, a ->
            Reducer.lockDiffers(b, Section.PRE) && !Reducer.lockDiffers(a, Section.PRE)
        }
        engine("pre.lock.play_unlocks")
        tap("pre.card.pitch", buildPre, "pre_comp", interaction = "select", canonical = { Action.FocusPre(PreBlock.PITCH) }) { b, a -> b.ui.preFocus != PreBlock.PITCH && a.ui.preFocus == PreBlock.PITCH }
        tap("pre.card.comp", buildPre, "pre_pitch", interaction = "select", canonical = { Action.FocusPre(PreBlock.COMP) }) { b, a -> b.ui.preFocus != PreBlock.COMP && a.ui.preFocus == PreBlock.COMP }
        tap("pre.card.nam1", buildPre, "pre_pitch", interaction = "select", canonical = { Action.FocusPre(PreBlock.NAM1) }) { b, a -> b.ui.preFocus != PreBlock.NAM1 && a.ui.preFocus == PreBlock.NAM1 }
        tap("pre.card.nam2", buildPre, "pre_pitch", interaction = "select", canonical = { Action.FocusPre(PreBlock.NAM2) }) { b, a -> b.ui.preFocus != PreBlock.NAM2 && a.ui.preFocus == PreBlock.NAM2 }
        toggle("pre.pitch.on", buildPre, Param.PITCH_ON, "pre_pitch")
        toggle("pre.comp.on", buildPre, Param.COMP_ON, "pre_comp")
        toggle("pre.nam1.on", buildPre, Param.NAM1_ON, "pre_nam1")
        toggle("pre.nam2.on", buildPre, Param.NAM2_ON, "pre_nam2")
        select("pre.pitch.mode.transpose", buildPre, Param.PITCH_MODE, 0.0, "pre_pitch")
        select("pre.pitch.mode.octaver", buildPre, Param.PITCH_MODE, 1.0, "pre_pitch_transpose")
        drag("pre.pitch.semi", buildPre, Param.PITCH_SEMI, "pre_pitch_transpose")
        drag("pre.pitch.mix", buildPre, Param.PITCH_MIX, "pre_pitch_transpose")
        drag("pre.pitch.level", buildPre, Param.PITCH_LEVEL, "pre_pitch_transpose")
        select("pre.pitch.char.instant", buildPre, Param.PITCH_CHAR, 1.0, "pre_pitch_transpose_poly")
        select("pre.pitch.char.poly", buildPre, Param.PITCH_CHAR, 2.0, "pre_pitch_transpose")
        drag("pre.pitch.octdown", buildPre, Param.PITCH_OCT_DOWN, "pre_pitch")
        drag("pre.pitch.octup", buildPre, Param.PITCH_OCT_UP, "pre_pitch")
        drag("pre.pitch.dry", buildPre, Param.PITCH_DRY, "pre_pitch")
        drag("pre.pitch.oct_level", buildPre, Param.PITCH_LEVEL, "pre_pitch")
        select("pre.pitch.voicing.vintage", buildPre, Param.PITCH_VOICING, 0.0, "pre_pitch")
        select("pre.pitch.voicing.modern", buildPre, Param.PITCH_VOICING, 1.0, "pre_pitch_vintage")
        drag("pre.comp.input", buildPre, Param.COMP_AMOUNT, "pre_comp")
        drag("pre.comp.attack", buildPre, Param.COMP_ATTACK, "pre_comp")
        drag("pre.comp.release", buildPre, Param.COMP_RELEASE, "pre_comp")
        drag("pre.comp.output", buildPre, Param.COMP_LEVEL, "pre_comp")
        tap("pre.nam1.capture", buildPre, "pre_nam1", canonical = { Action.Open(Overlay.CaptureMenu(1)) }) { _, a -> (a.ui.top as? Overlay.CaptureMenu)?.slot == 1 }
        tap("pre.nam2.capture", buildPre, "pre_nam2", canonical = { Action.Open(Overlay.CaptureMenu(2)) }) { _, a -> (a.ui.top as? Overlay.CaptureMenu)?.slot == 2 }
        drag("pre.nam1.gain", buildPre, Param.NAM1_GAIN, "pre_nam1")
        drag("pre.nam1.bass", buildPre, Param.NAM1_BASS, "pre_nam1")
        drag("pre.nam1.mid", buildPre, Param.NAM1_MID, "pre_nam1")
        drag("pre.nam1.midhz", buildPre, Param.NAM1_MID_HZ, "pre_nam1")
        drag("pre.nam1.treble", buildPre, Param.NAM1_TREBLE, "pre_nam1")
        drag("pre.nam1.level", buildPre, Param.NAM1_LEVEL, "pre_nam1")
        drag("pre.nam2.gain", buildPre, Param.NAM2_GAIN, "pre_nam2")
        drag("pre.nam2.bass", buildPre, Param.NAM2_BASS, "pre_nam2")
        drag("pre.nam2.mid", buildPre, Param.NAM2_MID, "pre_nam2")
        drag("pre.nam2.midhz", buildPre, Param.NAM2_MID_HZ, "pre_nam2")
        drag("pre.nam2.treble", buildPre, Param.NAM2_TREBLE, "pre_nam2")
        drag("pre.nam2.level", buildPre, Param.NAM2_LEVEL, "pre_nam2")
        tap("pre.capture.select", ScreenId.PRE_CAPTURE_MENU, canonical = { Action.SelectCapture(1, 2) }, check = changed { it.param(Param.NAM1_CAPTURE) })
        tap("pre.capture.empty", ScreenId.PRE_CAPTURE_MENU, canonical = { Action.SelectCapture(1, 0) }) { b, a -> b.param(Param.NAM1_CAPTURE) != 0.0 && a.param(Param.NAM1_CAPTURE) == 0.0 }
        view("pre.capture.custom", ScreenId.PRE_CAPTURE_MENU) { s -> s.library.pedals.any { it.custom } }
        tap("pre.capture.manage", ScreenId.PRE_CAPTURE_MENU, canonical = { Action.Open(Overlay.PedalManage) }, check = opened<Overlay.PedalManage>())
        tap("pre.capture.close", ScreenId.PRE_CAPTURE_MENU, canonical = { Action.Back }, check = closed<Overlay.CaptureMenu>())
        na("pre.comp.ratio", "the desktop knob row has no RATIO; the param keeps its default")
        na("pre.comp.mix", "the desktop knob row has no MIX; the param keeps its default")
        na("pre.boost", "reserved legacy params kept only for old chunks; PRE captures replaced the block")

        // POST
        tap("post.lock", buildPost, interaction = "toggle", canonical = { s -> Action.SetLock(Section.POST, s.locks.post == null) }) { b, a -> (b.locks.post == null) != (a.locks.post == null) }
        tap("post.store", buildPost, "post_locked_differs", canonical = { Action.StoreLock(Section.POST) }) { b, a ->
            Reducer.lockDiffers(b, Section.POST) && !Reducer.lockDiffers(a, Section.POST)
        }
        tap("post.card.chorus", buildPost, "post_delay", interaction = "select", canonical = { Action.FocusPost(PostBlock.CHORUS) }) { b, a -> b.ui.postFocus != PostBlock.CHORUS && a.ui.postFocus == PostBlock.CHORUS }
        tap("post.card.delay", buildPost, "post_chorus", interaction = "select", canonical = { Action.FocusPost(PostBlock.DELAY) }) { b, a -> b.ui.postFocus != PostBlock.DELAY && a.ui.postFocus == PostBlock.DELAY }
        tap("post.card.reverb", buildPost, "post_delay", interaction = "select", canonical = { Action.FocusPost(PostBlock.REVERB) }) { b, a -> b.ui.postFocus != PostBlock.REVERB && a.ui.postFocus == PostBlock.REVERB }
        tap("post.card.trem", buildPost, "post_delay", interaction = "select", canonical = { Action.FocusPost(PostBlock.TREM) }) { b, a -> b.ui.postFocus != PostBlock.TREM && a.ui.postFocus == PostBlock.TREM }
        toggle("post.chorus.on", buildPost, Param.CHORUS_ON, "post_chorus")
        toggle("post.delay.on", buildPost, Param.DELAY_ON, "post_delay")
        toggle("post.reverb.on", buildPost, Param.REVERB_ON, "post_reverb")
        toggle("post.trem.on", buildPost, Param.TREM_ON, "post_trem")
        engine("post.mode_memory")
        select("post.chorus.mode.classic", buildPost, Param.CHORUS_MODE, 0.0, "post_chorus")
        select("post.chorus.mode.warped", buildPost, Param.CHORUS_MODE, 1.0, "post_chorus")
        select("post.chorus.mode.clear", buildPost, Param.CHORUS_MODE, 2.0, "post_chorus")
        select("post.chorus.mode.ensemble", buildPost, Param.CHORUS_MODE, 3.0, "post_chorus_classic")
        drag("post.chorus.rate", buildPost, Param.CHORUS_RATE, "post_chorus")
        drag("post.chorus.depth", buildPost, Param.CHORUS_DEPTH, "post_chorus")
        drag("post.chorus.tone", buildPost, Param.CHORUS_TONE, "post_chorus")
        drag("post.chorus.width", buildPost, Param.CHORUS_WIDTH, "post_chorus")
        drag("post.chorus.mix", buildPost, Param.CHORUS_MIX, "post_chorus")
        select("post.delay.mode.digital", buildPost, Param.DELAY_MODE, 0.0, "post_delay_analog")
        select("post.delay.mode.analog", buildPost, Param.DELAY_MODE, 1.0, "post_delay")
        select("post.delay.mode.reverse", buildPost, Param.DELAY_MODE, 2.0, "post_delay")
        drag("post.delay.time", buildPost, Param.DELAY_TIME, "post_delay")
        drag("post.delay.feedback", buildPost, Param.DELAY_FEEDBACK, "post_delay")
        drag("post.delay.mix", buildPost, Param.DELAY_MIX, "post_delay")
        drag("post.delay.tone", buildPost, Param.DELAY_TONE, "post_delay")
        drag("post.delay.grit", buildPost, Param.DELAY_AGE, "post_delay")
        drag("post.delay.wear", buildPost, Param.DELAY_AGE, "post_delay_analog")
        drag("post.delay.bloom", buildPost, Param.DELAY_AGE, "post_delay_reverse")
        toggle("post.delay.pingpong", buildPost, Param.DELAY_PING_PONG, "post_delay")
        toggle("post.delay.sync", buildPost, Param.DELAY_SYNC, "post_delay")
        tap("post.delay.division", buildPost, "post_delay_sync", interaction = "select", canonical = { Action.SetParam(Param.DELAY_DIVISION, 4.0) }, check = changed { it.param(Param.DELAY_DIVISION) })
        select("post.reverb.mode.hall", buildPost, Param.REVERB_MODE, 0.0, "post_reverb_plate")
        select("post.reverb.mode.plate", buildPost, Param.REVERB_MODE, 1.0, "post_reverb")
        select("post.reverb.mode.oktaverb", buildPost, Param.REVERB_MODE, 2.0, "post_reverb")
        drag("post.reverb.mix", buildPost, Param.REVERB_MIX, "post_reverb")
        drag("post.reverb.decay", buildPost, Param.REVERB_DECAY, "post_reverb")
        drag("post.reverb.tone", buildPost, Param.REVERB_TONE, "post_reverb")
        drag("post.reverb.predelay", buildPost, Param.REVERB_PRE_DELAY, "post_reverb")
        select("post.reverb.sub.halo", buildPost, Param.REVERB_SUB, 0.0, "post_reverb_okt")
        select("post.reverb.sub.shimmer", buildPost, Param.REVERB_SUB, 1.0, "post_reverb_okt_halo")
        select("post.reverb.sub.bloom", buildPost, Param.REVERB_SUB, 2.0, "post_reverb_okt")
        drag("post.reverb.intensity", buildPost, Param.REVERB_SHIMMER, "post_reverb_okt")
        select("post.trem.mode.optical", buildPost, Param.TREM_MODE, 0.0, "post_trem_bias")
        select("post.trem.mode.bias", buildPost, Param.TREM_MODE, 1.0, "post_trem")
        select("post.trem.mode.harmonic", buildPost, Param.TREM_MODE, 2.0, "post_trem")
        drag("post.trem.rate", buildPost, Param.TREM_RATE, "post_trem")
        drag("post.trem.depth", buildPost, Param.TREM_DEPTH, "post_trem")
        drag("post.trem.shape", buildPost, Param.TREM_SHAPE, "post_trem")
        drag("post.trem.mix", buildPost, Param.TREM_MIX, "post_trem")
        drag("post.trem.xover", buildPost, Param.TREM_XOVER, "post_trem_harmonic")
        toggle("post.trem.sync", buildPost, Param.TREM_SYNC, "post_trem")
        tap("post.trem.division", buildPost, "post_trem_sync", interaction = "select", canonical = { Action.SetParam(Param.TREM_DIVISION, 4.0) }, check = changed { it.param(Param.TREM_DIVISION) })
        engine("post.tempo")
        na("post.tempo.daw", "Android VoLum is a standalone app; there is no DAW song tempo to follow")

        // Presets
        val menu = ScreenId.PRESETS_MENU
        tap("presets.menu.default", menu, "preset_dirty", canonical = { Action.ResetToDefault }) { b, a -> b.rig.presetId != null && a.rig.presetId == null && !a.rig.dirty }
        tap("presets.menu.factory", menu, canonical = { Action.LoadPreset(Catalog.factoryId(14, 0)) }) { b, a ->
            a.rig.presetId != b.rig.presetId && a.library.preset(a.rig.presetId)?.factory == true
        }
        tap("presets.menu.user", menu, "on_factory", canonical = { Action.LoadPreset("u:1") }) { b, a ->
            a.rig.presetId != b.rig.presetId && a.library.preset(a.rig.presetId)?.factory == false
        }
        tap("presets.menu.overwrite", menu, "preset_dirty", canonical = { s -> Action.RequestOverwritePreset(s.rig.presetId!!) }) { _, a -> a.ui.confirm?.kind == ConfirmKind.OVERWRITE_PRESET }
        tap("presets.menu.save_new", menu, canonical = { s -> Action.Open(Overlay.NameDialog(NamePurpose.SAVE_PRESET, "New Preset")) }, check = opened<Overlay.NameDialog>())
        tap("presets.menu.manage", menu, canonical = { Action.Open(Overlay.PresetManage) }, check = opened<Overlay.PresetManage>())
        tap("presets.menu.close", menu, canonical = { Action.Back }, check = closed<Overlay.PresetMenu>())
        tap("presets.save.open", buildAmp, canonical = { s -> Action.Open(Overlay.NameDialog(NamePurpose.SAVE_PRESET, Reducer.nameSuggestion(s))) }) { _, a ->
            (a.ui.top as? Overlay.NameDialog)?.purpose == NamePurpose.SAVE_PRESET
        }
        engine("presets.factory_catalog")
        engine("presets.save.dedupe")
        val manage = ScreenId.PRESETS_MANAGE
        tap("presets.manage.overwrite", manage, canonical = { Action.RequestOverwritePreset("u:1") }) { _, a -> a.ui.confirm?.kind == ConfirmKind.OVERWRITE_PRESET }
        tap("presets.manage.rename", manage, canonical = { Action.RequestRenamePreset("u:1") }) { _, a -> (a.ui.top as? Overlay.NameDialog)?.purpose == NamePurpose.RENAME_PRESET }
        tap("presets.manage.delete", manage, canonical = { Action.RequestDeletePreset("u:1") }) { _, a -> a.ui.confirm?.kind == ConfirmKind.DELETE_PRESET }
        tap("presets.manage.close", manage, canonical = { Action.Back }, check = closed<Overlay.PresetManage>())
        val name = ScreenId.DIALOG_NAME
        tap("name.field", name, interaction = "text", text = "Riff Machine", canonical = { Action.SetNameText("Riff Machine") }) { _, a ->
            (a.ui.top as? Overlay.NameDialog)?.text == "Riff Machine"
        }
        tap("name.save", name, "on_factory", canonical = { Action.CommitName }) { b, a ->
            a.library.presets.count { !it.factory } == b.library.presets.count { !it.factory } + 1 && a.ui.top !is Overlay.NameDialog
        }
        tap("name.update", name, "save_dialog_dirty", canonical = { Action.CommitName }) { b, a ->
            b.rig.dirty && !a.rig.dirty && a.library.presets.size == b.library.presets.size && a.rig.presetId == b.rig.presetId
        }
        tap("name.cancel", name, canonical = { Action.Back }) { b, a -> b.ui.top is Overlay.NameDialog && a.ui.top !is Overlay.NameDialog && a.library == b.library }
        tap("name.rename", name, "rename_dialog", canonical = { Action.CommitName }) { b, a -> a.library.preset("u:1")?.name != b.library.preset("u:1")?.name }

        // Tuner and metronome
        view("tuner.note", ScreenId.TUNER)
        view("tuner.cents", ScreenId.TUNER)
        view("tuner.muted", ScreenId.TUNER) { it.tunerOpen }
        tap("tuner.close", ScreenId.TUNER, canonical = { Action.Back }, check = closed<Overlay.Tuner>())
        val metro = ScreenId.METRONOME
        tap("metronome.on", metro, interaction = "toggle", canonical = { s -> Action.SetMetronomeOn(!s.metronome.on) }, check = changed { it.metronome.on })
        tap("metronome.bpm", metro, interaction = "drag", canonical = { Action.SetBpm(250.0) }, check = changed { it.metronome.bpm })
        tap("metronome.volume", metro, interaction = "drag", canonical = { Action.SetMetronomeVolume(0.1) }, check = changed { it.metronome.volume })
        listOf("1_4" to TimeSig.S1_4, "2_4" to TimeSig.S2_4, "3_4" to TimeSig.S3_4, "4_4" to TimeSig.S4_4, "6_8" to TimeSig.S6_8).forEach { (suffix, sig) ->
            tap("metronome.sig.$suffix", metro, if (sig == TimeSig.S4_4) "metronome_3_4" else "default", interaction = "select", canonical = { Action.SetTimeSig(sig) }) { b, a ->
                b.metronome.sig != sig && a.metronome.sig == sig
            }
        }
        view("metronome.beat", metro, "metronome_on")
        tap("metronome.close", metro, canonical = { Action.Back }, check = closed<Overlay.Metronome>())

        // MIDI
        val midiTab = ScreenId.SETTINGS_MIDI
        tap("midi.channel.all", midiTab, "midi_one_channel", interaction = "select", canonical = { Action.SetMidiChannel(0) }) { b, a -> b.midi.channel != 0 && a.midi.channel == 0 }
        tap("midi.channel.one", midiTab, interaction = "select", canonical = { s -> Action.SetMidiChannel(s.midi.lastOneChannel) }) { b, a -> b.midi.channel == 0 && a.midi.channel != 0 }
        tap("midi.channel.number", midiTab, "midi_one_channel", interaction = "select", canonical = { Action.SetMidiChannel(4) }) { b, a -> a.midi.channel != 0 && a.midi.channel != b.midi.channel }
        tap("midi.recall_cc", midiTab, canonical = { Action.SetRecallCc(103) }, check = changed { it.midi.recallCc })
        tap("midi.bank.next", midiTab, canonical = { s -> Action.SetMidiBank(s.midi.bank + 1) }, check = changed { it.midi.bank })
        tap("midi.bank.prev", midiTab, "midi_bank_2", canonical = { s -> Action.SetMidiBank(s.midi.bank - 1) }, check = changed { it.midi.bank })
        tap("midi.slot.assign", midiTab, canonical = { Action.Open(Overlay.SoundPicker(0, replace = true)) }, check = opened<Overlay.SoundPicker>())
        tap("midi.slot.move", midiTab, interaction = "action:Move down", canonical = { Action.MoveSound(0, 1) }) { b, a ->
            a.library.sounds.size == b.library.sounds.size && a.library.sounds != b.library.sounds
        }
        tap("midi.slot.clear", midiTab, canonical = { Action.ClearSound(0) }) { b, a -> a.library.sounds.size == b.library.sounds.size - 1 }
        tap("midi.device.select", ScreenId.SETTINGS_AUDIO, "midi_ble_found", canonical = { Action.SelectMidiDevice(FakeEngine.bleController.id) }, check = changed { it.audio.midiDeviceId })
        tap("midi.device.ble_scan", ScreenId.SETTINGS_AUDIO, canonical = { Action.ScanBleMidi }) { b, a -> a.audio.midiDevices.size == b.audio.midiDevices.size + 1 }
        view("midi.device.lost", ScreenId.SETTINGS_AUDIO, "midi_device_lost") { it.audio.midiLost }
        engine("midi.program_change")
        engine("midi.recall_cc_message")
        engine("midi.empty_program")
        engine("midi.channel_filter")
        na("midi.learn", "desktop VoLum deliberately has no MIDI Learn; the slot grid (midi.slot.*) is the learn/assign model")
        na("midi.bank_select", "desktop ignores Bank Select, notes, pitch bend and MIDI output on purpose")
        na("midi.vst3_channel", "plug-in format rule; Android VoLum is a standalone app")

        // Custom content
        val ampPicker = ScreenId.AMP_PICKER
        tap("custom.amp.add", ampPicker, canonical = { Action.OpenBuilder(null) }) { b, a -> b.ui.top !is Overlay.CustomAmpBuilder && a.ui.top is Overlay.CustomAmpBuilder && a.builder.editAmpId == null }
        tap("custom.amp.edit", ampPicker, canonical = { Action.OpenBuilder("c:1") }) { _, a -> a.ui.top is Overlay.CustomAmpBuilder && a.builder.editAmpId != null }
        tap("custom.amp.delete", ampPicker, canonical = { Action.RequestDeleteAmp("c:1") }) { _, a -> a.ui.confirm?.kind == ConfirmKind.DELETE_AMP }
        val builder = ScreenId.CUSTOM_AMP_BUILDER
        tap("builder.name", builder, interaction = "text", text = "Skeleton Key II", canonical = { Action.BuilderSetName("Skeleton Key II") }) { _, a -> a.builder.name == "Skeleton Key II" }
        tap("builder.add_files", builder, interaction = "file", canonical = { Action.PickFiles(PickKind.NAM_AMP_FILES) }) { b, a -> a.builder.files.size > b.builder.files.size }
        tap("builder.file.cab", builder, "builder_files", interaction = "select", canonical = { Action.BuilderSetFileCab(0, 2) }, check = changed { s -> s.builder.files.map { it.cabSlot } })
        tap("builder.file.channel", builder, "builder_files", interaction = "select", canonical = { Action.BuilderSetFileChannel(0, 3) }, check = changed { s -> s.builder.files.map { it.channel } })
        tap("builder.file.remove", builder, "builder_files", canonical = { Action.BuilderRemoveFile(0) }) { b, a -> a.builder.files.size == b.builder.files.size - 1 }
        tap("builder.cab_name", builder, interaction = "text", text = "Royer", canonical = { Action.BuilderSetCabName(1, "Royer") }, check = changed { it.builder.cabNames })
        tap("builder.art", builder, interaction = "select", canonical = { Action.BuilderSetArt(2) }, check = changed { it.builder.art })
        tap("builder.save", builder, "builder_files", canonical = { Action.BuilderSave }) { b, a -> a.library.amps.size == b.library.amps.size + 1 }
        tap("builder.close", builder, canonical = { Action.Back }, check = closed<Overlay.CustomAmpBuilder>())
        engine("builder.autofill")
        tap("ir.menu.select", ScreenId.CAB_IR_MENU, canonical = { Action.SelectIr("ir:royer") }) { b, a -> b.rig.cab != Cab.CUSTOM_IR && a.rig.cab == Cab.CUSTOM_IR && a.rig.irId != null }
        tap("ir.menu.manage", ScreenId.CAB_IR_MENU, canonical = { Action.Open(Overlay.IrManage) }, check = opened<Overlay.IrManage>())
        tap("ir.menu.close", ScreenId.CAB_IR_MENU, canonical = { Action.Back }, check = closed<Overlay.IrMenu>())
        val irManage = ScreenId.CUSTOM_IR_MANAGE
        tap("ir.import", irManage, interaction = "file", canonical = { Action.PickFiles(PickKind.IR_FILES) }) { b, a -> a.library.irs.size > b.library.irs.size }
        tap("ir.import.invalid", irManage, "import_invalid", interaction = "file", canonical = { Action.PickFiles(PickKind.IR_FILES) }) { b, a ->
            a.ui.banner?.kind == BannerKind.ERROR && a.library.irs == b.library.irs
        }
        tap("ir.config.open", irManage, canonical = { Action.OpenIrConfig("ir:royer") }) { b, a -> b.ui.irConfigOpen == null && a.ui.irConfigOpen != null }
        tap("ir.config.level", irManage, "ir_config", interaction = "drag", canonical = { Action.SetIrLevel("ir:royer", 6.0) }, check = changed { s -> s.library.irs.map { it.levelDb } })
        tap("ir.config.lowcut", irManage, "ir_config", interaction = "drag", canonical = { Action.SetIrLowCut("ir:royer", 120.0) }, check = changed { s -> s.library.irs.map { it.lowCutHz } })
        tap("ir.config.highcut", irManage, "ir_config", interaction = "drag", canonical = { Action.SetIrHighCut("ir:royer", 8000.0) }, check = changed { s -> s.library.irs.map { it.highCutHz } })
        tap("ir.rename", irManage, canonical = { Action.RequestRenameIr("ir:royer") }) { _, a -> (a.ui.top as? Overlay.NameDialog)?.purpose == NamePurpose.RENAME_IR }
        tap("ir.delete", irManage, canonical = { Action.RequestDeleteIr("ir:royer") }) { _, a -> a.ui.confirm?.kind == ConfirmKind.DELETE_IR }
        tap("ir.manage.close", irManage, canonical = { Action.Back }, check = closed<Overlay.IrManage>())
        engine("ir.naming")
        val pedalManage = ScreenId.CUSTOM_PEDAL_MANAGE
        tap("pedal.import", pedalManage, interaction = "file", canonical = { Action.PickFiles(PickKind.PEDAL_FILES) }) { b, a -> a.library.pedals.size > b.library.pedals.size }
        tap("pedal.rename", pedalManage, canonical = { Action.RequestRenamePedal(64) }) { _, a -> (a.ui.top as? Overlay.NameDialog)?.purpose == NamePurpose.RENAME_PEDAL }
        tap("pedal.delete", pedalManage, canonical = { Action.RequestDeletePedal(64) }) { _, a -> a.ui.confirm?.kind == ConfirmKind.DELETE_PEDAL }
        tap("pedal.manage.close", pedalManage, canonical = { Action.Back }, check = closed<Overlay.PedalManage>())
        engine("content.delete_fallback")

        // Packs
        val system = ScreenId.SETTINGS_SYSTEM
        tap("pack.export.open", system, canonical = { Action.Open(Overlay.PackExport) }, check = opened<Overlay.PackExport>())
        tap("pack.import.open", system, interaction = "file", canonical = { Action.PickFiles(PickKind.PACK_OPEN) }) { b, a ->
            b.ui.top !is Overlay.PackImport && a.ui.top is Overlay.PackImport && a.packImport != null
        }
        val export = ScreenId.PACK_EXPORT
        tap("pack.export.scope.everything", export, "export_sounds", interaction = "select", canonical = { Action.SetExportScope(ExportScope.EVERYTHING) }) { b, a -> b.packExport.scope != ExportScope.EVERYTHING && a.packExport.scope == ExportScope.EVERYTHING }
        tap("pack.export.scope.sounds", export, interaction = "select", canonical = { Action.SetExportScope(ExportScope.SOUNDS) }) { b, a -> b.packExport.scope != ExportScope.SOUNDS && a.packExport.scope == ExportScope.SOUNDS }
        tap("pack.export.scope.amp", export, interaction = "select", canonical = { Action.SetExportScope(ExportScope.AMP) }) { b, a -> b.packExport.scope != ExportScope.AMP && a.packExport.scope == ExportScope.AMP }
        tap("pack.export.sound_tick", export, "export_sounds", interaction = "toggle", canonical = { Action.ToggleExportSound("u:2") }, check = changed { it.packExport.ticked })
        tap("pack.export.amp_pick", export, "export_amp", interaction = "select", canonical = { Action.SetExportAmp("c:1") }, check = changed { it.packExport.ampId })
        view("pack.export.also", export, "export_sounds") { it.packExport.alsoIncluding(it.library).isNotEmpty() }
        tap("pack.export.save", export, interaction = "file", canonical = { Action.PickFiles(PickKind.PACK_SAVE) }) { b, a -> b.packExport.savedAs == null && a.packExport.savedAs != null }
        tap("pack.export.close", export, canonical = { Action.Back }, check = closed<Overlay.PackExport>())
        val import = ScreenId.PACK_IMPORT
        view("pack.import.rows", import, "pack_preview") { it.packImport?.rows?.isNotEmpty() == true }
        tap("pack.import.tick", import, "pack_preview", interaction = "toggle", canonical = { s -> Action.ToggleImportRow(s.packImport!!.rows.first().id) }, check = changed { s -> s.packImport?.rows?.map { it.ticked } })
        view("pack.import.outcome", import, "pack_conflict") { s -> s.packImport?.rows?.map { it.outcome }?.toSet()?.containsAll(listOf(PackOutcome.REPLACE, PackOutcome.KEEP_MINE, PackOutcome.SKIP)) == true }
        tap("pack.import.merge.overwrite", import, "pack_merge_add", interaction = "select", canonical = { Action.SetMerge(MergeMode.OVERWRITE) }) { b, a -> b.packImport?.merge != MergeMode.OVERWRITE && a.packImport?.merge == MergeMode.OVERWRITE }
        tap("pack.import.merge.add", import, "pack_preview", interaction = "select", canonical = { Action.SetMerge(MergeMode.ADD) }) { b, a -> b.packImport?.merge != MergeMode.ADD && a.packImport?.merge == MergeMode.ADD }
        tap("pack.import.merge.reset", import, "pack_preview", interaction = "select", canonical = { Action.SetMerge(MergeMode.RESET) }) { b, a -> b.packImport?.merge != MergeMode.RESET && a.packImport?.merge == MergeMode.RESET }
        tap("pack.import.machine", import, "pack_preview", interaction = "toggle", canonical = { s -> Action.SetRestoreMachine(!s.packImport!!.restoreMachine) }, check = changed { it.packImport?.restoreMachine })
        tap("pack.import.confirm", import, "pack_preview", canonical = { Action.ConfirmImport }) { b, a -> a.library.packBackups == b.library.packBackups + 1 }
        tap("pack.import.close", import, "pack_preview", canonical = { Action.Back }, check = closed<Overlay.PackImport>())
        view("pack.import.refused", import, "pack_refused") { it.packImport?.refused == true }
        view("pack.import.damaged", import, "pack_damaged") { it.packImport?.damaged == true }
        engine("pack.packbak")
        na("pack.plugin_rules", "plug-in-only rule; Android VoLum is a standalone app")

        // Settings
        val signal = ScreenId.SETTINGS_SIGNAL
        tap("settings.tab.signal", midiTab, interaction = "select", canonical = { Action.SetSettingsTab(SettingsTab.SIGNAL) }) { _, a -> (a.ui.top as? Overlay.Settings)?.tab == SettingsTab.SIGNAL }
        tap("settings.tab.midi", signal, interaction = "select", canonical = { Action.SetSettingsTab(SettingsTab.MIDI) }) { _, a -> (a.ui.top as? Overlay.Settings)?.tab == SettingsTab.MIDI }
        tap("settings.tab.system", signal, interaction = "select", canonical = { Action.SetSettingsTab(SettingsTab.SYSTEM) }) { _, a -> (a.ui.top as? Overlay.Settings)?.tab == SettingsTab.SYSTEM }
        tap("settings.close", signal, canonical = { Action.Back }, check = closed<Overlay.Settings>())
        tap("signal.calibration.level", signal, interaction = "text", text = "18", canonical = { Action.SetCalibrationDbu(18.0) }) { _, a -> a.signal.calibrationDbu == 18.0 }
        tap("signal.calibration.on", signal, interaction = "toggle", canonical = { s -> Action.SetCalibrate(!s.signal.calibrate) }, check = changed { it.signal.calibrate })
        view("signal.calibration.unavailable", signal) { it.amp?.inputLevelDbu == null }
        tap("signal.output.raw", signal, interaction = "select", canonical = { Action.SetOutputMode(0) }) { b, a -> b.signal.outputMode != 0 && a.signal.outputMode == 0 }
        tap("signal.output.normalized", signal, "output_raw", interaction = "select", canonical = { Action.SetOutputMode(1) }) { b, a -> b.signal.outputMode != 1 && a.signal.outputMode == 1 }
        tap("signal.output.calibrated", signal, "custom_levels", interaction = "select", canonical = { Action.SetOutputMode(2) }) { b, a -> b.signal.outputMode != 2 && a.signal.outputMode == 2 }
        tap("signal.perf.full", signal, "lite", interaction = "select", canonical = { Action.SetLite(false) }) { b, a -> b.signal.lite && !a.signal.lite }
        tap("signal.perf.lite", signal, interaction = "select", canonical = { Action.SetLite(true) }) { b, a -> !b.signal.lite && a.signal.lite }
        tap("signal.animate_art", signal, interaction = "toggle", canonical = { s -> Action.SetAnimateArt(!s.signal.animateArt) }, check = changed { it.signal.animateArt })
        tap("signal.audio_devices", signal, canonical = { Action.Open(Overlay.AudioDevices) }, check = opened<Overlay.AudioDevices>())
        na("system.shortcuts", "no keyboard on a phone stage rig; every shortcut's action has its own touch feature id")
        view("system.model_info", system)
        view("system.about.version", system)
        tap("system.about.manual", system, canonical = { Action.OpenLink("https://github.com/guitarlum/VoLum/blob/main/docs/user-guide.en.md") }) { b, a ->
            a.hostLog.size == b.hostLog.size + 1 && a.hostLog.last().startsWith("link:")
        }
        tap("system.update.auto", system, interaction = "toggle", canonical = { s -> Action.SetAutoUpdate(!s.update.auto) }, check = changed { it.update.auto })
        tap("system.update.check_now", system, canonical = { Action.CheckUpdatesNow }) { b, a -> a.update.checks == b.update.checks + 1 }
        view("system.update.badge", play, "update_available") { it.update.available != null }
        tap("system.update.link", system, "update_available", canonical = { Action.OpenLink("https://github.com/guitarlum/VoLum/releases/latest") }) { b, a ->
            a.hostLog.size == b.hostLog.size + 1 && a.hostLog.last().startsWith("link:")
        }
        tap("system.share_log", system, canonical = { Action.ShareLog }) { b, a -> a.hostLog.size == b.hostLog.size + 1 && a.hostLog.last().startsWith("share:") }
        val audio = ScreenId.SETTINGS_AUDIO
        tap("audio.input.device", audio, interaction = "select", canonical = { Action.SelectInput(FakeEngine.builtinMic.id) }, check = changed { it.audio.inputId })
        tap("audio.input.channel", audio, interaction = "select", canonical = { Action.SelectInputChannel(1) }, check = changed { it.audio.inputChannel })
        tap("audio.output.device", audio, interaction = "select", canonical = { Action.SelectOutput(FakeEngine.builtinSpeaker.id) }, check = changed { it.audio.outputId })
        tap("audio.sample_rate", audio, interaction = "select", canonical = { Action.SetSampleRate(44100) }, check = changed { it.audio.sampleRate })
        tap("audio.buffer", audio, interaction = "select", canonical = { Action.SetBuffer(256) }, check = changed { it.audio.bufferFrames })
        tap("audio.devices.close", audio, canonical = { Action.Back }, check = closed<Overlay.AudioDevices>())
        na("audio.driver", "Android has one low-latency path (AAudio); there is no driver to choose")
        val status = ScreenId.AUDIO_STATUS
        tap("audio.permission.grant", status, "mic_denied", canonical = { Action.RequestMicPermission }) { b, a -> b.audio.permission != Permission.GRANTED && a.audio.permission == Permission.GRANTED }
        tap("audio.permission.settings", status, "mic_denied_permanent", canonical = { Action.OpenAppSettings }) { b, a -> a.hostLog.size == b.hostLog.size + 1 && a.hostLog.last() == "settings:app" }
        view("audio.permission.denied", status, "mic_denied") { it.audio.permission != Permission.GRANTED }
        view("audio.no_interface", status, "no_interface") { it.audio.status == InterfaceStatus.NO_INTERFACE }
        tap("audio.no_interface.rescan", status, "no_interface", canonical = { Action.RescanAudio }) { b, a ->
            b.audio.status == InterfaceStatus.NO_INTERFACE && a.audio.status == InterfaceStatus.OK && a.audio.inputs.firstOrNull { it.id == a.audio.inputId }?.usb == true
        }
        tap("audio.no_interface.builtin", status, "no_interface", canonical = { Action.UseBuiltinInput }) { b, a ->
            b.audio.status == InterfaceStatus.NO_INTERFACE && a.audio.status == InterfaceStatus.OK && a.audio.inputId == FakeEngine.builtinMic.id
        }
        view("audio.unplugged", status, "interface_unplugged") { it.audio.status == InterfaceStatus.UNPLUGGED }
        tap("audio.unplugged.reconnect", status, "interface_unplugged", canonical = { Action.ReconnectInterface }) { b, a ->
            b.audio.status == InterfaceStatus.UNPLUGGED && a.audio.status == InterfaceStatus.OK
        }
        tap("audio.status.close", status, canonical = { Action.Back }, check = closed<Overlay.AudioStatus>())
        view("audio.input.indicator", play)

        // Not applicable on Android
        na("keys.everywhere", "no hardware keyboard on a phone rig; mapped to nav.view.*, nav.tuner.open, nav.metronome.open, nav.settings.open, presets.save.open, play.save and per-panel close controls")
        na("keys.build", "keyboard only; mapped to nav.section.*, amp.list.select, amp.channel, pre.card.*/post.card.*, dual.focus.*, amp.cab.*, *.on")
        na("keys.knob", "keyboard only; mapped to knob drag, amp.knob.exact and amp.knob.reset")
        na("keys.play", "keyboard only; mapped to play.sound.next/prev and play.stomp.*")
        na("daw.plugin", "Android VoLum is a standalone app; there is no plug-in host")
        na("daw.initial_state", "no plug-in instances on Android")
        na("daw.shared_library", "one app per device; nothing to share with")
        na("daw.b_key", "keyboard and DAW transport rule")
        na("daw.latency", "no host to compensate; audio.buffer covers latency")
        na("trouble.another_copy", "Android runs one task per app; a second copy cannot start")
        na("trouble.directsound_48k", "Windows driver issue; audio.sample_rate lists what the device offers")
        na("trouble.macos_au", "macOS AU only")
        na("trouble.macos_zip", "macOS install path only")
        na("files.locations", "Android keeps settings, library and log in app-private storage with no user path; Packs (pack.*) and system.share_log replace file access")
    }

    val all: List<FeatureSpec> = list.toList()
    val byId: Map<String, FeatureSpec> = all.associateBy { it.id }
    val ui: List<FeatureSpec> = all.filter { it.isUi }

    fun isKnown(id: String): Boolean = id in byId
}
