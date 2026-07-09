package com.lum.volum.ui.proposals

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
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
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

@Composable
fun FractalStageLive(controller: DemoController, onGallery: () -> Unit) {
    val palette = paletteFor(ProposalId.FractalStage)
    val state = controller.state
    Box(Modifier.fillMaxSize().background(palette.background)) {
        FractalField(palette, Modifier.fillMaxWidth().fillMaxHeight(.72f))
        Column(Modifier.fillMaxSize().padding(horizontal = 18.dp, vertical = 20.dp), verticalArrangement = Arrangement.SpaceBetween) {
            Column {
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
                    Text("VOLUM / 01", color = palette.primary, fontFamily = palette.bodyFont, fontSize = 10.sp, letterSpacing = 2.sp)
                    LiveStatus(state, palette, compact = true)
                }
                Spacer(Modifier.height(18.dp))
                Text("FRACTAL STAGE", color = palette.text, fontFamily = palette.displayFont, fontSize = 18.sp, maxLines = 1)
            }
            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                Text("BLACKBIRD 30", color = palette.text, fontFamily = palette.displayFont, fontSize = 20.sp, letterSpacing = 1.sp, maxLines = 1)
                Text("EDGE  ·  EVO 4 / INPUT 1", color = palette.primary, fontFamily = palette.bodyFont, fontSize = 11.sp, letterSpacing = 2.sp)
                Spacer(Modifier.height(22.dp))
                OutputMeter(state.peak, palette, Modifier.fillMaxWidth(.84f).height(12.dp))
            }
            Column(
                Modifier
                    .fillMaxWidth()
                    .background(palette.surface.copy(.97f), RoundedCornerShape(22.dp))
                    .border(1.dp, palette.hairline, RoundedCornerShape(22.dp))
                    .padding(14.dp),
                verticalArrangement = Arrangement.spacedBy(10.dp),
            ) {
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                    CompactAction("Power", palette, { controller.dispatch(DemoAction.TogglePower) }, Modifier.weight(1f), active = state.powered)
                    CompactAction("Bypass", palette, { controller.dispatch(DemoAction.ToggleBypass) }, Modifier.weight(1f), active = state.bypassed)
                    CompactAction("Tuner", palette, { controller.dispatch(DemoAction.OpenTuner) }, Modifier.weight(1f))
                }
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                    CompactAction("Amp", palette, { controller.dispatch(DemoAction.ApplyScenario(DemoScenario.Loading)) }, Modifier.weight(1f))
                    LabButton("Edit rig", palette, { controller.dispatch(DemoAction.OpenEdit) }, Modifier.weight(1.55f))
                    CompactAction("Lab", palette, onGallery, Modifier.weight(1f))
                }
            }
        }
    }
}

@Composable
fun BlueFocusLive(controller: DemoController, onGallery: () -> Unit) {
    val palette = paletteFor(ProposalId.BlueFocus)
    val state = controller.state
    Column(
        Modifier
            .fillMaxSize()
            .background(Brush.verticalGradient(listOf(palette.background, Color(0xFF0A1C2D))))
            .padding(20.dp),
        verticalArrangement = Arrangement.SpaceBetween,
    ) {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
            ScreenTitle("02 / one thing now", "BLUE FOCUS", palette)
            CompactAction("Gallery", palette, onGallery, Modifier.width(86.dp))
        }
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
            CompactAction("‹", palette, {}, Modifier.width(54.dp))
            Column(Modifier.weight(1f), horizontalAlignment = Alignment.CenterHorizontally) {
                Text("LIVE SOUND", color = palette.muted, fontFamily = palette.bodyFont, fontSize = 11.sp, letterSpacing = 3.sp)
                Spacer(Modifier.height(6.dp))
                Text("BLACKBIRD", color = palette.text, fontFamily = palette.displayFont, fontSize = 18.sp, maxLines = 1)
                Text("30  /  EDGE", color = palette.primary, fontFamily = palette.displayFont, fontSize = 15.sp)
            }
            CompactAction("›", palette, {}, Modifier.width(54.dp))
        }
        Column(
            Modifier
                .fillMaxWidth()
                .background(palette.surface.copy(.95f), RoundedCornerShape(28.dp))
                .border(1.dp, palette.primary.copy(.35f), RoundedCornerShape(28.dp))
                .padding(20.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
        ) {
            LiveStatus(state, palette)
            Spacer(Modifier.height(18.dp))
            OutputMeter(state.peak, palette, Modifier.fillMaxWidth().height(14.dp))
            Spacer(Modifier.height(18.dp))
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                CompactAction("Power", palette, { controller.dispatch(DemoAction.TogglePower) }, Modifier.weight(1f), active = state.powered)
                CompactAction("Bypass", palette, { controller.dispatch(DemoAction.ToggleBypass) }, Modifier.weight(1f), active = state.bypassed)
                CompactAction("Tune", palette, { controller.dispatch(DemoAction.OpenTuner) }, Modifier.weight(1f))
            }
        }
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            CompactAction("Change amp", palette, { controller.dispatch(DemoAction.ApplyScenario(DemoScenario.Loading)) }, Modifier.weight(1f))
            LabButton("Focus an edit", palette, { controller.dispatch(DemoAction.OpenEdit) }, Modifier.weight(1.15f))
        }
    }
}

@Composable
fun AuricPedalboardLive(controller: DemoController, onGallery: () -> Unit) {
    val palette = paletteFor(ProposalId.AuricPedalboard)
    val state = controller.state
    Column(
        Modifier
            .fillMaxSize()
            .background(
                Brush.verticalGradient(listOf(Color(0xFF0A0806), Color(0xFF17100A), Color(0xFF080706)))
            )
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(14.dp)) {
            Text("AURIC", color = palette.primary, fontFamily = palette.displayFont, fontSize = 18.sp, letterSpacing = 3.sp)
            Text("PEDALBOARD", color = palette.text, fontFamily = palette.displayFont, fontSize = 13.sp, letterSpacing = 2.sp)
            Spacer(Modifier.weight(1f))
            LiveStatus(state, palette, compact = true)
            OutputMeter(state.peak, palette, Modifier.width(130.dp).height(10.dp))
            CompactAction("Gallery", palette, onGallery, Modifier.width(82.dp))
        }
        Row(Modifier.fillMaxWidth().weight(1f), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            AmpPedal(controller, palette, Modifier.weight(1.25f))
            StompPedal("Gate", "−66 dB", state.gateEnabled, palette, { controller.dispatch(DemoAction.ToggleGate) }, Modifier.weight(1f))
            StompPedal("Delay", "Analog", state.delayEnabled, palette, { controller.dispatch(DemoAction.ToggleDelay) }, Modifier.weight(1f))
            StompPedal("Reverb", "Hall", state.reverbEnabled, palette, { controller.dispatch(DemoAction.ToggleReverb) }, Modifier.weight(1f))
            StompPedal("Tremolo", "Harmonic", state.tremoloEnabled, palette, { controller.dispatch(DemoAction.ToggleTremolo) }, Modifier.weight(1f))
        }
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            CompactAction("Power", palette, { controller.dispatch(DemoAction.TogglePower) }, Modifier.weight(1f), active = state.powered)
            CompactAction("Bypass all", palette, { controller.dispatch(DemoAction.ToggleBypass) }, Modifier.weight(1f), active = state.bypassed)
            CompactAction("Tuner", palette, { controller.dispatch(DemoAction.OpenTuner) }, Modifier.weight(1f))
            LabButton("Raise editor tray", palette, { controller.dispatch(DemoAction.OpenEdit) }, Modifier.weight(1.6f))
        }
    }
}

@Composable
private fun AmpPedal(controller: DemoController, palette: ProposalPalette, modifier: Modifier) {
    Column(
        modifier
            .fillMaxHeight()
            .background(Brush.verticalGradient(listOf(Color(0xFF45351E), Color(0xFF1E160E))), RoundedCornerShape(16.dp))
            .border(1.dp, palette.primary, RoundedCornerShape(16.dp))
            .clickable { controller.dispatch(DemoAction.OpenEdit) }
            .padding(15.dp),
        verticalArrangement = Arrangement.SpaceBetween,
    ) {
        Text("AMP", color = palette.primary, fontFamily = palette.displayFont, fontSize = 10.sp, letterSpacing = 2.sp)
        Column {
            Text("BLACKBIRD", color = palette.text, fontFamily = palette.displayFont, fontSize = 11.sp, maxLines = 1)
            Text("30 · EDGE", color = palette.primary, fontFamily = palette.bodyFont, fontWeight = FontWeight.Bold, fontSize = 12.sp)
        }
        Text("TAP TO SHAPE", color = palette.muted, fontFamily = palette.bodyFont, fontSize = 9.sp)
    }
}

@Composable
private fun StompPedal(
    name: String,
    detail: String,
    enabled: Boolean,
    palette: ProposalPalette,
    onClick: () -> Unit,
    modifier: Modifier,
) {
    Column(
        modifier
            .fillMaxHeight()
            .background(if (enabled) palette.surfaceHigh else palette.surface, RoundedCornerShape(16.dp))
            .border(1.dp, if (enabled) palette.primary else palette.hairline, RoundedCornerShape(16.dp))
            .clickable(role = Role.Switch, onClick = onClick)
            .padding(14.dp)
            .semantics { contentDescription = "$name ${if (enabled) "on" else "off"}" },
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.SpaceBetween,
    ) {
        Text(name.uppercase(), color = if (enabled) palette.primary else palette.text, fontFamily = palette.displayFont, fontSize = 10.sp, textAlign = TextAlign.Center)
        Box(Modifier.size(12.dp).background(if (enabled) palette.primary else palette.hairline, CircleShape))
        Text(detail, color = palette.muted, fontFamily = palette.bodyFont, fontSize = 10.sp)
        Box(
            Modifier
                .size(46.dp)
                .background(Color(0xFF080706), CircleShape)
                .border(2.dp, if (enabled) palette.primary else palette.muted, CircleShape)
        )
    }
}

@Composable
fun SignalAtlasLive(controller: DemoController, onGallery: () -> Unit) {
    val palette = paletteFor(ProposalId.SignalAtlas)
    val state = controller.state
    Row(Modifier.fillMaxSize().background(palette.background).padding(16.dp), horizontalArrangement = Arrangement.spacedBy(14.dp)) {
        Column(
            Modifier
                .width(152.dp)
                .fillMaxHeight()
                .background(palette.surface, RoundedCornerShape(16.dp))
                .border(1.dp, palette.hairline, RoundedCornerShape(16.dp))
                .padding(14.dp),
            verticalArrangement = Arrangement.SpaceBetween,
        ) {
            Text("04 / MAP", color = palette.primary, fontFamily = palette.bodyFont, fontSize = 9.sp, letterSpacing = 2.sp)
            Text("SIGNAL\nATLAS", color = palette.text, fontFamily = palette.displayFont, fontSize = 15.sp, lineHeight = 22.sp)
            LiveStatus(state, palette, compact = true)
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                CompactAction("Power", palette, { controller.dispatch(DemoAction.TogglePower) }, Modifier.fillMaxWidth(), active = state.powered)
                CompactAction("Bypass", palette, { controller.dispatch(DemoAction.ToggleBypass) }, Modifier.fillMaxWidth(), active = state.bypassed)
            }
        }
        Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.SpaceBetween) {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
                Text("EVO 4 / INPUT 1  →  OUTPUT", color = palette.muted, fontFamily = palette.bodyFont, fontSize = 11.sp, letterSpacing = 1.5.sp)
                OutputMeter(state.peak, palette, Modifier.width(180.dp).height(10.dp))
            }
            SignalChain(controller, palette, Modifier.fillMaxWidth().weight(1f))
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                CompactAction("Tuner", palette, { controller.dispatch(DemoAction.OpenTuner) }, Modifier.width(78.dp))
                CompactAction("Lab", palette, onGallery, Modifier.width(68.dp))
                LabButton("Open node inspector", palette, { controller.dispatch(DemoAction.OpenEdit) }, Modifier.width(210.dp))
            }
        }
    }
}

@Composable
private fun SignalChain(controller: DemoController, palette: ProposalPalette, modifier: Modifier) {
    val s = controller.state
    val nodes = listOf(
        "IN" to true,
        "GATE" to s.gateEnabled,
        "BLACKBIRD" to s.modelLoaded,
        "DELAY" to s.delayEnabled,
        "REVERB" to s.reverbEnabled,
        "TREM" to s.tremoloEnabled,
        "OUT" to s.powered,
    )
    Box(modifier) {
        Canvas(Modifier.fillMaxSize()) {
            val y = size.height / 2f
            drawLine(palette.hairline, Offset(20f, y), Offset(size.width - 20f, y), 4f, StrokeCap.Round)
            val path = Path().apply {
                moveTo(size.width * .18f, y)
                cubicTo(size.width * .34f, y - 90f, size.width * .58f, y + 80f, size.width * .82f, y)
            }
            drawPath(path, palette.secondary.copy(.3f), style = Stroke(2f))
        }
        Row(Modifier.fillMaxSize(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.SpaceEvenly) {
            nodes.forEach { (name, active) ->
                Column(horizontalAlignment = Alignment.CenterHorizontally) {
                    Box(
                        Modifier
                            .size(if (name == "BLACKBIRD") 82.dp else 60.dp)
                            .background(if (active) palette.surfaceHigh else palette.surface, CircleShape)
                            .border(2.dp, if (active) palette.primary else palette.hairline, CircleShape)
                            .clickable { controller.dispatch(DemoAction.OpenEdit) },
                        contentAlignment = Alignment.Center,
                    ) {
                        Text(name, color = if (active) palette.text else palette.muted, fontFamily = palette.displayFont, fontSize = if (name == "BLACKBIRD") 8.sp else 9.sp, textAlign = TextAlign.Center)
                    }
                    Spacer(Modifier.height(9.dp))
                    Box(Modifier.size(7.dp).background(if (active) palette.secondary else palette.hairline, CircleShape))
                }
            }
        }
    }
}
