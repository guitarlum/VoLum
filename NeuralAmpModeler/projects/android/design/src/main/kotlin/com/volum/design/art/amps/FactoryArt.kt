package com.volum.design.art.amps

import com.volum.design.art.ArtAnimator
import com.volum.design.art.HeroArt
import com.volum.design.art.StaticArtAnimator

/** `art/VoLumArtDispatch.h` + `art/VoLumArtRegistry.h`: hero art and PLAY animator by fractal case. */
object FactoryArt {
    class Spec(val case: Int, val amp: String, val motif: String)

    val specs = listOf(
        Spec(0, "Ampete One", "Dragon Nebula"),
        Spec(1, "Bad Cat Mini Cat", "Eyes in the Dark"),
        Spec(2, "Brunetti XL 2", "Verdant Fern"),
        Spec(3, "Fryette Deliv. 120", "Spiral Galaxy"),
        Spec(4, "H&K TriAmp Mk2", "Lissajous Nebula"),
        Spec(5, "Lichtlaerm Prom.", "Koch Deep"),
        Spec(6, "Marshall 2204", "Windswept Glow"),
        Spec(7, "Marshall JMP 2203", "Depth Triforce"),
        Spec(8, "Marshall JVM", "Levy Nebula"),
        Spec(9, "Orange OD120", "Ember Bulb"),
        Spec(10, "Orange ORS100", "Julia Nebula"),
        Spec(11, "Sebago Texas Fl.", "Clifford Nebula"),
        Spec(12, "Soldano SLO100", "Beacon Sweep"),
        Spec(13, "THC Sunset", "Dark Sun"),
        Spec(14, "Diezel Herbert Mk1", "Lichtenberg Glow"),
    )

    private fun norm(case: Int) = ((case % 15) + 15) % 15

    fun hero(case: Int): HeroArt = when (norm(case)) {
        0 -> NebulaHeroes::ampeteOne
        1 -> SceneHeroes::badCat
        2 -> SceneHeroes::brunetti
        3 -> SceneHeroes::fryette
        4 -> NebulaHeroes::hkTriamp
        5 -> NebulaHeroes::lichtlaerm
        6 -> SceneHeroes::marshall2204
        7 -> SceneHeroes::jmp2203
        8 -> SceneHeroes::jvm210
        9 -> NebulaHeroes::orangeOd120
        10 -> NebulaHeroes::orangeOrs100
        11 -> NebulaHeroes::sebago
        12 -> SceneHeroes::soldano
        13 -> ThcSunset::hero
        else -> SceneHeroes::diezelHerbert
    }

    /** Arts whose animator is ported move; the rest draw their static layer lit by PLAY's bloom. */
    fun animator(case: Int): ArtAnimator = when (norm(case)) {
        4 -> HkTriampAnimator()
        12 -> SoldanoAnimator()
        13 -> ThcSunset.animator()
        else -> StaticArtAnimator(hero(case))
    }
}
