package com.volum.design.theme

import androidx.compose.ui.unit.dp

/**
 * `VoLumMetrics` of `VoLumColorHelpers.h` plus the stroke weights and bracket
 * sizes the desktop controls repeat. One desktop px = 1 dp.
 */
object VoLumMetrics {
    val Xs = 4.dp
    val Sm = 8.dp
    val Md = 12.dp
    val Lg = 18.dp
    val Xl = 28.dp

    val Radius = 4.dp
    val RadiusLg = 8.dp
    val RadiusSelection = 3.dp

    val Hairline = 1.dp
    val StrokeFocus = 1.6.dp
    val StrokeFrame = 2.2.dp

    /** Gold L-brackets: window 22 / hero 16 / lane 12 / card 8 / quiet block 6, inset 6 / 6 / 6 / 4 / 3. */
    val BracketWindow = 22.dp
    val BracketHero = 16.dp
    val BracketLane = 12.dp
    val BracketCard = 8.dp
    val BracketQuiet = 6.dp

    /** Smallest touch target any operable control may have (`TouchTargetTest`). */
    val TouchMin = 48.dp
}

/** Motion constants. Desktop cuts are instant; these are the shortest fades that still read as a cut at 6 fps. */
object VoLumMotion {
    const val PressMs = 90
    const val ToggleMs = 110
    const val PanelMs = 180
}
