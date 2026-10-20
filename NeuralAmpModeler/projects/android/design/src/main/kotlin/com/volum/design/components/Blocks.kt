package com.volum.design.components

import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsFocusedAsState
import androidx.compose.foundation.interaction.collectIsPressedAsState
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
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
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.stateDescription
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.design.theme.VFitText
import com.volum.design.theme.VText
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumMotion
import com.volum.design.theme.VoLumType
import com.volum.design.theme.ic
import com.volum.design.theme.withOpacity

/**
 * The PRE / POST block frame (`_DrawQuietBlock` / `_DrawExpandedFrame`):
 * panel depth, a hairline (gold and heavier when [locked]) and teal corner
 * brackets (gold when locked), 6 dp on a collapsed block, 8 dp when [expanded].
 * This is the POST surface; PRE adds [preCrosshatch], AMP is [ampCircuitBoard].
 */
fun Modifier.volumSectionFrame(locked: Boolean = false, expanded: Boolean = false): Modifier {
    val accent = if (locked) VoLumColors.Gold.withOpacity(0.6f) else VoLumColors.TealDim
    return panelDepth()
        .hairlineFrame(if (locked) VoLumColors.Gold.withOpacity(0.45f) else VoLumColors.Frame, if (locked) 1.2.dp else 1.dp)
        .cornerBrackets(accent, if (expanded) 8.dp else 6.dp, if (expanded) 4.dp else 3.dp)
}

/** The padlock's ink (`_DrawLockIcon`): gold when [locked], an opaque warm grey when open, brighter while [hot]. */
fun lockInk(locked: Boolean, hot: Boolean): Color = when {
    locked -> if (hot) VoLumColors.Gold else VoLumColors.Gold.withOpacity(0.72f)
    else -> if (hot) VoLumColors.TextBright else ic(255, 118, 110, 100)
}

/**
 * The header row of a PRE / POST block (`_LayoutHeaderControls`): the title
 * in Josefin Bold 10 (gold when [locked]), the padlock, the teal
 * store-to-amp arrow when the locked block is [dirty] and [onStore] is set,
 * and on a collapsed block ([onExpand] set) the `>` chevron. The title and
 * chevron are one 48 dp expand target; lock and store are their own 48 dp
 * targets laid over its right side. [storeFirst] puts the arrow left of the
 * lock, as POST does.
 */
@Composable
fun VoLumBlockHeader(
    title: String,
    locked: Boolean,
    onLockToggle: () -> Unit,
    modifier: Modifier = Modifier,
    onExpand: (() -> Unit)? = null,
    dirty: Boolean = false,
    onStore: (() -> Unit)? = null,
    storeFirst: Boolean = false,
    expandModifier: Modifier = Modifier,
    lockModifier: Modifier = Modifier,
    storeModifier: Modifier = Modifier,
) {
    val labelInk = if (locked) VoLumColors.Gold else VoLumColors.GoldDim
    Box(modifier.fillMaxWidth().height(48.dp)) {
        if (onExpand != null) {
            val source = remember { MutableInteractionSource() }
            val pressed by source.collectIsPressedAsState()
            Row(
                expandModifier
                    .fillMaxSize()
                    .clickable(source, indication = null, role = Role.Button, onClickLabel = "Open $title", onClick = onExpand)
                    .drawBehind { if (pressed) drawRect(ic(20, 80, 140, 160)) }
                    .padding(start = 8.dp, end = 4.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                VText(title, Modifier.weight(1f), style = VoLumType.label(10.sp, labelInk))
                VoLumIcon(VoLumGlyph.ChevronRight, color = if (pressed) VoLumColors.TextBright else labelInk)
            }
        } else {
            VText(title, Modifier.align(Alignment.CenterStart).padding(start = 8.dp), style = VoLumType.label(10.sp, labelInk))
        }
        Row(Modifier.align(Alignment.CenterEnd).padding(end = if (onExpand != null) 12.dp else 0.dp)) {
            val store = locked && dirty && onStore != null
            if (store && storeFirst) StoreTarget(title, onStore!!, storeModifier)
            LockTarget(title, locked, onLockToggle, lockModifier)
            if (store && !storeFirst) StoreTarget(title, onStore!!, storeModifier)
        }
    }
}

/** The padlock drawn only, for a caller that owns the touch: [locked] closed / open, [hot] while pressed. */
@Composable
fun LockFace(locked: Boolean, hot: Boolean, modifier: Modifier = Modifier) {
    Box(
        modifier.drawBehind { if (hot) drawCircle(lockInk(locked, false).withOpacity(0.16f), 13.dp.toPx()) },
        contentAlignment = Alignment.Center,
    ) { VoLumIcon(if (locked) VoLumGlyph.Lock else VoLumGlyph.Unlock, color = lockInk(locked, hot)) }
}

@Composable
private fun LockTarget(title: String, locked: Boolean, onToggle: () -> Unit, modifier: Modifier) {
    val source = remember { MutableInteractionSource() }
    val pressed by source.collectIsPressedAsState()
    LockFace(
        locked,
        pressed,
        modifier
            .size(48.dp)
            .clickable(source, indication = null, role = Role.Switch, onClick = onToggle)
            .semantics {
                contentDescription = if (locked) "Unlock $title" else "Lock $title"
                stateDescription = if (locked) "Locked" else "Unlocked"
            },
    )
}

@Composable
private fun StoreTarget(title: String, onStore: () -> Unit, modifier: Modifier) {
    val source = remember { MutableInteractionSource() }
    val pressed by source.collectIsPressedAsState()
    Box(
        modifier
            .size(48.dp)
            .clickable(source, indication = null, role = Role.Button, onClick = onStore)
            .semantics { contentDescription = "Store $title to amp" }
            .drawBehind { if (pressed) drawCircle(VoLumColors.Teal.withOpacity(0.18f), 16.dp.toPx() * 0.46f * 1.6f) },
        contentAlignment = Alignment.Center,
    ) { VoLumIcon(VoLumGlyph.Store, color = if (pressed) VoLumColors.Teal else VoLumColors.TealDim.withOpacity(0.48f)) }
}

/**
 * A collapsed PRE / POST block (`_DrawQuietBlock`): [volumSectionFrame], the
 * [VoLumBlockHeader] with lock and chevron, the hairline under it (gold when
 * locked) and the slots ([QuietSlot] rows) in [content].
 */
@Composable
fun VoLumQuietBlock(
    title: String,
    locked: Boolean,
    onLockToggle: () -> Unit,
    onExpand: () -> Unit,
    modifier: Modifier = Modifier,
    dirty: Boolean = false,
    onStore: (() -> Unit)? = null,
    storeFirst: Boolean = false,
    expandModifier: Modifier = Modifier,
    lockModifier: Modifier = Modifier,
    storeModifier: Modifier = Modifier,
    content: @Composable ColumnScope.() -> Unit,
) {
    Column(modifier.volumSectionFrame(locked).padding(2.dp)) {
        VoLumBlockHeader(title, locked, onLockToggle, Modifier, onExpand, dirty, onStore, storeFirst, expandModifier, lockModifier, storeModifier)
        Spacer(
            Modifier.fillMaxWidth().height(1.dp).padding(horizontal = 4.dp).drawBehind {
                drawRect(if (locked) VoLumColors.Gold.withOpacity(0.35f) else VoLumColors.TealDim.withOpacity(0.55f))
            },
        )
        content()
    }
}

/**
 * The Custom IR cab button (`VoLumSpeakerRowControl`, F7): the impulse glyph
 * and "Custom IR", or the IR's name in copper when it is the [active] cab.
 * [enabled] false is a custom amp with no DIRECT capture. A tap opens the IR
 * menu ([onClick]); it is not a radio, so it carries an "Active" state instead.
 */
@Composable
fun VoLumIrCabButton(
    active: Boolean,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    irName: String? = null,
    enabled: Boolean = true,
    minWidth: Dp = 104.dp,
) {
    val source = remember { MutableInteractionSource() }
    val pressed by source.collectIsPressedAsState()
    IrCabFace(
        active,
        pressed && enabled,
        enabled,
        irName,
        modifier
            .heightIn(min = 48.dp)
            .widthIn(min = minWidth)
            .clickable(source, indication = null, enabled = enabled, role = Role.Button, onClickLabel = "Custom IR menu", onClick = onClick)
            .semantics { stateDescription = if (active) "Active" else "Inactive" },
    )
}

@Composable
internal fun IrCabFace(active: Boolean, hot: Boolean, enabled: Boolean, irName: String?, modifier: Modifier) {
    val ink = when {
        !enabled -> ic(90, 196, 150, 120)
        active -> VoLumColors.BtnIrOnText
        hot -> VoLumColors.TextBright
        else -> VoLumColors.BtnOffText
    }
    Row(
        modifier
            .padding(vertical = 10.dp)
            .drawWithCache {
                val cr = CornerRadius(3.dp.toPx())
                val glowR = CornerRadius(5.dp.toPx())
                val pad = 2.5.dp.toPx()
                val px = 1.dp.toPx()
                val thin = Stroke(px)
                val bold = Stroke(1.35f * px)
                onDrawBehind {
                    val inner = Size(size.width - px, size.height - px)
                    when {
                        !enabled -> {
                            drawRoundRect(VoLumColors.BtnOffBg, cornerRadius = cr)
                            drawRoundRect(ic(50, 196, 122, 70), Offset(px / 2, px / 2), inner, cr, style = thin)
                        }
                        active -> {
                            drawRoundRect(ic(70, 196, 122, 80), Offset(-pad, -pad), Size(size.width + 2 * pad, size.height + 2 * pad), glowR)
                            drawRoundRect(VoLumColors.BtnIrOnBg, cornerRadius = cr)
                            if (hot) drawRoundRect(VoLumColors.SelBgSoft, cornerRadius = cr)
                            drawRoundRect(VoLumColors.BtnIrOnBorder, Offset(px / 2, px / 2), inner, cr, style = if (hot) bold else thin)
                        }
                        else -> {
                            drawRoundRect(if (hot) VoLumColors.SelBgSoft else VoLumColors.BtnOffBg, cornerRadius = cr)
                            drawRoundRect(if (hot) VoLumColors.GoldDim else VoLumColors.BtnOffBorder, Offset(px / 2, px / 2), inner, cr, style = if (hot) bold else thin)
                        }
                    }
                }
            }
            .padding(start = 9.dp, end = 8.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        VoLumIcon(VoLumGlyph.IrBars, color = ink)
        val label = if (active && !irName.isNullOrEmpty()) irName else "Custom IR"
        VFitText(label, Modifier.weight(1f, fill = false), style = VoLumType.label(if (active && !irName.isNullOrEmpty()) 11.sp else 12.sp, ink), minSize = 9.sp)
    }
}

/** The name field's frame (`VoLumNameDialogControl`): the dark well, a gold-dim hairline, gold while [focused]. */
fun Modifier.volumFieldFrame(focused: Boolean): Modifier = drawWithCache {
    val px = 1.dp.toPx()
    val stroke = Stroke(px)
    onDrawBehind {
        drawRect(VoLumColors.WellDark)
        drawRect(if (focused) VoLumColors.Gold else VoLumColors.GoldDim, Offset(px / 2, px / 2), Size(size.width - px, size.height - px), style = stroke)
    }
}

/**
 * The single-line name entry of the name dialog: Josefin Bold 13 bright on
 * the dark well, a bright 1 dp caret, amber selection (from [com.volum.design.theme.VoLumTheme]),
 * [placeholder] in dim ink while empty. [modifier] (feature id) lands on the
 * text field node; it is at least 48 dp tall.
 */
@Composable
fun VoLumTextField(
    value: String,
    onValueChange: (String) -> Unit,
    modifier: Modifier = Modifier,
    placeholder: String? = null,
    keyboardOptions: KeyboardOptions = KeyboardOptions.Default,
    keyboardActions: KeyboardActions = KeyboardActions.Default,
    enabled: Boolean = true,
) {
    val source = remember { MutableInteractionSource() }
    val focused by source.collectIsFocusedAsState()
    BasicTextField(
        value,
        onValueChange,
        modifier.heightIn(min = 48.dp),
        enabled = enabled,
        textStyle = VoLumType.label(13.sp, VoLumColors.TextBright),
        keyboardOptions = keyboardOptions,
        keyboardActions = keyboardActions,
        singleLine = true,
        interactionSource = source,
        cursorBrush = SolidColor(VoLumColors.TextBright),
        decorationBox = { inner ->
            Box(Modifier.fillMaxWidth().heightIn(min = 48.dp).volumFieldFrame(focused).padding(horizontal = 8.dp), contentAlignment = Alignment.CenterStart) {
                if (value.isEmpty() && placeholder != null) VText(placeholder, style = VoLumType.body(13.sp, VoLumColors.TextDim))
                inner()
            }
        },
    )
}

/**
 * The status lamp of pedal cards and stomps, drawn only: a 4.5 dp dot, dark
 * when off, [color] (teal) when on. Gate / clip / pick lamps pass their ink
 * ([VoLumColors.Danger] for clip). [glow] 0..1 crossfades between the two.
 */
@Composable
fun VoLumLed(on: Boolean, modifier: Modifier = Modifier, color: Color = VoLumColors.Teal, animate: Boolean = true) {
    val glow by animateFloatAsState(if (on) 1f else 0f, tween(if (animate) VoLumMotion.ToggleMs else 0), label = "lamp")
    Spacer(modifier.size(12.dp).drawBehind { drawCircle(lerp(ic(255, 42, 48, 52), color, glow), 4.5.dp.toPx()) })
}

/**
 * The NAMSwitch track drawn only (34 x 18 dp): gold track when [checked],
 * the dark handle sliding over in [VoLumMotion.ToggleMs], the gold wash while
 * [pressed]. [VoLumToggle] is this plus the label and the 48 dp target.
 */
@Composable
fun VoLumSwitchTrack(checked: Boolean, modifier: Modifier = Modifier, pressed: Boolean = false) {
    val pos by animateFloatAsState(if (checked) 1f else 0f, tween(VoLumMotion.ToggleMs), label = "track")
    Spacer(modifier.size(34.dp, 18.dp).drawWithCache {
        val cr = CornerRadius(7.dp.toPx())
        val sh = 1.5.dp.toPx()
        val handleR = size.height / 2f - 1.dp.toPx()
        val rim = Stroke(0.8.dp.toPx())
        val ballBase = ic(255, 22, 22, 27)
        val ballLight = ic(110, 96, 96, 106)
        val inner = Size(size.width - 2 * sh, size.height - 2 * sh)
        onDrawBehind {
            drawRoundRect(VoLumColors.KnobGold.withOpacity(0.3f), cornerRadius = cr)
            drawRoundRect(ic(60, 0, 0, 0), Offset(sh, sh), size, cr)
            drawRoundRect(lerp(Color.Black, VoLumColors.KnobGold, pos), Offset(sh, sh), inner, cr)
            if (pressed) drawRoundRect(VoLumColors.KnobGold.withOpacity(0.5f), Offset(sh, sh), inner, cr)
            val cx = handleR + 1.dp.toPx() + (size.width - 2 * handleR - 2.dp.toPx()) * pos
            val cy = size.height / 2f
            drawCircle(ballBase, handleR, Offset(cx, cy))
            drawCircle(ballLight, handleR * 0.5f, Offset(cx - handleR * 0.25f, cy - handleR * 0.3f))
            drawCircle(Color.Black.copy(alpha = 0.6f), handleR, Offset(cx, cy), style = rim)
        }
    })
}
