package com.volum.ui.b

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
import androidx.compose.foundation.layout.width
import androidx.compose.runtime.Composable
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.ScreenId
import com.volum.core.TimeSig
import com.volum.core.VoLumState
import com.volum.design.components.VoLumBrassButton
import com.volum.design.components.insetWell
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumType
import com.volum.design.theme.withOpacity
import com.volum.harness.featureId
import kotlin.math.abs
import kotlin.math.roundToInt

/** Full-panel tuner readable from across the stage; the output is muted while it is open. */
@Composable
internal fun Tuner(s: VoLumState) {
    val st = LocalStage.current
    val tele = st.tele
    val note by remember { derivedStateOf { tele.value.tunerNote } }
    val octave by remember { derivedStateOf { tele.value.tunerOctave } }
    val cents by remember { derivedStateOf { tele.value.tunerCents.roundToInt() } }
    val hz by remember { derivedStateOf { (tele.value.tunerHz * 10).roundToInt() / 10.0 } }
    val inTune = abs(cents) <= 3
    Panel(s, ScreenId.TUNER, "Tuner", arrayOf("tuner.close"), subtitle = "play one open string; the note turns teal when it is in tune") {
        Column(Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Row(Modifier.fillMaxWidth().weight(1f), verticalAlignment = Alignment.CenterVertically) {
                FitText(if (cents < -3) "FLAT" else "", VoLumType.label(22.sp, VoLumColors.Amber), Modifier.width(150.dp).fillMaxHeight())
                Box(Modifier.weight(1f).fillMaxHeight().featureId("tuner.note"), contentAlignment = Alignment.Center) {
                    FitText("$note$octave", VoLumType.display(150.sp, if (inTune) VoLumColors.Teal else VoLumColors.TextBright), Modifier.fillMaxSize(), fitHeight = true)
                }
                FitText(if (cents > 3) "SHARP" else "", VoLumType.label(22.sp, VoLumColors.Amber), Modifier.width(150.dp).fillMaxHeight())
            }
            Box(Modifier.fillMaxWidth().height(60.dp).insetWell(3.dp).featureId("tuner.cents")) {
                Spacer(Modifier.fillMaxSize().padding(horizontal = 12.dp).drawBehind {
                    val c = tele.value.tunerCents.toFloat().coerceIn(-50f, 50f)
                    val mid = size.width / 2
                    for (i in -50..50 step 5) {
                        val x = mid + i / 50f * (size.width / 2)
                        val long = i % 25 == 0
                        drawLine(if (i == 0) VoLumColors.Gold else VoLumColors.Frame, Offset(x, if (long) 6.dp.toPx() else 16.dp.toPx()),
                            Offset(x, size.height - 22.dp.toPx()), if (long) 2.dp.toPx() else 1.dp.toPx())
                    }
                    val x = mid + c / 50f * (size.width / 2)
                    drawLine(if (abs(c) <= 3) VoLumColors.Teal else VoLumColors.Amber, Offset(x, 2.dp.toPx()), Offset(x, size.height - 20.dp.toPx()), 5.dp.toPx(), StrokeCap.Round)
                })
                FitText("${if (cents > 0) "+" else ""}$cents cents · $hz Hz", VoLumType.value(12.sp, VoLumColors.Gold), Modifier.align(Alignment.BottomCenter).fillMaxWidth().height(18.dp))
            }
            Row(Modifier.fillMaxWidth().height(64.dp), horizontalArrangement = Arrangement.spacedBy(16.dp), verticalAlignment = Alignment.CenterVertically) {
                Box(Modifier.weight(1f).featureId("tuner.muted")) {
                    Status("OUTPUT MUTED WHILE THE TUNER IS OPEN", Modifier.fillMaxWidth().height(24.dp), VoLumColors.Amber)
                }
                TextPad("DONE", { st.go(Action.Back) }, Modifier.width(240.dp).fillMaxHeight(), arrayOf("tuner.close"), tone = Tone.PRIMARY, size = 17.sp)
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
    Panel(s, ScreenId.METRONOME, "Metronome", arrayOf("metronome.close"), subtitle = "a click on the output; the tempo also drives synced delay and tremolo") {
        Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(18.dp)) {
            Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                Row(Modifier.fillMaxWidth().height(72.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                    TextPad("−", { st.go(Action.SetBpm(m.bpm - 1)) }, Modifier.width(72.dp).fillMaxHeight(), arrayOf("metronome.bpm"), size = 26.sp)
                    Column(Modifier.weight(1f).fillMaxHeight().insetWell(3.dp), horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.Center) {
                        FitText("${m.bpm.roundToInt()}", VoLumType.value(34.sp, VoLumColors.Gold), Modifier.fillMaxWidth().height(44.dp), fitHeight = true)
                        FitText("BPM", VoLumType.label(10.sp, VoLumColors.TextDim), Modifier.fillMaxWidth().height(14.dp))
                    }
                    TextPad("+", { st.go(Action.SetBpm(m.bpm + 1)) }, Modifier.width(72.dp).fillMaxHeight(), arrayOf("metronome.bpm"), size = 26.sp)
                }
                Fader("TEMPO", m.bpm, 30.0, 300.0, { "${it.roundToInt()} BPM" }, { st.go(Action.SetBpm(it.roundToInt().toDouble())) },
                    Modifier.fillMaxWidth().height(52.dp), arrayOf("metronome.bpm"))
                Row(Modifier.fillMaxWidth().height(48.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    FitText("TIME", VoLumType.label(12.sp, VoLumColors.TextBright), Modifier.width(84.dp), align = TextAlign.Start)
                    TimeSig.entries.forEach { sig ->
                        VoLumBrassButton(sig.label, m.sig == sig, { st.go(Action.SetTimeSig(sig)) },
                            Modifier.weight(1f).height(48.dp).featureId("metronome.sig.${sigIds.getValue(sig)}"), minWidth = 48.dp)
                    }
                }
                Fader("VOLUME", m.volume, 0.0, 1.0, { "${(it * 100).roundToInt()}%" }, { st.go(Action.SetMetronomeVolume(it)) },
                    Modifier.fillMaxWidth().height(52.dp), arrayOf("metronome.volume"))
            }
            Column(Modifier.width(250.dp).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                Pad({ st.go(Action.SetMetronomeOn(!m.on)) }, Modifier.fillMaxWidth().weight(1f), arrayOf("metronome.on"), selected = m.on, style = Sel.Brass,
                    role = Role.Switch, haptic = true, description = if (m.on) "Stop the click" else "Start the click") { ink ->
                    Column(horizontalAlignment = Alignment.CenterHorizontally) {
                        FitText(if (m.on) "STOP" else "START", VoLumType.display(38.sp, if (m.on) ink else VoLumColors.Gold), Modifier.width(200.dp).height(48.dp))
                        FitText(if (m.on) "clicking · tap to stop" else "the click is off · tap to start", VoLumType.label(11.sp, if (m.on) ink else VoLumColors.TextDim), Modifier.width(200.dp).height(16.dp))
                    }
                }
                Row(Modifier.fillMaxWidth().height(52.dp).featureId("metronome.beat"), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    repeat(m.sig.beats) { i ->
                        Spacer(Modifier.weight(1f).fillMaxHeight().drawBehind {
                            val lit = m.on && beat == i
                            val cr = CornerRadius(3.dp.toPx())
                            drawRoundRect(VoLumColors.WellDark, cornerRadius = cr)
                            drawRoundRect(if (lit) (if (i == 0) VoLumColors.Gold else VoLumColors.GoldDim) else (if (i == 0) VoLumColors.Gold.withOpacity(0.18f) else VoLumColors.Frame), cornerRadius = cr)
                        })
                    }
                }
            }
        }
    }
}
