package com.lum.volum.ui.proposals

import androidx.activity.compose.BackHandler
import androidx.compose.foundation.background
import androidx.compose.foundation.border
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
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.lum.volum.ui.theme.Josefin
import com.lum.volum.ui.theme.Michroma

private val galleryPalette = ProposalPalette(
    background = Color(0xFF07090D),
    surface = Color(0xFF11151C),
    surfaceHigh = Color(0xFF1B222D),
    primary = Color(0xFFE7B85E),
    secondary = Color(0xFF57C7E8),
    danger = Color(0xFFFF625C),
    text = Color(0xFFF4F2EC),
    muted = Color(0xFFA7AFBA),
    hairline = Color(0xFF2C3542),
    displayFont = Michroma,
    bodyFont = Josefin,
)

@Composable
fun ProposalLab(
    microphoneGranted: Boolean,
    requestMicrophonePermission: ((Boolean) -> Unit) -> Unit,
    onOrientationChange: (ProposalOrientation?) -> Unit,
    startProposal: String? = null,
    startEntry: String? = null,
) {
    val controllers = remember { ProposalId.entries.associateWith { DemoController() } }
    val initialSpec = remember(startProposal) {
        ProposalId.fromSlug(startProposal)?.let { id -> proposalSpecs.first { it.id == id } }
    }
    var selected by remember { mutableStateOf(initialSpec) }
    var started by remember { mutableStateOf(false) }

    LaunchedEffect(selected) {
        onOrientationChange(selected?.orientation)
    }
    LaunchedEffect(initialSpec, startEntry) {
        if (!started && initialSpec != null) {
            val entry = when (startEntry?.lowercase()) {
                "edit" -> EntryPoint.Edit
                "walkthrough" -> EntryPoint.Walkthrough
                else -> EntryPoint.Live
            }
            controllers.getValue(initialSpec.id).dispatch(DemoAction.Enter(entry, microphoneGranted))
            started = true
        }
    }

    val spec = selected
    if (spec == null) {
        ProposalGallery(
            onOpen = { item, entry ->
                controllers.getValue(item.id).dispatch(DemoAction.Enter(entry, microphoneGranted))
                selected = item
            },
            onScenario = { item, scenario ->
                controllers.getValue(item.id).dispatch(DemoAction.ApplyScenario(scenario))
                selected = item
            },
        )
    } else {
        val controller = controllers.getValue(spec.id)
        BackHandler {
            if (controller.state.step == JourneyStep.Live) selected = null
            else controller.dispatch(DemoAction.BackToLive)
        }
        ProposalExperience(
            spec = spec,
            controller = controller,
            permissionGranted = microphoneGranted,
            requestPermission = requestMicrophonePermission,
            onGallery = { selected = null },
        )
    }
}

@Composable
private fun ProposalExperience(
    spec: ProposalSpec,
    controller: DemoController,
    permissionGranted: Boolean,
    requestPermission: ((Boolean) -> Unit) -> Unit,
    onGallery: () -> Unit,
) {
    when (controller.state.step) {
        JourneyStep.Permission,
        JourneyStep.Usb,
        JourneyStep.Device,
        JourneyStep.Model,
        JourneyStep.Loading,
        JourneyStep.Error -> JourneyPrelude(spec, controller, permissionGranted, requestPermission, onGallery)
        JourneyStep.Tuner -> TunerExperience(spec, controller) { controller.dispatch(DemoAction.BackToLive) }
        JourneyStep.Edit -> {
            val style = when (spec.id) {
                ProposalId.FractalStage, ProposalId.ThumbDeck -> EditorStyle.Scrub
                ProposalId.BlueFocus, ProposalId.OrbitCockpit -> EditorStyle.Focus
                ProposalId.AuricPedalboard, ProposalId.SignalAtlas, ProposalId.FlightRack -> EditorStyle.Faders
                ProposalId.BlackoutLive -> EditorStyle.Steppers
            }
            CompleteRigEditor(
                spec,
                controller,
                style,
                onLive = { controller.dispatch(DemoAction.BackToLive) },
                onGallery = onGallery,
            )
        }
        JourneyStep.Live -> when (spec.id) {
            ProposalId.FractalStage -> FractalStageLive(controller, onGallery)
            ProposalId.BlueFocus -> BlueFocusLive(controller, onGallery)
            ProposalId.AuricPedalboard -> AuricPedalboardLive(controller, onGallery)
            ProposalId.SignalAtlas -> SignalAtlasLive(controller, onGallery)
            ProposalId.ThumbDeck -> ThumbDeckLive(controller, onGallery)
            ProposalId.BlackoutLive -> BlackoutLiveScreen(controller, onGallery)
            ProposalId.FlightRack -> FlightRackLive(controller, onGallery)
            ProposalId.OrbitCockpit -> OrbitCockpitLive(controller, onGallery)
        }
    }
}

@Composable
private fun ProposalGallery(
    onOpen: (ProposalSpec, EntryPoint) -> Unit,
    onScenario: (ProposalSpec, DemoScenario) -> Unit,
) {
    Box(
        Modifier
            .fillMaxSize()
            .background(
                Brush.verticalGradient(
                    listOf(Color(0xFF080B11), Color(0xFF101621), Color(0xFF07090D))
                )
            )
    ) {
        LazyColumn(
            Modifier.fillMaxSize(),
            contentPadding = androidx.compose.foundation.layout.PaddingValues(20.dp),
            verticalArrangement = Arrangement.spacedBy(14.dp),
        ) {
            item {
                GalleryHeader()
            }
            items(proposalSpecs, key = { it.id.slug }) { spec ->
                ProposalCard(spec, onOpen, onScenario)
            }
            item {
                Text(
                    "DESIGN EXPLORATION · MOCK STATE · NO DSP CHANGES",
                    color = galleryPalette.muted,
                    fontFamily = Josefin,
                    fontSize = 10.sp,
                    letterSpacing = 2.sp,
                    modifier = Modifier.padding(vertical = 18.dp),
                )
            }
        }
    }
}

@Composable
private fun GalleryHeader() {
    Column(Modifier.fillMaxWidth().padding(vertical = 10.dp)) {
        Text("VOLUM", color = galleryPalette.text, fontFamily = Michroma, fontSize = 28.sp, letterSpacing = 7.sp)
        Text("ANDROID / UI PROPOSAL LAB", color = galleryPalette.primary, fontFamily = Josefin, fontWeight = FontWeight.Bold, fontSize = 12.sp, letterSpacing = 3.sp)
        Spacer(Modifier.height(18.dp))
        Text(
            "Eight complete ways to move from cable to live performance.",
            color = galleryPalette.text,
            fontFamily = Michroma,
            fontSize = 20.sp,
            lineHeight = 29.sp,
        )
        Spacer(Modifier.height(9.dp))
        Text(
            "Four evolve VoLum’s fractal / gold / blue identity. Four deliberately redraw the mobile guitar tool.",
            color = galleryPalette.muted,
            fontFamily = Josefin,
            fontSize = 14.sp,
            lineHeight = 20.sp,
        )
    }
}

@Composable
private fun ProposalCard(
    spec: ProposalSpec,
    onOpen: (ProposalSpec, EntryPoint) -> Unit,
    onScenario: (ProposalSpec, DemoScenario) -> Unit,
) {
    val palette = paletteFor(spec.id)
    val shape = RoundedCornerShape(20.dp)
    Column(
        Modifier
            .fillMaxWidth()
            .background(palette.surface.copy(.9f), shape)
            .border(1.dp, palette.hairline, shape)
            .padding(18.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.Top) {
            Row(horizontalArrangement = Arrangement.spacedBy(14.dp)) {
                Text(spec.number, color = palette.primary, fontFamily = Michroma, fontSize = 12.sp)
                Column {
                    Text(spec.name.uppercase(), color = palette.text, fontFamily = palette.displayFont, fontSize = 17.sp)
                    Text(
                        "${if (spec.currentVibe) "VOLUM EVOLUTION" else "NEW DIRECTION"} · ${spec.orientation.name.uppercase()}",
                        color = palette.primary,
                        fontFamily = palette.bodyFont,
                        fontWeight = FontWeight.Bold,
                        fontSize = 9.sp,
                        letterSpacing = 1.3.sp,
                    )
                }
            }
            Box(Modifier.width(48.dp).height(5.dp).background(palette.secondary, RoundedCornerShape(3.dp)))
        }
        Text(spec.thesis, color = palette.text, fontFamily = palette.bodyFont, fontSize = 14.sp, lineHeight = 20.sp)
        Text(spec.navigation, color = palette.muted, fontFamily = palette.bodyFont, fontSize = 12.sp)
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            LabButton("Walkthrough", palette, { onOpen(spec, EntryPoint.Walkthrough) }, Modifier.weight(1.3f))
            CompactAction("Live", palette, { onOpen(spec, EntryPoint.Live) }, Modifier.weight(1f))
            CompactAction("Deep edit", palette, { onOpen(spec, EntryPoint.Edit) }, Modifier.weight(1f))
        }
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("SCENARIOS", color = palette.muted, fontFamily = palette.bodyFont, fontSize = 9.sp, letterSpacing = 1.2.sp, modifier = Modifier.align(Alignment.CenterVertically))
            CompactAction("USB off", palette, { onScenario(spec, DemoScenario.NoUsb) }, Modifier.weight(1f))
            CompactAction("Load", palette, { onScenario(spec, DemoScenario.Loading) }, Modifier.weight(1f))
            CompactAction("Error", palette, { onScenario(spec, DemoScenario.Error) }, Modifier.weight(1f))
        }
    }
}
