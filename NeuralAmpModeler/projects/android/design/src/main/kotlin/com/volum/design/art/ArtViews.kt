package com.volum.design.art

import androidx.compose.animation.core.withInfiniteAnimationFrameNanos
import androidx.compose.foundation.layout.Spacer
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.LongState
import androidx.compose.runtime.mutableLongStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.drawWithCache
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.drawIntoCanvas
import androidx.compose.ui.graphics.nativeCanvas
import com.volum.core.Telemetry

/**
 * The frame clock every moving VoLum surface reads in its draw phase only, so a
 * frame redraws without recomposing. Runs on `withInfiniteAnimationFrameNanos`
 * (the harness test clock skips it) and stops with the composition, when the
 * window is not drawn, or when [running] is false.
 */
@Composable
fun rememberArtFrames(running: Boolean = true): LongState {
    val t = remember { mutableLongStateOf(0L) }
    LaunchedEffect(running) {
        if (!running) return@LaunchedEffect
        val onFrame: (Long) -> Unit = { t.longValue = it }
        while (true) withInfiniteAnimationFrameNanos(onFrame)
    }
    return t
}

/** Runs [block] on the native canvas, scaled so one art unit is one dp. */
internal inline fun DrawScope.artUnits(g: ArtCanvas, block: (ArtCanvas) -> Unit) {
    drawIntoCanvas { c ->
        val n = c.nativeCanvas
        n.save()
        n.scale(density, density)
        block(g.begin(n))
        n.restore()
    }
}

/**
 * An amp's static hero art (what BUILD shows), cached once per size into a
 * bitmap and blitted 1:1. Use it wherever the art does not move.
 */
@Composable
fun AmpArt(art: ArtRef, modifier: Modifier = Modifier) {
    Spacer(
        modifier.drawWithCache {
            val box = Box(0f, 0f, size.width / density, size.height / density)
            val layer = ArtLayer().apply { build(box, density) { art.hero(it, box) } }
            val g = ArtCanvas()
            onDrawBehind { artUnits(g) { it.drawLayer(layer) } }
        },
    )
}

/** The amp-list thumbnail (`DrawSidebarMiniFractal` / custom art), on the hero well. */
@Composable
fun AmpThumbnail(art: ArtRef, modifier: Modifier = Modifier, selected: Boolean = false) {
    Spacer(
        modifier.drawWithCache {
            val box = Box(0f, 0f, size.width / density, size.height / density)
            val layer = ArtLayer().apply { build(box, density) { art.thumbnail(it, box) } }
            val frame = argb(if (selected) 80 else 58, 120, 195, 210)
            val g = ArtCanvas()
            onDrawBehind {
                artUnits(g) {
                    it.drawLayer(layer)
                    it.drawRect(frame, 0.5f, 0.5f, box.r - 0.5f, box.b - 0.5f)
                }
            }
        },
    )
}

/** What drives PLAY art: the fake engine's telemetry, read per frame (never as Compose state). */
fun interface ArtInput {
    fun read(): Telemetry
}

/**
 * Per-panel state of a moving art: the animator, the motion clock and the
 * idle pulse phase. Advanced from the draw phase, so it is plain objects.
 */
internal class StageMotion(val art: ArtRef, animate: Boolean) {
    val animator: ArtAnimator = if (animate) art.animator() else StaticArtAnimator(art.hero)
    val state = ArtMotionState()
    val frame = ArtMotion()
    var lastNanos = 0L
    var idlePhase = 0f
    var glow = 0f

    fun tick(nanos: Long, energy: Float, attack: Float): ArtMotion {
        val dt = if (lastNanos == 0L || nanos <= lastNanos) 0f else (nanos - lastNanos) / 1e9f
        if (nanos > lastNanos) lastNanos = nanos
        glow = PlayLight.glow(energy, attack)
        state.advance(energy, attack, PlayLight.bloomWeight(glow), dt)
        idlePhase = (idlePhase + ArtTiming.IDLE_PHASE_PER_SECOND * dt) % ArtCanvas.TAU
        return state.live(art.seed, frame)
    }
}

/**
 * The art inside a PLAY stage panel, moving with [input]. [picks] false ignores
 * pick transients, which is how the desktop reference sheets were shot
 * (`VOLUM_ART_ANIM_DEBUG=<art>:<energy>:run`, pick 0). [animate] false is the
 * SYSTEM tab's "Animate art" off: the static art, still lit by the bloom.
 */
@Composable
fun MovingArt(
    art: ArtRef,
    input: ArtInput,
    modifier: Modifier = Modifier,
    animate: Boolean = true,
    picks: Boolean = true,
    running: Boolean = true,
) {
    val frames = rememberArtFrames(running)
    val motion = remember(art, animate) { StageMotion(art, animate) }
    DisposableEffect(motion) { onDispose { motion.animator.drop() } }
    Spacer(
        modifier.drawWithCache {
            val box = Box(0f, 0f, size.width / density, size.height / density)
            motion.animator.prepare(box, density)
            val g = ArtCanvas()
            onDrawBehind {
                val tele = input.read()
                val attack = if (picks) PlayLight.attackFromPickAge(tele.pickAgeMs) else 0f
                val m = motion.tick(frames.longValue, tele.energy.toFloat(), attack)
                artUnits(g) {
                    it.save()
                    it.clip(box.l, box.t, box.r, box.b)
                    motion.animator.draw(it, box, m)
                    it.restore()
                }
            }
        },
    )
}
