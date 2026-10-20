package com.volum.ui.c

import androidx.compose.runtime.Composable
import androidx.compose.runtime.ReadOnlyComposable
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.TextUnit
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

/**
 * Every colour and type role a Studio Instrument surface uses. The values are a
 * neutral stand-in until `:design` (ticket 02) is merged: VoLum's palette, fonts
 * and art come only from `:design`, never from a copy here. Merging
 * `feature/android-design` replaces the right-hand sides of this file and nothing else.
 */
internal object Look {
    val ground = Color(0xFF0B0B0C)
    val glass = Color(0xE0141416)
    val glassHi = Color(0xF01C1C1F)
    val panel = Color(0xFF19191C)
    val raised = Color(0xFF26262A)
    val line = Color(0xFF3A3A40)
    val lineHi = Color(0xFF6A6A72)
    val text = Color(0xFFECECEE)
    val dim = Color(0xFF9C9CA4)
    val faint = Color(0xFF5E5E66)

    /** Selected / engaged (`:design` AmberPicker). */
    val pick = Color(0xFFF2F2F2)
    val pickInk = Color(0xFF111113)

    /** Signal glow on the floating blocks (`:design` gold). */
    val glow = Color(0xFFE6E6E6)

    /** List selection ink (`:design` ListTeal). */
    val listInk = Color(0xFFBFC6C8)

    /** Secondary metal for pedal and block frames (`:design` Brass). */
    val metal = Color(0xFF8E8E94)

    val danger = Color(0xFFE0574A)
    val warn = Color(0xFFE0B04A)
    val ok = Color(0xFF6CC48A)

    /** Display face for the logo, Sound names and the amp title (`:design` Poiret One). */
    val display: FontFamily = FontFamily.Default

    /** Labels and values (`:design` Josefin Sans). */
    val label: FontFamily = FontFamily.Default

    /** Numeric readouts (`:design` Michroma). */
    val tech: FontFamily = FontFamily.Monospace
}

internal object Metrics {
    val top = 52.dp
    val band = 68.dp
    val sheetTop = top + band
    val rail = 296.dp
    val target = 48.dp
}

/**
 * Text size that grows with the system font scale up to [cap] and then stops, so
 * dense instrument labels never wrap or clip at font scale 1.3.
 */
@Composable
@ReadOnlyComposable
internal fun capped(base: Float, cap: Float = 1.15f): TextUnit {
    val scale = LocalDensity.current.fontScale
    return (base * minOf(scale, cap) / scale).sp
}

@Composable
@ReadOnlyComposable
internal fun labelStyle(size: Float = 11f, color: Color = Look.dim, bold: Boolean = true, cap: Float = 1.15f) =
    TextStyle(color = color, fontSize = capped(size, cap), fontFamily = Look.label, fontWeight = if (bold) FontWeight.SemiBold else FontWeight.Normal, letterSpacing = (size * 0.08f).sp)

@Composable
@ReadOnlyComposable
internal fun bodyStyle(size: Float = 14f, color: Color = Look.text, cap: Float = 1.3f) =
    TextStyle(color = color, fontSize = capped(size, cap), fontFamily = Look.label)

@Composable
@ReadOnlyComposable
internal fun displayStyle(size: Float, color: Color = Look.text, cap: Float = 1.15f) =
    TextStyle(color = color, fontSize = capped(size, cap), fontFamily = Look.display, fontWeight = FontWeight.Light, letterSpacing = (size * 0.02f).sp)

@Composable
@ReadOnlyComposable
internal fun techStyle(size: Float = 12f, color: Color = Look.text, cap: Float = 1.15f) =
    TextStyle(color = color, fontSize = capped(size, cap), fontFamily = Look.tech)
