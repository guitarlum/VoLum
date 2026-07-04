package com.lum.volum.ui.theme

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Typography
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.Font
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.sp
import com.lum.volum.R

/**
 * VoLum mobile visual identity: a dark "boutique amp in a dim studio" aesthetic.
 * Warm near-black panels, a tube-amber glow for anything live/active, and a cool
 * teal for meters and secondary accents. Type reuses the desktop brand fonts
 * (Michroma display + Josefin Sans text) so the app reads as VoLum, not a stock
 * Material app.
 */
object VoLumColors {
    val Void = Color(0xFF0C0B0E)          // deepest background
    val Panel = Color(0xFF161318)          // card surface
    val PanelHi = Color(0xFF201C23)        // raised / pressed surface
    val Hairline = Color(0xFF2C2830)       // subtle borders
    val Amber = Color(0xFFF0A552)          // primary glow (active, knobs)
    val AmberDim = Color(0xFF6E4F30)       // inactive arc track (warm)
    val Teal = Color(0xFF54D6C4)           // secondary accent / meter low
    val Violet = Color(0xFFB58CF0)         // pedal accent (reverb)
    val Green = Color(0xFF8BD46A)          // pedal accent (gate)
    val Red = Color(0xFFE5544B)            // stop / clip
    val TextHi = Color(0xFFF3EEE6)         // primary text (warm white)
    val TextMid = Color(0xFFB0A79C)        // secondary text
    val TextLow = Color(0xFF6D655D)        // tertiary / disabled
}

val Michroma = FontFamily(Font(R.font.michroma_regular, FontWeight.Normal))
val Josefin = FontFamily(
    Font(R.font.josefin_regular, FontWeight.Normal),
    Font(R.font.josefin_semibold, FontWeight.SemiBold),
)

private val VoLumTypography = Typography(
    titleLarge = TextStyle(fontFamily = Michroma, fontWeight = FontWeight.Normal, fontSize = 22.sp, letterSpacing = 4.sp),
    titleMedium = TextStyle(fontFamily = Michroma, fontWeight = FontWeight.Normal, fontSize = 13.sp, letterSpacing = 3.sp),
    labelLarge = TextStyle(fontFamily = Josefin, fontWeight = FontWeight.SemiBold, fontSize = 14.sp, letterSpacing = 1.sp),
    labelMedium = TextStyle(fontFamily = Josefin, fontWeight = FontWeight.SemiBold, fontSize = 11.sp, letterSpacing = 2.sp),
    bodyMedium = TextStyle(fontFamily = Josefin, fontWeight = FontWeight.Normal, fontSize = 14.sp),
    bodySmall = TextStyle(fontFamily = Josefin, fontWeight = FontWeight.Normal, fontSize = 12.sp, letterSpacing = 0.5.sp),
)

private val VoLumScheme = darkColorScheme(
    primary = VoLumColors.Amber,
    onPrimary = VoLumColors.Void,
    secondary = VoLumColors.Teal,
    background = VoLumColors.Void,
    onBackground = VoLumColors.TextHi,
    surface = VoLumColors.Panel,
    onSurface = VoLumColors.TextHi,
    surfaceVariant = VoLumColors.PanelHi,
    outline = VoLumColors.Hairline,
    error = VoLumColors.Red,
)

@Composable
fun VoLumTheme(content: @Composable () -> Unit) {
    // Always dark: this is a stage/studio tool.
    @Suppress("UNUSED_EXPRESSION") isSystemInDarkTheme()
    MaterialTheme(colorScheme = VoLumScheme, typography = VoLumTypography, content = content)
}
