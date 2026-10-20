package com.volum.design.components

import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsPressedAsState
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.selection.toggleable
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
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.design.art.ArtCanvas
import com.volum.design.art.ArtLayer
import com.volum.design.art.Box as ArtBox
import com.volum.design.art.Motif
import com.volum.design.art.Motifs
import com.volum.design.art.artUnits
import com.volum.design.theme.VFitText
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumMotion
import com.volum.design.theme.VoLumType
import com.volum.design.theme.ic
import com.volum.design.theme.withOpacity

/**
 * A motif drawn once per (size, effect, bypass, variant) into a bitmap. The lit
 * and dimmed layers are both kept, so a bypass toggle crossfades between two
 * blits in [VoLumMotion.ToggleMs] (the desktop cuts in one frame).
 */
@Composable
fun MotifArt(motif: Motif, on: Boolean, modifier: Modifier = Modifier, variant: Int = 0) {
    val lit by animateFloatAsState(if (on) 1f else 0f, tween(VoLumMotion.ToggleMs), label = "motif")
    Spacer(
        modifier.drawWithCache {
            val box = ArtBox(0f, 0f, size.width / density, size.height / density)
            val onL = ArtLayer().apply { build(box, density) { Motifs.draw(it, box, motif, false, variant) } }
            val offL = ArtLayer().apply { build(box, density) { Motifs.draw(it, box, motif, true, variant) } }
            val g = ArtCanvas()
            onDrawBehind {
                artUnits(g) {
                    if (lit < 1f) it.drawLayer(offL, 1f - lit)
                    if (lit > 0f) it.drawLayer(onL, lit)
                }
            }
        },
    )
}

/**
 * A focused pedal card (`VoLumPedalCardControl`): amber glow when [focused],
 * panel depth, a border that reads focus (amber), on (teal) or bypassed
 * (brass frame), 8 dp brackets, the cached motif with the bypass veil, the
 * preset footer auto-shrinking 11.5 -> 8 sp, and the LED that toggles bypass.
 * The card body taps [onClick] (focus / capture menu); the LED is its own 48 dp
 * target ([onToggle], [ledModifier]).
 */
@Composable
fun PedalCard(
    motif: Motif,
    title: String,
    on: Boolean,
    onClick: () -> Unit,
    onToggle: (Boolean) -> Unit,
    modifier: Modifier = Modifier,
    focused: Boolean = false,
    variant: Int = 0,
    ledModifier: Modifier = Modifier,
) {
    val source = remember { MutableInteractionSource() }
    val pressed by source.collectIsPressedAsState()
    val veil by animateFloatAsState(if (on) 0f else 0.68f, tween(VoLumMotion.ToggleMs), label = "veil")
    val border = when {
        focused -> VoLumColors.Amber
        pressed -> if (on) VoLumColors.Teal else VoLumColors.GoldDim
        on -> VoLumColors.TealDim
        else -> VoLumColors.Frame
    }
    Box(
        modifier
            .clickable(source, indication = null, role = Role.Button, onClick = onClick)
            .drawWithCache {
                val pad = 2.5.dp.toPx()
                val gr = CornerRadius(6.dp.toPx())
                onDrawBehind { if (focused) drawRoundRect(ic(44, 232, 168, 92), Offset(-pad, -pad), Size(size.width + 2 * pad, size.height + 2 * pad), gr) }
            }
            .panelDepth(4.dp)
            .drawBehind { if (pressed) drawRoundRect(ic(18, 80, 140, 160), cornerRadius = CornerRadius(4.dp.toPx())) }
            .hairlineFrame(border, radius = 4.dp)
            .cornerBrackets(border, 8.dp, 4.dp),
    ) {
        Box(Modifier.fillMaxSize().padding(start = 2.dp, top = 2.dp, end = 2.dp, bottom = 22.dp)) {
            MotifArt(motif, on, Modifier.fillMaxSize(), variant)
            Spacer(Modifier.fillMaxSize().drawBehind { if (veil > 0f) drawRect(VoLumColors.HeroBg.withOpacity(veil)) })
        }
        Row(
            Modifier.align(Alignment.BottomStart).fillMaxWidth().height(22.dp).padding(start = 10.dp, end = 24.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            VFitText(title, Modifier.weight(1f), style = VoLumType.label(11.5.sp, if (on) VoLumColors.Cream else VoLumColors.CreamDim), minSize = 6.5.sp)
        }
        Led(on, onToggle, ledModifier.align(Alignment.BottomEnd))
    }
}

@Composable
private fun Led(on: Boolean, onToggle: (Boolean) -> Unit, modifier: Modifier) {
    val source = remember { MutableInteractionSource() }
    val pressed by source.collectIsPressedAsState()
    val glow by animateFloatAsState(if (on) 1f else 0f, tween(VoLumMotion.ToggleMs), label = "led")
    Spacer(
        modifier.size(48.dp).toggleable(on, source, indication = null, role = Role.Switch, onValueChange = onToggle).drawBehind {
            val c = Offset(size.width - 14.dp.toPx(), size.height - 14.dp.toPx())
            if (pressed) drawCircle(VoLumColors.SelBg, 10.dp.toPx(), c)
            drawCircle(lerp(ic(255, 42, 48, 52), VoLumColors.Teal, glow), 4.5.dp.toPx(), c)
        },
    )
}

/**
 * One slot of a collapsed PRE / POST block (`_DrawQuietSlot`): teal edge bar
 * when on, a 20 dp motif, the label, the mini pill. The row opens the effect
 * ([onOpen]); the pill is its own target ([onToggle], [toggleModifier]).
 */
@Composable
fun QuietSlot(
    motif: Motif,
    label: String,
    on: Boolean,
    onOpen: () -> Unit,
    onToggle: (Boolean) -> Unit,
    modifier: Modifier = Modifier,
    variant: Int = 0,
    toggleModifier: Modifier = Modifier,
) = QuietSlotRow(motif, label, on, onOpen, onToggle, modifier, variant, toggleModifier, previewPillPressed = false)

/** [QuietSlot] with the pill held down for the gallery. */
@Composable
internal fun QuietSlotRow(
    motif: Motif,
    label: String,
    on: Boolean,
    onOpen: () -> Unit,
    onToggle: (Boolean) -> Unit,
    modifier: Modifier,
    variant: Int,
    toggleModifier: Modifier,
    previewPillPressed: Boolean,
) {
    val source = remember { MutableInteractionSource() }
    val pressed by source.collectIsPressedAsState()
    val pillSource = remember { MutableInteractionSource() }
    val pillHeld by pillSource.collectIsPressedAsState()
    val pillPressed = pillHeld || previewPillPressed
    Row(
        modifier
            .heightIn(min = 48.dp)
            .drawWithCache {
                val px = 1.dp.toPx()
                val edge = Stroke(px)
                onDrawBehind {
                    if (pressed) {
                        drawRect(ic(20, 80, 140, 160))
                        drawRect(VoLumColors.TealDim.withOpacity(0.35f), Offset(px, px), Size(size.width - 2 * px, size.height - 2 * px), style = edge)
                    }
                    if (on) drawRect(VoLumColors.Teal, Offset(px, 2 * px), Size(3 * px, size.height - 4 * px))
                }
            },
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Row(
            Modifier.weight(1f).heightIn(min = 48.dp).clickable(source, indication = null, role = Role.Button, onClick = onOpen).padding(start = 6.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(4.dp),
        ) {
            MotifArt(motif, on, Modifier.size(20.dp), variant)
            VFitText(label.uppercase(), Modifier.weight(1f), style = VoLumType.label(10.sp, if (on) VoLumColors.Cream else VoLumColors.CreamDim), minSize = 8.sp)
        }
        Box(
            toggleModifier
                .size(48.dp)
                .toggleable(on, pillSource, indication = null, role = Role.Switch, onValueChange = onToggle)
                .drawWithCache {
                    val px = 1.dp.toPx()
                    val wash = Size(32.dp.toPx(), 24.dp.toPx())
                    val at = Offset((size.width - wash.width) / 2, (size.height - wash.height) / 2)
                    val cr = CornerRadius(wash.height / 2)
                    val edge = Stroke(px)
                    onDrawBehind {
                        if (pillPressed) {
                            drawRoundRect(ic(20, 80, 140, 160), at, wash, cr)
                            drawRoundRect(VoLumColors.TealDim.withOpacity(0.35f), at, wash, cr, style = edge)
                        }
                    }
                },
            contentAlignment = Alignment.Center,
        ) {
            MiniPill(on)
        }
    }
}
