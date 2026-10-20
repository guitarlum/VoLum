package com.volum.design.art

import kotlin.math.max

/** A static hero art: `Draw<Amp>Hero(g, rect)`. */
typealias HeroArt = (ArtCanvas, Box) -> Unit

/**
 * `art/VoLumArtAnimator.h` contract:
 * - [prepare] builds every cached layer for [r] at density [d]; called whenever
 *   the rect or density changes, never per frame.
 * - [draw] draws the whole art for this frame from what [prepare] cached. It
 *   allocates nothing and is a pure function of (r, m).
 * - Rest rule: `m.isRest` draws the static hero exactly (plus PLAY's bloom).
 */
abstract class ArtAnimator {
    abstract fun prepare(r: Box, d: Float)
    abstract fun draw(g: ArtCanvas, r: Box, m: ArtMotion)
    open fun drop() {}
}

/**
 * Class A (`AdditiveArtAnimator`): the whole static art in one layer, motion
 * drawn on top. Subclasses add [drawExtras] (skipped at rest).
 */
abstract class AdditiveArtAnimator(private val hero: HeroArt) : ArtAnimator() {
    protected val full = ArtLayer()

    override fun prepare(r: Box, d: Float) {
        if (!full.matches(r, d)) full.build(r, d) { hero(it, r) }
        full.addBitmap()
        prepareExtras(r, d)
    }

    override fun draw(g: ArtCanvas, r: Box, m: ArtMotion) {
        if (m.isRest) {
            g.drawLayerLit(full, m.bloom)
            return
        }
        val keep = 1f - baseDim(m).coerceIn(0f, 1f)
        g.drawLayer(full, keep)
        g.drawLayerAdd(full, m.bloom * keep)
        drawExtras(g, r, m)
    }

    override fun drop() = full.reset()

    protected open fun prepareExtras(r: Box, d: Float) {}

    /** How far the static art steps back (0..1) while the extras play; 0 at rest. */
    protected open fun baseDim(m: ArtMotion): Float = 0f

    protected abstract fun drawExtras(g: ArtCanvas, r: Box, m: ArtMotion)
}

/** An art whose registry flag is off on the desktop: the static layer, lit by PLAY's bloom. */
class StaticArtAnimator(hero: HeroArt) : AdditiveArtAnimator(hero) {
    override fun drawExtras(g: ArtCanvas, r: Box, m: ArtMotion) {}
}

/** `Phase(clock, rate, period)` of the animators. */
fun phase(clock: Double, rate: Double, period: Double): Float = ((clock * rate) % period).toFloat()

fun maxf(a: Float, b: Float) = max(a, b)
