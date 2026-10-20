package com.volum.design.components

import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsPressedAsState
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.height
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
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.stateDescription
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.design.theme.VFitText
import com.volum.design.theme.VText
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumType
import com.volum.design.theme.ic
import com.volum.design.theme.withOpacity

/**
 * `VoLumBuildHeaderPlateControl` / the PLAY surface's top band: the panel
 * gradient with one hairline along the bottom. The desktop plate is 46 dp;
 * on a phone it is whatever height the header row takes (48 dp minimum).
 */
fun Modifier.volumHeaderPlate(): Modifier = drawWithCache {
    val brush = Brush.verticalGradient(listOf(VoLumColors.PanelTop, VoLumColors.PanelBot))
    val px = 1.dp.toPx()
    onDrawBehind {
        drawRect(brush)
        drawLine(VoLumColors.Frame, Offset(0f, size.height - px / 2), Offset(size.width, size.height - px / 2), px)
    }
}

/**
 * The PLAY / BUILD destination toggle (`VoLumModeToggleControl`), drawn only:
 * the 44 x 26 brass frame with the glyph of where a tap goes. [inPlay] true
 * shows BUILD's faders, false shows PLAY's stomp ring. [pressed] is the hover
 * look of the desktop. Fills its node; give it at least 44 x 26 dp.
 */
@Composable
fun ModeToggleFace(inPlay: Boolean, pressed: Boolean, modifier: Modifier = Modifier) {
    Box(
        modifier.drawWithCache {
            val w = 44.dp.toPx()
            val h = 26.dp.toPx()
            val l = (size.width - w) / 2
            val t = (size.height - h) / 2
            val inset = 1.5.dp.toPx()
            val wash = CornerRadius(3.dp.toPx())
            val frame = CornerRadius(4.dp.toPx())
            val stroke = Stroke(1.5.dp.toPx())
            onDrawBehind {
                if (pressed) drawRoundRect(VoLumColors.SelBgSoft, Offset(l + inset, t + inset), Size(w - 2 * inset, h - 2 * inset), wash)
                drawRoundRect(VoLumColors.GoldDim.withOpacity(if (pressed) 0.95f else 0.72f), Offset(l, t), Size(w, h), frame, style = stroke)
            }
        },
        contentAlignment = Alignment.Center,
    ) {
        val ink = selectionInk(VoLumSelectionStyle.Brass, pressed)
        VoLumIcon(if (inPlay) VoLumGlyph.BuildFaders else VoLumGlyph.PlayRing, color = ink, alpha = if (pressed) 1f else 0.9f)
    }
}

/**
 * The PLAY / BUILD switch in the header cluster: one 56 x 48 dp target that
 * goes to the other view. [modifier] (feature id) lands on the clickable node.
 */
@Composable
fun VoLumModeToggle(inPlay: Boolean, onSwitch: () -> Unit, modifier: Modifier = Modifier) {
    val source = remember { MutableInteractionSource() }
    val pressed by source.collectIsPressedAsState()
    ModeToggleFace(
        inPlay,
        pressed,
        modifier
            .size(56.dp, 48.dp)
            .clickable(source, indication = null, role = Role.Button, onClickLabel = if (inPlay) "Switch to BUILD" else "Switch to PLAY", onClick = onSwitch)
            .semantics { contentDescription = if (inPlay) "Switch to BUILD" else "Switch to PLAY" },
    )
}

/**
 * A header tool drawn only (`NAMCircleButtonControl` / `VoLumMetronomeButtonControl`):
 * the 26 dp glyph, the round wash while [pressed], [active] false dims it to
 * the desktop's 35 % (the metronome when it is not clicking), [badge] puts
 * the gold update dot on its shoulder (`VoLumUpdateBadgeControl`).
 */
@Composable
fun HeaderToolFace(glyph: VoLumGlyph, pressed: Boolean, modifier: Modifier = Modifier, active: Boolean = true, badge: Boolean = false) {
    Box(
        modifier.drawWithCache {
            val c = Offset(size.width / 2, size.height / 2)
            val washR = 16.dp.toPx()
            val dotR = 3.5.dp.toPx()
            val dot = Offset(c.x + 13.dp.toPx() - 1.5.dp.toPx(), c.y - 13.dp.toPx() + 2.5.dp.toPx())
            val rim = Stroke(1.dp.toPx())
            onDrawWithContent {
                if (pressed) drawCircle(if (glyph == VoLumGlyph.Metronome) ic(44, 200, 200, 200) else ic(44, 242, 242, 242), washR, c)
                drawContent()
                if (badge) {
                    drawCircle(VoLumColors.Gold, dotR, dot)
                    drawCircle(VoLumColors.HeroBg, dotR, dot, style = rim)
                }
            }
        },
        contentAlignment = Alignment.Center,
    ) {
        VoLumIcon(glyph, color = VoLumColors.TextBright, alpha = if (active || pressed) 1f else 0.35f)
    }
}

/** A header tool button: tuner, metronome or gear in a 48 dp target. [modifier] (feature id) lands on the clickable node. */
@Composable
fun VoLumHeaderTool(
    glyph: VoLumGlyph,
    onClick: () -> Unit,
    contentDescription: String,
    modifier: Modifier = Modifier,
    active: Boolean = true,
    badge: Boolean = false,
) {
    val source = remember { MutableInteractionSource() }
    val pressed by source.collectIsPressedAsState()
    HeaderToolFace(
        glyph,
        pressed,
        modifier
            .size(48.dp)
            .clickable(source, indication = null, role = Role.Button, onClick = onClick)
            .semantics {
                this.contentDescription = contentDescription
                if (glyph == VoLumGlyph.Metronome) stateDescription = if (active) "On" else "Off"
            },
        active,
        badge,
    )
}

/**
 * The right-rail cluster of the desktop header (`LayoutHeaderChrome`): the
 * PLAY / BUILD toggle, then tuner, metronome and gear, in that order. Each is
 * its own 48 dp target with its own modifier for feature ids.
 */
@Composable
fun VoLumHeaderCluster(
    inPlay: Boolean,
    onSwitchView: () -> Unit,
    onTuner: () -> Unit,
    onMetronome: () -> Unit,
    onSettings: () -> Unit,
    modifier: Modifier = Modifier,
    metronomeOn: Boolean = false,
    updateBadge: Boolean = false,
    viewModifier: Modifier = Modifier,
    tunerModifier: Modifier = Modifier,
    metronomeModifier: Modifier = Modifier,
    settingsModifier: Modifier = Modifier,
) {
    Row(modifier, verticalAlignment = Alignment.CenterVertically) {
        VoLumModeToggle(inPlay, onSwitchView, viewModifier)
        VoLumHeaderTool(VoLumGlyph.Tuner, onTuner, "Tuner", tunerModifier)
        VoLumHeaderTool(VoLumGlyph.Metronome, onMetronome, "Metronome", metronomeModifier, active = metronomeOn)
        VoLumHeaderTool(VoLumGlyph.Gear, onSettings, "Settings", settingsModifier, badge = updateBadge)
    }
}

/** What the preset bar says (`VoLumPresetBarControl::Draw`): "No Preset", the name, either with "  (unsaved)". */
fun presetBarLabel(name: String?, dirty: Boolean): String {
    val base = if (name.isNullOrEmpty()) "No Preset" else name
    return if (dirty) "$base  (unsaved)" else base
}

/** The preset bar's ink: amber when unsaved, cream-dim for a clean "No Preset", bright for a clean preset. */
fun presetBarInk(name: String?, dirty: Boolean): Color = when {
    dirty -> VoLumColors.Amber
    name.isNullOrEmpty() -> VoLumColors.CreamDim
    else -> VoLumColors.TextBright
}

/** Which part of the preset bar is under the finger (for [PresetBarFace]). */
enum class PresetBarPart { PREV, NAME, NEXT }

/**
 * The preset bar drawn only (`VoLumPresetBarControl`): the square brass
 * frame, the gold Josefin `<` and `>` in their 48 dp end cells, the name
 * between them auto-shrinking 13 -> 9 sp before it clips (never an
 * ellipsis). [canStep] false greys the arrows (an empty preset bank).
 * Fills its node; the frame is 32 dp tall, centred.
 */
@Composable
fun PresetBarFace(name: String?, dirty: Boolean, modifier: Modifier = Modifier, canStep: Boolean = true, pressed: PresetBarPart? = null) {
    Row(modifier.presetBarFrame(pressed == PresetBarPart.NAME), verticalAlignment = Alignment.CenterVertically) {
        PresetArrowFace("<", canStep, pressed == PresetBarPart.PREV, Modifier.width(48.dp).fillMaxHeight())
        Box(Modifier.weight(1f).fillMaxHeight(), contentAlignment = Alignment.Center) { PresetName(name, dirty) }
        PresetArrowFace(">", canStep, pressed == PresetBarPart.NEXT, Modifier.width(48.dp).fillMaxHeight())
    }
}

/**
 * The preset bar in the header: `<` steps back, the name opens the preset
 * menu, `>` steps on. Three 48 dp targets, each with its own modifier for
 * feature ids ([prevModifier], [openModifier], [nextModifier]); [modifier]
 * sizes the whole bar (240 dp wide on the desktop).
 */
@Composable
fun VoLumPresetBar(
    name: String?,
    dirty: Boolean,
    onOpen: () -> Unit,
    onPrev: () -> Unit,
    onNext: () -> Unit,
    modifier: Modifier = Modifier,
    canStep: Boolean = true,
    openModifier: Modifier = Modifier,
    prevModifier: Modifier = Modifier,
    nextModifier: Modifier = Modifier,
) {
    val openSource = remember { MutableInteractionSource() }
    val openPressed by openSource.collectIsPressedAsState()
    Row(modifier.widthIn(min = 240.dp).height(48.dp).presetBarFrame(openPressed), verticalAlignment = Alignment.CenterVertically) {
        PresetArrow("<", "Previous preset", canStep, onPrev, prevModifier)
        Box(
            openModifier
                .weight(1f)
                .fillMaxHeight()
                .clickable(openSource, indication = null, role = Role.Button, onClickLabel = "Open presets", onClick = onOpen)
                .semantics { stateDescription = presetBarLabel(name, dirty) },
            contentAlignment = Alignment.Center,
        ) { PresetName(name, dirty) }
        PresetArrow(">", "Next preset", canStep, onNext, nextModifier)
    }
}

private fun Modifier.presetBarFrame(hot: Boolean): Modifier = drawWithCache {
    val h = 32.dp.toPx().coerceAtMost(size.height)
    val t = (size.height - h) / 2
    val px = 1.dp.toPx()
    val stroke = Stroke(px)
    onDrawBehind {
        drawRect(if (hot) VoLumColors.ItemSelBg else VoLumColors.BtnOffBg, Offset(0f, t), Size(size.width, h))
        drawRect(VoLumColors.Frame, Offset(px / 2, t + px / 2), Size(size.width - px, h - px), style = stroke)
    }
}

@Composable
private fun PresetName(name: String?, dirty: Boolean) {
    VFitText(presetBarLabel(name, dirty), Modifier.padding(horizontal = 2.dp), style = VoLumType.label(13.sp, presetBarInk(name, dirty)), minSize = 9.sp, textAlign = TextAlign.Center)
}

@Composable
private fun PresetArrowFace(glyph: String, enabled: Boolean, pressed: Boolean, modifier: Modifier) {
    Box(
        modifier.drawBehind {
            if (pressed) {
                val h = 32.dp.toPx().coerceAtMost(size.height) - 2.dp.toPx()
                drawRect(VoLumColors.SelBg, Offset(1.dp.toPx(), (size.height - h) / 2), Size(size.width - 2.dp.toPx(), h))
            }
        },
        contentAlignment = Alignment.Center,
    ) {
        VText(glyph, style = VoLumType.label(15.sp, if (!enabled) VoLumColors.CreamDim else if (pressed) VoLumColors.TextBright else VoLumColors.Gold))
    }
}

@Composable
private fun PresetArrow(glyph: String, description: String, enabled: Boolean, onClick: () -> Unit, modifier: Modifier) {
    val source = remember { MutableInteractionSource() }
    val pressed by source.collectIsPressedAsState()
    PresetArrowFace(
        glyph,
        enabled,
        pressed,
        modifier
            .width(48.dp)
            .fillMaxHeight()
            .clickable(source, indication = null, enabled = enabled, role = Role.Button, onClick = onClick)
            .semantics { contentDescription = description },
    )
}

/**
 * The first-run pointer at the PLAY / BUILD toggle (`VoLumBuildTipControl`):
 * "Tweak your sound" in a faint gold pill with a pointer on its right. Put it
 * immediately left of the toggle; a tap goes to BUILD like the toggle itself.
 */
@Composable
fun VoLumBuildTip(onClick: () -> Unit, modifier: Modifier = Modifier) {
    val source = remember { MutableInteractionSource() }
    val pressed by source.collectIsPressedAsState()
    Box(
        modifier
            .size(148.dp, 48.dp)
            .clickable(source, indication = null, role = Role.Button, onClick = onClick)
            .drawWithCache {
                val px = 1.dp.toPx()
                val h = 26.dp.toPx()
                val t = (size.height - h) / 2
                val pillR = size.width - 9 * px
                val cr = CornerRadius(4 * px)
                val stroke = Stroke(px)
                val mid = size.height / 2
                val pointer = Path().apply {
                    moveTo(pillR, mid - 5 * px)
                    lineTo(size.width - px, mid)
                    lineTo(pillR, mid + 5 * px)
                    close()
                }
                onDrawBehind {
                    drawRoundRect(VoLumColors.Gold.withOpacity(if (pressed) 0.18f else 0.10f), Offset(0f, t), Size(pillR, h), cr)
                    drawRoundRect(VoLumColors.GoldDim.withOpacity(if (pressed) 0.95f else 0.75f), Offset(px / 2, t + px / 2), Size(pillR - px, h - px), cr, style = stroke)
                    drawPath(pointer, VoLumColors.GoldDim)
                }
            }
            .padding(end = 9.dp),
        contentAlignment = Alignment.Center,
    ) {
        VFitText("Tweak your sound", style = VoLumType.label(12.sp, VoLumColors.Gold), minSize = 9.sp)
    }
}
