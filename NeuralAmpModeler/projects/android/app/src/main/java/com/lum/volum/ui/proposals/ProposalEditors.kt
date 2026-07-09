package com.lum.volum.ui.proposals

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
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
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.cos
import kotlin.math.sin

enum class EditorStyle { Scrub, Focus, Faders, Steppers }

@Composable
fun CompleteRigEditor(
    spec: ProposalSpec,
    controller: DemoController,
    style: EditorStyle,
    onLive: () -> Unit,
    onGallery: () -> Unit,
) {
    val palette = paletteFor(spec.id)
    val landscape = spec.orientation == ProposalOrientation.Landscape
    Row(
        Modifier
            .fillMaxSize()
            .background(palette.background)
            .padding(if (landscape) 18.dp else 16.dp),
        horizontalArrangement = Arrangement.spacedBy(18.dp),
    ) {
        if (landscape) {
            EditorRail(spec, palette, onLive, onGallery, Modifier.width(154.dp).fillMaxHeight())
        }
        Column(
            Modifier
                .weight(1f)
                .fillMaxHeight()
                .verticalScroll(rememberScrollState())
                .padding(end = 4.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp),
        ) {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
                ScreenTitle("Protected workspace", "Shape the rig", palette)
                if (!landscape) CompactAction("Live", palette, onLive, Modifier.width(84.dp), active = true)
            }
            when (style) {
                EditorStyle.Focus -> FocusAmpEditor(controller, palette, compact = landscape)
                EditorStyle.Scrub -> ScrubAmpEditor(controller, palette)
                EditorStyle.Faders -> FaderAmpEditor(controller, palette)
                EditorStyle.Steppers -> StepperAmpEditor(controller, palette)
            }
            EffectEditor(controller, palette, style, landscape)
            ErrorRecoveryCard(controller, palette)
            if (!landscape) {
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                    CompactAction("Gallery", palette, onGallery, Modifier.weight(1f))
                    LabButton("Done · return live", palette, onLive, Modifier.weight(2f))
                }
            }
            Spacer(Modifier.height(12.dp))
        }
    }
}

@Composable
private fun EditorRail(
    spec: ProposalSpec,
    palette: ProposalPalette,
    onLive: () -> Unit,
    onGallery: () -> Unit,
    modifier: Modifier,
) {
    Column(
        modifier
            .background(palette.surface, RoundedCornerShape(16.dp))
            .border(1.dp, palette.hairline, RoundedCornerShape(16.dp))
            .padding(14.dp),
        verticalArrangement = Arrangement.SpaceBetween,
    ) {
        Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text(spec.name.uppercase(), color = palette.primary, fontFamily = palette.displayFont, fontSize = 11.sp, lineHeight = 17.sp)
            Spacer(Modifier.height(8.dp))
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                CompactAction("AMP", palette, {}, Modifier.weight(1f), active = true)
                CompactAction("FX", palette, {}, Modifier.weight(1f))
            }
        }
        Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
            LabButton("Return live", palette, onLive, Modifier.fillMaxWidth())
            CompactAction("Gallery", palette, onGallery, Modifier.fillMaxWidth())
        }
    }
}

@Composable
private fun EditorSurface(
    title: String,
    palette: ProposalPalette,
    content: @Composable () -> Unit,
) {
    Column(
        Modifier
            .fillMaxWidth()
            .background(palette.surface, RoundedCornerShape(18.dp))
            .border(1.dp, palette.hairline, RoundedCornerShape(18.dp))
            .padding(18.dp),
    ) {
        Text(title.uppercase(), color = palette.primary, fontFamily = palette.displayFont, fontSize = 11.sp, letterSpacing = 1.5.sp)
        Spacer(Modifier.height(14.dp))
        content()
    }
}

@Composable
private fun ScrubAmpEditor(controller: DemoController, palette: ProposalPalette) {
    val s = controller.state
    EditorSurface("Amp · Blackbird 30", palette) {
        Column(verticalArrangement = Arrangement.spacedBy(9.dp)) {
            ParamRows(s).forEach { (label, parameter, value) ->
                HorizontalScrub(label, value, palette, { controller.dispatch(DemoAction.SetValue(parameter, it)) }, Modifier.fillMaxWidth())
            }
        }
    }
}

@Composable
private fun FocusAmpEditor(controller: DemoController, palette: ProposalPalette, compact: Boolean) {
    val s = controller.state
    if (compact) {
        Row(
            Modifier
                .fillMaxWidth()
                .height(170.dp)
                .background(palette.surface, RoundedCornerShape(18.dp))
                .border(1.dp, palette.hairline, RoundedCornerShape(18.dp))
                .padding(14.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(16.dp),
        ) {
            Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text("FOCUSED CONTROL · MID", color = palette.primary, fontFamily = palette.displayFont, fontSize = 10.sp, letterSpacing = 1.sp)
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    CompactAction("Drive", palette, {}, Modifier.weight(1f))
                    CompactAction("Mid", palette, {}, Modifier.weight(1f), active = true)
                    CompactAction("More", palette, {}, Modifier.weight(1f))
                }
                Text("Bass 48  ·  Treble 63  ·  Level 58", color = palette.muted, fontFamily = palette.bodyFont, fontSize = 10.sp)
            }
            FocusDial(
                "Mid",
                s.mid,
                palette,
                { controller.dispatch(DemoAction.SetValue(Parameter.Mid, it)) },
                diameter = 94.dp,
                showSteps = false,
            )
        }
        return
    }
    EditorSurface("Focused control · Mid", palette) {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceEvenly, verticalAlignment = Alignment.CenterVertically) {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                listOf("DRIVE", "BASS", "MID", "TREBLE", "LEVEL").forEach { name ->
                    CompactAction(name, palette, {}, Modifier.width(92.dp), active = name == "MID")
                }
            }
            FocusDial(
                "Mid",
                s.mid,
                palette,
                { controller.dispatch(DemoAction.SetValue(Parameter.Mid, it)) },
                diameter = 136.dp,
                showSteps = true,
            )
        }
    }
}

@Composable
private fun FaderAmpEditor(controller: DemoController, palette: ProposalPalette) {
    val s = controller.state
    EditorSurface("Amp channel", palette) {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(16.dp)) {
            ParamRows(s).forEach { (label, parameter, value) ->
                SafeFader(label, value, palette, { controller.dispatch(DemoAction.SetValue(parameter, it)) }, Modifier.weight(1f))
            }
        }
    }
}

@Composable
private fun StepperAmpEditor(controller: DemoController, palette: ProposalPalette) {
    val s = controller.state
    EditorSurface("Amp · deliberate steps", palette) {
        Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
            ParamRows(s).forEach { (label, parameter, value) ->
                Row(
                    Modifier.fillMaxWidth().height(58.dp),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(6.dp),
                ) {
                    Text(label.uppercase(), color = palette.text, fontFamily = palette.bodyFont, fontWeight = FontWeight.Bold, fontSize = 12.sp, maxLines = 1, modifier = Modifier.width(54.dp))
                    CompactAction("−", palette, { controller.dispatch(DemoAction.SetValue(parameter, (value - .02f).coerceAtLeast(0f))) }, Modifier.width(48.dp))
                    Text("${(value * 100).toInt()}", color = palette.primary, fontFamily = palette.displayFont, fontSize = 16.sp, textAlign = TextAlign.Center, modifier = Modifier.width(42.dp))
                    CompactAction("+", palette, { controller.dispatch(DemoAction.SetValue(parameter, (value + .02f).coerceAtMost(1f))) }, Modifier.width(48.dp))
                }
            }
        }
    }
}

private data class ParamRow(val label: String, val parameter: Parameter, val value: Float)
private fun ParamRows(s: DemoSnapshot) = listOf(
    ParamRow("Drive", Parameter.Drive, s.drive),
    ParamRow("Bass", Parameter.Bass, s.bass),
    ParamRow("Mid", Parameter.Mid, s.mid),
    ParamRow("Treble", Parameter.Treble, s.treble),
    ParamRow("Level", Parameter.Level, s.level),
)

@Composable
private fun EffectEditor(controller: DemoController, palette: ProposalPalette, style: EditorStyle, landscape: Boolean) {
    val s = controller.state
    EditorSurface("Gate + post effects", palette) {
        Column(verticalArrangement = Arrangement.spacedBy(14.dp)) {
            if (landscape) {
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                    EffectToggle("Gate", s.gateEnabled, "−66 dB", palette, { controller.dispatch(DemoAction.ToggleGate) }, Modifier.weight(1f))
                    EffectToggle("Delay", s.delayEnabled, "Analog · 28%", palette, { controller.dispatch(DemoAction.ToggleDelay) }, Modifier.weight(1f))
                    EffectToggle("Reverb", s.reverbEnabled, "Hall · 34%", palette, { controller.dispatch(DemoAction.ToggleReverb) }, Modifier.weight(1f))
                    EffectToggle("Tremolo", s.tremoloEnabled, "Harmonic · 42%", palette, { controller.dispatch(DemoAction.ToggleTremolo) }, Modifier.weight(1f))
                }
            } else {
                Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
                    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                        EffectToggle("Gate", s.gateEnabled, "−66 dB", palette, { controller.dispatch(DemoAction.ToggleGate) }, Modifier.weight(1f))
                        EffectToggle("Delay", s.delayEnabled, "Analog · 28%", palette, { controller.dispatch(DemoAction.ToggleDelay) }, Modifier.weight(1f))
                    }
                    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                        EffectToggle("Reverb", s.reverbEnabled, "Hall · 34%", palette, { controller.dispatch(DemoAction.ToggleReverb) }, Modifier.weight(1f))
                        EffectToggle("Tremolo", s.tremoloEnabled, "Harmonic · 42%", palette, { controller.dispatch(DemoAction.ToggleTremolo) }, Modifier.weight(1f))
                    }
                }
            }
            when (style) {
                EditorStyle.Scrub -> HorizontalScrub("Delay mix", s.delayMix, palette, { controller.dispatch(DemoAction.SetValue(Parameter.Delay, it)) }, Modifier.fillMaxWidth())
                EditorStyle.Focus -> FocusDial("Reverb mix", s.reverbMix, palette, { controller.dispatch(DemoAction.SetValue(Parameter.Reverb, it)) }, Modifier.align(Alignment.CenterHorizontally))
                EditorStyle.Faders -> Row(horizontalArrangement = Arrangement.spacedBy(16.dp)) {
                    SafeFader("Gate", s.gateThreshold, palette, { controller.dispatch(DemoAction.SetValue(Parameter.Gate, it)) }, Modifier.weight(1f))
                    SafeFader("Delay", s.delayMix, palette, { controller.dispatch(DemoAction.SetValue(Parameter.Delay, it)) }, Modifier.weight(1f))
                    SafeFader("Reverb", s.reverbMix, palette, { controller.dispatch(DemoAction.SetValue(Parameter.Reverb, it)) }, Modifier.weight(1f))
                    SafeFader("Trem", s.tremoloDepth, palette, { controller.dispatch(DemoAction.SetValue(Parameter.Tremolo, it)) }, Modifier.weight(1f))
                }
                EditorStyle.Steppers -> ModeRow(listOf("Digital", "Analog", "Reverse"), s.delayMode, palette, { controller.dispatch(DemoAction.SetMode(Effect.Delay, it)) }, Modifier.fillMaxWidth())
            }
            Text("DELAY MODE", color = palette.muted, fontFamily = palette.bodyFont, fontSize = 10.sp, letterSpacing = 1.sp)
            ModeRow(listOf("Digital", "Analog", "Reverse"), s.delayMode, palette, { controller.dispatch(DemoAction.SetMode(Effect.Delay, it)) }, Modifier.fillMaxWidth())
            Text("REVERB MODE", color = palette.muted, fontFamily = palette.bodyFont, fontSize = 10.sp, letterSpacing = 1.sp)
            ModeRow(listOf("Hall", "Plate", "Oktaverb"), s.reverbMode, palette, { controller.dispatch(DemoAction.SetMode(Effect.Reverb, it)) }, Modifier.fillMaxWidth())
            Text("TREMOLO MODE", color = palette.muted, fontFamily = palette.bodyFont, fontSize = 10.sp, letterSpacing = 1.sp)
            ModeRow(listOf("Optical", "Bias", "Harmonic"), s.tremoloMode, palette, { controller.dispatch(DemoAction.SetMode(Effect.Tremolo, it)) }, Modifier.fillMaxWidth())
        }
    }
}

@Composable
private fun ErrorRecoveryCard(controller: DemoController, palette: ProposalPalette) {
    Row(
        Modifier
            .fillMaxWidth()
            .background(palette.danger.copy(.08f), RoundedCornerShape(14.dp))
            .border(1.dp, palette.danger.copy(.45f), RoundedCornerShape(14.dp))
            .padding(14.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(14.dp),
    ) {
        Column(Modifier.weight(1f)) {
            Text("RECOVERY PREVIEW", color = palette.danger, fontFamily = palette.displayFont, fontSize = 10.sp)
            Text("Test a failed model load. The active rig remains unchanged.", color = palette.muted, fontFamily = palette.bodyFont, fontSize = 12.sp)
        }
        CompactAction("Show error", palette, { controller.dispatch(DemoAction.ApplyScenario(DemoScenario.Error)) }, Modifier.width(112.dp))
    }
}

@Composable
fun TunerExperience(
    spec: ProposalSpec,
    controller: DemoController,
    onLive: () -> Unit,
) {
    val palette = paletteFor(spec.id)
    val landscape = spec.orientation == ProposalOrientation.Landscape
    val state = controller.state
    Box(
        Modifier
            .fillMaxSize()
            .background(Brush.radialGradient(listOf(palette.surfaceHigh, palette.background)))
            .padding(24.dp)
    ) {
        Column(Modifier.fillMaxSize(), horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.SpaceBetween) {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
                ScreenTitle("Raw input", "Tuner", palette)
                CompactAction("Back to live", palette, onLive, Modifier.width(130.dp), active = true)
            }
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(if (landscape) 70.dp else 24.dp)) {
                Text("E2", color = palette.secondary, fontFamily = palette.displayFont, fontSize = if (landscape) 92.sp else 78.sp)
                Column(horizontalAlignment = Alignment.CenterHorizontally) {
                    TunerArc(palette, Modifier.size(if (landscape) 220.dp else 190.dp))
                    Text("+2 cents  ·  82.5 Hz", color = palette.text, fontFamily = palette.bodyFont, fontSize = 14.sp)
                }
            }
            Text("IN TUNE  ·  ${state.latencyMs} ms", color = palette.secondary, fontFamily = palette.displayFont, fontSize = 12.sp, letterSpacing = 3.sp)
        }
    }
}

@Composable
private fun TunerArc(palette: ProposalPalette, modifier: Modifier) {
    Canvas(modifier) {
        drawArc(palette.hairline, 200f, 140f, false, style = Stroke(8.dp.toPx(), cap = StrokeCap.Round))
        for (i in -5..5) {
            val angle = Math.toRadians((270 + i * 12).toDouble())
            val c = Offset(size.width / 2f, size.height / 2f)
            val outer = size.minDimension * .44f
            val inner = outer - if (i == 0) 28f else 16f
            drawLine(
                if (i == 0) palette.secondary else palette.muted,
                Offset(c.x + cos(angle).toFloat() * inner, c.y + sin(angle).toFloat() * inner),
                Offset(c.x + cos(angle).toFloat() * outer, c.y + sin(angle).toFloat() * outer),
                if (i == 0) 5f else 2f,
            )
        }
        drawCircle(palette.secondary.copy(.18f), 24f, center)
        drawCircle(palette.secondary, 9f, center)
    }
}
