package com.volum.design.components

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsPressedAsState
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxScope
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.draw.drawWithCache
import androidx.compose.ui.draw.drawWithContent
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.design.art.AmpArt
import com.volum.design.art.ArtRef
import com.volum.design.theme.VText
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumType
import com.volum.design.theme.ic

/** One hero lane: the amp's name and art; null [art] is the empty SUPPORT lane ("ADD AMP"). */
data class HeroAmp(val name: String, val art: ArtRef?)

/**
 * The BUILD hero (`VoLumHeroImageControl`). Mono: panel depth, hero border,
 * 16 dp brackets (top-right left to the Dual Amp chip), the static art inset 6.
 * Dual: MAIN and SUPPORT lanes side by side, the focused one outlined in its
 * accent (amber / teal), each with a role caption and a title strip.
 * BUILD art does not move (the desktop animates PLAY only).
 */
@Composable
fun VoLumHero(
    main: HeroAmp,
    modifier: Modifier = Modifier,
    dual: Boolean = false,
    support: HeroAmp? = null,
    supportFocused: Boolean = false,
    onDualChip: (() -> Unit)? = null,
    chipModifier: Modifier = Modifier,
    onLane: ((support: Boolean) -> Unit)? = null,
    laneModifier: (support: Boolean) -> Modifier = { Modifier },
    laneTrailing: (@Composable (support: Boolean) -> Unit)? = null,
) {
    Box(modifier) {
        if (!dual) {
            Box(
                Modifier.fillMaxSize().panelDepth().hairlineFrame(VoLumColors.HeroBorder)
                    .cornerBrackets(VoLumColors.HeroCorner, 16.dp, 6.dp, Corners.NO_TR),
            ) {
                main.art?.let { AmpArt(it, Modifier.fillMaxSize().padding(6.dp)) }
            }
        } else {
            val mainChip: (@Composable BoxScope.() -> Unit)? = if (onDualChip != null) {
                { DualChip(true, onDualChip, Modifier.align(Alignment.TopEnd), chipModifier) }
            } else null
            Row(Modifier.fillMaxSize()) {
                HeroLane(main, "MAIN", !supportFocused, VoLumColors.Amber, Modifier.weight(1f).fillMaxHeight().then(laneModifier(false)), onLane?.let { { it(false) } }, chipSpace = true, chip = mainChip) { laneTrailing?.invoke(false) }
                Spacer(Modifier.width(8.dp).fillMaxHeight().background(VoLumColors.Bg))
                HeroLane(support ?: HeroAmp("Choose support amp", null), "SUPPORT", supportFocused, VoLumColors.Teal, Modifier.weight(1f).fillMaxHeight().then(laneModifier(true)), onLane?.let { { it(true) } }, chipSpace = false, chip = null) { laneTrailing?.invoke(true) }
            }
        }
        if (onDualChip != null && !dual) DualChip(false, onDualChip, Modifier.align(Alignment.TopEnd), chipModifier)
    }
}

@Composable
private fun HeroLane(amp: HeroAmp, role: String, focused: Boolean, accent: Color, modifier: Modifier, onTap: (() -> Unit)?, chipSpace: Boolean, chip: (@Composable BoxScope.() -> Unit)?, trailing: @Composable () -> Unit) {
    val source = remember { MutableInteractionSource() }
    val pressed by source.collectIsPressedAsState()
    val corner = if (focused || pressed) accent else VoLumColors.HeroCorner
    Box(
        modifier
            .then(if (onTap != null) Modifier.clickable(source, indication = null, role = Role.Button, onClick = onTap) else Modifier)
            .panelDepth()
            .drawWithContent {
                drawContent()
                if (pressed) drawRect(VoLumColors.SelBgSoft)
            }
            .hairlineFrame(if (focused || pressed) accent else VoLumColors.HeroBorder)
            .cornerBrackets(corner, 12.dp, 6.dp, if (chipSpace) Corners.NO_TR else Corners.ALL),
    ) {
        Box(Modifier.fillMaxSize().padding(start = 10.dp, top = 28.dp, end = 10.dp, bottom = 38.dp)) {
            if (amp.art != null) AmpArt(amp.art, Modifier.fillMaxSize()) else EmptyLaneArt(Modifier.fillMaxSize())
        }
        VText(role, Modifier.align(Alignment.TopStart).padding(start = 12.dp, top = 8.dp), style = VoLumType.label(10.sp, accent))
        Row(
            Modifier.align(Alignment.BottomCenter).fillMaxWidth().padding(start = 8.dp, end = 8.dp, bottom = 8.dp).height(22.dp).background(ic(190, 12, 12, 18)),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            VText(amp.name, Modifier.weight(1f).padding(horizontal = 3.dp), style = VoLumType.label(12.sp), textAlign = TextAlign.Center)
            trailing()
        }
    }
}

/** `DrawEmptySupportArt`: the empty SUPPORT lane's "+" and "ADD AMP". */
@Composable
private fun EmptyLaneArt(modifier: Modifier) {
    Box(modifier, contentAlignment = Alignment.Center) {
        Column(horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Spacer(Modifier.size(44.dp).drawWithCache {
                val ring = Stroke(1.dp.toPx())
                val t = 2.dp.toPx()
                val arm = 14.dp.toPx()
                val dim = VoLumColors.Teal.copy(alpha = 140 / 255f)
                onDrawBehind {
                    val c = Offset(size.width / 2, size.height / 2)
                    drawCircle(VoLumColors.Teal.copy(alpha = 60 / 255f), 22.dp.toPx(), c, style = ring)
                    drawRect(dim, Offset(c.x - arm, c.y - t), Size(2 * arm, 2 * t))
                    drawRect(dim, Offset(c.x - t, c.y - arm), Size(2 * t, 2 * arm))
                }
            })
            VText("ADD AMP", style = VoLumType.label(10.sp, VoLumColors.Teal.copy(alpha = 140 / 255f)))
        }
    }
}

/** The Dual Amp chip (two-panel glyph), filled when Dual Amp is on. A 48 dp target around a 24x16 chip. */
@Composable
private fun DualChip(dual: Boolean, onClick: () -> Unit, modifier: Modifier, chipModifier: Modifier) {
    val source = remember { MutableInteractionSource() }
    val pressed by source.collectIsPressedAsState()
    Box(
        modifier.then(chipModifier).size(48.dp)
            .clickable(source, indication = null, role = Role.Switch, onClick = onClick)
            .drawWithCache {
                val px = 1.dp.toPx()
                val w = 24.dp.toPx()
                val h = 16.dp.toPx()
                val thin = Stroke(px)
                val bold = Stroke(1.4f * px)
                onDrawBehind {
                    val l = size.width - w - 6.dp.toPx()
                    val t = 6.dp.toPx()
                    val accent = VoLumColors.Amber
                    val border = if (dual || pressed) accent else VoLumColors.GoldDim
                    val fill = if (dual) accent.copy(alpha = (if (pressed) 95 else 70) / 255f) else if (pressed) accent.copy(alpha = 34 / 255f) else Color.Transparent
                    drawRect(fill, Offset(l, t), Size(w, h))
                    drawRect(border, Offset(l, t), Size(w, h), style = if (pressed) bold else thin)
                    val cy = t + h / 2
                    val gx = l + w / 2
                    drawRect(border, Offset(gx - 5 * px, cy - 4 * px), Size(4 * px, 8 * px), style = thin)
                    drawRect(border, Offset(gx + px, cy - 4 * px), Size(4 * px, 8 * px), style = thin)
                }
            },
    )
}

/** The amp name under the hero (`VoLumAmpNameControl`): Josefin Bold 21 gold over a diamond rule. */
@Composable
fun AmpTitle(name: String, modifier: Modifier = Modifier) {
    Column(modifier, horizontalAlignment = Alignment.CenterHorizontally) {
        VText(name, style = VoLumType.AmpTitle, textAlign = TextAlign.Center)
        Spacer(Modifier.width(110.dp).height(12.dp).diamondRule())
    }
}

/** `VoLumLogoControl`: "VoLum" in Poiret One, "NAM PLAYER" under it, the stepped Deco rule and diamond. */
@Composable
fun VoLumLogo(modifier: Modifier = Modifier, compact: Boolean = false) {
    Column(modifier, horizontalAlignment = if (compact) Alignment.Start else Alignment.CenterHorizontally) {
        VText("VoLum", style = if (compact) VoLumType.display(20.sp, VoLumColors.Gold) else VoLumType.Logo)
        VText("NAM PLAYER", Modifier.padding(top = if (compact) 0.dp else 0.dp), style = VoLumType.LogoSub)
        if (!compact) Spacer(Modifier.width(60.dp).height(16.dp).drawBehind {
            val line = ic(38, 200, 162, 78)
            val cx = size.width / 2
            val px = 1.dp.toPx()
            drawLine(line, Offset(cx - 30 * px, 2 * px), Offset(cx + 30 * px, 2 * px), px)
            drawLine(line, Offset(cx - 18 * px, 5 * px), Offset(cx + 18 * px, 5 * px), px)
            val dy = 11 * px
            val half = 3 * px
            drawLine(VoLumColors.GoldDim, Offset(cx, dy - half), Offset(cx + half, dy), px)
            drawLine(VoLumColors.GoldDim, Offset(cx + half, dy), Offset(cx, dy + half), px)
            drawLine(VoLumColors.GoldDim, Offset(cx, dy + half), Offset(cx - half, dy), px)
            drawLine(VoLumColors.GoldDim, Offset(cx - half, dy), Offset(cx, dy - half), px)
        })
    }
}
