package com.lum.volum.ui.prototype

import androidx.activity.compose.BackHandler
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.remember
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.unit.Density

@Composable
fun VoLumPrototype(
    microphoneGranted: Boolean,
    requestMicrophonePermission: ((Boolean) -> Unit) -> Unit,
    onImmersiveChange: (Boolean) -> Unit,
    startEntry: String? = null,
) {
    val controller = remember {
        PrototypeController(initialState(microphoneGranted, startEntry))
    }
    val state = controller.state
    val dispatch: (PrototypeAction) -> Unit = controller::dispatch

    LaunchedEffect(state.route) {
        onImmersiveChange(state.route in setOf(PrototypeRoute.Workspace, PrototypeRoute.Tuner))
    }

    BackHandler(
        enabled = state.route != PrototypeRoute.Setup ||
            state.selectedBlock != null ||
            state.selectedSection != null,
    ) {
        when (state.route) {
            PrototypeRoute.Workspace -> dispatch(PrototypeAction.ClearSelection)
            PrototypeRoute.Browser -> dispatch(PrototypeAction.CancelBrowser)
            PrototypeRoute.Tuner -> dispatch(PrototypeAction.CloseTuner)
            PrototypeRoute.Error -> dispatch(PrototypeAction.RecoverToWorkspace)
            PrototypeRoute.Loading -> Unit
            PrototypeRoute.Setup -> Unit
        }
    }

    when (state.route) {
        PrototypeRoute.Setup -> SetupJourney(state, dispatch, requestMicrophonePermission)
        PrototypeRoute.Workspace -> StageFontScale {
            SignalPathWorkspace(state, dispatch)
        }
        PrototypeRoute.Browser -> CatalogBrowser(state, dispatch)
        PrototypeRoute.Tuner -> StageFontScale {
            TunerExperience(state, dispatch)
        }
        PrototypeRoute.Loading -> LoadingExperience(state, dispatch)
        PrototypeRoute.Error -> ErrorExperience(state, dispatch)
    }
}

@Composable
private fun StageFontScale(content: @Composable () -> Unit) {
    val current = LocalDensity.current
    val stageDensity = remember(current.density, current.fontScale) {
        Density(current.density, current.fontScale.coerceAtMost(1.1f))
    }
    CompositionLocalProvider(LocalDensity provides stageDensity, content = content)
}

private fun initialState(
    microphoneGranted: Boolean,
    startEntry: String?,
): PrototypeState {
    val base = PrototypeState(
        microphoneGranted = microphoneGranted,
        setupStep = if (microphoneGranted) SetupStep.Usb else SetupStep.Permission,
    )
    return when (startEntry?.lowercase()) {
        "workspace", "live" -> base.copy(
            route = PrototypeRoute.Workspace,
            microphoneGranted = true,
            usbConnected = true,
            modelLoaded = true,
            powered = true,
        )
        "pre" -> base.copy(
            route = PrototypeRoute.Workspace,
            microphoneGranted = true,
            usbConnected = true,
            modelLoaded = true,
            powered = true,
            selectedSection = RigSection.Pre,
        )
        "amp" -> base.copy(
            route = PrototypeRoute.Workspace,
            microphoneGranted = true,
            usbConnected = true,
            modelLoaded = true,
            powered = true,
            selectedBlock = RigBlock.MainAmp,
        )
        "post" -> base.copy(
            route = PrototypeRoute.Workspace,
            microphoneGranted = true,
            usbConnected = true,
            modelLoaded = true,
            powered = true,
            selectedSection = RigSection.Post,
        )
        "delay" -> base.copy(
            route = PrototypeRoute.Workspace,
            microphoneGranted = true,
            usbConnected = true,
            modelLoaded = true,
            powered = true,
            selectedBlock = RigBlock.Delay,
        )
        "support" -> base.copy(
            route = PrototypeRoute.Workspace,
            microphoneGranted = true,
            usbConnected = true,
            modelLoaded = true,
            powered = true,
            supportEnabled = true,
            blocks = base.blocks +
                (RigBlock.SupportAmp to true) +
                (RigBlock.SupportIr to true),
            selectedSection = RigSection.Amp,
        )
        "browser" -> base.copy(
            route = PrototypeRoute.Browser,
            microphoneGranted = true,
            usbConnected = true,
            modelLoaded = true,
            powered = true,
            browserKind = BrowserKind.Amp,
            browserSelectedId = browserCatalogs.getValue(BrowserKind.Amp).first().id,
            browserApply = BrowserApply.ReturnWorkspace,
            browserBack = PrototypeRoute.Workspace,
        )
        "tuner" -> base.copy(
            route = PrototypeRoute.Tuner,
            microphoneGranted = true,
            usbConnected = true,
            modelLoaded = true,
            powered = true,
        )
        "error" -> base.copy(
            route = PrototypeRoute.Error,
            microphoneGranted = true,
            usbConnected = true,
            modelLoaded = true,
            powered = true,
            loadingLabel = "Loading Unverified model",
            errorMessage = "The amp model could not be verified. Your live rig is unchanged.",
        )
        else -> base
    }
}
