package com.volum.design.theme

import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.Font
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.LineHeightStyle
import androidx.compose.ui.unit.TextUnit
import androidx.compose.ui.unit.em
import androidx.compose.ui.unit.sp
import com.volum.design.R

/**
 * The desktop's four faces (`VoLumLayoutBuild.inc.cpp` LoadFont, macOS weights):
 * "Poiret-One", "Josefin-Sans" (Regular), "Josefin-Bold" (SemiBold) and
 * "Michroma-Regular". There is no other font in VoLum; `FontFamily.Default`
 * would be Roboto.
 */
object VoLumFonts {
    val Poiret = FontFamily(Font(R.font.poiret_one_regular))
    val Josefin = FontFamily(Font(R.font.josefin_sans_regular, FontWeight.Normal), Font(R.font.josefin_sans_semibold, FontWeight.SemiBold))
    val JosefinBold = FontFamily(Font(R.font.josefin_sans_semibold, FontWeight.SemiBold))
    val Michroma = FontFamily(Font(R.font.michroma_regular))
}

/**
 * `VoLumType::` of `VoLumColorHelpers.h`. Desktop sizes are logical pixels of a
 * 900x600 window; one desktop px is one dp here, so `size` is passed in sp at
 * the same number and scales with the system font size.
 *
 * - [display]: Poiret One. Logo, PLAY sound name, overlay titles.
 * - [label] / [caption]: Josefin Bold. Every control label, knob values, amp names, buttons.
 * - [value]: Michroma. Program numbers (00..127), MIDI channel / CC readouts.
 * - [body]: Josefin Sans. Footer status, dialog body copy.
 */
object VoLumType {
    private val tight = LineHeightStyle(LineHeightStyle.Alignment.Center, LineHeightStyle.Trim.Both)

    private fun style(family: FontFamily, weight: FontWeight, size: TextUnit, color: Color, letterSpacing: TextUnit = 0.sp) =
        TextStyle(fontFamily = family, fontWeight = weight, fontSize = size, color = color, lineHeight = 1.15.em, lineHeightStyle = tight, letterSpacing = letterSpacing)

    fun display(size: TextUnit, color: Color = VoLumColors.TextBright) = style(VoLumFonts.Poiret, FontWeight.Normal, size, color)
    fun caption(size: TextUnit, color: Color = VoLumColors.TextBright) = style(VoLumFonts.JosefinBold, FontWeight.SemiBold, size, color)
    fun label(size: TextUnit, color: Color = VoLumColors.TextBright) = style(VoLumFonts.JosefinBold, FontWeight.SemiBold, size, color)
    fun value(size: TextUnit, color: Color = VoLumColors.Gold) = style(VoLumFonts.Michroma, FontWeight.Normal, size, color)
    fun body(size: TextUnit, color: Color = VoLumColors.TextDim) = style(VoLumFonts.Josefin, FontWeight.Normal, size, color)

    /** The roles the desktop uses at fixed sizes, so candidates do not invent new ones. */
    val Logo = display(30.sp, VoLumColors.Gold)
    val LogoSub = label(10.sp, VoLumColors.GoldDim)
    val SoundTitle = display(32.sp)
    val SoundRowName = display(17.sp, VoLumColors.Cream)
    val AmpTitle = label(21.sp, VoLumColors.Gold)
    val KnobLabel = label(13.sp)
    val KnobValue = label(13.sp, VoLumColors.TextBright)
    val SectionHeader = label(10.sp, VoLumColors.GoldDim)
    val ListRow = label(13.sp, VoLumColors.TextMed)
    val Button = label(12.sp, VoLumColors.Cream)
    val Caption = label(9.sp, VoLumColors.CreamDim)
    val Footer = body(12.5.sp)
    val ProgramNumber = value(17.sp)
    val Stepper = label(19.sp, VoLumColors.Gold)
}
