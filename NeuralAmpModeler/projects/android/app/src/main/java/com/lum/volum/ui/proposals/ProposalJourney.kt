package com.lum.volum.ui.proposals

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.BoxWithConstraints
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
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.delay

@Composable
fun JourneyPrelude(
    spec: ProposalSpec,
    controller: DemoController,
    permissionGranted: Boolean,
    requestPermission: ((Boolean) -> Unit) -> Unit,
    onGallery: () -> Unit,
) {
    val state = controller.state
    val palette = paletteFor(spec.id)
    if (state.step == JourneyStep.Loading) {
        LaunchedEffect(state.step) {
            delay(900)
            controller.dispatch(DemoAction.FinishLoading)
        }
    }
    BoxWithConstraints(
        Modifier
            .fillMaxSize()
            .background(
                Brush.linearGradient(
                    listOf(palette.background, palette.surface.copy(alpha = .78f), palette.background)
                )
            )
    ) {
        ConceptPreludeBackdrop(spec, palette, Modifier.fillMaxSize())
        val landscape = maxWidth > maxHeight
        if (landscape) {
            Row(Modifier.fillMaxSize().padding(22.dp), horizontalArrangement = Arrangement.spacedBy(24.dp)) {
                JourneyRail(spec, state, palette, onGallery, Modifier.width(170.dp).fillMaxHeight())
                JourneyCard(spec, controller, permissionGranted, requestPermission, palette, Modifier.weight(1f).fillMaxHeight())
            }
        } else {
            Column(Modifier.fillMaxSize().padding(22.dp)) {
                JourneyRail(spec, state, palette, onGallery, Modifier.fillMaxWidth().height(112.dp))
                Spacer(Modifier.height(18.dp))
                JourneyCard(spec, controller, permissionGranted, requestPermission, palette, Modifier.weight(1f).fillMaxWidth())
            }
        }
    }
}

@Composable
private fun JourneyRail(
    spec: ProposalSpec,
    state: DemoSnapshot,
    palette: ProposalPalette,
    onGallery: () -> Unit,
    modifier: Modifier,
) {
    Column(modifier, verticalArrangement = Arrangement.SpaceBetween) {
        Column {
            Text(spec.number, color = palette.primary, fontFamily = palette.displayFont, fontSize = 11.sp, letterSpacing = 2.sp)
            Text(spec.name.uppercase(), color = palette.text, fontFamily = palette.displayFont, fontSize = 15.sp, lineHeight = 21.sp)
            Spacer(Modifier.height(8.dp))
            Text(
                when (state.step) {
                    JourneyStep.Permission -> "01  LISTEN"
                    JourneyStep.Usb -> "02  CONNECT"
                    JourneyStep.Device -> "03  INPUT"
                    JourneyStep.Model -> "04  AMP"
                    JourneyStep.Loading -> "05  PREPARE"
                    JourneyStep.Error -> "RECOVERY"
                    else -> "READY"
                },
                color = palette.muted,
                fontFamily = palette.bodyFont,
                fontSize = 10.sp,
                letterSpacing = 1.4.sp,
            )
        }
        CompactAction("Gallery", palette, onGallery, Modifier.fillMaxWidth())
    }
}

@Composable
private fun JourneyCard(
    spec: ProposalSpec,
    controller: DemoController,
    permissionGranted: Boolean,
    requestPermission: ((Boolean) -> Unit) -> Unit,
    palette: ProposalPalette,
    modifier: Modifier,
) {
    val state = controller.state
    val shape = RoundedCornerShape(if (spec.id == ProposalId.BlackoutLive) 2.dp else 22.dp)
    Column(
        modifier
            .clip(shape)
            .background(palette.surface.copy(alpha = .96f))
            .border(1.dp, palette.hairline, shape)
            .padding(24.dp),
        verticalArrangement = Arrangement.Center,
    ) {
        val title: String
        val body: String
        val action: String
        val dispatch: () -> Unit
        when (state.step) {
            JourneyStep.Permission -> {
                title = if (state.errorMessage == null) "Hear the guitar. Nothing else." else "Microphone remains off"
                body = state.errorMessage
                    ?: "VoLum needs microphone access for your interface input. Audio stays on this device."
                action = if (permissionGranted) "Continue" else if (state.errorMessage == null) "Allow microphone" else "Try again"
                dispatch = {
                    if (permissionGranted) {
                        controller.dispatch(DemoAction.GrantPermission)
                    } else {
                        requestPermission { granted ->
                            controller.dispatch(if (granted) DemoAction.GrantPermission else DemoAction.DenyPermission)
                        }
                    }
                }
            }
            JourneyStep.Usb -> {
                title = if (state.usbConnected) "Interface found" else "Connect your interface"
                body = "Plug a class-compliant USB interface into USB-C. Your current rig remains safe while we wait."
                action = "Simulate EVO 4 connected"
                dispatch = { controller.dispatch(DemoAction.ConnectUsb) }
            }
            JourneyStep.Device -> {
                title = "Choose the guitar input"
                body = "Audient EVO 4 · Input 1\n48 kHz · low-latency path\n\nBuilt-in microphone · fallback"
                action = "Use EVO 4 · Input 1"
                dispatch = { controller.dispatch(DemoAction.SelectDevice) }
            }
            JourneyStep.Model -> {
                title = "Choose your amp"
                body = "BLACKBIRD 30 · EDGE\nOpen clean / articulate breakup\n\nCALIFORNIA LEAD\nTight high-gain / focused low end"
                action = "Load Blackbird 30"
                dispatch = { controller.dispatch(DemoAction.SelectModel) }
            }
            JourneyStep.Loading -> {
                title = "Warming the rig"
                body = "Verifying model · preparing the signal path · preserving previous live sound"
                action = ""
                dispatch = {}
            }
            JourneyStep.Error -> {
                title = "The amp did not load"
                body = state.errorMessage ?: "Your previous live rig is still active. Check the model and try again."
                action = "Retry safely"
                dispatch = { controller.dispatch(DemoAction.Retry) }
            }
            else -> {
                title = "Rig ready"
                body = "Input, model, and monitoring are prepared."
                action = "Open live"
                dispatch = { controller.dispatch(DemoAction.OpenLive) }
            }
        }
        Text(title, color = palette.text, fontFamily = palette.displayFont, fontSize = if (spec.orientation == ProposalOrientation.Landscape) 25.sp else 27.sp, lineHeight = 34.sp)
        Spacer(Modifier.height(16.dp))
        Text(body, color = palette.muted, fontFamily = palette.bodyFont, fontSize = 15.sp, lineHeight = 22.sp)
        Spacer(Modifier.height(26.dp))
        if (state.step == JourneyStep.Loading) {
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(16.dp)) {
                CircularProgressIndicator(color = palette.primary, modifier = Modifier.size(34.dp), strokeWidth = 3.dp)
                Text("48%", color = palette.primary, fontFamily = palette.displayFont, fontSize = 16.sp)
            }
        } else {
            LabButton(action, palette, dispatch, Modifier.fillMaxWidth())
        }
    }
}
