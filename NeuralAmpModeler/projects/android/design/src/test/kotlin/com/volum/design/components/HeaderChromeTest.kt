package com.volum.design.components

import com.volum.design.theme.VoLumColors
import com.volum.design.theme.ic
import com.volum.design.theme.withOpacity
import org.junit.Assert.assertEquals
import org.junit.Test

class HeaderChromeTest {
    @Test
    fun presetBarSaysWhatTheDesktopBarSays() {
        assertEquals("No Preset", presetBarLabel(null, false))
        assertEquals("No Preset", presetBarLabel("", false))
        assertEquals("No Preset  (unsaved)", presetBarLabel(null, true))
        assertEquals("Sunset Crunch", presetBarLabel("Sunset Crunch", false))
        assertEquals("Sunset Crunch  (unsaved)", presetBarLabel("Sunset Crunch", true))
    }

    @Test
    fun presetBarInkIsAmberWhenUnsavedAndDimForACleanEmptyBar() {
        assertEquals(VoLumColors.Amber, presetBarInk(null, true))
        assertEquals(VoLumColors.Amber, presetBarInk("Sunset Crunch", true))
        assertEquals(VoLumColors.CreamDim, presetBarInk(null, false))
        assertEquals(VoLumColors.TextBright, presetBarInk("Sunset Crunch", false))
    }

    @Test
    fun padlockInkFollowsTheTriptychHeader() {
        assertEquals(VoLumColors.Gold, lockInk(locked = true, hot = true))
        assertEquals(VoLumColors.Gold.withOpacity(0.72f), lockInk(locked = true, hot = false))
        assertEquals(VoLumColors.TextBright, lockInk(locked = false, hot = true))
        assertEquals(ic(255, 118, 110, 100), lockInk(locked = false, hot = false))
    }
}
