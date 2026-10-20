package com.volum.ui.b

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.runtime.Composable
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.ScreenId
import com.volum.core.TimeSig
import com.volum.core.VoLumState
import com.volum.harness.featureId
import kotlin.math.abs
import kotlin.math.roundToInt

/** Full-screen tuner readable from across the stage; the output is muted while it is open. */
@Composable
internal fun Tuner(s: VoLumState) {
    val st = LocalStage.current
    val tele = st.tele
    val note by remember { derivedStateOf { tele.value.tunerNote } }
    val octave by remember { derivedStateOf { tele.value.tunerOctave } }
    val cents by remember { derivedStateOf { tele.value.tunerCents.roundToInt() } }
    val hz by remember { derivedStateOf { (tele.value.tunerHz * 10).roundToInt() / 10.0 } }
    val inTune = abs(cents) <= 3
    Panel(ScreenId.TUNER, "Tuner", arrayOf("tuner.close"), subtitle = "play one string; the needle centres when it is in tune") {
        Column(Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Row(Modifier.fillMaxWidth().weight(1f), verticalAlignment = Alignment.CenterVertically) {
                FitText(if (cents < -3) "♭" else "", Type.display(56.sp, Ink.dim), Modifier.width(120.dp).fillMaxHeight())
                Box(Modifier.weight(1f).fillMaxHeight().featureId("tuner.note"), contentAlignment = Alignment.Center) {
                    FitText("$note$octave", Type.display(150.sp, if (inTune) Ink.hot else Ink.text), Modifier.fillMaxSize(), fitHeight = true)
                }
                FitText(if (cents > 3) "♯" else "", Type.display(56.sp, Ink.dim), Modifier.width(120.dp).fillMaxHeight())
            }
            Box(Modifier.fillMaxWidth().height(64.dp).featureId("tuner.cents")) {
                Canvas(Modifier.fillMaxSize()) {
                    val c = tele.value.tunerCents.toFloat().coerceIn(-50f, 50f)
                    val mid = size.width / 2
                    for (i in -50..50 step 5) {
                        val x = mid + i / 50f * (size.width / 2 - 8.dp.toPx())
                        val long = i % 25 == 0
                        drawLine(if (i == 0) Ink.hot else Ink.line, Offset(x, if (long) 4.dp.toPx() else 16.dp.toPx()), Offset(x, size.height - 20.dp.toPx()), if (long) 2.dp.toPx() else 1.dp.toPx())
                    }
                    val x = mid + c / 50f * (size.width / 2 - 8.dp.toPx())
                    drawLine(if (abs(c) <= 3) Ink.hot else Ink.alert, Offset(x, 0f), Offset(x, size.height - 16.dp.toPx()), 5.dp.toPx(), StrokeCap.Round)
                }
                FitText("${if (cents > 0) "+" else ""}$cents cents · $hz Hz", Type.tech(13.sp), Modifier.align(Alignment.BottomCenter).fillMaxWidth().height(18.dp))
            }
            Row(Modifier.fillMaxWidth().height(64.dp), horizontalArrangement = Arrangement.spacedBy(12.dp), verticalAlignment = Alignment.CenterVertically) {
                Box(Modifier.weight(1f).fillMaxHeight().clip(RoundedCornerShape(8.dp)).background(Ink.panel).featureId("tuner.muted"), contentAlignment = Alignment.Center) {
                    FitText("OUTPUT MUTED WHILE TUNING", Type.label(13.sp, Ink.text), Modifier.fillMaxWidth().padding(horizontal = 12.dp))
                }
                TextPad("DONE", { st.go(Action.Back) }, Modifier.width(260.dp).fillMaxHeight(), arrayOf("tuner.close"), style = Type.label(18.sp, Ink.hot))
            }
        }
    }
}

private val sigIds = mapOf(TimeSig.S1_4 to "1_4", TimeSig.S2_4 to "2_4", TimeSig.S3_4 to "3_4", TimeSig.S4_4 to "4_4", TimeSig.S6_8 to "6_8")

@Composable
internal fun Metronome(s: VoLumState) {
    val st = LocalStage.current
    val m = s.metronome
    val tele = st.tele
    val beat by remember { derivedStateOf { tele.value.beat } }
    Panel(ScreenId.METRONOME, "Metronome", arrayOf("metronome.close"), subtitle = "click on the output; the tempo also drives synced delay and tremolo") {
        Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(16.dp)) {
            Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Row(Modifier.fillMaxWidth().height(72.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    TextPad("−", { st.go(Action.SetBpm(m.bpm - 1)) }, Modifier.width(72.dp).fillMaxHeight(), style = Type.display(30.sp))
                    Column(Modifier.weight(1f), horizontalAlignment = Alignment.CenterHorizontally) {
                        FitText("${m.bpm.roundToInt()}", Type.display(48.sp, Ink.hot), Modifier.fillMaxWidth().height(52.dp), fitHeight = true)
                        FitText("BPM", Type.label(11.sp), Modifier.fillMaxWidth().height(16.dp))
                    }
                    TextPad("+", { st.go(Action.SetBpm(m.bpm + 1)) }, Modifier.width(72.dp).fillMaxHeight(), style = Type.display(30.sp))
                }
                Fader("TEMPO", m.bpm, 30.0, 300.0, { "${it.roundToInt()} BPM" }, { st.go(Action.SetBpm(it.roundToInt().toDouble())) },
                    Modifier.fillMaxWidth().height(56.dp), arrayOf("metronome.bpm"))
                Choice(TimeSig.entries.map { it.label }, m.sig.ordinal, { st.go(Action.SetTimeSig(TimeSig.entries[it])) }, Modifier.fillMaxWidth().height(56.dp),
                    { i -> arrayOf("metronome.sig.${sigIds.getValue(TimeSig.entries[i])}") })
                Fader("VOLUME", m.volume, 0.0, 1.0, { "${(it * 100).roundToInt()}%" }, { st.go(Action.SetMetronomeVolume(it)) },
                    Modifier.fillMaxWidth().height(56.dp), arrayOf("metronome.volume"))
            }
            Column(Modifier.width(260.dp).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                Pad({ st.go(Action.SetMetronomeOn(!m.on)) }, Modifier.fillMaxWidth().weight(1f), arrayOf("metronome.on"), selected = m.on, sel = Sel.BRASS, haptic = true,
                    description = if (m.on) "Stop the click" else "Start the click") {
                    FitText(if (m.on) "STOP" else "START", Type.display(36.sp, if (m.on) Ink.hot else Ink.text), Modifier.fillMaxWidth())
                }
                Row(Modifier.fillMaxWidth().height(56.dp).featureId("metronome.beat"), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    repeat(m.sig.beats) { i ->
                        Canvas(Modifier.weight(1f).fillMaxHeight()) {
                            val lit = m.on && beat == i
                            drawRoundRect(if (lit) Ink.hot else if (i == 0) Ink.line else Ink.raised, size = Size(size.width, size.height),
                                cornerRadius = androidx.compose.ui.geometry.CornerRadius(6.dp.toPx()))
                        }
                    }
                }
            }
        }
    }
}
