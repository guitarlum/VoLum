package com.lum.volum.ui.prototype

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class PrototypeModelsTest {
    @Test
    fun setupWalkthroughReachesWorkspaceThroughExplicitSelections() {
        var state = PrototypeState()
        state = reducePrototype(state, PrototypeAction.GrantPermission)
        assertEquals(SetupStep.Usb, state.setupStep)

        state = reducePrototype(state, PrototypeAction.ConnectUsb)
        assertEquals(PrototypeRoute.Browser, state.route)
        assertEquals(BrowserKind.Device, state.browserKind)

        state = reducePrototype(state, PrototypeAction.ApplyBrowserItem)
        assertEquals(BrowserKind.Amp, state.browserKind)

        state = reducePrototype(state, PrototypeAction.ApplyBrowserItem)
        assertEquals(PrototypeRoute.Loading, state.route)

        state = reducePrototype(state, PrototypeAction.FinishLoading)
        assertEquals(PrototypeRoute.Workspace, state.route)
        assertTrue(state.modelLoaded)
        assertTrue(state.powered)
    }

    @Test
    fun sectionAndBlockFocusAreMutuallyExclusiveAndClearSafely() {
        var state = readyState()
        state = reducePrototype(state, PrototypeAction.SelectSection(RigSection.Pre))
        assertEquals(RigSection.Pre, state.selectedSection)
        assertNull(state.selectedBlock)

        state = reducePrototype(state, PrototypeAction.SelectBlock(RigBlock.Compressor))
        assertNull(state.selectedSection)
        assertEquals(RigBlock.Compressor, state.selectedBlock)

        state = reducePrototype(state, PrototypeAction.ClearSelection)
        assertNull(state.selectedSection)
        assertNull(state.selectedBlock)
        assertEquals(PrototypeRoute.Workspace, state.route)
    }

    @Test
    fun disabledSupportLaneCannotBeToggledUntilEnabled() {
        var state = readyState()
        state = reducePrototype(state, PrototypeAction.ToggleBlock(RigBlock.SupportAmp))
        assertFalse(state.blocks.getValue(RigBlock.SupportAmp))

        state = reducePrototype(state, PrototypeAction.ToggleSupport)
        assertTrue(state.supportEnabled)
        assertTrue(state.blocks.getValue(RigBlock.SupportAmp))
        assertTrue(state.blocks.getValue(RigBlock.SupportIr))
    }

    @Test
    fun blockPowerDoesNotChangeSelection() {
        var state = readyState().copy(selectedBlock = RigBlock.Delay)
        state = reducePrototype(state, PrototypeAction.ToggleBlock(RigBlock.Reverb))
        assertEquals(RigBlock.Delay, state.selectedBlock)
        assertFalse(state.blocks.getValue(RigBlock.Reverb))
    }

    @Test
    fun browserDoesNotApplyUntilExplicitConfirmation() {
        val initial = readyState()
        var state = reducePrototype(
            initial,
            PrototypeAction.OpenBrowser(BrowserKind.Amp),
        )
        state = reducePrototype(state, PrototypeAction.SelectBrowserItem("california"))
        assertEquals(initial.selectedAmp, state.selectedAmp)

        state = reducePrototype(state, PrototypeAction.ApplyBrowserItem)
        assertEquals("California Lead", state.selectedAmp)
        assertEquals(PrototypeRoute.Loading, state.route)
    }

    @Test
    fun failedModelPreservesCurrentRigAndHasRecovery() {
        val initial = readyState()
        var state = reducePrototype(initial, PrototypeAction.OpenBrowser(BrowserKind.Amp))
        state = reducePrototype(state, PrototypeAction.SelectBrowserItem("broken"))
        state = reducePrototype(state, PrototypeAction.ApplyBrowserItem)

        assertEquals(PrototypeRoute.Error, state.route)
        assertEquals(initial.selectedAmp, state.selectedAmp)
        assertTrue(state.errorMessage?.contains("unchanged") == true)

        state = reducePrototype(state, PrototypeAction.RecoverToWorkspace)
        assertEquals(PrototypeRoute.Workspace, state.route)
        assertNull(state.errorMessage)
    }

    @Test
    fun parameterUndoReturnsToGestureStartValue() {
        var state = readyState()
        val original = state.parameters.getValue(RigParameter.MainInput)
        state = reducePrototype(state, PrototypeAction.BeginParameterEdit(RigParameter.MainInput))
        state = reducePrototype(state, PrototypeAction.SetParameter(RigParameter.MainInput, .93f))
        assertEquals(.93f, state.parameters.getValue(RigParameter.MainInput))

        state = reducePrototype(state, PrototypeAction.UndoParameterEdit)
        assertEquals(original, state.parameters.getValue(RigParameter.MainInput))
        assertNull(state.undoEdit)
    }

    @Test
    fun liveControlsModesAndSectionLocksRemainIndependent() {
        var state = readyState()
        state = reducePrototype(state, PrototypeAction.ToggleBypass)
        state = reducePrototype(state, PrototypeAction.TogglePower)
        state = reducePrototype(state, PrototypeAction.SetMode(RigBlock.Delay, 2))
        state = reducePrototype(state, PrototypeAction.ToggleSectionLock(RigSection.Pre))

        assertTrue(state.bypassed)
        assertFalse(state.powered)
        assertEquals(2, state.modes.getValue(RigBlock.Delay))
        assertTrue(state.preLocked)
        assertFalse(state.postLocked)
    }

    @Test
    fun deniedPermissionRemainsRecoverable() {
        val state = reducePrototype(PrototypeState(), PrototypeAction.DenyPermission)
        assertEquals(PrototypeRoute.Setup, state.route)
        assertEquals(SetupStep.Permission, state.setupStep)
        assertFalse(state.microphoneGranted)
        assertTrue(state.errorMessage?.contains("retry") == true)
    }

    @Test
    fun ampLanesKeepChannelsFeaturesAndPolarityIndependent() {
        var state = readyState()
        state = reducePrototype(state, PrototypeAction.StepChannel(AmpLane.Main, 1))
        state = reducePrototype(state, PrototypeAction.ToggleSupport)
        state = reducePrototype(state, PrototypeAction.SelectLane(AmpLane.Support))
        state = reducePrototype(state, PrototypeAction.StepChannel(AmpLane.Support, -1))
        state = reducePrototype(state, PrototypeAction.ToggleLaneFeature(AmpLane.Support, LaneFeature.Eq))
        state = reducePrototype(state, PrototypeAction.ToggleSupportPolarity)

        assertEquals(2, state.mainChannel)
        assertEquals(1, state.supportChannel)
        assertTrue(state.mainEqEnabled)
        assertFalse(state.supportEqEnabled)
        assertTrue(state.supportPolarityInverted)
        assertEquals(AmpLane.Support, state.selectedLane)
    }

    @Test
    fun lockedSceneMarksDirtyAndStoreClearsIt() {
        var state = readyState()
        state = reducePrototype(state, PrototypeAction.ToggleSectionLock(RigSection.Pre))
        state = reducePrototype(state, PrototypeAction.SetParameter(RigParameter.CompAmount, .8f))
        assertTrue(state.preDirty)

        state = reducePrototype(state, PrototypeAction.StoreSection(RigSection.Pre))
        assertFalse(state.preDirty)
        assertTrue(state.feedbackMessage?.contains("PRE stored") == true)
    }

    @Test
    fun settingsOwnDeviceAndPerformanceConfiguration() {
        var state = reducePrototype(readyState(), PrototypeAction.OpenSettings)
        assertEquals(PrototypeRoute.Settings, state.route)
        state = reducePrototype(state, PrototypeAction.SetBufferFrames(128))
        state = reducePrototype(state, PrototypeAction.SetOutputMode(2))
        state = reducePrototype(
            state,
            PrototypeAction.OpenBrowser(
                BrowserKind.Device,
                BrowserApply.ReturnWorkspace,
                PrototypeRoute.Settings,
            ),
        )
        state = reducePrototype(state, PrototypeAction.ApplyBrowserItem)
        assertEquals(PrototypeRoute.Settings, state.route)
        assertEquals(128, state.bufferFrames)
        assertEquals(2, state.outputMode)
    }

    @Test
    fun tempoFeaturesRetainModeSpecificOptions() {
        var state = readyState()
        state = reducePrototype(state, PrototypeAction.ToggleDelaySync)
        state = reducePrototype(state, PrototypeAction.StepDelayDivision(1))
        state = reducePrototype(state, PrototypeAction.ToggleDelayPingPong)
        state = reducePrototype(state, PrototypeAction.ToggleTremoloSync)
        state = reducePrototype(state, PrototypeAction.SetReverbSubMode(2))

        assertTrue(state.delaySync)
        assertEquals(5, state.delayDivision)
        assertTrue(state.delayPingPong)
        assertTrue(state.tremoloSync)
        assertEquals(2, state.reverbSubMode)
    }

    @Test
    fun ampEditsMarkPresetDirtyAndOverwriteClearsIt() {
        var state = reducePrototype(readyState(), PrototypeAction.StepChannel(AmpLane.Main, 1))
        assertTrue(state.presetDirty)
        state = reducePrototype(state, PrototypeAction.OverwritePreset)
        assertFalse(state.presetDirty)
    }

    @Test
    fun libraryManagementUsesStagedEditorFlow() {
        var state = reducePrototype(
            readyState(),
            PrototypeAction.OpenBrowser(
                BrowserKind.Amp,
                BrowserApply.ReturnWorkspace,
                PrototypeRoute.Settings,
                manage = true,
            ),
        )
        state = reducePrototype(state, PrototypeAction.AddLibraryItem)
        assertEquals(PrototypeRoute.ContentEditor, state.route)
        state = reducePrototype(state, PrototypeAction.StageContentFiles)
        assertTrue(state.contentFilesStaged)
        state = reducePrototype(state, PrototypeAction.SaveContentEditor)
        assertEquals(PrototypeRoute.Browser, state.route)
        assertTrue(state.feedbackMessage?.contains("imported") == true)
    }

    private fun readyState() = PrototypeState(
        route = PrototypeRoute.Workspace,
        microphoneGranted = true,
        usbConnected = true,
        modelLoaded = true,
        powered = true,
    )
}
