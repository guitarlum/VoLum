package com.volum.ui.b

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
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
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicText
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.BannerKind
import com.volum.core.InterfaceStatus
import com.volum.core.Overlay
import com.volum.core.Permission
import com.volum.core.ScreenId
import com.volum.core.SettingsTab
import com.volum.core.View
import com.volum.core.VoLumState
import com.volum.harness.featureId
import com.volum.harness.screen

/** Header shared by PLAY and BUILD; [middle] holds the PLAY sound actions or the BUILD preset bar. */
@Composable
internal fun TopBar(s: VoLumState, tuner: Boolean = true, middle: @Composable RowScope.() -> Unit) {
    val st = LocalStage.current
    Row(
        Modifier.fillMaxWidth().height(56.dp).padding(horizontal = 8.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        Column(Modifier.width(72.dp)) {
            FitText("VoLum", Type.display(24.sp), Modifier.fillMaxWidth().height(28.dp), align = TextAlign.Start)
            FitText("NAM PLAYER", Type.label(8.sp), Modifier.fillMaxWidth().height(12.dp), align = TextAlign.Start)
        }
        Row(Modifier.width(132.dp).height(48.dp), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
            TextPad("PLAY", { st.go(Action.SetView(View.PLAY)) }, Modifier.weight(1f).height(48.dp), arrayOf("nav.view.play"),
                selected = s.ui.view == View.PLAY, sel = Sel.AMBER_PICKER, role = Role.Tab)
            TextPad("BUILD", { st.go(Action.SetView(View.BUILD)) }, Modifier.weight(1f).height(48.dp), arrayOf("nav.view.build"),
                selected = s.ui.view == View.BUILD, sel = Sel.AMBER_PICKER, role = Role.Tab)
        }
        middle()
        InputChip(s, Modifier.width(100.dp).height(48.dp))
        if (tuner) TextPad("TUNER", { st.go(Action.Open(Overlay.Tuner)) }, Modifier.width(72.dp).height(48.dp), arrayOf("nav.tuner.open"), style = Type.label(13.sp, Ink.hot))
        Pad({ st.go(Action.Open(Overlay.Metronome)) }, Modifier.width(56.dp).height(48.dp), arrayOf("nav.metronome.open"), description = "Metronome") {
            MetronomeGlyph(Modifier.size(24.dp))
        }
        Pad({ st.go(Action.Open(Overlay.Settings(SettingsTab.SIGNAL))) }, Modifier.width(56.dp).height(48.dp), arrayOf("nav.settings.open"), description = "Settings") {
            GearGlyph(Modifier.size(24.dp))
            if (s.update.available != null) {
                Box(Modifier.align(Alignment.TopEnd).offset((-6).dp, 6.dp).size(10.dp).clip(RoundedCornerShape(5.dp)).background(Ink.hot).featureId("system.update.badge"))
            }
        }
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
        fill = if (problem != null) Ink.alert.copy(alpha = 0.35f) else Ink.raised, description = "Input: ${problem ?: name}") {
        Column(Modifier.fillMaxSize().padding(horizontal = 8.dp), verticalArrangement = Arrangement.Center) {
            FitText("IN", Type.label(9.sp, if (problem != null) Ink.alert else Ink.dim), Modifier.fillMaxWidth().height(14.dp), align = TextAlign.Start)
            FitText(problem ?: name, Type.label(11.sp, Ink.text), Modifier.fillMaxWidth().height(20.dp), align = TextAlign.Start)
        }
    }
}

@Composable
private fun GearGlyph(modifier: Modifier) = Canvas(modifier) {
    val c = Offset(size.width / 2, size.height / 2)
    for (i in 0 until 8) {
        val a = Math.toRadians(i * 45.0)
        drawLine(Ink.text, c, Offset(c.x + (size.width / 2) * Math.cos(a).toFloat(), c.y + (size.height / 2) * Math.sin(a).toFloat()), 3.dp.toPx())
    }
    drawCircle(Ink.text, size.minDimension * 0.33f, c)
    drawCircle(Ink.raised, size.minDimension * 0.15f, c)
}

@Composable
private fun MetronomeGlyph(modifier: Modifier) = Canvas(modifier) {
    val w = size.width
    val h = size.height
    drawLine(Ink.text, Offset(w * 0.3f, h), Offset(w * 0.45f, 0f), 2.dp.toPx())
    drawLine(Ink.text, Offset(w * 0.7f, h), Offset(w * 0.55f, 0f), 2.dp.toPx())
    drawLine(Ink.text, Offset(w * 0.25f, h), Offset(w * 0.75f, h), 2.dp.toPx())
    drawLine(Ink.hot, Offset(w * 0.5f, h * 0.8f), Offset(w * 0.75f, h * 0.2f), 2.dp.toPx())
}

/** Full-screen panel every overlay sits in: title, header actions, a large close target. */
@Composable
internal fun Panel(
    screen: ScreenId,
    title: String,
    closeIds: Array<out String>,
    modifier: Modifier = Modifier,
    subtitle: String? = null,
    actions: @Composable RowScope.() -> Unit = {},
    body: @Composable BoxScope.() -> Unit,
) {
    val st = LocalStage.current
    Column(
        modifier.fillMaxSize().background(Ink.ground).swallowTouches().screen(screen).brackets().padding(horizontal = 12.dp, vertical = 6.dp),
    ) {
        Row(Modifier.fillMaxWidth().height(56.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Column(Modifier.weight(1f)) {
                FitText(title, Type.display(24.sp), Modifier.fillMaxWidth().height(32.dp), align = TextAlign.Start)
                if (subtitle != null) FitText(subtitle, Type.label(11.sp), Modifier.fillMaxWidth().height(16.dp), align = TextAlign.Start)
            }
            actions()
            Pad({ st.go(Action.Back) }, Modifier.width(64.dp).height(48.dp), closeIds, description = "Close") {
                BasicText("✕", style = Type.display(22.sp))
            }
        }
        Box(Modifier.weight(1f).fillMaxWidth().padding(top = 6.dp, bottom = 4.dp)) { body() }
    }
}

/** Error / info banner across the top; errors stay until dismissed. */
@Composable
internal fun BannerStrip(s: VoLumState) {
    val b = s.ui.banner ?: return
    val st = LocalStage.current
    val error = b.kind == BannerKind.ERROR
    Row(
        Modifier.fillMaxWidth().padding(horizontal = 160.dp, vertical = 4.dp).height(56.dp)
            .clip(RoundedCornerShape(8.dp)).background(if (error) Ink.alert else Ink.raised).swallowTouches().padding(start = 16.dp, end = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Box(Modifier.weight(1f).then(if (error) Modifier.featureId("banner.error") else Modifier)) {
            FitText(b.message, Type.value(14.sp, Ink.hot), Modifier.fillMaxWidth(), align = TextAlign.Start)
        }
        TextPad("DISMISS", { st.go(Action.DismissBanner) }, Modifier.width(104.dp).height(48.dp), arrayOf("banner.dismiss"), fill = Ink.ground.copy(alpha = 0.4f))
    }
}

/** Ask-first dialog for deletes and overwrites; covers whatever asked. */
@Composable
internal fun ConfirmLayer(s: VoLumState) {
    val c = s.ui.confirm ?: return
    val st = LocalStage.current
    Box(Modifier.fillMaxSize().background(Ink.scrim).swallowTouches().screen(s.screen()), contentAlignment = Alignment.Center) {
        Column(
            Modifier.widthIn(max = 520.dp).fillMaxWidth(0.6f).clip(RoundedCornerShape(12.dp)).background(Ink.panel).brackets().padding(20.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp),
        ) {
            Box(Modifier.fillMaxWidth().featureId("dialog.confirm.message")) {
                FitText(c.message, Type.display(22.sp), Modifier.fillMaxWidth())
            }
            FitText("This cannot be undone.", Type.value(13.sp, Ink.dim), Modifier.fillMaxWidth().height(20.dp))
            Row(Modifier.fillMaxWidth().height(64.dp), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                TextPad("CANCEL", { st.go(Action.CancelConfirm) }, Modifier.weight(1f).fillMaxHeight(), arrayOf("dialog.confirm.cancel"))
                TextPad(confirmVerb(c.kind), { st.go(Action.AcceptConfirm) }, Modifier.weight(1f).fillMaxHeight(), arrayOf("dialog.confirm.accept"),
                    fill = Ink.alert.copy(alpha = 0.6f), style = Type.label(14.sp, Ink.hot))
            }
        }
    }
}

private fun confirmVerb(k: com.volum.core.ConfirmKind) = if (k == com.volum.core.ConfirmKind.OVERWRITE_PRESET) "OVERWRITE" else "DELETE"
