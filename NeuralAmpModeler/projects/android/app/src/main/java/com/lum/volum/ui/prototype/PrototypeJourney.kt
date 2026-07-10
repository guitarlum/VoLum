package com.lum.volum.ui.prototype

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.role
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.delay

@Composable
fun SetupJourney(
    state: PrototypeState,
    dispatch: (PrototypeAction) -> Unit,
    requestMicrophonePermission: ((Boolean) -> Unit) -> Unit,
) {
    Row(
        Modifier
            .fillMaxSize()
            .background(PrototypeTheme.canvas),
    ) {
        SetupIdentity(state, Modifier.weight(.38f))
        Box(
            Modifier
                .weight(.62f)
                .fillMaxHeight()
                .background(PrototypeTheme.panel)
                .padding(20.dp),
            contentAlignment = Alignment.Center,
        ) {
            when (state.setupStep) {
                SetupStep.Permission -> PermissionStep(state, dispatch, requestMicrophonePermission)
                SetupStep.Usb -> UsbStep(state, dispatch)
            }
        }
    }
}

@Composable
private fun SetupIdentity(state: PrototypeState, modifier: Modifier = Modifier) {
    Box(
        modifier
            .fillMaxHeight()
            .background(
                Brush.radialGradient(
                    listOf(PrototypeTheme.teal.copy(.13f), PrototypeTheme.canvas),
                ),
            )
            .padding(28.dp),
    ) {
        Canvas(Modifier.fillMaxSize()) {
            val mid = size.height * .56f
            drawLine(
                PrototypeTheme.lineBright,
                Offset(0f, mid),
                Offset(size.width, mid),
                2.dp.toPx(),
                StrokeCap.Round,
            )
            repeat(6) { index ->
                val x = size.width * (index + 1) / 7f
                drawCircle(
                    if (index <= if (state.setupStep == SetupStep.Permission) 0 else 2) PrototypeTheme.teal else PrototypeTheme.line,
                    5.dp.toPx(),
                    Offset(x, mid),
                )
            }
        }
        Column {
            Text(
                "VOLUM",
                color = PrototypeTheme.text,
                fontFamily = PrototypeTheme.display,
                fontSize = 24.sp,
                letterSpacing = 6.sp,
            )
            Text(
                "ANDROID SIGNAL PATH",
                color = PrototypeTheme.amber,
                fontFamily = PrototypeTheme.body,
                fontWeight = FontWeight.Bold,
                fontSize = 10.sp,
                letterSpacing = 2.sp,
            )
        }
        Column(Modifier.align(Alignment.BottomStart)) {
            Text(
                "01  AUDIO ACCESS",
                color = if (state.setupStep == SetupStep.Permission) PrototypeTheme.teal else PrototypeTheme.muted,
                fontFamily = PrototypeTheme.body,
                fontSize = 10.sp,
            )
            Text(
                "02  USB INTERFACE",
                color = if (state.setupStep == SetupStep.Usb) PrototypeTheme.teal else PrototypeTheme.muted,
                fontFamily = PrototypeTheme.body,
                fontSize = 10.sp,
            )
            Text("03  INPUT + AMP", color = PrototypeTheme.muted, fontFamily = PrototypeTheme.body, fontSize = 10.sp)
        }
    }
}

@Composable
private fun PermissionStep(
    state: PrototypeState,
    dispatch: (PrototypeAction) -> Unit,
    requestMicrophonePermission: ((Boolean) -> Unit) -> Unit,
) {
    Column(
        Modifier.fillMaxWidth(.86f),
        verticalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        Text("MICROPHONE ACCESS", color = PrototypeTheme.amber, fontFamily = PrototypeTheme.body, fontSize = 10.sp)
        Text(
            "Hear the guitar.\nNothing else.",
            color = PrototypeTheme.text,
            fontFamily = PrototypeTheme.display,
            fontSize = 18.sp,
            lineHeight = 24.sp,
        )
        Text(
            "VoLum needs live audio input to tune and process your instrument. Audio stays on this device.",
            color = PrototypeTheme.muted,
            fontFamily = PrototypeTheme.body,
            fontSize = 11.sp,
            lineHeight = 15.sp,
            maxLines = 2,
        )
        state.errorMessage?.let {
            Text(
                it,
                color = PrototypeTheme.red,
                fontFamily = PrototypeTheme.body,
                fontSize = 9.sp,
                lineHeight = 12.sp,
                maxLines = 2,
                overflow = TextOverflow.Ellipsis,
            )
        }
        PrototypeButton(
            "Allow microphone",
            onClick = {
                if (state.microphoneGranted) {
                    dispatch(PrototypeAction.GrantPermission)
                } else {
                    requestMicrophonePermission { granted ->
                        dispatch(if (granted) PrototypeAction.GrantPermission else PrototypeAction.DenyPermission)
                    }
                }
            },
            modifier = Modifier.width(220.dp),
            primary = true,
        )
    }
}

@Composable
private fun UsbStep(
    state: PrototypeState,
    dispatch: (PrototypeAction) -> Unit,
) {
    Column(
        Modifier.fillMaxWidth(.86f),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        Text("USB AUDIO", color = PrototypeTheme.amber, fontFamily = PrototypeTheme.body, fontSize = 10.sp)
        Text(
            if (state.usbConnected) "Interface connected." else "Connect your interface.",
            color = PrototypeTheme.text,
            fontFamily = PrototypeTheme.display,
            fontSize = 21.sp,
        )
        Row(
            Modifier
                .fillMaxWidth()
                .height(72.dp)
                .background(PrototypeTheme.inset, RoundedCornerShape(10.dp))
                .border(
                    1.dp,
                    if (state.usbConnected) PrototypeTheme.teal else PrototypeTheme.line,
                    RoundedCornerShape(10.dp),
                )
                .padding(12.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            Box(
                Modifier
                    .width(10.dp)
                    .height(10.dp)
                    .background(if (state.usbConnected) PrototypeTheme.teal else PrototypeTheme.red, CircleShape),
            )
            Column {
                Text(
                    if (state.usbConnected) "AUDIENT EVO 4" else "NO USB INTERFACE",
                    color = PrototypeTheme.text,
                    fontFamily = PrototypeTheme.display,
                    fontSize = 10.sp,
                )
                Text(
                    if (state.usbConnected) "Ready to choose an input" else "USB-C OTG · class-compliant interface",
                    color = PrototypeTheme.muted,
                    fontFamily = PrototypeTheme.body,
                    fontSize = 10.sp,
                )
            }
        }
        PrototypeButton(
            if (state.usbConnected) "Choose input" else "Simulate connection",
            onClick = { dispatch(PrototypeAction.ConnectUsb) },
            modifier = Modifier.width(220.dp),
            primary = true,
        )
    }
}

@Composable
fun CatalogBrowser(
    state: PrototypeState,
    dispatch: (PrototypeAction) -> Unit,
) {
    val kind = state.browserKind ?: return
    val items = browserCatalogs.getValue(kind)
    val selected = items.firstOrNull { it.id == state.browserSelectedId } ?: items.first()
    val largeText = LocalDensity.current.fontScale > 1.15f
    Column(Modifier.fillMaxSize().background(PrototypeTheme.canvas)) {
        Row(
            Modifier
                .fillMaxWidth()
                .height(54.dp)
                .background(PrototypeTheme.panel)
                .border(1.dp, PrototypeTheme.line)
                .padding(horizontal = 12.dp, vertical = 5.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(10.dp),
        ) {
            PrototypeButton("Back", { dispatch(PrototypeAction.CancelBrowser) }, Modifier.width(74.dp))
            Column(Modifier.weight(1f)) {
                Text(kind.title.uppercase(), color = PrototypeTheme.text, fontFamily = PrototypeTheme.display, fontSize = 13.sp)
                if (!largeText) {
                    Text(
                        "Browse on the left. Nothing changes until Apply.",
                        color = PrototypeTheme.muted,
                        fontFamily = PrototypeTheme.body,
                        fontSize = 9.sp,
                    )
                }
            }
            Text(
                "${items.size} ITEMS",
                color = PrototypeTheme.amber,
                fontFamily = PrototypeTheme.body,
                fontSize = 9.sp,
            )
        }
        Row(Modifier.fillMaxSize()) {
            LazyColumn(
                Modifier
                    .weight(.48f)
                    .fillMaxHeight()
                    .background(PrototypeTheme.inset),
                contentPadding = PaddingValues(10.dp),
                verticalArrangement = Arrangement.spacedBy(7.dp),
            ) {
                items(items, key = { it.id }) { item ->
                    BrowserRow(
                        item = item,
                        selected = item.id == selected.id,
                        onClick = { dispatch(PrototypeAction.SelectBrowserItem(item.id)) },
                    )
                }
            }
            BrowserPreview(
                kind = kind,
                item = selected,
                onApply = { dispatch(PrototypeAction.ApplyBrowserItem) },
                modifier = Modifier.weight(.52f),
            )
        }
    }
}

@Composable
private fun BrowserRow(
    item: BrowserItem,
    selected: Boolean,
    onClick: () -> Unit,
) {
    Row(
        Modifier
            .fillMaxWidth()
            .height(64.dp)
            .background(
                if (selected) PrototypeTheme.teal.copy(.14f) else PrototypeTheme.panel,
                RoundedCornerShape(9.dp),
            )
            .border(
                1.dp,
                if (selected) PrototypeTheme.teal else PrototypeTheme.line,
                RoundedCornerShape(9.dp),
            )
            .clickable(role = Role.Button, onClick = onClick)
            .semantics {
                role = Role.Button
                contentDescription = item.name
            }
            .then(
                Modifier.background(
                    if (selected) PrototypeTheme.teal.copy(.02f) else PrototypeTheme.panel,
                    RoundedCornerShape(9.dp),
                ),
            )
            .padding(horizontal = 12.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        PrototypeButton(
            if (selected) "Active" else "Select",
            onClick,
            Modifier.width(82.dp),
            active = selected,
        )
        Column(Modifier.weight(1f)) {
            Text(
                item.name,
                color = PrototypeTheme.text,
                fontFamily = PrototypeTheme.display,
                fontSize = 9.sp,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
            Text(item.subtitle, color = PrototypeTheme.muted, fontFamily = PrototypeTheme.body, fontSize = 9.sp)
        }
        Text(item.tag, color = PrototypeTheme.amber, fontFamily = PrototypeTheme.body, fontSize = 8.sp)
    }
}

@Composable
private fun BrowserPreview(
    kind: BrowserKind,
    item: BrowserItem,
    onApply: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val largeText = LocalDensity.current.fontScale > 1.15f
    Column(
        modifier
            .fillMaxHeight()
            .background(
                Brush.radialGradient(
                    listOf(PrototypeTheme.amber.copy(.07f), PrototypeTheme.panel),
                ),
            )
            .padding(if (largeText) 12.dp else 16.dp),
        verticalArrangement = Arrangement.spacedBy(if (largeText) 4.dp else 8.dp),
    ) {
        Text("PREVIEW", color = PrototypeTheme.amber, fontFamily = PrototypeTheme.body, fontSize = 9.sp)
        Text(
            item.name,
            color = PrototypeTheme.text,
            fontFamily = PrototypeTheme.display,
            fontSize = if (largeText) 14.sp else 16.sp,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
        )
        Text(
            item.subtitle,
            color = PrototypeTheme.teal,
            fontFamily = PrototypeTheme.body,
            fontSize = if (largeText) 9.sp else 10.sp,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
        )
        Text(
            item.detail,
            color = PrototypeTheme.muted,
            fontFamily = PrototypeTheme.body,
            fontSize = if (largeText) 9.sp else 11.sp,
            lineHeight = if (largeText) 12.sp else 15.sp,
            maxLines = 2,
            overflow = TextOverflow.Ellipsis,
        )
        Spacer(Modifier.weight(1f))
        if (!largeText) {
            Text(
                if (kind == BrowserKind.Device) "Input changes after confirmation."
                else "The current live rig remains active while this item loads.",
                color = PrototypeTheme.muted,
                fontFamily = PrototypeTheme.body,
                fontSize = 9.sp,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
        }
        PrototypeButton(
            if (kind == BrowserKind.Device) "Use this input" else "Load / apply",
            onApply,
            Modifier.fillMaxWidth(),
            primary = item.id != "broken",
            danger = item.id == "broken",
        )
    }
}

@Composable
fun LoadingExperience(
    state: PrototypeState,
    dispatch: (PrototypeAction) -> Unit,
) {
    LaunchedEffect(state.loadingLabel) {
        delay(900)
        dispatch(PrototypeAction.FinishLoading)
    }
    Box(
        Modifier
            .fillMaxSize()
            .background(PrototypeTheme.canvas),
        contentAlignment = Alignment.Center,
    ) {
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(22.dp)) {
            CircularProgressIndicator(color = PrototypeTheme.amber, strokeWidth = 3.dp, modifier = Modifier.width(48.dp))
            Column {
                Text("PLEASE WAIT", color = PrototypeTheme.amber, fontFamily = PrototypeTheme.body, fontSize = 9.sp)
                Text(state.loadingLabel, color = PrototypeTheme.text, fontFamily = PrototypeTheme.display, fontSize = 18.sp)
                Text(
                    "Your previous live rig remains unchanged until verification completes.",
                    color = PrototypeTheme.muted,
                    fontFamily = PrototypeTheme.body,
                    fontSize = 11.sp,
                )
            }
        }
    }
}

@Composable
fun ErrorExperience(
    state: PrototypeState,
    dispatch: (PrototypeAction) -> Unit,
) {
    Box(Modifier.fillMaxSize().background(PrototypeTheme.canvas), contentAlignment = Alignment.Center) {
        Column(
            Modifier
                .width(520.dp)
                .background(PrototypeTheme.panel, RoundedCornerShape(14.dp))
                .border(1.dp, PrototypeTheme.red, RoundedCornerShape(14.dp))
                .padding(22.dp),
            verticalArrangement = Arrangement.spacedBy(10.dp),
        ) {
            Text("LOAD FAILED · RIG SAFE", color = PrototypeTheme.red, fontFamily = PrototypeTheme.body, fontSize = 10.sp)
            Text("Could not verify this model.", color = PrototypeTheme.text, fontFamily = PrototypeTheme.display, fontSize = 17.sp)
            Text(
                state.errorMessage ?: "The selected item could not be loaded.",
                color = PrototypeTheme.muted,
                fontFamily = PrototypeTheme.body,
                fontSize = 12.sp,
            )
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                PrototypeButton("Back to rig", { dispatch(PrototypeAction.RecoverToWorkspace) }, Modifier.weight(1f))
                PrototypeButton("Retry", { dispatch(PrototypeAction.RetryLoading) }, Modifier.weight(1f), primary = true)
            }
        }
    }
}

@Composable
fun TunerExperience(
    state: PrototypeState,
    dispatch: (PrototypeAction) -> Unit,
) {
    Box(
        Modifier
            .fillMaxSize()
            .background(
                Brush.radialGradient(
                    listOf(PrototypeTheme.teal.copy(.13f), PrototypeTheme.canvas),
                ),
            ),
    ) {
        PrototypeButton(
            "Close tuner",
            { dispatch(PrototypeAction.CloseTuner) },
            Modifier.align(Alignment.TopStart).padding(12.dp).width(112.dp),
        )
        Column(Modifier.align(Alignment.Center), horizontalAlignment = Alignment.CenterHorizontally) {
            Text("TUNER · INPUT MUTED", color = PrototypeTheme.teal, fontFamily = PrototypeTheme.body, fontSize = 10.sp)
            Text("A", color = PrototypeTheme.text, fontFamily = PrototypeTheme.display, fontSize = 72.sp)
            Text("440.0 Hz", color = PrototypeTheme.muted, fontFamily = PrototypeTheme.body, fontSize = 14.sp)
            Spacer(Modifier.height(14.dp))
            Canvas(Modifier.width(440.dp).height(34.dp).semantics { contentDescription = "Tuner centered, zero cents" }) {
                val center = size.width / 2
                drawLine(PrototypeTheme.lineBright, Offset(0f, size.height / 2), Offset(size.width, size.height / 2), 2.dp.toPx())
                repeat(9) { tick ->
                    val x = size.width * tick / 8f
                    drawLine(
                        PrototypeTheme.lineBright,
                        Offset(x, size.height * .32f),
                        Offset(x, size.height * .68f),
                        1.dp.toPx(),
                    )
                }
                drawLine(
                    PrototypeTheme.teal,
                    Offset(center, 0f),
                    Offset(center, size.height),
                    4.dp.toPx(),
                    StrokeCap.Round,
                )
            }
            Text("0 cents · IN TUNE", color = PrototypeTheme.teal, fontFamily = PrototypeTheme.display, fontSize = 10.sp)
        }
        ValueBadge(
            "Input",
            state.selectedDevice,
            Modifier.align(Alignment.BottomStart).padding(12.dp).width(220.dp),
        )
    }
}
