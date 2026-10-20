package com.volum.ui.b

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsPressedAsState
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxScope
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.BannerKind
import com.volum.core.ConfirmKind
import com.volum.core.InterfaceStatus
import com.volum.core.NamePurpose
import com.volum.core.Overlay
import com.volum.core.Permission
import com.volum.core.PickKind
import com.volum.core.Reducer
import com.volum.core.ScreenId
import com.volum.core.SettingsTab
import com.volum.core.View
import com.volum.core.VoLumState
import com.volum.design.components.CrossButton
import com.volum.design.components.HeaderToolFace
import com.volum.design.components.ModeToggleFace
import com.volum.design.components.VoLumBanner
import com.volum.design.components.VoLumBannerKind
import com.volum.design.components.VoLumBuildTip
import com.volum.design.components.VoLumButton
import com.volum.design.components.VoLumConfirmDialog
import com.volum.design.components.VoLumGlyph
import com.volum.design.components.VoLumLogo
import com.volum.design.components.cornerBrackets
import com.volum.design.components.hairlineFrame
import com.volum.design.components.insetWell
import com.volum.design.components.panelDepth
import com.volum.design.components.volumHeaderPlate
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumType
import com.volum.design.theme.withOpacity
import com.volum.harness.featureId
import com.volum.harness.screen
import kotlinx.coroutines.delay

/** Height of the header both surfaces share; its live tools (view switch, tuner) are this square. */
internal val HeaderHeight = 64.dp

/**
 * The header PLAY and BUILD share, right-anchored so every tool sits in the same
 * place on both: logo, the surface's own [middle], SAVE PRESET, the input chip,
 * then the desktop cluster (view switch, tuner, metronome, settings).
 */
@Composable
internal fun Header(s: VoLumState, middle: @Composable RowScope.() -> Unit) {
    val st = LocalStage.current
    val inPlay = s.ui.view == View.PLAY
    Row(
        Modifier.fillMaxWidth().height(HeaderHeight).volumHeaderPlate().padding(start = 12.dp, end = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        VoLumLogo(Modifier.padding(end = 4.dp), compact = true)
        middle()
        TextPad("SAVE PRESET", { st.go(Action.Open(Overlay.NameDialog(NamePurpose.SAVE_PRESET, Reducer.nameSuggestion(s)))) },
            Modifier.width(108.dp).height(48.dp), arrayOf(if (inPlay) "play.save" else "presets.save.open"), tone = Tone.PRIMARY)
        InputChip(s, Modifier.width(100.dp).height(52.dp))
        if (inPlay && !s.ui.buildTipSeen) VoLumBuildTip({ st.go(Action.SetView(View.BUILD)) }, Modifier.featureId("tip.build"))
        HeaderTool(if (inPlay) "BUILD" else "PLAY", { st.go(Action.SetView(if (inPlay) View.BUILD else View.PLAY)) }, 64.dp,
            arrayOf(if (inPlay) "nav.view.build" else "nav.view.play"), if (inPlay) "Switch to BUILD" else "Switch to PLAY") { pressed ->
            ModeToggleFace(inPlay, pressed, Modifier.fillMaxWidth().height(44.dp))
        }
        HeaderTool("TUNER", { st.go(Action.Open(Overlay.Tuner)) }, 64.dp, arrayOf("nav.tuner.open"), "Tuner") { pressed ->
            HeaderToolFace(VoLumGlyph.Tuner, pressed, Modifier.fillMaxWidth().height(44.dp))
        }
        HeaderTool("METRONOME", { st.go(Action.Open(Overlay.Metronome)) }, 58.dp, arrayOf("nav.metronome.open"), "Metronome") { pressed ->
            HeaderToolFace(VoLumGlyph.Metronome, pressed, Modifier.fillMaxWidth().height(44.dp), active = s.metronome.on)
        }
        val update = s.update.available
        HeaderTool(if (update != null) "UPDATE" else "SETTINGS", { st.go(Action.Open(Overlay.Settings(if (update != null) SettingsTab.SYSTEM else SettingsTab.SIGNAL))) },
            58.dp, if (update != null) arrayOf("nav.settings.open", "system.update.badge") else arrayOf("nav.settings.open"),
            if (update != null) "Settings: VoLum $update is available" else "Settings", captionInk = if (update != null) VoLumColors.Gold else VoLumColors.CreamDim) { pressed ->
            HeaderToolFace(VoLumGlyph.Gear, pressed, Modifier.fillMaxWidth().height(44.dp), badge = update != null)
        }
    }
}

/** A header tool: the `:design` face over a small caption naming it, one [width] x 64 dp target. */
@Composable
private fun HeaderTool(
    caption: String,
    onClick: () -> Unit,
    width: Dp,
    ids: Array<String>,
    description: String,
    captionInk: androidx.compose.ui.graphics.Color = VoLumColors.CreamDim,
    face: @Composable (pressed: Boolean) -> Unit,
) {
    val source = remember { MutableInteractionSource() }
    val pressed by source.collectIsPressedAsState()
    Column(
        Modifier.width(width).height(HeaderHeight)
            .clickable(source, indication = null, role = Role.Button, onClick = onClick)
            .semantics { contentDescription = description }
            .featureId(*ids),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        face(pressed)
        FitText(caption, VoLumType.label(8.sp, if (pressed) VoLumColors.TextBright else captionInk), Modifier.fillMaxWidth().height(14.dp).padding(horizontal = 2.dp))
    }
}

/** Input state at a glance: the interface in use, or what is wrong with it. Opens the audio status panel. */
@Composable
internal fun InputChip(s: VoLumState, modifier: Modifier = Modifier) {
    val st = LocalStage.current
    val problem = when {
        s.audio.permission != Permission.GRANTED -> "MIC BLOCKED"
        s.audio.status == InterfaceStatus.NO_INTERFACE -> "NO INTERFACE"
        s.audio.status == InterfaceStatus.UNPLUGGED -> "UNPLUGGED"
        else -> null
    }
    val name = s.audio.inputs.firstOrNull { it.id == s.audio.inputId }?.name ?: "NO INPUT"
    Pad({ st.go(Action.Open(Overlay.AudioStatus)) }, modifier, arrayOf("nav.audio_status.open", "audio.input.indicator"),
        tone = if (problem != null) Tone.DANGER else Tone.FRAMED, description = "Input: ${problem ?: name}") { _ ->
        Column(Modifier.fillMaxSize().padding(horizontal = 8.dp), verticalArrangement = Arrangement.Center) {
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                com.volum.design.components.VoLumLed(true, color = if (problem != null) VoLumColors.Danger else VoLumColors.Teal)
                FitText("INPUT", VoLumType.label(8.sp, if (problem != null) VoLumColors.Danger else VoLumColors.CreamDim), Modifier.weight(1f).height(12.dp), align = TextAlign.Start)
            }
            FitText(problem ?: name, VoLumType.label(11.sp, if (problem != null) VoLumColors.TextBright else VoLumColors.Cream), Modifier.fillMaxWidth().height(18.dp), align = TextAlign.Start)
        }
    }
}

/**
 * Every overlay: a framed panel over the dimmed stage (the desktop's modal frame),
 * title and actions on top, a banner under them when one is up, then the body.
 */
@Composable
internal fun Panel(
    s: VoLumState,
    screen: ScreenId,
    title: String,
    closeIds: Array<out String>,
    modifier: Modifier = Modifier,
    subtitle: String? = null,
    actions: @Composable RowScope.() -> Unit = {},
    body: @Composable BoxScope.() -> Unit,
) {
    val st = LocalStage.current
    Box(modifier.fillMaxSize().background(VoLumColors.Scrim).swallowTouches().screen(screen)) {
        Column(
            Modifier.fillMaxSize().padding(horizontal = 18.dp, vertical = 8.dp)
                .panelDepth(6.dp).hairlineFrame(VoLumColors.GoldDim.withOpacity(0.45f), radius = 6.dp)
                .cornerBrackets(VoLumColors.Corner, 16.dp, 6.dp)
                .padding(start = 18.dp, end = 10.dp, top = 4.dp, bottom = 10.dp),
        ) {
            Row(Modifier.fillMaxWidth().height(56.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Column(Modifier.weight(1f)) {
                    FitText(title, VoLumType.display(24.sp, VoLumColors.Gold), Modifier.fillMaxWidth().height(30.dp), align = TextAlign.Start)
                    if (subtitle != null) FitText(subtitle, VoLumType.body(11.sp, VoLumColors.CreamDim), Modifier.fillMaxWidth().height(16.dp), align = TextAlign.Start)
                }
                actions()
                CrossButton({ st.go(Action.Back) }, Modifier.size(56.dp, 48.dp).ids(closeIds).semantics { contentDescription = "Close" })
            }
            if (s.ui.top != null && s.screen() == screen) BannerSlot(s)
            Box(Modifier.weight(1f).fillMaxWidth().padding(top = 4.dp)) { body() }
        }
    }
}

/** What the error in [s] lets you do next, and the feature id of the control that already does it elsewhere. */
private data class Recovery(val label: String, val ids: Array<String>, val action: Action)

private fun recoveryFor(s: VoLumState): Recovery? {
    if (s.ui.banner?.kind != BannerKind.ERROR) return null
    return when (s.ui.top) {
        Overlay.IrManage -> Recovery("TRY ANOTHER FILE", arrayOf("ir.import"), Action.PickFiles(PickKind.IR_FILES))
        Overlay.PedalManage -> Recovery("TRY ANOTHER FILE", arrayOf("pedal.import"), Action.PickFiles(PickKind.PEDAL_FILES))
        Overlay.CustomAmpBuilder -> Recovery("TRY OTHER FILES", arrayOf("builder.add_files"), Action.PickFiles(PickKind.NAM_AMP_FILES))
        is Overlay.Settings, Overlay.PackImport -> Recovery("CHOOSE ANOTHER PACK", arrayOf("pack.import.open"), Action.PickFiles(PickKind.PACK_OPEN))
        null -> if (s.ui.view == View.BUILD) Recovery("CHOOSE ANOTHER AMP", emptyArray(), Action.Open(Overlay.AmpPicker)) else null
        else -> null
    }
}

/** The error / info strip, in the flow under the header or a panel title so it never covers a control; errors stay until dismissed. */
@Composable
internal fun BannerSlot(s: VoLumState, modifier: Modifier = Modifier) {
    val st = LocalStage.current
    val b = s.ui.banner
    val error = b?.kind == BannerKind.ERROR
    val recovery = recoveryFor(s)
    Row(modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        VoLumBanner(
            b?.message, if (error) VoLumBannerKind.ERROR else VoLumBannerKind.INFO, { st.go(Action.DismissBanner) },
            Modifier.weight(1f).padding(vertical = 3.dp).then(if (error) Modifier.featureId("banner.error") else Modifier),
            dismissModifier = Modifier.featureId("banner.dismiss"),
        )
        if (b != null && recovery != null) {
            VoLumButton(recovery.label, { st.go(Action.DismissBanner); st.go(recovery.action) }, Modifier.height(48.dp).ids(recovery.ids), primary = true)
        }
    }
}

/** Ask-first dialog for deletes and overwrites (`:design` confirm), over whatever asked. */
@Composable
internal fun ConfirmLayer(s: VoLumState) {
    val c = s.ui.confirm ?: return
    val st = LocalStage.current
    val (title, verb) = when (c.kind) {
        ConfirmKind.OVERWRITE_PRESET -> "Overwrite preset" to "Overwrite"
        ConfirmKind.DELETE_PRESET -> "Delete preset" to "Delete"
        ConfirmKind.DELETE_AMP -> "Delete custom amp" to "Delete"
        ConfirmKind.DELETE_IR -> "Delete IR" to "Delete"
        ConfirmKind.DELETE_PEDAL -> "Delete pedal" to "Delete"
    }
    val tail = if (c.kind == ConfirmKind.OVERWRITE_PRESET) "The saved sound is replaced." else "This cannot be undone."
    Box(Modifier.fillMaxSize().screen(s.screen())) {
        VoLumConfirmDialog(title, "${c.message} $tail", verb, { st.go(Action.AcceptConfirm) }, { st.go(Action.CancelConfirm) },
            Modifier.featureId("dialog.confirm.message"), confirmModifier = Modifier.featureId("dialog.confirm.accept"),
            cancelModifier = Modifier.featureId("dialog.confirm.cancel"))
    }
}

/** After CLEAR on PLAY: what went, and a big UNDO that puts it back on its program for six seconds. */
@Composable
internal fun BoxScope.UndoToast(s: VoLumState) {
    val st = LocalStage.current
    val c = st.cleared.value ?: return
    LaunchedEffect(c) {
        delay(6000)
        if (st.cleared.value == c) st.cleared.value = null
    }
    if (s.ui.top != null || s.ui.view != View.PLAY) return
    Row(
        Modifier.align(Alignment.BottomCenter).padding(bottom = 84.dp).widthIn(max = 520.dp)
            .insetWell(3.dp).hairlineFrame(VoLumColors.Gold.withOpacity(0.6f), radius = 3.dp).swallowTouches()
            .padding(start = 16.dp, end = 6.dp, top = 6.dp, bottom = 6.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        FitText("Cleared ${twoDigit(c.program)} · ${c.name}", VoLumType.label(13.sp, VoLumColors.Cream), Modifier.weight(1f, fill = false), align = TextAlign.Start)
        VoLumButton("UNDO", {
            st.go(Action.AssignSound(c.program, c.sound.presetId))
            st.cleared.value = null
        }, Modifier.height(56.dp).width(120.dp), primary = true)
    }
}

internal fun twoDigit(n: Int): String = com.volum.design.components.twoDigits(n)
