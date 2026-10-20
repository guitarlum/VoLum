package com.volum.ui.c

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.text.BasicText
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
import com.volum.core.Action
import com.volum.core.TimeSig
import kotlin.math.abs
import kotlin.math.cos
import kotlin.math.sin

/** The tuner sits on the art itself: a see-through scrim, a big note and a cents arc. */
@Composable
internal fun Tuner(ui: Ui, modifier: Modifier) {
    Box(modifier.background(Look.ground.copy(alpha = 0.25f)), contentAlignment = Alignment.Center) {
        Row(Modifier.fillMaxWidth().padding(horizontal = 24.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(24.dp)) {
            Column(Modifier.width(200.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                Caption("Tuner")
                Row(Modifier.ids("tuner.muted"), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Led(true, 10.dp, Look.warn)
                    BasicText("OUTPUT MUTED", style = labelStyle(11f, Look.warn))
                }
                Note("Muted while the tuner is open so you can tune on stage in silence.", maxLines = 3)
                Btn("CLOSE TUNER", listOf("tuner.close"), { ui.back() }, strong = true, modifier = Modifier.fillMaxWidth())
            }
            TunerReadout(ui, Modifier.weight(1f).height(300.dp))
        }
    }
}

@Composable
private fun TunerReadout(ui: Ui, modifier: Modifier) {
    val t by ui.tel.collectAsState()
    val cents = t.tunerCents
    val inTune = abs(cents) < 3
    Box(
        modifier.drawBehind {
            val c = Offset(size.width / 2, size.height * 0.78f)
            val r = size.height * 0.62f
            val sweep = 100f
            drawArc(Look.line, 270f - sweep / 2, sweep, false, Offset(c.x - r, c.y - r), Size(r * 2, r * 2), style = Stroke(3.dp.toPx()))
            for (i in -5..5) {
                val a = Math.toRadians(270.0 + i * sweep / 10.0)
                val inner = if (i == 0) r * 0.86f else r * 0.92f
                drawLine(if (i == 0) Look.pick else Look.lineHi, c + Offset((cos(a) * inner).toFloat(), (sin(a) * inner).toFloat()), c + Offset((cos(a) * r).toFloat(), (sin(a) * r).toFloat()), 2.dp.toPx())
            }
            val a = Math.toRadians(270.0 + (cents.coerceIn(-50.0, 50.0) / 50.0) * sweep / 2)
            val dx = cos(a).toFloat()
            val dy = sin(a).toFloat()
            drawLine(if (inTune) Look.ok else Look.glow, c + Offset(dx * r * 0.74f, dy * r * 0.74f), c + Offset(dx * r * 1.04f, dy * r * 1.04f), 5.dp.toPx(), cap = StrokeCap.Round)
        },
        contentAlignment = Alignment.Center,
    ) {
        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            Row(verticalAlignment = Alignment.Bottom) {
                BasicText(t.tunerNote, style = displayStyle(84f, if (inTune) Look.ok else Look.text), modifier = Modifier.ids("tuner.note"))
                BasicText("${t.tunerOctave}", style = displayStyle(28f, Look.dim))
            }
            BasicText("%+.0f cents · %.1f Hz".format(cents, t.tunerHz), style = techStyle(14f, if (inTune) Look.ok else Look.text), modifier = Modifier.ids("tuner.cents"))
        }
    }
}

@Composable
internal fun Metronome(ui: Ui, modifier: Modifier) {
    val m = ui.s.metronome
    PanelFrame("Metronome", null, "metronome.close", { ui.back() }, modifier, actions = {
        Switch(if (m.on) "CLICK ON" else "CLICK OFF", m.on, listOf("metronome.on"), { ui.d(Action.SetMetronomeOn(!m.on)) })
    }) {
        Row(Modifier.fillMaxWidth().fillMaxHeight(), horizontalArrangement = Arrangement.spacedBy(24.dp)) {
            Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                Row(verticalAlignment = Alignment.Bottom, horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                    BasicText("%.0f".format(m.bpm), style = displayStyle(46f))
                    BasicText("BPM", style = labelStyle(11f), modifier = Modifier.padding(bottom = 10.dp))
                    Fill()
                    Btn("−1", emptyList(), { ui.d(Action.SetBpm(m.bpm - 1)) })
                    Btn("+1", emptyList(), { ui.d(Action.SetBpm(m.bpm + 1)) })
                }
                Fader("Tempo", m.bpm, 30.0, 300.0, listOf("metronome.bpm"), { "%.0f BPM".format(it) }, { ui.d(Action.SetBpm(it)) }, Modifier.fillMaxWidth())
                Fader("Click volume", m.volume, 0.0, 1.0, listOf("metronome.volume"), { "%.0f%%".format(it * 100) }, { ui.d(Action.SetMetronomeVolume(it)) }, Modifier.fillMaxWidth())
            }
            Column(Modifier.width(330.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                Caption("Time signature")
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    TimeSig.entries.forEach { sig ->
                        val suffix = sig.label.replace('/', '_')
                        Choice(sig.label, m.sig == sig, listOf("metronome.sig.$suffix"), { ui.d(Action.SetTimeSig(sig)) }, minWidth = 56.dp)
                    }
                }
                Caption("Beat")
                Beats(ui, m.sig.beats)
                Note("Delay and tremolo TEMPO SYNC follow this tempo.")
            }
        }
    }
}

@Composable
private fun Beats(ui: Ui, beats: Int) {
    val t by ui.tel.collectAsState()
    Row(Modifier.ids("metronome.beat"), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        repeat(beats) { i ->
            Box(
                Modifier.size(28.dp).drawBehind {
                    val on = t.beat == i
                    val accent = i == 0
                    val k = if (on) (1f - t.beatPhase.toFloat()).coerceIn(0.3f, 1f) else 0f
                    drawCircle(Look.raised, size.minDimension / 2)
                    if (on) drawCircle((if (accent) Look.pick else Look.glow).copy(alpha = k), size.minDimension / 2)
                    drawCircle(if (accent) Look.lineHi else Look.line, size.minDimension / 2, style = Stroke(1.dp.toPx()))
                },
            )
        }
    }
}
