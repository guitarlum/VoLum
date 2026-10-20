package com.volum.core

/** One timed step of a scripted interaction: dispatch [action] and/or pin the art energy. */
data class MotionStep(val atMs: Long, val action: Action? = null, val energy: Double? = null)

/**
 * A scripted interaction the box records with `screenrecord` and turns into a
 * contact sheet. The first four mirror the desktop reference sheets in
 * `.scratch/android-shootout/reference/` (BUILD hero idle, PLAY art at energy
 * 0 / 0.5 / 1, a pedal toggle, a section switch).
 */
data class MotionScript(
    val id: String,
    val screen: ScreenId,
    val fixture: String,
    val durationMs: Long,
    val steps: List<MotionStep>,
    val scope: Scope = Scope.CANDIDATE,
    val description: String,
)

object MotionRegistry {
    private fun energyRamp() = listOf(MotionStep(0, energy = 0.0), MotionStep(2500, energy = 0.5), MotionStep(5000, energy = 1.0))

    val all: List<MotionScript> = listOf(
        MotionScript("build_idle", ScreenId.BUILD_AMP, "default", 6000, listOf(MotionStep(0, energy = 0.0)),
            description = "BUILD hero art idle"),
        MotionScript("play_energy", ScreenId.PLAY, "default", 7500, energyRamp(),
            description = "PLAY art at energy 0, 0.5 and 1 (2.5 s each)"),
        MotionScript("pedal_toggle", ScreenId.BUILD_PRE, "pre_nam1", 6000, listOf(
            MotionStep(1500, Action.ToggleParam(Param.NAM1_ON)),
            MotionStep(3500, Action.ToggleParam(Param.NAM1_ON)),
        ), description = "NAM 1 pedal toggled off and on"),
        MotionScript("section_switch", ScreenId.BUILD_PRE, "default", 6000, listOf(
            MotionStep(1500, Action.SetSection(Section.AMP)),
            MotionStep(3500, Action.SetSection(Section.POST)),
        ), description = "PRE to AMP to POST"),
        MotionScript("view_switch", ScreenId.PLAY, "default", 5000, listOf(
            MotionStep(1200, Action.SetView(View.BUILD)),
            MotionStep(3200, Action.SetView(View.PLAY)),
        ), description = "PLAY to BUILD and back"),
        MotionScript("sound_recall", ScreenId.PLAY, "default", 5000, listOf(
            MotionStep(1000, Action.StepSound(1)),
            MotionStep(2500, Action.StepSound(1)),
            MotionStep(4000, Action.RecallSound(5)),
        ), description = "stepping through Sounds on PLAY"),
        MotionScript("knob_sweep", ScreenId.BUILD_AMP, "default", 5000, (0..8).map { i ->
            MotionStep(800L + i * 400, Action.SetParam(Param.BASS, if (i < 5) 5.0 + i else 9.0 - (i - 4) * 1.5))
        }, description = "AMP BASS swept up and back"),
        MotionScript("overlays", ScreenId.BUILD_AMP, "default", 7000, listOf(
            MotionStep(1000, Action.Open(Overlay.PresetMenu)),
            MotionStep(2500, Action.Back),
            MotionStep(3200, Action.Open(Overlay.Tuner)),
            MotionStep(4700, Action.Back),
            MotionStep(5300, Action.Open(Overlay.Settings(SettingsTab.SIGNAL))),
            MotionStep(6200, Action.SetSettingsTab(SettingsTab.MIDI)),
        ), description = "preset menu, tuner and settings opening and closing"),
        MotionScript("design_hero", ScreenId.DESIGN_GALLERY, "default", 6000, listOf(MotionStep(0, energy = 0.0)),
            scope = Scope.DESIGN, description = "design gallery hero art idle"),
        MotionScript("design_play_energy", ScreenId.DESIGN_GALLERY, "default", 7500, energyRamp(),
            scope = Scope.DESIGN, description = "design gallery PLAY art at energy 0, 0.5 and 1"),
        MotionScript("design_pedal_toggle", ScreenId.DESIGN_GALLERY, "pre_nam1", 6000, listOf(
            MotionStep(1500, Action.ToggleParam(Param.NAM1_ON)),
            MotionStep(3500, Action.ToggleParam(Param.NAM1_ON)),
        ), scope = Scope.DESIGN, description = "design gallery pedal toggle"),
    )

    private val byId = all.associateBy { it.id }
    fun spec(id: String): MotionScript = byId[id] ?: error("unknown motion script '$id'")
    fun inScope(scope: Scope): List<MotionScript> = all.filter { it.scope == scope }
}
