package com.volum.core

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

/** The Dual Amp SUPPORT lane takes custom IRs and custom amps the way MAIN does (desktop `kSupportIRToggle`, `supportActiveIrId`). */
class SupportLaneTest {
    private fun store(fixture: String) = VoLumStore(Fixtures.build(fixture))

    @Test
    fun theIrMenuPlaysOnTheFocusedLane() {
        val s = store("dual_amp_support")
        val main = s.current.rig
        s.dispatch(Action.Open(Overlay.IrMenu))
        s.dispatch(Action.SelectIr("ir:royer"))
        assertEquals(Cab.CUSTOM_IR, s.current.rig.support.cab)
        assertEquals("ir:royer", s.current.rig.support.irId)
        assertEquals("MAIN keeps its cab", main.cab, s.current.rig.cab)
        assertEquals("MAIN keeps its IR", main.irId, s.current.rig.irId)
        assertTrue("picking closes the menu", s.current.ui.top !is Overlay.IrMenu)

        val m = store("dual_amp")
        m.dispatch(Action.SelectIr("ir:royer"))
        assertEquals(Cab.CUSTOM_IR, m.current.rig.cab)
        assertNull("SUPPORT is untouched while MAIN is focused", m.current.rig.support.irId)
    }

    @Test
    fun selectSupportIrNeedsANoCabChannelAndAKnownIr() {
        val s = store("dual_custom_support_no_direct")
        val before = s.current
        s.dispatch(Action.SelectSupportIr("ir:royer"))
        assertEquals("a channel without a No Cab capture refuses Custom IR", before, s.current)
        s.dispatch(Action.SelectSupportCab(Cab.CUSTOM_IR))
        assertEquals(before, s.current)
        val ok = store("dual_amp_support")
        ok.dispatch(Action.SelectSupportIr("ir:missing"))
        assertEquals(Cab.G12, ok.current.rig.support.cab)
        ok.dispatch(Action.SelectSupportCab(Cab.CUSTOM_IR))
        assertEquals("Custom IR with no IR picked yet is refused", Cab.G12, ok.current.rig.support.cab)
        ok.dispatch(Action.SelectSupportIr("ir:royer"))
        assertEquals(Cab.CUSTOM_IR, ok.current.rig.support.cab)
    }

    @Test
    fun leavingANoCabChannelOrTheAmpDropsTheSupportIr() {
        val s = store("dual_amp_empty_support")
        s.dispatch(Action.SelectSupportAmp("c:1"))
        s.dispatch(Action.FocusLane(Lane.SUPPORT))
        s.dispatch(Action.SelectSupportIr("ir:royer"))
        assertEquals(Cab.CUSTOM_IR, s.current.rig.support.cab)
        s.dispatch(Action.SelectSupportChannel(1))
        assertNull(s.current.rig.support.irId)
        assertEquals("the custom amp's real cab on that channel", Cab.G12, s.current.rig.support.cab)

        val t = store("dual_support_ir")
        t.dispatch(Action.SelectSupportAmp("c:1"))
        assertNull(t.current.rig.support.irId)
        assertTrue("a new SUPPORT amp never sits on Custom IR without an IR", t.current.rig.support.cab != Cab.CUSTOM_IR)
    }
}
