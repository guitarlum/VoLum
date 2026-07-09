package com.lum.volum.ui.proposals

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotSame
import org.junit.Assert.assertTrue
import org.junit.Test

class ProposalModelsTest {
    @Test
    fun registryHasEightDistinctArchitectures() {
        assertEquals(8, proposalSpecs.size)
        assertEquals(8, proposalSpecs.map { it.id }.distinct().size)
        assertEquals(4, proposalSpecs.count { it.orientation == ProposalOrientation.Portrait })
        assertEquals(4, proposalSpecs.count { it.orientation == ProposalOrientation.Landscape })
        assertEquals(4, proposalSpecs.count { it.currentVibe })
    }

    @Test
    fun walkthroughCoversPermissionUsbDeviceModelAndLive() {
        var state = reduceDemo(DemoSnapshot(), DemoAction.Enter(EntryPoint.Walkthrough, false))
        assertEquals(JourneyStep.Permission, state.step)
        state = reduceDemo(state, DemoAction.GrantPermission)
        assertEquals(JourneyStep.Usb, state.step)
        state = reduceDemo(state, DemoAction.ConnectUsb)
        assertEquals(JourneyStep.Device, state.step)
        state = reduceDemo(state, DemoAction.SelectDevice)
        assertEquals(JourneyStep.Model, state.step)
        state = reduceDemo(state, DemoAction.SelectModel)
        assertEquals(JourneyStep.Loading, state.step)
        assertTrue(state.busy)
        state = reduceDemo(state, DemoAction.FinishLoading)
        assertEquals(JourneyStep.Live, state.step)
        assertTrue(state.modelLoaded)
        assertTrue(state.powered)
    }

    @Test
    fun errorRecoveryPreservesExplicitRetryPath() {
        val error = reduceDemo(DemoSnapshot(), DemoAction.ApplyScenario(DemoScenario.Error))
        assertEquals(JourneyStep.Error, error.step)
        assertTrue(error.errorMessage?.contains("unchanged") == true)
        val retry = reduceDemo(error, DemoAction.Retry)
        assertEquals(JourneyStep.Loading, retry.step)
        assertTrue(retry.busy)
    }

    @Test
    fun deniedPermissionStaysRecoverableBeforeUsbSetup() {
        val denied = reduceDemo(DemoSnapshot(), DemoAction.DenyPermission)
        assertEquals(JourneyStep.Permission, denied.step)
        assertFalse(denied.microphoneGranted)
        assertTrue(denied.errorMessage?.contains("still off") == true)
    }

    @Test
    fun controllersDoNotShareEdits() {
        val first = DemoController()
        val second = DemoController()
        first.dispatch(DemoAction.SetValue(Parameter.Drive, .91f))
        assertNotSame(first, second)
        assertEquals(.91f, first.state.drive)
        assertEquals(.61f, second.state.drive)
    }

    @Test
    fun safeBackReturnsToLiveAndClearsError() {
        var state = reduceDemo(DemoSnapshot(), DemoAction.ApplyScenario(DemoScenario.Error))
        state = reduceDemo(state, DemoAction.BackToLive)
        assertEquals(JourneyStep.Live, state.step)
        assertFalse(state.busy)
        assertEquals(null, state.errorMessage)
    }
}
