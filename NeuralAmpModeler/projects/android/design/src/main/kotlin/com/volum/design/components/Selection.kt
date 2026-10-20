package com.volum.design.components

import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.drawWithCache
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import com.volum.design.theme.VoLumColors

/**
 * The selection SSOT (`DrawVoLumSelection` / `SelectionInkColor` /
 * `VoLumSelectionStyle`). Every mutually exclusive control draws its active and
 * pressed background through [volumSelection] and its label through
 * [selectionInk]; nothing hand-draws a highlight.
 *
 * - [AmberPicker]: mode pickers and sub-mode pills (solid amber, dark ink).
 * - [Brass]: cab row, metronome time signatures, PLAY sound rows (brass wash + outline).
 * - [ListTeal]: amp list and list menus (faint brass wash + outline).
 */
enum class VoLumSelectionStyle { AmberPicker, Brass, ListTeal }

/**
 * Background chrome for one item of a group. [hot] is the desktop hover look
 * and, on touch, the pressed state (it telegraphs the target under the finger).
 * [glow] adds the soft brass glow the cab row puts under its active button.
 */
fun Modifier.volumSelection(
    active: Boolean,
    hot: Boolean = false,
    style: VoLumSelectionStyle,
    radius: Dp = 3.dp,
    inset: Dp = 1.dp,
    glow: Boolean = false,
): Modifier = drawWithCache {
    val i = inset.toPx()
    val cr = CornerRadius(radius.toPx())
    val topLeft = Offset(i, i)
    val fillSize = Size(size.width - 2 * i, size.height - 2 * i)
    val px = 1.dp.toPx()
    val stroke = Stroke(px)
    val glowPad = 2.5.dp.toPx()
    val glowR = CornerRadius(5.dp.toPx())
    onDrawBehind {
        when (style) {
            VoLumSelectionStyle.AmberPicker -> when {
                active -> drawRoundRect(VoLumColors.Amber, topLeft, fillSize, if (radius > 0.dp) cr else CornerRadius.Zero)
                hot -> drawRoundRect(VoLumColors.AmberHover, topLeft, fillSize, if (radius > 0.dp) cr else CornerRadius.Zero)
            }
            VoLumSelectionStyle.ListTeal -> when {
                active -> {
                    drawRoundRect(VoLumColors.ItemSelBg, topLeft, fillSize, cr)
                    drawRoundRect(VoLumColors.ItemSelBorder, topLeft + Offset(px / 2, px / 2), Size(fillSize.width - px, fillSize.height - px), cr, style = stroke)
                }
                hot -> drawRoundRect(VoLumColors.ItemHover.copy(alpha = 0.16f), topLeft, fillSize, cr)
            }
            VoLumSelectionStyle.Brass -> when {
                active -> {
                    if (glow) drawRoundRect(VoLumColors.SelGlow, Offset(-glowPad, -glowPad), Size(size.width + 2 * glowPad, size.height + 2 * glowPad), glowR)
                    drawRoundRect(VoLumColors.SelBg, topLeft, fillSize, cr)
                    drawRoundRect(VoLumColors.SelBorder, topLeft + Offset(px / 2, px / 2), Size(fillSize.width - px, fillSize.height - px), cr, style = stroke)
                }
                hot -> drawRoundRect(VoLumColors.SelBgSoft, topLeft, fillSize, cr)
            }
        }
    }
}

/** On-selection text colour matching [volumSelection] per style. */
fun selectionInk(style: VoLumSelectionStyle, active: Boolean): Color = if (!active) VoLumColors.TextBright else when (style) {
    VoLumSelectionStyle.AmberPicker -> VoLumColors.AmberInk
    VoLumSelectionStyle.Brass, VoLumSelectionStyle.ListTeal -> VoLumColors.SelText
}
