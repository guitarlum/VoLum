package com.volum.ui.a

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.runtime.Composable
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.TimeSig
import kotlin.math.abs
import kotlin.math.roundToInt

/** Full-screen tuner: the note, a cents needle drawn per frame, and the mute it implies on stage. */
@Composable
internal fun Tuner(c: Ctx) {
    val note by remember(c.tel) { derivedStateOf { c.tel.value.tunerNote + c.tel.value.tunerOctave } }
    val cents by remember(c.tel) { derivedStateOf { c.tel.value.tunerCents.roundToInt() } }
    Sheet("Tuner", listOf("tuner.close"), c) {
        Column(Modifier.fillMaxWidth().weight(1f), horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(10.dp)) {
            T(note, Type.display(64.sp), Modifier.ids("tuner.note"), align = TextAlign.Center)
            Canvas(Modifier.width(560.dp).height(44.dp).ids("tuner.cents")) {
                val mid = size.width / 2
                for (i in -5..5) {
                    val x = mid + i * size.width / 10f
                    drawLine(if (i == 0) Ink.text else Ink.line, Offset(x, size.height * (if (i == 0) 0.05f else 0.35f)), Offset(x, size.height * 0.95f), 2.dp.toPx())
                }
                val t = c.tel.value.tunerCents.toFloat().coerceIn(-50f, 50f) / 50f
                val inTune = abs(t) < 0.1f
                drawLine(if (inTune) Ink.selected else Ink.dim, Offset(mid + t * mid, 0f), Offset(mid + t * mid, size.height), 4.dp.toPx(), StrokeCap.Round)
            }
            T((if (cents > 0) "+" else "") + "$cents cents", Type.value(14.sp), align = TextAlign.Center)
            T("OUTPUT MUTED WHILE TUNING", Type.label(11.sp, Ink.text), Modifier.ids("tuner.muted"))
            Chip("DONE", { c.d(Action.Back) }, Modifier.width(200.dp), listOf("tuner.close"), selected = true)
        }
    }
}

@Composable
internal fun Metronome(c: Ctx) {
    val m = c.s.metronome
    Sheet("Metronome", listOf("metronome.close"), c, width = 620.dp, actions = {
        Switch(if (m.on) "ON" else "OFF", m.on, { c.d(Action.SetMetronomeOn(!m.on)) }, ids = listOf("metronome.on"))
    }) {
        Column(Modifier.fillMaxWidth().weight(1f).padding(bottom = 10.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                T("${m.bpm.roundToInt()}", Type.display(44.sp), Modifier.width(120.dp))
                T("BPM", Type.label(12.sp), Modifier.weight(1f))
                Beats(c, m.sig.beats)
            }
            Slider("TEMPO", m.bpm, 30.0, 300.0, "${m.bpm.roundToInt()} BPM", { c.d(Action.SetBpm(it.roundToInt().toDouble())) }, Modifier.fillMaxWidth(), listOf("metronome.bpm"))
            Slider("VOLUME", m.volume, 0.0, 1.0, "${(m.volume * 100).roundToInt()} %", { c.d(Action.SetMetronomeVolume(it)) }, Modifier.fillMaxWidth(), listOf("metronome.volume"))
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                TimeSig.entries.forEach { sig ->
                    Chip(sig.label, { c.d(Action.SetTimeSig(sig)) }, Modifier.weight(1f), listOf("metronome.sig." + sig.label.replace('/', '_')), selected = m.sig == sig)
                }
            }
        }
    }
}

/** Beat lights; the current beat comes from telemetry in the draw phase. */
@Composable
private fun Beats(c: Ctx, beats: Int) {
    Canvas(Modifier.width((beats * 22).dp).height(22.dp).ids("metronome.beat")) {
        val beat = c.tel.value.beat
        val step = size.width / beats
        for (i in 0 until beats) {
            val on = i == beat
            drawCircle(if (on) Ink.selected else Ink.line, if (i == 0) 8.dp.toPx() else 6.dp.toPx(), Offset(step * i + step / 2, size.height / 2))
        }
    }
}
