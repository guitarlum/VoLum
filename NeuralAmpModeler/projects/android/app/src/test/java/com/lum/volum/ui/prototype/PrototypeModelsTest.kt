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
        val original = state.parameters.getValue(RigParameter.Drive)
        state = reducePrototype(state, PrototypeAction.BeginParameterEdit(RigParameter.Drive))
        state = reducePrototype(state, PrototypeAction.SetParameter(RigParameter.Drive, .93f))
        assertEquals(.93f, state.parameters.getValue(RigParameter.Drive))

        state = reducePrototype(state, PrototypeAction.UndoParameterEdit)
        assertEquals(original, state.parameters.getValue(RigParameter.Drive))
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

    private fun readyState() = PrototypeState(
        route = PrototypeRoute.Workspace,
        microphoneGranted = true,
        usbConnected = true,
        modelLoaded = true,
        powered = true,
    )
}
