package com.volum.design.theme

import androidx.compose.ui.graphics.Color

/** Desktop `IColor(A, R, G, B)` argument order. */
fun ic(a: Int, r: Int, g: Int, b: Int): Color = Color(r, g, b, a)

/** Desktop `IColor::WithOpacity`: the alpha is replaced, not multiplied. */
fun Color.withOpacity(alpha: Float): Color = copy(alpha = ((alpha * 255f).toInt().coerceIn(0, 255)) / 255f)

/**
 * The desktop palette, one entry per `VoLumColors::` constant in
 * `VoLumColorHelpers.h` (Art Deco Noir) plus the knob/toggle style colours of
 * `NeuralAmpModeler.cpp` (`kGold`, `kGoldDim`, `kTeal`, `kTealBright`).
 * Nothing else is a VoLum colour; a candidate that needs a new one asks here.
 */
object VoLumColors {
    val Bg = ic(255, 17, 17, 24)
    val SidebarBg = ic(255, 17, 17, 24)
    val SidebarBg2 = ic(255, 12, 12, 20)
    val SidebarBorder = ic(50, 200, 162, 78)
    val ItemHover = ic(10, 200, 162, 78)
    val ItemSelBg = ic(18, 200, 162, 78)
    val ItemSelBorder = ic(51, 200, 162, 78)
    val TextDim = ic(255, 232, 218, 200)
    val TextMed = ic(255, 245, 232, 218)
    val TextBright = ic(255, 255, 248, 238)
    val Gold = ic(255, 252, 222, 145)
    val GoldDim = ic(255, 235, 210, 145)
    val MeterGreen = ic(255, 42, 138, 42)
    val Divider = ic(30, 200, 162, 78)
    val Frame = ic(72, 200, 162, 78)
    val Corner = ic(255, 200, 162, 78)
    val BtnOffBg = ic(5, 200, 162, 78)
    val BtnOffBorder = ic(40, 200, 162, 78)
    val BtnOffText = ic(255, 244, 228, 210)
    val BtnCabOnBg = ic(72, 115, 88, 52)
    val BtnCabOnBorder = ic(235, 225, 195, 115)
    val BtnAmpOnBg = ic(78, 48, 125, 118)
    val BtnAmpOnBorder = ic(215, 165, 230, 220)
    val BtnAmpOnText = ic(255, 252, 255, 248)
    val HeroBg = ic(255, 12, 12, 18)
    val HeroBorder = ic(50, 200, 162, 78)
    val HeroCorner = ic(102, 200, 162, 78)

    val Teal = ic(255, 91, 196, 196)
    val TealDim = ic(255, 75, 162, 162)
    val Amber = ic(255, 232, 168, 92)
    val Cream = ic(255, 237, 227, 208)
    val CreamDim = ic(255, 166, 149, 124)

    val CustomArtBright = ic(220, 120, 210, 220)
    val CustomArtDim = ic(205, 100, 178, 208)

    val BtnIrOnBg = ic(82, 196, 122, 80)
    val BtnIrOnBorder = ic(235, 226, 156, 112)
    val BtnIrOnText = ic(255, 255, 236, 214)

    val SelBg = ic(40, 200, 162, 78)
    val SelBgSoft = ic(20, 200, 162, 78)
    val SelBorder = ic(235, 226, 156, 112)
    val SelGlow = ic(64, 252, 222, 145)
    val SelText = ic(255, 255, 244, 224)

    val Danger = ic(255, 228, 92, 80)
    val DangerFill = ic(78, 228, 92, 80)
    val DangerGlow = ic(54, 228, 92, 80)

    val PanelTop = ic(255, 23, 23, 31)
    val PanelBot = ic(255, 13, 13, 19)
    val WellDark = ic(255, 9, 9, 14)
    val InnerShadow = ic(120, 0, 0, 0)
    val RimLight = ic(26, 255, 248, 238)

    /** Canvas gradient of `VoLumBackgroundControl` and the PLAY surface. */
    val CanvasTop = ic(255, 21, 21, 29)
    val CanvasBot = ic(255, 12, 12, 18)

    /** Knob / switch style colours (`NeuralAmpModeler.cpp`). */
    val KnobGold = ic(255, 200, 162, 78)
    val KnobGoldDim = ic(255, 138, 112, 48)
    /** `kGold.WithContrast(0.1f)`: +25 on every channel. */
    val KnobGoldHi = ic(255, 225, 187, 103)
    val KnobTeal = ic(255, 91, 196, 196)
    val KnobTealHi = ic(255, 156, 224, 224)
    val KnobGoldBright = ic(255, 252, 235, 218)

    /** Sunset-grid cyan of the hero arts (`volumart::kTeal`). */
    val GridCyan = ic(255, 120, 210, 220)

    /** Amber hover wash of `DrawVoLumSelection(AmberPicker)`. */
    val AmberHover = ic(48, 226, 165, 78)

    /** Dark ink on an amber selection (`SelectionInkColor(AmberPicker, true)`). */
    val AmberInk = ic(255, 26, 18, 8)

    /** Dimming scrim behind modals (`VoLumConfirmDialogControl`). */
    val Scrim = ic(190, 8, 10, 14)
}
