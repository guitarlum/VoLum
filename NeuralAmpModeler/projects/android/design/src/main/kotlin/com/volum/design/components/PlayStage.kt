package com.volum.design.components

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.draw.drawWithCache
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.unit.dp
import com.volum.design.art.ArtCanvas
import com.volum.design.art.ArtCommon
import com.volum.design.art.ArtInput
import com.volum.design.art.ArtRef
import com.volum.design.art.Box as ArtBox
import com.volum.design.art.PlayLight
import com.volum.design.art.StageMotion
import com.volum.design.art.argb
import com.volum.design.art.artUnits
import com.volum.design.art.opacity
import com.volum.design.art.rememberArtFrames
import com.volum.design.art.withA
import com.volum.design.theme.VText
import com.volum.design.theme.VoLumColors
import com.volum.design.theme.VoLumType
import com.volum.design.theme.withOpacity
import kotlin.math.max
import androidx.compose.ui.unit.sp

private val GOLD = argb(255, 252, 222, 145)
private val TEAL = argb(255, 91, 196, 196)
private val CORNER = argb(255, 200, 162, 78)
private val PANEL_TOP = argb(255, 23, 23, 31)
private val PANEL_BOT = argb(255, 13, 13, 19)

/**
 * One PLAY amp panel (`VoLumPlaySurface::DrawAmpPanel`): BUILD's panel
 * backdrop, the moving art inset 18, an additive glow, and the frame corona
 * whose opacity follows the playing level (plus a slow idle pulse in silence),
 * with gold corner brackets. At energy 0 it is pixel-for-pixel the static art.
 * [support] tints frame and brackets teal (Dual Amp SUPPORT lane); [role] is
 * the Dual Amp "MAIN - name" caption.
 */
@Composable
fun PlayStagePanel(
    art: ArtRef,
    input: ArtInput,
    modifier: Modifier = Modifier,
    support: Boolean = false,
    role: String? = null,
    animate: Boolean = true,
    picks: Boolean = true,
    running: Boolean = true,
) {
    val frames = rememberArtFrames(running)
    val motion = remember(art, animate) { StageMotion(art, animate) }
    DisposableEffect(motion) { onDispose { motion.animator.drop() } }
    Box(modifier) {
        Spacer(
            Modifier.fillMaxSize().drawWithCache {
                val d = density
                val panel = ArtBox(0f, 0f, size.width / d, size.height / d)
                val paint = panel.padded(-18f)
                motion.animator.prepare(paint, d)
                val accent = if (support) TEAL else GOLD
                val backdrop = ArtCanvas.linear(0f, 0f, 0f, panel.b, PANEL_TOP, PANEL_BOT)
                val glowShader = ArtCanvas.addRadial(panel.mw, panel.mh, 0.62f * max(panel.w, panel.h), withA(accent, 0.30f), withA(accent, 0f))
                val corner = opacity(if (support) TEAL else CORNER, 0.8f)
                val g = ArtCanvas()
                onDrawBehind {
                    val tele = input.read()
                    val attack = if (picks) PlayLight.attackFromPickAge(tele.pickAgeMs) else 0f
                    val m = motion.tick(frames.longValue, tele.energy.toFloat(), attack)
                    val glow = motion.glow
                    artUnits(g) { c ->
                        c.fillShader(backdrop, panel.l, panel.t, panel.r, panel.b)
                        c.save()
                        c.clip(paint.l, paint.t, paint.r, paint.b)
                        motion.animator.draw(c, paint, m)
                        c.restore()
                        if (glow > 0f) c.fillShader(glowShader, panel.l, panel.t, panel.r, panel.b, glow, add = true)
                        val corona = PlayLight.coronaOpacity(glow, PlayLight.idlePulse(motion.idlePhase))
                        val frame = opacity(accent, corona)
                        c.drawRect(frame, panel.l, panel.t, panel.r, panel.b, 2.2f)
                        c.drawRect(opacity(frame, corona * 0.45f), panel.l - 2f, panel.t - 2f, panel.r + 2f, panel.b + 2f, 3f)
                        ArtCommon.cornerAccent(c, panel.l + 6f, panel.t + 6f, 14f, false, false, corner)
                        ArtCommon.cornerAccent(c, panel.r - 6f, panel.t + 6f, 14f, true, false, corner)
                    }
                }
            },
        )
        if (role != null) {
            VText(
                role,
                Modifier.align(Alignment.TopStart).padding(start = 24.dp, top = 4.dp, end = 8.dp),
                style = VoLumType.label(9.sp, if (support) VoLumColors.Teal else VoLumColors.GoldDim),
            )
        }
    }
}

/**
 * The PLAY name banner under the stage (`DrawStage`): program number in
 * Michroma, the Sound in Poiret One, the amp in teal, "(unsaved)" in amber.
 */
@Composable
fun PlayBanner(title: String, modifier: Modifier = Modifier, program: Int? = null, secondary: String? = null, dirty: Boolean = false) {
    Row(
        modifier
            .background(VoLumColors.Bg.copy(red = 5 / 255f, green = 7 / 255f, blue = 11 / 255f, alpha = 205 / 255f))
            .drawBehind { drawLine(VoLumColors.GoldDim.withOpacity(0.5f), Offset(0f, 0f), Offset(size.width, 0f), 1.dp.toPx()) }
            .padding(start = if (program != null) 18.dp else 22.dp, end = 14.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(14.dp),
    ) {
        if (program != null) VText(twoDigits(program), Modifier.width(36.dp), style = VoLumType.value(19.sp, VoLumColors.Gold))
        Row(Modifier.weight(1f), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(14.dp)) {
            VText(title, Modifier.weight(1f, fill = false), style = VoLumType.SoundTitle)
            if (secondary != null) VText(secondary, style = VoLumType.label(10.sp, VoLumColors.TealDim))
        }
        if (dirty) VText("(unsaved)", style = VoLumType.label(10.sp, VoLumColors.Amber))
    }
}

/** `TwoDigits`: program numbers read 00..127. */
fun twoDigits(n: Int): String = if (n in 0..9) "0$n" else n.toString()
