package com.volum.ui.a

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.runtime.Composable
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Action
import com.volum.core.TimeSig
import com.volum.design.components.VoLumBrassButton
import com.volum.design.components.VoLumButton
import com.volum.design.components.VoLumGlyph
import com.volum.design.components.VoLumKnob
import com.volum.design.components.VoLumTextField
import com.volum.design.components.VoLumToggle
import com.volum.design.theme.VText
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumType
import kotlin.math.abs
import kotlin.math.roundToInt

/** Within this many cents the tuner reads in tune (teal); outside it the note and needle turn amber. */
private const val InTuneCents = 5

/** The tuner: the note large in Poiret One, a cents strip with a needle drawn per frame, and the mute it implies on stage. */
@Composable
internal fun Tuner(c: Ctx) {
    val note by remember(c.tel) { derivedStateOf { c.tel.value.tunerNote } }
    val octave by remember(c.tel) { derivedStateOf { c.tel.value.tunerOctave } }
    val cents by remember(c.tel) { derivedStateOf { c.tel.value.tunerCents.roundToInt() } }
    val inTune = abs(cents) <= InTuneCents
    val ink = if (inTune) VoLumColors.Teal else VoLumColors.Amber
    Sheet("Tuner", listOf("tuner.close"), c, width = 640.dp, fill = false) {
        Column(Modifier.fillMaxWidth(), horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Row(verticalAlignment = Alignment.Bottom, horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                VText(note, Modifier.ids("tuner.note"), style = VoLumType.display(64.sp, ink))
                VText("$octave", Modifier.padding(bottom = 10.dp), style = VoLumType.value(16.sp, ink))
            }
            Canvas(Modifier.width(520.dp).height(40.dp).ids("tuner.cents")) {
                val mid = size.width / 2
                for (i in -10..10) {
                    val x = mid + i * size.width / 20f
                    val major = i % 5 == 0
                    val tick = if (i == 0) VoLumColors.Teal else if (major) VoLumColors.GoldDim else VoLumColors.Frame
                    drawLine(tick, Offset(x, size.height * (if (i == 0) 0.0f else if (major) 0.3f else 0.5f)), Offset(x, size.height), (if (i == 0) 2f else 1f) * density)
                }
                val t = c.tel.value.tunerCents.toFloat().coerceIn(-50f, 50f) / 50f
                val needle = if (abs(c.tel.value.tunerCents) <= InTuneCents) VoLumColors.Teal else VoLumColors.Amber
                drawLine(needle, Offset(mid + t * mid, 0f), Offset(mid + t * mid, size.height), 4.dp.toPx(), StrokeCap.Round)
            }
            Row(Modifier.width(520.dp)) {
                VText("FLAT", style = VoLumType.label(9.sp, VoLumColors.CreamDim))
                Fill()
                VText(when { inTune -> "IN TUNE"; cents < 0 -> "$cents CENTS"; else -> "+$cents CENTS" }, style = VoLumType.value(12.sp, ink))
                Fill()
                VText("SHARP", style = VoLumType.label(9.sp, VoLumColors.CreamDim))
            }
            VText("Output muted while tuning", Modifier.ids("tuner.muted"), style = VoLumType.label(11.sp, VoLumColors.Amber))
            VoLumButton("Done", { c.d(Action.Back) }, Modifier.width(220.dp).ids("tuner.close"), primary = true)
        }
    }
}

/** The metronome: on / off, BPM by steps or typed, TEMPO and VOLUME knobs, the time signature, and the beat lights. */
@Composable
internal fun Metronome(c: Ctx) {
    val m = c.s.metronome
    Sheet("Metronome", listOf("metronome.close"), c, width = 680.dp, fill = false, actions = {
        VoLumToggle(m.on, { c.d(Action.SetMetronomeOn(it)) }, Modifier.ids("metronome.on"), label = if (m.on) "ON" else "OFF")
    }) {
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            Caption("BPM", Modifier.width(34.dp))
            GlyphButton(VoLumGlyph.Prev, { c.d(Action.SetBpm((m.bpm.roundToInt() - 1).toDouble())) }, "Slower", Modifier.ids("metronome.bpm"), tint = VoLumColors.Gold)
            BpmField(c, m.bpm.roundToInt())
            GlyphButton(VoLumGlyph.Next, { c.d(Action.SetBpm((m.bpm.roundToInt() + 1).toDouble())) }, "Faster", Modifier.ids("metronome.bpm"), tint = VoLumColors.Gold)
            Fill()
            VoLumKnob(m.bpm, { c.d(Action.SetBpm(it.roundToInt().toDouble())) }, 30.0..300.0, 120.0, "TEMPO", "${m.bpm.roundToInt()} BPM", Modifier.ids("metronome.bpm"), parseEntry = ::parseNumber)
            VoLumKnob(m.volume, { c.d(Action.SetMetronomeVolume(it)) }, 0.0..1.0, 0.7, "VOLUME", "${(m.volume * 100).roundToInt()} %", Modifier.ids("metronome.volume"))
        }
        Row(Modifier.fillMaxWidth().padding(top = 6.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            Caption("TIME", Modifier.width(34.dp))
            TimeSig.entries.forEach { sig ->
                VoLumBrassButton(sig.label, m.sig == sig, { c.d(Action.SetTimeSig(sig)) }, Modifier.ids("metronome.sig." + sig.label.replace('/', '_')))
            }
            Fill()
            Beats(c, m.sig.beats)
        }
        Hint(if (m.on) "Clicking. Tempo-synced delay and tremolo follow this BPM." else "Off. Tempo-synced delay and tremolo still follow this BPM.", Modifier.padding(top = 4.dp), lines = 1)
    }
}

/** Tap-to-type BPM between the steps; Done applies it. */
@Composable
private fun BpmField(c: Ctx, bpm: Int) {
    var text by remember(bpm) { mutableStateOf("$bpm") }
    VoLumTextField(
        text, { t -> text = t.filter { it.isDigit() }.take(3) },
        Modifier.width(84.dp).ids("metronome.bpm"),
        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number, imeAction = ImeAction.Done),
        keyboardActions = KeyboardActions(onDone = { text.toIntOrNull()?.let { c.d(Action.SetBpm(it.toDouble())) } }),
    )
}

/** Beat lights; the current beat comes from telemetry in the draw phase, the downbeat larger. */
@Composable
private fun Beats(c: Ctx, beats: Int) {
    Canvas(Modifier.width((beats * 22).dp).height(22.dp).ids("metronome.beat")) {
        val beat = c.tel.value.beat
        val step = size.width / beats
        for (i in 0 until beats) {
            val on = i == beat
            drawCircle(if (on) (if (i == 0) VoLumColors.Gold else VoLumColors.Teal) else VoLumColors.Frame, if (i == 0) 7.dp.toPx() else 5.dp.toPx(), Offset(step * i + step / 2, size.height / 2))
        }
    }
}
