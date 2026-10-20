package com.volum.ui.c

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.TimeSig
import com.volum.design.art.rememberArtFrames
import com.volum.design.components.VoLumLed
import com.volum.design.theme.VText
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumType
import com.volum.design.theme.withOpacity
import kotlin.math.abs
import kotlin.math.cos
import kotlin.math.sin

/**
 * The tuner sits on the art: the instrument steps back, leaving a big note, a cents
 * arc in the VoLum inks, and CLOSE. The dial sits right of the art's centre so its arc
 * clears the sun's corona.
 */
@Composable
internal fun Tuner(ui: Ui, modifier: Modifier) {
    val t by ui.tel.collectAsState()
    val cents = t.tunerCents
    val ink = when {
        abs(cents) < 3 -> VoLumColors.Teal
        abs(cents) < 15 -> VoLumColors.Gold
        else -> VoLumColors.Amber
    }
    val verdict = when {
        abs(cents) < 3 -> "IN TUNE"
        cents < 0 -> "FLAT"
        else -> "SHARP"
    }
    Box(modifier.background(Glass.Veil), contentAlignment = Alignment.Center) {
        Row(Modifier.fillMaxSize().padding(horizontal = 28.dp, vertical = 8.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(24.dp)) {
            Column(Modifier.width(220.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                VText("Tuner", style = VoLumType.display(30.sp, VoLumColors.Gold))
                Row(Modifier.ids("tuner.muted"), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    VoLumLed(true, color = VoLumColors.Danger)
                    VText("OUTPUT MUTED", style = VoLumType.label(12.sp, VoLumColors.Danger))
                }
                Note("Muted while the tuner is open, so you can tune on stage in silence.", maxLines = 3)
                Btn("Close tuner", listOf("tuner.close"), { ui.back() }, Modifier.fillMaxWidth().height(60.dp), primary = true)
            }
            Spacer(Modifier.weight(0.35f))
            Box(
                Modifier.weight(1f).fillMaxHeight().drawBehind {
                    val r = minOf(size.height * 0.62f, size.width * 0.42f)
                    val c = Offset(size.width / 2, size.height / 2 + r * 0.5f)
                    val sweep = 100f
                    drawArc(VoLumColors.Frame, 270f - sweep / 2, sweep, false, Offset(c.x - r, c.y - r), Size(r * 2, r * 2), style = Stroke(3.dp.toPx()))
                    drawArc(VoLumColors.Teal.withOpacity(0.35f), 270f - 3f, 6f, false, Offset(c.x - r, c.y - r), Size(r * 2, r * 2), style = Stroke(9.dp.toPx()))
                    for (i in -5..5) {
                        val a = Math.toRadians(270.0 + i * sweep / 10.0)
                        val inner = if (i == 0) r * 0.84f else r * 0.92f
                        drawLine(if (i == 0) VoLumColors.Gold else VoLumColors.GoldDim.withOpacity(0.6f), c + Offset((cos(a) * inner).toFloat(), (sin(a) * inner).toFloat()), c + Offset((cos(a) * r).toFloat(), (sin(a) * r).toFloat()), 2.dp.toPx())
                    }
                    val a = Math.toRadians(270.0 + (cents.coerceIn(-50.0, 50.0) / 50.0) * sweep / 2)
                    val dx = cos(a).toFloat()
                    val dy = sin(a).toFloat()
                    drawLine(ink.withOpacity(0.3f), c + Offset(dx * r * 0.70f, dy * r * 0.70f), c + Offset(dx * r * 1.06f, dy * r * 1.06f), 12.dp.toPx(), cap = StrokeCap.Round)
                    drawLine(ink, c + Offset(dx * r * 0.72f, dy * r * 0.72f), c + Offset(dx * r * 1.04f, dy * r * 1.04f), 5.dp.toPx(), cap = StrokeCap.Round)
                },
                contentAlignment = Alignment.Center,
            ) {
                Column(Modifier.padding(top = 24.dp), horizontalAlignment = Alignment.CenterHorizontally) {
                    Row(verticalAlignment = Alignment.Bottom) {
                        VText(t.tunerNote, Modifier.ids("tuner.note"), style = VoLumType.display(84.sp, ink))
                        VText("${t.tunerOctave}", Modifier.padding(bottom = 14.dp), style = VoLumType.display(30.sp, VoLumColors.CreamDim))
                    }
                    VText(verdict, style = VoLumType.label(13.sp, ink))
                    VText("%+.0f cents  ·  %.1f Hz".format(cents, t.tunerHz), Modifier.ids("tuner.cents"), style = VoLumType.label(14.sp, VoLumColors.TextBright))
                }
            }
        }
    }
}

/** The metronome hangs under its glyph: tempo shown once, big beat lamps you can follow at arm's length. */
@Composable
internal fun Metronome(ui: Ui, modifier: Modifier) {
    val m = ui.s.metronome
    PanelFrame(ui, "Metronome", "metronome.close", { ui.back() }, modifier, actions = {
        Toggle("Click", m.on, listOf("metronome.on"), { ui.d(Action.SetMetronomeOn(!m.on)) })
    }) {
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(14.dp)) {
            Column(Modifier.width(110.dp), horizontalAlignment = Alignment.CenterHorizontally) {
                VText("%.0f".format(m.bpm), style = VoLumType.value(34.sp, VoLumColors.Gold))
                VText("BPM", style = VoLumType.label(10.sp, VoLumColors.CreamDim))
            }
            Knob("TEMPO", m.bpm, 30.0, 300.0, listOf("metronome.bpm"), { "%.0f".format(it) }, { it.toDoubleOrNull() }, { ui.d(Action.SetBpm(Math.round(it).toDouble())) }, { ui.d(Action.SetBpm(120.0)) }, readout = false)
            Knob("VOLUME", m.volume, 0.0, 1.0, listOf("metronome.volume"), { "%.0f%%".format(it * 100) }, { it.toDoubleOrNull()?.let { v -> if (v > 1) v / 100 else v } }, { ui.d(Action.SetMetronomeVolume(it)) }, { ui.d(Action.SetMetronomeVolume(0.7)) })
            Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                Caption("Time signature")
                Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                    TimeSig.entries.forEach { sig ->
                        Brass(sig.label, m.sig == sig, listOf("metronome.sig." + sig.label.replace('/', '_')), { ui.d(Action.SetTimeSig(sig)) })
                    }
                }
                Beats(ui, m.sig.beats)
            }
        }
        Note("Delay and tremolo TEMPO SYNC follow this tempo.", size = 11.sp, maxLines = 1)
    }
}

/** One cue each: an amber fill is the beat sounding now, a heavy gold ring is the accented downbeat. */
@Composable
private fun Beats(ui: Ui, beats: Int) {
    val frames = rememberArtFrames()
    Row(Modifier.ids("metronome.beat"), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        repeat(beats) { i ->
            Spacer(
                Modifier.size(34.dp).drawBehind {
                    frames.longValue
                    val t = ui.tel.value
                    val on = t.beat == i
                    val k = if (on) (1f - t.beatPhase.toFloat()).coerceIn(0.65f, 1f) else 0f
                    val r = size.minDimension / 2
                    drawCircle(VoLumColors.WellDark, r)
                    if (on) drawCircle(VoLumColors.Amber.withOpacity(k), r - 3.dp.toPx())
                    if (i == 0) drawCircle(VoLumColors.Gold, r - 1.25.dp.toPx(), style = Stroke(2.5.dp.toPx()))
                    else drawCircle(VoLumColors.Frame, r - 0.5.dp.toPx(), style = Stroke(1.dp.toPx()))
                },
            )
        }
    }
}
