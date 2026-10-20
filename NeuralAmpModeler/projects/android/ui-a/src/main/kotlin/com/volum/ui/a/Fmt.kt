package com.volum.ui.a

import com.volum.core.Param
import com.volum.core.Pedal
import com.volum.core.PostBlock
import com.volum.core.PreBlock
import com.volum.core.VoLumState
import com.volum.design.art.Motif
import java.util.Locale
import kotlin.math.abs
import kotlin.math.roundToInt

internal fun f(pattern: String, vararg args: Any): String = String.format(Locale.US, pattern, *args)

/** Readout under a knob, in the desktop's units. */
internal fun fmt(p: Param, v: Double): String = when {
    p.isEnum -> p.options[v.roundToInt().coerceIn(0, p.options.lastIndex)]
    p == Param.MAIN_PAN || p == Param.SUPPORT_PAN -> pan(v)
    p == Param.PITCH_SEMI -> (if (v > 0) "+" else "") + f("%d st", v.roundToInt())
    p.unit == "dB" -> f("%.1f dB", v)
    p.unit == "Hz" && p.max > 100 -> if (v >= 1000) f("%.2f kHz", v / 1000) else f("%.0f Hz", v)
    p.unit == "Hz" -> f("%.1f Hz", v)
    p.unit == "ms" && p.max <= 30 -> f("%.1f ms", v)
    p.unit == "ms" -> f("%.0f ms", v)
    p.unit == "s" -> f("%.1f s", v)
    p.max <= 1.0 -> f("%d %%", (v * 100).roundToInt())
    else -> f("%.1f", v)
}

private fun pan(v: Double): String {
    val pct = (abs(v) * 100).roundToInt()
    return when {
        pct == 0 -> "C"
        v < 0 -> "L$pct"
        else -> "R$pct"
    }
}

internal fun hz(v: Double): String = if (v >= 1000) f("%.1f kHz", v / 1000) else f("%.0f Hz", v)

internal fun program(n: Int): String = n.toString().padStart(2, '0')

internal fun captureName(s: VoLumState, slot: Int): String {
    val idx = s.param(if (slot == 1) Param.NAM1_CAPTURE else Param.NAM2_CAPTURE).roundToInt()
    return s.library.pedals.firstOrNull { it.index == idx }?.name ?: "EMPTY"
}

internal fun preTitle(b: PreBlock, s: VoLumState): String = when (b) {
    PreBlock.PITCH -> if (s.param(Param.PITCH_MODE) == 0.0) "Transpose" else "Octaver"
    PreBlock.COMP -> "Compressor"
    PreBlock.NAM1 -> captureFullName(s, 1).takeIf { it != "Empty" } ?: "NAM Pedal 1"
    PreBlock.NAM2 -> captureFullName(s, 2).takeIf { it != "Empty" } ?: "NAM Pedal 2"
}

internal fun postTitle(b: PostBlock): String = when (b) {
    PostBlock.CHORUS -> "Chorus"
    PostBlock.DELAY -> "Delay"
    PostBlock.REVERB -> "Reverb"
    PostBlock.TREM -> "Tremolo"
}

internal fun postCard(b: PostBlock): String = when (b) {
    PostBlock.CHORUS -> "CHORUS"
    PostBlock.DELAY -> "DELAY"
    PostBlock.REVERB -> "REVERB"
    PostBlock.TREM -> "TREM"
}

/** PLAY stomp and chain-card label: the capture's short name on the NAM slots, as the desktop prints it. */
internal fun stompLabel(b: PreBlock, s: VoLumState): String = when (b) {
    PreBlock.PITCH -> "PITCH"
    PreBlock.COMP -> "COMP"
    PreBlock.NAM1 -> captureName(s, 1).takeIf { it != "EMPTY" } ?: "NAM 1"
    PreBlock.NAM2 -> captureName(s, 2).takeIf { it != "EMPTY" } ?: "NAM 2"
}

internal val PreBlock.motif: Motif
    get() = when (this) {
        PreBlock.PITCH -> Motif.PITCH
        PreBlock.COMP -> Motif.COMP
        PreBlock.NAM1 -> Motif.NAM1
        PreBlock.NAM2 -> Motif.NAM2
    }

internal val PostBlock.motif: Motif
    get() = when (this) {
        PostBlock.CHORUS -> Motif.CHORUS
        PostBlock.DELAY -> Motif.DELAY
        PostBlock.REVERB -> Motif.REVERB
        PostBlock.TREM -> Motif.TREM
    }

/** Motif variant 1 is the Octaver's chevrons, 0 the Transpose helix. */
internal fun pitchVariant(b: PreBlock, s: VoLumState): Int = if (b == PreBlock.PITCH && s.param(Param.PITCH_MODE) == 1.0) 1 else 0

/** Desktop `kPrePedalCaptureMetadata`: full name and group of each bundled PRE capture, keyed by its short name. */
internal enum class CaptureGroup(val label: String) { KLON("KLON"), TS("TS / BOOST"), DIST("DISTORTION"), FUZZ("FUZZ") }

internal data class CaptureInfo(val full: String, val group: CaptureGroup, val rank: Int)

private val captureInfo = mapOf(
    "Klon" to CaptureInfo("Klon", CaptureGroup.KLON, 10),
    "Myth" to CaptureInfo("PettyJohn Myth", CaptureGroup.KLON, 20),
    "TS" to CaptureInfo("Halcyon TS", CaptureGroup.TS, 30),
    "TS+" to CaptureInfo("Halcyon TS +Gain", CaptureGroup.TS, 40),
    "Mash" to CaptureInfo("PettyJohn Mash", CaptureGroup.TS, 50),
    "Revival Drive" to CaptureInfo("Revival Drive", CaptureGroup.DIST, 60),
    "Fatbee" to CaptureInfo("Fatbee", CaptureGroup.FUZZ, 70),
    "Nuke" to CaptureInfo("PettyJohn Nuke", CaptureGroup.FUZZ, 80),
    "Bender" to CaptureInfo("JHS Bender", CaptureGroup.FUZZ, 90),
)

internal fun captureInfo(p: Pedal): CaptureInfo? = if (p.custom) null else captureInfo[p.name]

/** The capture's desktop full name ("Halcyon TS"); custom pedals keep their own name. */
internal fun captureFullName(s: VoLumState, slot: Int): String {
    val idx = s.param(if (slot == 1) Param.NAM1_CAPTURE else Param.NAM2_CAPTURE).roundToInt()
    val p = s.library.pedals.firstOrNull { it.index == idx } ?: return "Empty"
    if (p.index == 0) return "Empty"
    return captureInfo(p)?.full ?: p.name
}

internal val PreBlock.onParam: Param
    get() = when (this) {
        PreBlock.PITCH -> Param.PITCH_ON
        PreBlock.COMP -> Param.COMP_ON
        PreBlock.NAM1 -> Param.NAM1_ON
        PreBlock.NAM2 -> Param.NAM2_ON
    }

internal val PostBlock.onParam: Param
    get() = when (this) {
        PostBlock.CHORUS -> Param.CHORUS_ON
        PostBlock.DELAY -> Param.DELAY_ON
        PostBlock.REVERB -> Param.REVERB_ON
        PostBlock.TREM -> Param.TREM_ON
    }

internal val PreBlock.key: String get() = name.lowercase()
internal val PostBlock.key: String get() = name.lowercase()

/** Feature ids a knob carries; the main AMP knobs also take reset and exact entry. */
internal fun knobIds(p: Param): List<String> = when (p) {
    Param.PITCH_LEVEL -> listOf("pre.pitch.level", "pre.pitch.oct_level")
    Param.DELAY_AGE -> listOf("post.delay.grit", "post.delay.wear", "post.delay.bloom")
    Param.INPUT, Param.GATE, Param.BASS, Param.MID, Param.TREBLE, Param.OUTPUT -> listOfNotNull(knobId(p), "amp.knob.reset", "amp.knob.exact")
    else -> listOfNotNull(knobId(p))
}

private fun knobId(p: Param): String? = when (p) {
    Param.INPUT -> "amp.knob.input"
    Param.GATE -> "amp.knob.gate"
    Param.BASS -> "amp.knob.bass"
    Param.MID -> "amp.knob.mid"
    Param.TREBLE -> "amp.knob.treble"
    Param.OUTPUT -> "amp.knob.output"
    Param.MAIN_PAN -> "dual.pan.main"
    Param.SUPPORT_PAN -> "dual.pan.support"
    Param.SUPPORT_INPUT -> "dual.support.knob.input"
    Param.SUPPORT_GATE -> "dual.support.knob.gate"
    Param.SUPPORT_BASS -> "dual.support.knob.bass"
    Param.SUPPORT_MID -> "dual.support.knob.mid"
    Param.SUPPORT_TREBLE -> "dual.support.knob.treble"
    Param.SUPPORT_OUTPUT -> "dual.support.knob.output"
    Param.PITCH_SEMI -> "pre.pitch.semi"
    Param.PITCH_MIX -> "pre.pitch.mix"
    Param.PITCH_OCT_DOWN -> "pre.pitch.octdown"
    Param.PITCH_OCT_UP -> "pre.pitch.octup"
    Param.PITCH_DRY -> "pre.pitch.dry"
    Param.COMP_AMOUNT -> "pre.comp.input"
    Param.COMP_ATTACK -> "pre.comp.attack"
    Param.COMP_RELEASE -> "pre.comp.release"
    Param.COMP_LEVEL -> "pre.comp.output"
    Param.NAM1_GAIN -> "pre.nam1.gain"
    Param.NAM1_BASS -> "pre.nam1.bass"
    Param.NAM1_MID -> "pre.nam1.mid"
    Param.NAM1_MID_HZ -> "pre.nam1.midhz"
    Param.NAM1_TREBLE -> "pre.nam1.treble"
    Param.NAM1_LEVEL -> "pre.nam1.level"
    Param.NAM2_GAIN -> "pre.nam2.gain"
    Param.NAM2_BASS -> "pre.nam2.bass"
    Param.NAM2_MID -> "pre.nam2.mid"
    Param.NAM2_MID_HZ -> "pre.nam2.midhz"
    Param.NAM2_TREBLE -> "pre.nam2.treble"
    Param.NAM2_LEVEL -> "pre.nam2.level"
    Param.CHORUS_RATE -> "post.chorus.rate"
    Param.CHORUS_DEPTH -> "post.chorus.depth"
    Param.CHORUS_TONE -> "post.chorus.tone"
    Param.CHORUS_WIDTH -> "post.chorus.width"
    Param.CHORUS_MIX -> "post.chorus.mix"
    Param.DELAY_TIME -> "post.delay.time"
    Param.DELAY_FEEDBACK -> "post.delay.feedback"
    Param.DELAY_MIX -> "post.delay.mix"
    Param.DELAY_TONE -> "post.delay.tone"
    Param.REVERB_MIX -> "post.reverb.mix"
    Param.REVERB_DECAY -> "post.reverb.decay"
    Param.REVERB_TONE -> "post.reverb.tone"
    Param.REVERB_PRE_DELAY -> "post.reverb.predelay"
    Param.REVERB_SHIMMER -> "post.reverb.intensity"
    Param.TREM_RATE -> "post.trem.rate"
    Param.TREM_DEPTH -> "post.trem.depth"
    Param.TREM_SHAPE -> "post.trem.shape"
    Param.TREM_MIX -> "post.trem.mix"
    Param.TREM_XOVER -> "post.trem.xover"
    else -> null
}
