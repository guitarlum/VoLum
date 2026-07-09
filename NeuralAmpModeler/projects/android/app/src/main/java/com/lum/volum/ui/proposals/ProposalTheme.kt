package com.lum.volum.ui.proposals

import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontFamily
import com.lum.volum.ui.theme.Josefin
import com.lum.volum.ui.theme.Michroma

data class ProposalPalette(
    val background: Color,
    val surface: Color,
    val surfaceHigh: Color,
    val primary: Color,
    val secondary: Color,
    val danger: Color,
    val text: Color,
    val muted: Color,
    val hairline: Color,
    val displayFont: FontFamily,
    val bodyFont: FontFamily,
)

private val brand = ProposalPalette(
    background = Color(0xFF070B12),
    surface = Color(0xFF101826),
    surfaceHigh = Color(0xFF19263A),
    primary = Color(0xFFE8B65C),
    secondary = Color(0xFF54B7E8),
    danger = Color(0xFFFF5B55),
    text = Color(0xFFF5F2E9),
    muted = Color(0xFFBCC5D0),
    hairline = Color(0xFF2B4058),
    displayFont = Michroma,
    bodyFont = Josefin,
)

fun paletteFor(id: ProposalId): ProposalPalette = when (id) {
    ProposalId.FractalStage -> brand
    ProposalId.BlueFocus -> brand.copy(
        background = Color(0xFF050A10),
        surface = Color(0xFF0B1724),
        primary = Color(0xFF67C7FF),
        secondary = Color(0xFFE8B65C),
    )
    ProposalId.AuricPedalboard -> brand.copy(
        background = Color(0xFF0B0907),
        surface = Color(0xFF1A1510),
        surfaceHigh = Color(0xFF2A2118),
        primary = Color(0xFFF2C66D),
        secondary = Color(0xFF59B8D9),
        hairline = Color(0xFF59462E),
    )
    ProposalId.SignalAtlas -> brand.copy(
        background = Color(0xFF060B12),
        primary = Color(0xFFE9B85E),
        secondary = Color(0xFF4CC8F1),
    )
    ProposalId.ThumbDeck -> ProposalPalette(
        background = Color(0xFF10110E),
        surface = Color(0xFF20221B),
        surfaceHigh = Color(0xFF303429),
        primary = Color(0xFFD7F06A),
        secondary = Color(0xFFFF8B5B),
        danger = Color(0xFFFF5E64),
        text = Color(0xFFF6F7EC),
        muted = Color(0xFFC5CBB5),
        hairline = Color(0xFF424739),
        displayFont = Josefin,
        bodyFont = Josefin,
    )
    ProposalId.BlackoutLive -> ProposalPalette(
        background = Color(0xFF020202),
        surface = Color(0xFF0B0B0B),
        surfaceHigh = Color(0xFF151515),
        primary = Color(0xFFF4F2E9),
        secondary = Color(0xFFFF3E35),
        danger = Color(0xFFFF3E35),
        text = Color(0xFFF4F2E9),
        muted = Color(0xFFB5B5B5),
        hairline = Color(0xFF292929),
        displayFont = Michroma,
        bodyFont = Josefin,
    )
    ProposalId.FlightRack -> ProposalPalette(
        background = Color(0xFF10110F),
        surface = Color(0xFF24251F),
        surfaceHigh = Color(0xFF35372E),
        primary = Color(0xFFFFB12F),
        secondary = Color(0xFF93D37A),
        danger = Color(0xFFFF5D45),
        text = Color(0xFFF0EBD9),
        muted = Color(0xFFC5C1B1),
        hairline = Color(0xFF55564B),
        displayFont = Josefin,
        bodyFont = Josefin,
    )
    ProposalId.OrbitCockpit -> ProposalPalette(
        background = Color(0xFF08090D),
        surface = Color(0xFF131622),
        surfaceHigh = Color(0xFF22283A),
        primary = Color(0xFFFFD34E),
        secondary = Color(0xFF66F1D2),
        danger = Color(0xFFFF5D73),
        text = Color(0xFFF7F7F4),
        muted = Color(0xFFB5BED0),
        hairline = Color(0xFF30374B),
        displayFont = Michroma,
        bodyFont = Josefin,
    )
}
