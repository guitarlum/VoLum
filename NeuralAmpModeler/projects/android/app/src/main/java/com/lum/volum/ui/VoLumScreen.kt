package com.lum.volum.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.ui.draw.clip
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.lum.volum.VoLumState
import com.lum.volum.ui.theme.Josefin
import com.lum.volum.ui.theme.Michroma
import com.lum.volum.ui.theme.VoLumColors

/** Maps a real parameter value onto a Knob and back through a min..max range. */
@Composable
fun ParamKnob(
    label: String,
    value: Float,
    min: Float,
    max: Float,
    display: String,
    onChange: (Float) -> Unit,
    modifier: Modifier = Modifier,
    accent: Color = VoLumColors.Amber,
    enabled: Boolean = true,
) {
    val norm = ((value - min) / (max - min)).coerceIn(0f, 1f)
    Knob(
        label = label,
        value = norm,
        display = display,
        accent = accent,
        enabled = enabled,
        onValueChange = { n -> onChange(min + n * (max - min)) },
        modifier = modifier,
    )
}

@Composable
fun VoLumScreen(state: VoLumState) {
    Box(
        Modifier
            .fillMaxSize()
            .background(
                Brush.verticalGradient(listOf(Color(0xFF14110F), VoLumColors.Void))
            )
    ) {
        Column(
            Modifier
                .fillMaxSize()
                .verticalScroll(rememberScrollState())
                .padding(horizontal = 18.dp, vertical = 20.dp),
            verticalArrangement = Arrangement.spacedBy(14.dp),
        ) {
            Header(state)
            TransportStrip(state)
            TunerPanel(state)
            SignalPanel(state)
            AmpPanel(state)
            GatePanel(state)
            DelayPanel(state)
            ReverbPanel(state)
            TremoloPanel(state)
            Spacer(Modifier.height(4.dp))
            Text(
                "NAM core · Oboe low-latency · USB audio",
                fontFamily = Josefin, fontSize = 11.sp, letterSpacing = 1.sp,
                color = VoLumColors.TextLow, modifier = Modifier.fillMaxWidth(),
            )
            Spacer(Modifier.height(8.dp))
        }
    }
}

@Composable
private fun Header(state: VoLumState) {
    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
        Column {
            Text("VOLUM", fontFamily = Michroma, fontSize = 30.sp, letterSpacing = 8.sp, color = VoLumColors.TextHi)
            Text("MOBILE  MONITOR", fontFamily = Josefin, fontWeight = FontWeight.SemiBold, fontSize = 11.sp,
                letterSpacing = 5.sp, color = VoLumColors.Amber)
        }
        Column(horizontalAlignment = Alignment.End) {
            Text("LATENCY", fontFamily = Josefin, fontWeight = FontWeight.SemiBold, fontSize = 9.sp,
                letterSpacing = 2.sp, color = VoLumColors.TextLow)
            Text(state.latencyMs, fontFamily = Michroma, fontSize = 13.sp, color = VoLumColors.Teal)
        }
    }
}

@Composable
private fun TransportStrip(state: VoLumState) {
    Panel(Modifier.fillMaxWidth()) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            PowerButton(running = state.running) { state.toggleAudio() }
            Spacer(Modifier.width(16.dp))
            Column(Modifier.weight(1f)) {
                Text(
                    if (state.busy) state.status else if (state.running) state.status else "Idle",
                    fontFamily = Michroma, fontSize = 15.sp, letterSpacing = 1.sp,
                    color = if (state.running) VoLumColors.Teal else VoLumColors.TextMid,
                )
                Spacer(Modifier.height(8.dp))
                LevelMeter(state.peak, Modifier.fillMaxWidth())
                Spacer(Modifier.height(6.dp))
                Text(
                    if (state.running) "OUTPUT" else "press power to start",
                    fontFamily = Josefin, fontSize = 10.sp, letterSpacing = 2.sp, color = VoLumColors.TextLow,
                )
            }
            Spacer(Modifier.width(12.dp))
            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                Text("BYPASS", fontFamily = Josefin, fontWeight = FontWeight.SemiBold, fontSize = 10.sp,
                    letterSpacing = 1.sp, color = VoLumColors.TextMid, modifier = Modifier.padding(bottom = 6.dp))
                TogglePill(state.bypass, VoLumColors.Red) { state.applyBypass(!state.bypass) }
            }
        }
    }
}

private data class NoteReading(val name: String, val cents: Int, val inTune: Boolean, val voiced: Boolean)

private fun noteFromHz(hz: Float): NoteReading {
    if (hz <= 0f) return NoteReading("—", 0, false, false)
    val names = listOf("C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B")
    val midi = 69.0 + 12.0 * (kotlin.math.ln(hz / 440.0) / kotlin.math.ln(2.0))
    val nearest = kotlin.math.round(midi).toInt()
    val cents = ((midi - nearest) * 100.0).toInt()
    val name = names[((nearest % 12) + 12) % 12] + (nearest / 12 - 1)
    return NoteReading(name, cents, kotlin.math.abs(cents) <= 4, true)
}

@Composable
private fun TunerPanel(state: VoLumState) {
    val a = VoLumColors.Teal
    Panel(Modifier.fillMaxWidth()) {
        Column {
            PedalHeader("Tuner", state.tunerEnabled, a) { state.setTuner(it) }
            if (state.tunerEnabled) {
                Spacer(Modifier.height(12.dp))
                val reading = noteFromHz(state.tunerHz)
                val noteColor = when {
                    !reading.voiced -> VoLumColors.TextLow
                    reading.inTune -> VoLumColors.Green
                    else -> VoLumColors.Amber
                }
                Column(Modifier.fillMaxWidth(), horizontalAlignment = Alignment.CenterHorizontally) {
                    Text(reading.name, fontFamily = Michroma, fontSize = 46.sp, color = noteColor)
                    Spacer(Modifier.height(2.dp))
                    Text(
                        if (!state.running) "start audio to tune"
                        else if (reading.voiced) "%+d cents · %.1f Hz".format(reading.cents, state.tunerHz)
                        else "play a note",
                        fontFamily = Josefin, fontSize = 12.sp, letterSpacing = 1.sp, color = VoLumColors.TextMid,
                    )
                    Spacer(Modifier.height(12.dp))
                    CentsNeedle(if (reading.voiced) reading.cents else 0, reading.inTune && reading.voiced,
                        Modifier.fillMaxWidth().height(30.dp))
                }
            }
        }
    }
}

@Composable
private fun CentsNeedle(cents: Int, inTune: Boolean, modifier: Modifier = Modifier) {
    androidx.compose.foundation.Canvas(modifier) {
        val midX = size.width / 2f
        val midY = size.height / 2f
        // Baseline + graduation ticks.
        for (i in -5..5) {
            val x = midX + (i / 5f) * (size.width / 2f - 8f)
            val tall = i == 0
            drawLine(
                color = if (tall) VoLumColors.TextMid else VoLumColors.Hairline,
                start = androidx.compose.ui.geometry.Offset(x, midY - if (tall) 12f else 6f),
                end = androidx.compose.ui.geometry.Offset(x, midY + if (tall) 12f else 6f),
                strokeWidth = if (tall) 3f else 2f,
            )
        }
        // Moving indicator (clamped to +-50 cents).
        val c = cents.coerceIn(-50, 50) / 50f
        val ix = midX + c * (size.width / 2f - 8f)
        val col = if (inTune) VoLumColors.Green else VoLumColors.Amber
        drawCircle(color = col.copy(alpha = 0.25f), radius = 14f,
            center = androidx.compose.ui.geometry.Offset(ix, midY))
        drawCircle(color = col, radius = 7f, center = androidx.compose.ui.geometry.Offset(ix, midY))
    }
}

@Composable
private fun SignalPanel(state: VoLumState) {
    Panel(Modifier.fillMaxWidth()) {
        Column {
            SectionLabel("Signal")
            Spacer(Modifier.height(12.dp))
            Selector("Input device", state.inputDevices.map { it.label }, state.deviceIndex,
                { state.deviceIndex = it }, Modifier.fillMaxWidth())
            Spacer(Modifier.height(12.dp))
            Selector("Amp profile", state.models.map { it.removeSuffix(".nam") }, state.modelIndex,
                { state.modelIndex = it }, Modifier.fillMaxWidth())
            Spacer(Modifier.height(12.dp))
            Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                ActionButton("Load amp", VoLumColors.Amber, Modifier.weight(1f)) { state.loadSelectedModel() }
                ActionButton("Clear", VoLumColors.TextMid, Modifier.weight(1f)) { state.clearModel() }
            }
        }
    }
}

@Composable
private fun AmpPanel(state: VoLumState) {
    Panel(Modifier.fillMaxWidth()) {
        Column {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
                SectionLabel("Amp")
                TogglePill(state.toneEnabled, VoLumColors.Amber) { state.applyToneEnabled(!state.toneEnabled) }
            }
            Spacer(Modifier.height(14.dp))
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceEvenly) {
                ParamKnob("Drive", state.inputGainDb, -12f, 24f, "%+.1f".format(state.inputGainDb),
                    { state.setInputGain(it) })
                ParamKnob("Bass", state.bass, 0f, 10f, "%.1f".format(state.bass),
                    { state.bass = it; state.pushTone() }, enabled = state.toneEnabled)
                ParamKnob("Mid", state.mid, 0f, 10f, "%.1f".format(state.mid),
                    { state.mid = it; state.pushTone() }, enabled = state.toneEnabled)
            }
            Spacer(Modifier.height(16.dp))
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceEvenly) {
                ParamKnob("Treble", state.treble, 0f, 10f, "%.1f".format(state.treble),
                    { state.treble = it; state.pushTone() }, enabled = state.toneEnabled)
                ParamKnob("Level", state.outputGainDb, -24f, 12f, "%+.1f".format(state.outputGainDb),
                    { state.setOutputGain(it) })
                Spacer(Modifier.width(74.dp))
            }
        }
    }
}

@Composable
private fun GatePanel(state: VoLumState) {
    Panel(Modifier.fillMaxWidth()) {
        Column {
            PedalHeader("Noise gate", state.gateEnabled, VoLumColors.Green) {
                state.gateEnabled = it; state.pushGate()
            }
            Spacer(Modifier.height(12.dp))
            Row(verticalAlignment = Alignment.CenterVertically) {
                ParamKnob("Threshold", state.gateThresholdDb, -100f, 0f, "%.0f".format(state.gateThresholdDb),
                    { state.gateThresholdDb = it; state.pushGate() }, accent = VoLumColors.Green, enabled = state.gateEnabled)
                Spacer(Modifier.width(16.dp))
                Text(
                    "Silences hiss and hum below the threshold\nwhile you're not playing.",
                    fontFamily = Josefin, fontSize = 12.sp, color = VoLumColors.TextMid, lineHeight = 17.sp,
                )
            }
        }
    }
}

@Composable
private fun DelayPanel(state: VoLumState) {
    val a = VoLumColors.Teal
    Panel(Modifier.fillMaxWidth()) {
        Column {
            PedalHeader("Delay", state.delayEnabled, a) { state.delayEnabled = it; state.pushDelay() }
            Spacer(Modifier.height(14.dp))
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceEvenly) {
                ParamKnob("Mix", state.delayMix, 0f, 1f, "%.0f%%".format(state.delayMix * 100),
                    { state.delayMix = it; state.pushDelay() }, accent = a, enabled = state.delayEnabled)
                ParamKnob("Time", state.delayTimeMs, 40f, 1200f, "%.0f".format(state.delayTimeMs),
                    { state.delayTimeMs = it; state.pushDelay() }, accent = a, enabled = state.delayEnabled)
                ParamKnob("Feedback", state.delayFeedback, 0f, 0.9f, "%.0f%%".format(state.delayFeedback * 100),
                    { state.delayFeedback = it; state.pushDelay() }, accent = a, enabled = state.delayEnabled)
            }
            Spacer(Modifier.height(14.dp))
            ModeSegments(listOf("Digital", "Analog", "Reverse"), state.delayMode, a) {
                state.delayMode = it; state.pushDelay()
            }
        }
    }
}

@Composable
private fun ReverbPanel(state: VoLumState) {
    val a = VoLumColors.Violet
    Panel(Modifier.fillMaxWidth()) {
        Column {
            PedalHeader("Reverb", state.reverbEnabled, a) { state.reverbEnabled = it; state.pushReverb() }
            Spacer(Modifier.height(14.dp))
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceEvenly) {
                ParamKnob("Mix", state.reverbMix, 0f, 1f, "%.0f%%".format(state.reverbMix * 100),
                    { state.reverbMix = it; state.pushReverb() }, accent = a, enabled = state.reverbEnabled)
                ParamKnob("Decay", state.reverbDecay, 0.2f, 12f, "%.1fs".format(state.reverbDecay),
                    { state.reverbDecay = it; state.pushReverb() }, accent = a, enabled = state.reverbEnabled)
                ParamKnob("Tone", state.reverbTone, 0f, 10f, "%.1f".format(state.reverbTone),
                    { state.reverbTone = it; state.pushReverb() }, accent = a, enabled = state.reverbEnabled)
            }
            Spacer(Modifier.height(14.dp))
            ModeSegments(listOf("Hall", "Plate", "Oktaverb"), state.reverbMode, a) {
                state.reverbMode = it; state.pushReverb()
            }
        }
    }
}

@Composable
private fun TremoloPanel(state: VoLumState) {
    val a = VoLumColors.Amber
    Panel(Modifier.fillMaxWidth()) {
        Column {
            PedalHeader("Tremolo", state.tremEnabled, a) { state.tremEnabled = it; state.pushTremolo() }
            Spacer(Modifier.height(14.dp))
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceEvenly) {
                ParamKnob("Rate", state.tremRate, 0.5f, 12f, "%.1fHz".format(state.tremRate),
                    { state.tremRate = it; state.pushTremolo() }, accent = a, enabled = state.tremEnabled)
                ParamKnob("Depth", state.tremDepth, 0f, 1f, "%.0f%%".format(state.tremDepth * 100),
                    { state.tremDepth = it; state.pushTremolo() }, accent = a, enabled = state.tremEnabled)
                Spacer(Modifier.width(74.dp))
            }
            Spacer(Modifier.height(14.dp))
            ModeSegments(listOf("Optical", "Bias", "Harmonic"), state.tremMode, a) {
                state.tremMode = it; state.pushTremolo()
            }
        }
    }
}

@Composable
private fun ActionButton(text: String, accent: Color, modifier: Modifier = Modifier, onClick: () -> Unit) {
    val shape = RoundedCornerShape(12.dp)
    Box(
        modifier
            .clip(shape)
            .background(accent.copy(alpha = 0.14f))
            .border(1.dp, accent.copy(alpha = 0.5f), shape)
            .clickable { onClick() }
            .padding(vertical = 13.dp),
        contentAlignment = Alignment.Center,
    ) {
        Text(
            text.uppercase(), fontFamily = Josefin, fontWeight = FontWeight.SemiBold, fontSize = 13.sp,
            letterSpacing = 1.5.sp, color = accent,
        )
    }
}
