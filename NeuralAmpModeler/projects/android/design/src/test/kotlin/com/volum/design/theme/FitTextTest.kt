package com.volum.design.theme

import org.junit.Assert.assertEquals
import org.junit.Test

class FitTextTest {
    @Test
    fun anOverflowingLineJumpsToTheSizeItsWidthCallsFor() {
        assertEquals(10f, fitSize(13f, available = 100f, needed = 130f, min = 8f), 0f)
        assertEquals(12f, fitSize(13f, available = 120f, needed = 130f, min = 8f), 0f)
    }

    @Test
    fun theNextSizeIsOnTheHalfSpGridAndAtLeastOneStepDown() {
        assertEquals(12.5f, fitSize(13f, available = 129f, needed = 130f, min = 8f), 0f)
        assertEquals(12.5f, fitSize(13f, available = 200f, needed = 130f, min = 8f), 0f)
        assertEquals(12.5f, fitSize(13f, available = 100f, needed = 0f, min = 8f), 0f)
    }

    @Test
    fun itNeverGoesBelowTheFloor() {
        assertEquals(8f, fitSize(13f, available = 10f, needed = 130f, min = 8f), 0f)
        assertEquals(10.4f, fitSize(13f, available = 50f, needed = 130f, min = 10.4f), 0f)
    }
}
