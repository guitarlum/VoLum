package com.volum.design.art

import com.volum.core.Amp
import com.volum.design.art.amps.FactoryArt

/**
 * Which art an amp draws: a factory fractal case (0..14, `kAmpFractalCase`)
 * or a custom-amp style (0..5). `Amp.art` already carries the right number.
 */
data class ArtRef(val custom: Boolean, val index: Int) {
    val seed: Int get() = artMotionSeed(custom, index)

    val hero: HeroArt get() = if (custom) { g, r -> Fractals.customAmpArt(g, r, index) } else FactoryArt.hero(index)

    /** A fresh animator; PLAY owns one per stage panel. */
    fun animator(): ArtAnimator = if (custom) Fractals.CustomArtAnimator(index) else FactoryArt.animator(index)

    /** The 22 px list thumbnail. */
    fun thumbnail(g: ArtCanvas, r: Box) {
        if (custom) Fractals.customAmpArt(g, r, index) else Fractals.sidebarMini(g, r, index)
    }

    companion object {
        fun of(amp: Amp): ArtRef = ArtRef(amp.custom, amp.art)
        fun factory(case: Int) = ArtRef(false, case)
        fun custom(style: Int) = ArtRef(true, style)
    }
}
