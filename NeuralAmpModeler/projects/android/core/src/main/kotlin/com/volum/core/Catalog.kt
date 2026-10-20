package com.volum.core

/** The bundled library of desktop VoLum 1.3: 15 amps, 21 Factory presets, 9 PRE captures. */
object Catalog {
    private data class BundledAmp(val name: String, val channels: Int, val presets: List<String>)

    private val bundled = listOf(
        BundledAmp("Ampete One", 4, listOf("Ampete Rhythm", "Ampete Lead")),
        BundledAmp("Bad Cat Mini Cat", 3, listOf("BadCat Crunch")),
        BundledAmp("Brunetti XL 2", 3, listOf("American Lead")),
        BundledAmp("Diezel Herbert Mk1", 4, listOf("Thicc Rhythm", "Sanitarium")),
        BundledAmp("Fryette Deliv. 120", 2, listOf("Dry Rhythm")),
        BundledAmp("H&K TriAmp Mk2", 6, listOf("HiFi Heavy", "HiFi Crunch")),
        BundledAmp("Lichtlaerm Prom.", 3, listOf("Modern Rhythm", "Modern Lead")),
        BundledAmp("Marshall 2204", 6, listOf("JCM800 Crunch")),
        BundledAmp("Marshall JMP 2203", 6, listOf("Crack the Skye")),
        BundledAmp("Marshall JVM", 6, listOf("Modern British")),
        BundledAmp("Orange OD120", 5, listOf("Stoner")),
        BundledAmp("Orange ORS100", 2, listOf("An old Soul")),
        BundledAmp("Sebago Texas Fl.", 2, listOf("The bestest Clean")),
        BundledAmp("Soldano SLO100", 3, listOf("SLO Lead", "SLO Crunch")),
        BundledAmp("THC Sunset", 5, listOf("Sunset Crunch", "Sunset Clean")),
    )

    /** Desktop `kAmpFractalCase`: the art each bundled amp draws. */
    val artCase = listOf(0, 1, 2, 14, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13)

    fun ampId(index: Int) = "amp:$index"
    fun factoryId(ampIndex: Int, n: Int) = "f:$ampIndex:$n"

    val amps: List<Amp> = bundled.mapIndexed { i, b -> Amp(id = ampId(i), name = b.name, channels = b.channels, art = artCase[i]) }

    val pedals: List<Pedal> = listOf(
        Pedal(0, "EMPTY", false),
        Pedal(1, "Nuke", false),
        Pedal(2, "Bender", false),
        Pedal(3, "Myth", false),
        Pedal(4, "Mash", false),
        Pedal(5, "Revival Drive", false),
        Pedal(6, "Klon", false),
        Pedal(7, "TS", false),
        Pedal(8, "TS+", false),
        Pedal(9, "Fatbee", false),
    )

    val factoryPresets: List<Preset> = bundled.flatMapIndexed { i, b ->
        b.presets.mapIndexed { n, name ->
            val gainy = name.contains("Lead") || name.contains("Heavy") || name.contains("Crunch")
            val params = Param.defaults.toMutableMap().apply {
                put(Param.INPUT, if (gainy) 2.0 else 0.0)
                put(Param.BASS, 5.0 + n)
                put(Param.TREBLE, 6.0 - n * 0.5)
                put(Param.REVERB_ON, 1.0)
                if (gainy) {
                    put(Param.NAM1_ON, 1.0)
                    put(Param.NAM1_CAPTURE, 6.0)
                }
                if (name.contains("Lead")) {
                    put(Param.DELAY_ON, 1.0)
                }
            }
            Preset(
                id = factoryId(i, n),
                ampId = ampId(i),
                name = name,
                factory = true,
                rig = Rig(params = params, channel = (n + if (gainy) 1 else 0).coerceAtMost(b.channels - 1), cab = Cab.V30),
            )
        }
    }

    /** The five starting Sounds (user guide "The Five Starting Sounds"). */
    val startingSounds: Map<Int, Sound> = listOf(
        "The bestest Clean", "SLO Crunch", "Modern Rhythm", "Crack the Skye", "Ampete Lead",
    ).mapIndexed { program, name ->
        val p = factoryPresets.first { it.name == name }
        program to Sound(p.ampId, p.id)
    }.toMap()

    fun defaultRig(amp: Amp): Rig = Rig(cab = if (amp.custom) Cab.NONE else Cab.G12)
}
