package com.volum.design.theme

import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.tween
import androidx.compose.foundation.IndicationNodeFactory
import androidx.compose.foundation.LocalIndication
import androidx.compose.foundation.interaction.InteractionSource
import androidx.compose.foundation.interaction.PressInteraction
import androidx.compose.foundation.text.BasicText
import androidx.compose.foundation.text.selection.LocalTextSelectionColors
import androidx.compose.foundation.text.selection.TextSelectionColors
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.drawWithContent
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.ContentDrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.node.DelegatableNode
import androidx.compose.ui.node.DrawModifierNode
import androidx.compose.ui.node.invalidateDraw
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.TextUnit
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.launch
import kotlin.math.floor
import androidx.compose.ui.Modifier.Node as ModifierNode

/** The text style [VText] falls back to: Josefin Bold, the desktop's label face. */
val LocalVoLumTextStyle = staticCompositionLocalOf { VoLumType.label(13.sp) }

/**
 * Wrap every VoLum surface in this. It replaces Compose's grey default press
 * overlay with the brass press wash and gives text fields gold selection
 * handles, so nothing Material-coloured leaks in from the foundation defaults.
 */
@Composable
fun VoLumTheme(content: @Composable () -> Unit) {
    CompositionLocalProvider(
        LocalIndication provides VoLumPressIndication,
        LocalTextSelectionColors provides VoLumSelectionColors,
        LocalVoLumTextStyle provides VoLumType.label(13.sp),
        content = content,
    )
}

/** The name dialog's selection: amber wash (`IColor(90, 232, 168, 92)`), gold handles. */
private val VoLumSelectionColors = TextSelectionColors(handleColor = VoLumColors.Gold, backgroundColor = ic(90, 232, 168, 92))

/**
 * Press state of every clickable: the brass wash the desktop shows on hover,
 * at the brighter selection weight while a finger is down, plus a thin gold
 * rim. Rises in [VoLumMotion.PressMs], falls in twice that so a quick tap is
 * still visible.
 */
object VoLumPressIndication : IndicationNodeFactory {
    override fun create(interactionSource: InteractionSource): DelegatableNode = PressNode(interactionSource)
    override fun equals(other: Any?): Boolean = other === this
    override fun hashCode(): Int = 0x5011

    private class PressNode(private val source: InteractionSource) : ModifierNode(), DrawModifierNode {
        private val level = Animatable(0f)
        private val rim = Stroke(width = 1f)

        override fun onAttach() {
            coroutineScope.launch {
                var down = 0
                source.interactions.collect { i ->
                    when (i) {
                        is PressInteraction.Press -> down++
                        is PressInteraction.Release, is PressInteraction.Cancel -> down = (down - 1).coerceAtLeast(0)
                    }
                    val target = if (down > 0) 1f else 0f
                    val ms = if (target > 0f) VoLumMotion.PressMs else VoLumMotion.PressMs * 2
                    launch { level.animateTo(target, tween(ms)) { invalidateDraw() } }
                }
            }
        }

        override fun ContentDrawScope.draw() {
            drawContent()
            val p = level.value
            if (p <= 0f) return
            val r = CornerRadius(3.dp.toPx())
            drawRoundRect(VoLumColors.SelBg, cornerRadius = r, alpha = p)
            drawRoundRect(VoLumColors.GoldDim, cornerRadius = r, alpha = 0.55f * p, style = rim)
        }
    }
}

/**
 * The only text primitive candidates use. Defaults to [LocalVoLumTextStyle]
 * (Josefin Bold), single line, ellipsised: the desktop never wraps a label.
 */
@Composable
fun VText(
    text: String,
    modifier: Modifier = Modifier,
    style: TextStyle = LocalVoLumTextStyle.current,
    color: Color = Color.Unspecified,
    textAlign: TextAlign? = null,
    maxLines: Int = 1,
    overflow: TextOverflow = TextOverflow.Ellipsis,
) {
    val merged = style.merge(TextStyle(color = color, textAlign = textAlign ?: TextAlign.Unspecified))
    BasicText(text, modifier, merged, overflow = overflow, softWrap = maxLines > 1, maxLines = maxLines)
}

/**
 * Desktop auto-shrink (`VoLumAmpList` names 13 -> 10.5, pedal-card footers 11.5 -> 8):
 * the largest size on the 0.5 sp grid at which the line fits, then a hard clip
 * at [minSize]. The first overflowing layout jumps straight to the size its
 * measured width calls for, so a busy frame settles in one or two passes.
 * Hidden until it has settled, so it never flashes the overflowing size.
 */
@Composable
fun VFitText(
    text: String,
    modifier: Modifier = Modifier,
    style: TextStyle = LocalVoLumTextStyle.current,
    minSize: TextUnit = (style.fontSize.value * 0.8f).sp,
    color: Color = Color.Unspecified,
    textAlign: TextAlign? = null,
) {
    val max = style.fontSize
    var size by remember(text, max) { mutableFloatStateOf(max.value) }
    var settled by remember(text, max) { mutableFloatStateOf(0f) }
    BasicText(
        text,
        modifier.drawWithContent { if (settled > 0f) drawContent() },
        style.merge(TextStyle(color = color, fontSize = size.sp, textAlign = textAlign ?: TextAlign.Unspecified)),
        overflow = TextOverflow.Clip,
        softWrap = false,
        maxLines = 1,
        onTextLayout = { r ->
            if (r.didOverflowWidth && size - 0.5f >= minSize.value) size = fitSize(size, r.layoutInput.constraints.maxWidth.toFloat(), r.multiParagraph.intrinsics.maxIntrinsicWidth, minSize.value)
            else settled = 1f
        },
    )
}

/**
 * The next size [VFitText] tries after a line of [needed] px overflowed [available] px at
 * [size] sp: text width scales with the size, so the estimate is `size * available / needed`
 * rounded down to the 0.5 sp grid, at least one step below [size] and never below [min].
 */
internal fun fitSize(size: Float, available: Float, needed: Float, min: Float): Float {
    val guess = if (needed > 0f && available > 0f) floor(size * available / needed * 2f) / 2f else size - 0.5f
    return guess.coerceAtMost(size - 0.5f).coerceAtLeast(min)
}
