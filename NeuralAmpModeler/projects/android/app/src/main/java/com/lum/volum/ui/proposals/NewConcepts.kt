package com.lum.volum.ui.proposals

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.CutCornerShape
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
import androidx.compose.ui.graphics.nativeCanvas
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.cos
import kotlin.math.sin

@Composable
fun ThumbDeckLive(controller: DemoController, onGallery: () -> Unit) {
    val palette = paletteFor(ProposalId.ThumbDeck)
    val state = controller.state
    Column(
        Modifier
            .fillMaxSize()
            .background(palette.background)
            .padding(18.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp),
    ) {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
            ScreenTitle("Card 1 of 5", "LIVE DECK", palette)
            CompactAction("Gallery", palette, onGallery, Modifier.width(86.dp))
        }
        Box(Modifier.fillMaxWidth().weight(1f)) {
            Box(
                Modifier
                    .fillMaxSize()
                    .offset(y = 14.dp)
                    .background(palette.surfaceHigh.copy(.6f), RoundedCornerShape(26.dp))
                    .border(1.dp, palette.hairline, RoundedCornerShape(26.dp))
            )
            Column(
                Modifier
                    .fillMaxSize()
                    .padding(bottom = 12.dp)
                    .background(
                        Brush.linearGradient(listOf(Color(0xFF293326), palette.surface)),
                        RoundedCornerShape(26.dp),
                    )
                    .border(1.dp, palette.primary.copy(.65f), RoundedCornerShape(26.dp))
                    .padding(20.dp),
                verticalArrangement = Arrangement.SpaceBetween,
            ) {
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                    LiveStatus(state, palette)
                    Text("SWIPE  →", color = palette.primary, fontFamily = palette.bodyFont, fontWeight = FontWeight.Bold, fontSize = 11.sp)
                }
                Column {
                    Text("YOUR AMP", color = palette.muted, fontFamily = palette.bodyFont, fontSize = 11.sp, letterSpacing = 2.sp)
                    Text("Blackbird 30", color = palette.text, fontFamily = palette.displayFont, fontSize = 32.sp)
                    Text("Edge · warm articulate drive", color = palette.primary, fontFamily = palette.bodyFont, fontSize = 14.sp)
                }
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    DeckStatus("Gate", "−66 dB", palette, Modifier.weight(1f))
                    DeckStatus("Delay", "Analog", palette, Modifier.weight(1f))
                    DeckStatus("Reverb", "Hall", palette, Modifier.weight(1f))
                }
                Column {
                    OutputMeter(state.peak, palette, Modifier.fillMaxWidth().height(13.dp))
                    Spacer(Modifier.height(14.dp))
                    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(9.dp)) {
                        CompactAction("Power", palette, { controller.dispatch(DemoAction.TogglePower) }, Modifier.weight(1f), active = state.powered)
                        CompactAction("Bypass", palette, { controller.dispatch(DemoAction.ToggleBypass) }, Modifier.weight(1f), active = state.bypassed)
                        CompactAction("Tune", palette, { controller.dispatch(DemoAction.OpenTuner) }, Modifier.weight(1f))
                    }
                }
            }
        }
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            listOf("LIVE", "AMP", "FX", "SETUP").forEachIndexed { index, label ->
                CompactAction(
                    label,
                    palette,
                    { if (index == 0) Unit else controller.dispatch(DemoAction.OpenEdit) },
                    Modifier.weight(1f),
                    active = index == 0,
                )
            }
        }
    }
}

@Composable
fun BlackoutLiveScreen(controller: DemoController, onGallery: () -> Unit) {
    val palette = paletteFor(ProposalId.BlackoutLive)
    val state = controller.state
    Box(
        Modifier
            .fillMaxSize()
            .background(Color.Black)
            .pointerInput(Unit) {
                detectTapGestures(onLongPress = { controller.dispatch(DemoAction.OpenEdit) })
            }
            .semantics { contentDescription = "Blackout live. Hold anywhere to edit." },
    ) {
        Column(Modifier.fillMaxSize().padding(22.dp), verticalArrangement = Arrangement.SpaceBetween) {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
                Text("VOLUM", color = palette.text, fontFamily = palette.displayFont, fontSize = 17.sp, letterSpacing = 5.sp)
                LiveStatus(state, palette, compact = true)
            }
            Column(horizontalAlignment = Alignment.CenterHorizontally, modifier = Modifier.fillMaxWidth()) {
                Text("BLACKBIRD", color = palette.text, fontFamily = palette.displayFont, fontSize = 30.sp, letterSpacing = 2.sp)
                Text("30 / EDGE", color = palette.muted, fontFamily = palette.bodyFont, fontSize = 12.sp, letterSpacing = 3.sp)
                Spacer(Modifier.height(34.dp))
                Box(Modifier.size(156.dp).border(2.dp, if (state.powered) palette.text else palette.hairline, CircleShape), contentAlignment = Alignment.Center) {
                    Column(horizontalAlignment = Alignment.CenterHorizontally) {
                        Text(if (state.powered) "LIVE" else "OFF", color = if (state.powered) palette.text else palette.muted, fontFamily = palette.displayFont, fontSize = 22.sp)
                        Text("${state.latencyMs} ms", color = palette.muted, fontFamily = palette.bodyFont, fontSize = 12.sp)
                    }
                }
                Spacer(Modifier.height(30.dp))
                OutputMeter(state.peak, palette, Modifier.fillMaxWidth(.72f).height(8.dp))
            }
            Column {
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                    CompactAction("Power", palette, { controller.dispatch(DemoAction.TogglePower) }, Modifier.weight(1f), active = state.powered)
                    CompactAction("Bypass", palette, { controller.dispatch(DemoAction.ToggleBypass) }, Modifier.weight(1f), active = state.bypassed)
                    CompactAction("Tuner", palette, { controller.dispatch(DemoAction.OpenTuner) }, Modifier.weight(1f))
                }
                Spacer(Modifier.height(12.dp))
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                    Text(
                        "HOLD ANYWHERE TO EDIT",
                        color = palette.text,
                        fontFamily = palette.bodyFont,
                        fontWeight = FontWeight.Bold,
                        fontSize = 10.sp,
                        letterSpacing = 1.5.sp,
                        modifier = Modifier.align(Alignment.CenterVertically),
                    )
                    CompactAction("Gallery", palette, onGallery, Modifier.width(82.dp))
                }
            }
        }
    }
}

@Composable
fun FlightRackLive(controller: DemoController, onGallery: () -> Unit) {
    val palette = paletteFor(ProposalId.FlightRack)
    val state = controller.state
    Row(Modifier.fillMaxSize().background(Color(0xFF11120F)).padding(12.dp), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
        Column(
            Modifier
                .width(130.dp)
                .fillMaxHeight()
                .background(Color(0xFF1A1B17), CutCornerShape(topEnd = 16.dp, bottomStart = 16.dp))
                .border(1.dp, palette.hairline, CutCornerShape(topEnd = 16.dp, bottomStart = 16.dp))
                .padding(12.dp),
            verticalArrangement = Arrangement.SpaceBetween,
        ) {
            Column {
                Text("FLIGHT", color = palette.primary, fontFamily = palette.displayFont, fontSize = 15.sp, letterSpacing = 2.sp)
                Text("RACK / 07", color = palette.muted, fontFamily = palette.bodyFont, fontSize = 10.sp)
            }
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                listOf("LIVE", "AMP", "DYNAMICS", "POST").forEachIndexed { index, label ->
                    CompactAction(label, palette, { if (index > 0) controller.dispatch(DemoAction.OpenEdit) }, Modifier.fillMaxWidth(), active = index == 0)
                }
            }
            CompactAction("Gallery", palette, onGallery, Modifier.fillMaxWidth())
        }
        Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(9.dp)) {
            RackBay("SYSTEM", palette) {
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
                    LiveStatus(state, palette)
                    OutputMeter(state.peak, palette, Modifier.width(180.dp).height(12.dp))
                    Text("EVO 4 · 48 kHz", color = palette.text, fontFamily = palette.bodyFont, fontSize = 12.sp)
                }
            }
            RackBay("AMP BAY", palette, Modifier.weight(1f)) {
                Row(Modifier.fillMaxSize(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.SpaceBetween) {
                    Column {
                        Text("BLACKBIRD 30", color = palette.text, fontFamily = palette.displayFont, fontSize = 23.sp)
                        Text("EDGE · MODEL READY", color = palette.secondary, fontFamily = palette.bodyFont, fontSize = 11.sp, letterSpacing = 1.5.sp)
                    }
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        StatusReadout("DRIVE", "${(state.drive * 100).toInt()}", palette)
                        StatusReadout("LEVEL", "${(state.level * 100).toInt()}", palette)
                    }
                }
            }
            RackBay("POST BAY", palette, Modifier.weight(1f)) {
                Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(9.dp)) {
                    RackEffect("Gate", state.gateEnabled, palette, { controller.dispatch(DemoAction.ToggleGate) }, Modifier.weight(1f))
                    RackEffect("Delay", state.delayEnabled, palette, { controller.dispatch(DemoAction.ToggleDelay) }, Modifier.weight(1f))
                    RackEffect("Reverb", state.reverbEnabled, palette, { controller.dispatch(DemoAction.ToggleReverb) }, Modifier.weight(1f))
                    RackEffect("Tremolo", state.tremoloEnabled, palette, { controller.dispatch(DemoAction.ToggleTremolo) }, Modifier.weight(1f))
                }
            }
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(9.dp)) {
                CompactAction("Power", palette, { controller.dispatch(DemoAction.TogglePower) }, Modifier.weight(1f), active = state.powered)
                CompactAction("Bypass", palette, { controller.dispatch(DemoAction.ToggleBypass) }, Modifier.weight(1f), active = state.bypassed)
                CompactAction("Tuner", palette, { controller.dispatch(DemoAction.OpenTuner) }, Modifier.weight(1f))
                LabButton("Open rack bay", palette, { controller.dispatch(DemoAction.OpenEdit) }, Modifier.weight(1.5f))
            }
        }
    }
}

@Composable
private fun DeckStatus(
    label: String,
    value: String,
    palette: ProposalPalette,
    modifier: Modifier,
) {
    Column(
        modifier
            .height(58.dp)
            .background(palette.surfaceHigh.copy(.72f), RoundedCornerShape(10.dp))
            .border(1.dp, palette.hairline, RoundedCornerShape(10.dp))
            .padding(horizontal = 10.dp, vertical = 8.dp),
        verticalArrangement = Arrangement.SpaceBetween,
    ) {
        Text(label.uppercase(), color = palette.primary, fontFamily = palette.bodyFont, fontWeight = FontWeight.Bold, fontSize = 9.sp)
        Text(value, color = palette.muted, fontFamily = palette.bodyFont, fontSize = 10.sp)
    }
}

@Composable
private fun RackBay(title: String, palette: ProposalPalette, modifier: Modifier = Modifier, content: @Composable () -> Unit) {
    Row(
        modifier
            .fillMaxWidth()
            .background(palette.surface, CutCornerShape(topEnd = 14.dp, bottomStart = 14.dp))
            .border(1.dp, palette.hairline, CutCornerShape(topEnd = 14.dp, bottomStart = 14.dp))
            .padding(12.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(title, color = palette.primary, fontFamily = palette.displayFont, fontSize = 9.sp, modifier = Modifier.width(72.dp))
        Box(Modifier.weight(1f)) { content() }
    }
}

@Composable
private fun RackEffect(
    name: String,
    enabled: Boolean,
    palette: ProposalPalette,
    onClick: () -> Unit,
    modifier: Modifier,
) {
    Row(
        modifier
            .height(52.dp)
            .background(if (enabled) palette.primary.copy(.12f) else palette.surfaceHigh, RoundedCornerShape(8.dp))
            .border(1.dp, if (enabled) palette.primary else palette.hairline, RoundedCornerShape(8.dp))
            .clickable(onClick = onClick)
            .padding(horizontal = 10.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.SpaceBetween,
    ) {
        Text(name.uppercase(), color = if (enabled) palette.primary else palette.muted, fontFamily = palette.bodyFont, fontWeight = FontWeight.Bold, fontSize = 9.sp, maxLines = 1)
        Box(Modifier.size(8.dp).background(if (enabled) palette.primary else palette.hairline, CircleShape))
    }
}

@Composable
private fun StatusReadout(label: String, value: String, palette: ProposalPalette) {
    Column(Modifier.background(Color.Black, RoundedCornerShape(6.dp)).border(1.dp, palette.hairline, RoundedCornerShape(6.dp)).padding(10.dp), horizontalAlignment = Alignment.CenterHorizontally) {
        Text(value, color = palette.primary, fontFamily = palette.displayFont, fontSize = 16.sp)
        Text(label, color = palette.muted, fontFamily = palette.bodyFont, fontSize = 8.sp)
    }
}

@Composable
fun OrbitCockpitLive(controller: DemoController, onGallery: () -> Unit) {
    val palette = paletteFor(ProposalId.OrbitCockpit)
    val state = controller.state
    Row(
        Modifier
            .fillMaxSize()
            .background(Brush.radialGradient(listOf(Color(0xFF192033), palette.background)))
            .padding(14.dp),
        horizontalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        Box(Modifier.weight(1f).fillMaxHeight()) {
            OrbitGraphic(state, palette, Modifier.fillMaxSize())
            Column(Modifier.fillMaxSize(), verticalArrangement = Arrangement.SpaceBetween) {
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                    ScreenTitle("08 / spatial rig", "ORBIT", palette)
                    LiveStatus(state, palette, compact = true)
                }
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.Bottom) {
                    CompactAction("Gallery", palette, onGallery, Modifier.width(92.dp))
                    Text("TAP A SECTOR TO INSPECT", color = palette.muted, fontFamily = palette.bodyFont, fontSize = 10.sp, letterSpacing = 2.sp)
                }
            }
        }
        Column(
            Modifier
                .width(220.dp)
                .fillMaxHeight()
                .background(palette.surface.copy(.94f), RoundedCornerShape(20.dp))
                .border(1.dp, palette.hairline, RoundedCornerShape(20.dp))
                .padding(14.dp),
            verticalArrangement = Arrangement.SpaceBetween,
        ) {
            Column {
                Text("LIVE CORE", color = palette.primary, fontFamily = palette.displayFont, fontSize = 10.sp, letterSpacing = 1.5.sp)
                Spacer(Modifier.height(10.dp))
                Text("BLACKBIRD 30", color = palette.text, fontFamily = palette.displayFont, fontSize = 16.sp)
                Text("Edge · Input 1", color = palette.muted, fontFamily = palette.bodyFont, fontSize = 12.sp)
                Spacer(Modifier.height(14.dp))
                OutputMeter(state.peak, palette, Modifier.fillMaxWidth().height(10.dp))
            }
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                CompactAction("Power", palette, { controller.dispatch(DemoAction.TogglePower) }, Modifier.fillMaxWidth(), active = state.powered)
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    CompactAction("Bypass", palette, { controller.dispatch(DemoAction.ToggleBypass) }, Modifier.weight(1f), active = state.bypassed)
                    CompactAction("Tuner", palette, { controller.dispatch(DemoAction.OpenTuner) }, Modifier.weight(1f))
                }
                LabButton("Open console", palette, { controller.dispatch(DemoAction.OpenEdit) }, Modifier.fillMaxWidth())
            }
        }
    }
}

@Composable
private fun OrbitGraphic(state: DemoSnapshot, palette: ProposalPalette, modifier: Modifier) {
    val sectors = listOf(
        "AMP" to true,
        "GATE" to state.gateEnabled,
        "DELAY" to state.delayEnabled,
        "REVERB" to state.reverbEnabled,
        "TREM" to state.tremoloEnabled,
    )
    Canvas(modifier) {
        val c = center
        val radius = size.minDimension * .31f
        drawCircle(palette.surface.copy(.72f), radius * 1.45f, c)
        drawCircle(palette.hairline, radius * 1.12f, c, style = Stroke(2f))
        drawCircle(palette.secondary.copy(.12f), radius * .72f, c)
        drawCircle(palette.secondary, radius * .72f, c, style = Stroke(4f))
        sectors.forEachIndexed { index, (name, active) ->
            val a = Math.toRadians((index * 72 - 90).toDouble())
            val pos = Offset(c.x + cos(a).toFloat() * radius, c.y + sin(a).toFloat() * radius)
            drawCircle(if (active) palette.primary.copy(.18f) else palette.surface, radius * .28f, pos)
            drawCircle(if (active) palette.primary else palette.hairline, radius * .28f, pos, style = Stroke(3f))
            drawLine(palette.hairline, c, pos, 2f, StrokeCap.Round)
            drawContext.canvas.nativeCanvas.apply {
                val paint = android.graphics.Paint().apply {
                    color = (if (active) palette.text else palette.muted).toArgb()
                    textSize = 10.sp.toPx()
                    textAlign = android.graphics.Paint.Align.CENTER
                    typeface = android.graphics.Typeface.DEFAULT_BOLD
                }
                drawText(name, pos.x, pos.y + 4.dp.toPx(), paint)
            }
        }
        drawCircle(palette.background, radius * .48f, c)
        drawCircle(if (state.powered) palette.secondary else palette.muted, radius * .48f, c, style = Stroke(5f))
        drawContext.canvas.nativeCanvas.apply {
            val titlePaint = android.graphics.Paint().apply {
                color = palette.text.toArgb()
                textSize = 13.sp.toPx()
                textAlign = android.graphics.Paint.Align.CENTER
                typeface = android.graphics.Typeface.DEFAULT_BOLD
            }
            val detailPaint = android.graphics.Paint(titlePaint).apply {
                color = palette.muted.toArgb()
                textSize = 9.sp.toPx()
                typeface = android.graphics.Typeface.DEFAULT
            }
            drawText(if (state.powered) "LIVE" else "OFF", c.x, c.y - 2.dp.toPx(), titlePaint)
            drawText("${state.latencyMs} ms", c.x, c.y + 14.dp.toPx(), detailPaint)
        }
    }
}

private fun Color.toArgb(): Int = android.graphics.Color.argb(
    (alpha * 255).toInt(),
    (red * 255).toInt(),
    (green * 255).toInt(),
    (blue * 255).toInt(),
)
