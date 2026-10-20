package com.volum.ui.b

import androidx.compose.ui.unit.dp
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class LayoutRulesTest {
    @Test
    fun channelStepperKeepsRoomForItsValue() {
        val value = stepperValueWidth(ChannelColumn)
        assertTrue("the CHANNEL value gets $value of $ChannelColumn", value >= 40.dp)
    }

    @Test
    fun everyStepperWideEnoughForItsTwoTargetsShowsItsValue() {
        for (w in 96..600 step 4) assertTrue("stepper $w dp", stepperValueWidth(w.dp) >= 40.dp)
    }

    @Test
    fun bankArrowsAndSoundStepsAreLiveSized() {
        assertTrue(BankPadWidth >= 64.dp)
        assertTrue(StepPadWidth >= 64.dp)
    }

    @Test
    fun visibleCatalogRowsLandWholeInsideTheViewport() {
        for (density in listOf(2.0f, 2.625f, 3.0f, 3.5f)) {
            fun px(dp: Float) = Math.round(dp * density)
            for (heightDp in 120..420) {
                for ((minRow, gap) in listOf(52f to 4f, 56f to 4f, 56f to 6f, 48f to 6f, 72f to 8f)) {
                    val h = px(heightDp.toFloat())
                    val g = px(gap)
                    for (rows in listOf(1, 3, 5, 12)) {
                        val fit = gridRows(h, g, px(minRow), px(minRow * 1.4f), rows)
                        val shown = minOf(rows, fit.visible)
                        val used = shown * fit.rowPx + (shown - 1) * g
                        assertTrue("$rows rows of min $minRow dp in $heightDp dp at ${density}x use $used of $h px", used <= h)
                        if (h >= px(minRow)) assertTrue("rows keep their minimum", fit.rowPx >= px(minRow))
                    }
                }
            }
        }
    }

    @Test
    fun theFloorFollowsARecallOntoAnotherBank() {
        assertEquals(1, bankToFollow(before = 5, live = 12, shown = 0))
        assertNull("a recall on the bank shown stays", bankToFollow(before = 5, live = 3, shown = 0))
        assertNull("no LIVE, no jump", bankToFollow(before = 5, live = null, shown = 2))
        assertNull("LIVE unchanged keeps the bank the player paged to", bankToFollow(before = 5, live = 5, shown = 3))
        assertEquals("opening PLAY brings LIVE's bank up", 2, bankToFollow(before = null, live = 17, shown = 0))
    }
}
