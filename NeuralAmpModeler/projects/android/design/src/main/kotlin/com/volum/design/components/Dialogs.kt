package com.volum.design.components

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInVertically
import androidx.compose.animation.slideOutVertically
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsPressedAsState
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.draw.drawWithCache
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.StrokeJoin
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.design.theme.VText
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumMotion
import com.volum.design.theme.VoLumType
import com.volum.design.theme.ic

/**
 * The modal button (`VoLumConfirmDialogControl::DrawBtn`): quiet brass frame
 * when not [primary]; amber (caution) or red ([destructive]) wash and outline
 * when primary.
 */
@Composable
fun VoLumButton(
    label: String,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    primary: Boolean = false,
    destructive: Boolean = false,
    enabled: Boolean = true,
) {
    val source = remember { MutableInteractionSource() }
    val pressed by source.collectIsPressedAsState()
    val accent = if (destructive) VoLumColors.Danger else VoLumColors.Amber
    Box(
        modifier
            .heightIn(min = 48.dp)
            .widthIn(min = 88.dp)
            .clickable(source, indication = null, enabled = enabled, role = Role.Button, onClick = onClick)
            .padding(vertical = 8.dp)
            .drawWithCache {
                val cr = CornerRadius(3.dp.toPx())
                val px = 1.dp.toPx()
                val primaryStroke = Stroke(1.2f * px)
                val quietStroke = Stroke(px)
                onDrawBehind {
                    if (primary) {
                        drawRoundRect(if (destructive) VoLumColors.DangerFill else ic(70, 232, 168, 92), cornerRadius = cr)
                        if (pressed) drawRoundRect(accent.copy(alpha = 0.25f), cornerRadius = cr)
                        drawRoundRect(accent, Offset(0.6f * px, 0.6f * px), Size(size.width - 1.2f * px, size.height - 1.2f * px), cr, style = primaryStroke)
                    } else {
                        drawRoundRect(if (pressed) VoLumColors.SelBg else VoLumColors.BtnOffBg, cornerRadius = cr)
                        drawRoundRect(if (pressed) VoLumColors.GoldDim else VoLumColors.Frame, Offset(px / 2, px / 2), Size(size.width - px, size.height - px), cr, style = quietStroke)
                    }
                }
            }
            .padding(horizontal = 16.dp),
        contentAlignment = Alignment.Center,
    ) {
        VText(label, style = VoLumType.label(12.sp, if (!enabled) VoLumColors.CreamDim else if (primary) VoLumColors.TextBright else VoLumColors.Cream))
    }
}

/** The caution triangle with a bang, tinted to the modal accent (`_DrawWarnGlyph`). */
@Composable
fun WarnGlyph(color: Color, modifier: Modifier = Modifier) {
    Spacer(modifier.size(22.dp).drawWithCache {
        val px = 1.dp.toPx()
        val path = Path().apply {
            moveTo(size.width / 2, px)
            lineTo(size.width - px, size.height - px)
            lineTo(px, size.height - px)
            close()
        }
        val stroke = Stroke(1.6f * px, join = StrokeJoin.Round)
        onDrawBehind {
            drawPath(path, color, style = stroke)
            drawLine(color, Offset(size.width / 2, px + 6 * px), Offset(size.width / 2, size.height - 6 * px), 1.6f * px)
            drawCircle(color, px, Offset(size.width / 2, size.height - 4 * px))
        }
    })
}

/**
 * `VoLumConfirmDialogControl`: dimming scrim, a lifted panel with a soft
 * accent glow, the caution glyph and title, the message, Cancel and the
 * confirm action. Red when [confirmLabel] deletes or removes, amber otherwise.
 * Tapping the scrim cancels.
 */
@Composable
fun VoLumConfirmDialog(
    title: String,
    message: String,
    confirmLabel: String,
    onConfirm: () -> Unit,
    onCancel: () -> Unit,
    modifier: Modifier = Modifier,
    confirmModifier: Modifier = Modifier,
    cancelModifier: Modifier = Modifier,
) {
    val destructive = confirmLabel.contains("Delete") || confirmLabel.contains("Remove")
    val accent = if (destructive) VoLumColors.Danger else VoLumColors.Amber
    Box(
        modifier.fillMaxSize().background(VoLumColors.Scrim).clickable(indication = null, interactionSource = null, onClick = onCancel),
        contentAlignment = Alignment.Center,
    ) {
        Column(
            Modifier
                .width(444.dp)
                .clickable(indication = null, interactionSource = null) {}
                .drawWithCache {
                    val glowPad = 3.5.dp.toPx()
                    val cr = CornerRadius(6.dp.toPx())
                    val gr = CornerRadius(9.dp.toPx())
                    val px = 1.dp.toPx()
                    onDrawBehind {
                        drawRoundRect(if (destructive) VoLumColors.DangerGlow else VoLumColors.SelGlow, Offset(-glowPad, -glowPad), Size(size.width + 2 * glowPad, size.height + 2 * glowPad), gr)
                    }
                }
                .panelDepth(6.dp)
                .hairlineFrame(accent, 1.6.dp, 6.dp)
                .padding(horizontal = 18.dp, vertical = 14.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                WarnGlyph(accent)
                VText(title, Modifier.weight(1f).padding(end = 22.dp), style = VoLumType.label(15.sp, accent), textAlign = TextAlign.Center)
            }
            VText(message, Modifier.fillMaxWidth(), style = VoLumType.body(12.sp, VoLumColors.Cream), textAlign = TextAlign.Center, maxLines = 4)
            Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                VoLumButton("Cancel", onCancel, cancelModifier.weight(1f))
                VoLumButton(confirmLabel, onConfirm, confirmModifier.weight(1f), primary = true, destructive = destructive)
            }
        }
    }
}

enum class VoLumBannerKind { ERROR, INFO }

/**
 * Toast / error banner: an inset strip with an accent edge, the message in
 * Josefin Bold, and a drawn dismiss cross (Josefin has no U+00D7). Slides in
 * from the top edge it is anchored to. [message] null hides it.
 */
@Composable
fun VoLumBanner(
    message: String?,
    kind: VoLumBannerKind,
    onDismiss: (() -> Unit)?,
    modifier: Modifier = Modifier,
    dismissModifier: Modifier = Modifier,
) {
    val last = remember { arrayOfNulls<String>(1) }
    if (message != null) last[0] = message
    AnimatedVisibility(
        visible = message != null,
        modifier = modifier,
        enter = fadeIn(tween(VoLumMotion.PanelMs)) + slideInVertically(tween(VoLumMotion.PanelMs)) { -it / 2 },
        exit = fadeOut(tween(VoLumMotion.PanelMs)) + slideOutVertically(tween(VoLumMotion.PanelMs)) { -it / 2 },
    ) {
        val accent = if (kind == VoLumBannerKind.ERROR) VoLumColors.Danger else VoLumColors.Gold
        Row(
            Modifier
                .heightIn(min = 48.dp)
                .insetWell(3.dp)
                .drawBehind {
                    if (kind == VoLumBannerKind.ERROR) drawRoundRect(VoLumColors.DangerFill.copy(alpha = 0.18f), cornerRadius = CornerRadius(3.dp.toPx()))
                    drawRect(accent, Offset(0f, 0f), Size(3.dp.toPx(), size.height))
                }
                .hairlineFrame(accent.copy(alpha = 0.55f), radius = 3.dp)
                .padding(start = 14.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            VText(last[0] ?: "", Modifier.weight(1f, fill = false).padding(end = 8.dp), style = VoLumType.label(12.sp, if (kind == VoLumBannerKind.ERROR) VoLumColors.TextBright else VoLumColors.Cream))
            if (onDismiss != null) CrossButton(onDismiss, dismissModifier, accent)
        }
    }
}

/** A drawn clear / close cross in a 48 dp target (`DrawCrossGlyph`). */
@Composable
fun CrossButton(onClick: () -> Unit, modifier: Modifier = Modifier, color: Color = VoLumColors.TextBright) {
    val source = remember { MutableInteractionSource() }
    val pressed by source.collectIsPressedAsState()
    Spacer(
        modifier.size(48.dp).clickable(source, indication = null, role = Role.Button, onClick = onClick).drawBehind {
            val half = 14.dp.toPx() * 0.26f * 2
            val c = Offset(size.width / 2, size.height / 2)
            if (pressed) drawCircle(VoLumColors.SelBg, 14.dp.toPx(), c)
            drawLine(color, c + Offset(-half, -half), c + Offset(half, half), 1.4.dp.toPx(), StrokeCap.Round)
            drawLine(color, c + Offset(-half, half), c + Offset(half, -half), 1.4.dp.toPx(), StrokeCap.Round)
        },
    )
}
