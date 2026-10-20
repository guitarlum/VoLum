package com.volum.core

enum class ParamGroup { AMP, SUPPORT, DUAL, PRE, POST }

/**
 * The rig parameters the desktop UI surfaces (`NeuralAmpModeler/VoLumParams.h`),
 * with the desktop ranges and defaults from `NeuralAmpModeler.cpp`. Switches and
 * enums are stored as whole numbers.
 */
enum class Param(
    val label: String,
    val min: Double,
    val max: Double,
    val default: Double,
    val group: ParamGroup,
    val unit: String = "",
    val options: List<String> = emptyList(),
) {
    INPUT("INPUT", -20.0, 20.0, 0.0, ParamGroup.AMP, "dB"),
    GATE("GATE", -100.0, 0.0, -80.0, ParamGroup.AMP, "dB"),
    BASS("BASS", 0.0, 10.0, 5.0, ParamGroup.AMP),
    MID("MID", 0.0, 10.0, 5.0, ParamGroup.AMP),
    TREBLE("TREBLE", 0.0, 10.0, 5.0, ParamGroup.AMP),
    OUTPUT("OUTPUT", -40.0, 10.0, 0.0, ParamGroup.AMP, "dB"),
    GATE_ON("NOISE GATE", 0.0, 1.0, 1.0, ParamGroup.AMP),
    EQ_ON("EQ", 0.0, 1.0, 1.0, ParamGroup.AMP),

    SUPPORT_INPUT("INPUT", -20.0, 20.0, 0.0, ParamGroup.SUPPORT, "dB"),
    SUPPORT_GATE("GATE", -100.0, 0.0, -80.0, ParamGroup.SUPPORT, "dB"),
    SUPPORT_BASS("BASS", 0.0, 10.0, 5.0, ParamGroup.SUPPORT),
    SUPPORT_MID("MID", 0.0, 10.0, 5.0, ParamGroup.SUPPORT),
    SUPPORT_TREBLE("TREBLE", 0.0, 10.0, 5.0, ParamGroup.SUPPORT),
    SUPPORT_OUTPUT("OUTPUT", -40.0, 10.0, -3.0, ParamGroup.SUPPORT, "dB"),
    SUPPORT_GATE_ON("NOISE GATE", 0.0, 1.0, 1.0, ParamGroup.SUPPORT),
    SUPPORT_EQ_ON("EQ", 0.0, 1.0, 1.0, ParamGroup.SUPPORT),
    SUPPORT_POLARITY("Ø", 0.0, 1.0, 1.0, ParamGroup.SUPPORT),

    DUAL_ON("DUAL AMP", 0.0, 1.0, 0.0, ParamGroup.DUAL),
    MAIN_PAN("PAN", -1.0, 1.0, 0.0, ParamGroup.DUAL),
    SUPPORT_PAN("PAN", -1.0, 1.0, 0.0, ParamGroup.DUAL),

    PITCH_ON("PITCH", 0.0, 1.0, 0.0, ParamGroup.PRE),
    PITCH_MODE("MODE", 0.0, 1.0, 1.0, ParamGroup.PRE, options = listOf("TRANSPOSE", "OCTAVER")),
    PITCH_SEMI("SEMI", -12.0, 7.0, -2.0, ParamGroup.PRE, "st"),
    PITCH_MIX("MIX", 0.0, 1.0, 1.0, ParamGroup.PRE),
    PITCH_OCT_DOWN("OCT DN", 0.0, 1.0, 0.8, ParamGroup.PRE),
    PITCH_OCT_UP("OCT UP", 0.0, 1.0, 0.0, ParamGroup.PRE),
    PITCH_DRY("DRY", 0.0, 1.0, 1.0, ParamGroup.PRE),
    PITCH_VOICING("VOICING", 0.0, 1.0, 1.0, ParamGroup.PRE, options = listOf("VINTAGE", "MODERN")),
    PITCH_LEVEL("LEVEL", -20.0, 20.0, 0.0, ParamGroup.PRE, "dB"),
    PITCH_CHAR("CHARACTER", 0.0, 2.0, 1.0, ParamGroup.PRE, options = listOf("DROP", "INSTANT", "POLY")),

    COMP_ON("COMP", 0.0, 1.0, 0.0, ParamGroup.PRE),
    COMP_AMOUNT("INPUT", 0.0, 10.0, 3.0, ParamGroup.PRE),
    COMP_ATTACK("ATTACK", 0.1, 30.0, 4.0, ParamGroup.PRE, "ms"),
    COMP_RELEASE("RELEASE", 20.0, 800.0, 120.0, ParamGroup.PRE, "ms"),
    COMP_LEVEL("OUTPUT", -20.0, 20.0, 0.0, ParamGroup.PRE, "dB"),

    NAM1_ON("NAM 1", 0.0, 1.0, 0.0, ParamGroup.PRE),
    NAM1_CAPTURE("CAPTURE", 0.0, 127.0, 0.0, ParamGroup.PRE),
    NAM1_GAIN("GAIN", -20.0, 20.0, 0.0, ParamGroup.PRE, "dB"),
    NAM1_BASS("BASS", 0.0, 10.0, 5.0, ParamGroup.PRE),
    NAM1_MID("MID", 0.0, 10.0, 5.0, ParamGroup.PRE),
    NAM1_MID_HZ("MID Hz", 150.0, 2500.0, 650.0, ParamGroup.PRE, "Hz"),
    NAM1_TREBLE("TREBLE", 0.0, 10.0, 5.0, ParamGroup.PRE),
    NAM1_LEVEL("LEVEL", -20.0, 20.0, 0.0, ParamGroup.PRE, "dB"),

    NAM2_ON("NAM 2", 0.0, 1.0, 0.0, ParamGroup.PRE),
    NAM2_CAPTURE("CAPTURE", 0.0, 127.0, 0.0, ParamGroup.PRE),
    NAM2_GAIN("GAIN", -20.0, 20.0, 0.0, ParamGroup.PRE, "dB"),
    NAM2_BASS("BASS", 0.0, 10.0, 5.0, ParamGroup.PRE),
    NAM2_MID("MID", 0.0, 10.0, 5.0, ParamGroup.PRE),
    NAM2_MID_HZ("MID Hz", 150.0, 2500.0, 650.0, ParamGroup.PRE, "Hz"),
    NAM2_TREBLE("TREBLE", 0.0, 10.0, 5.0, ParamGroup.PRE),
    NAM2_LEVEL("LEVEL", -20.0, 20.0, 0.0, ParamGroup.PRE, "dB"),

    CHORUS_ON("CHORUS", 0.0, 1.0, 0.0, ParamGroup.POST),
    CHORUS_MODE("MODE", 0.0, 3.0, 3.0, ParamGroup.POST, options = listOf("CLASSIC", "WARPED", "CLEAR", "ENSEMBLE")),
    CHORUS_RATE("RATE", 0.0, 1.0, 0.35, ParamGroup.POST),
    CHORUS_DEPTH("DEPTH", 0.0, 1.0, 0.5, ParamGroup.POST),
    CHORUS_TONE("TONE", 0.0, 1.0, 0.5, ParamGroup.POST),
    CHORUS_WIDTH("WIDTH", 0.0, 1.0, 1.0, ParamGroup.POST),
    CHORUS_MIX("MIX", 0.0, 1.0, 0.5, ParamGroup.POST),

    DELAY_ON("DELAY", 0.0, 1.0, 0.0, ParamGroup.POST),
    DELAY_MODE("MODE", 0.0, 2.0, 0.0, ParamGroup.POST, options = listOf("DIGITAL", "ANALOG", "REVERSE")),
    DELAY_TIME("TIME", 10.0, 2000.0, 320.0, ParamGroup.POST, "ms"),
    DELAY_FEEDBACK("FEEDBACK", 0.0, 0.99, 0.35, ParamGroup.POST),
    DELAY_MIX("MIX", 0.0, 1.0, 0.28, ParamGroup.POST),
    DELAY_TONE("TONE", 0.0, 1.0, 0.5, ParamGroup.POST),
    DELAY_AGE("CHARACTER", 0.0, 1.0, 0.0, ParamGroup.POST),
    DELAY_PING_PONG("PING-PONG", 0.0, 1.0, 0.0, ParamGroup.POST),
    DELAY_SYNC("TEMPO SYNC", 0.0, 1.0, 0.0, ParamGroup.POST),
    DELAY_DIVISION("DIVISION", 0.0, 7.0, 1.0, ParamGroup.POST, options = DIVISIONS),

    REVERB_ON("REVERB", 0.0, 1.0, 0.0, ParamGroup.POST),
    REVERB_MODE("MODE", 0.0, 2.0, 0.0, ParamGroup.POST, options = listOf("HALL", "PLATE", "OKTAVERB")),
    REVERB_SUB("VOICE", 0.0, 2.0, 1.0, ParamGroup.POST, options = listOf("HALO", "SHIMMER", "BLOOM")),
    REVERB_MIX("MIX", 0.0, 1.0, 0.2, ParamGroup.POST),
    REVERB_DECAY("DECAY", 0.1, 10.0, 2.5, ParamGroup.POST, "s"),
    REVERB_TONE("TONE", 0.0, 10.0, 5.0, ParamGroup.POST),
    REVERB_PRE_DELAY("PRE-DLY", 0.0, 200.0, 10.0, ParamGroup.POST, "ms"),
    REVERB_SHIMMER("INTENSITY", 0.0, 1.0, 0.0, ParamGroup.POST),

    TREM_ON("TREM", 0.0, 1.0, 0.0, ParamGroup.POST),
    TREM_MODE("MODE", 0.0, 2.0, 0.0, ParamGroup.POST, options = listOf("OPTICAL", "BIAS", "HARMONIC")),
    TREM_RATE("RATE", 0.1, 20.0, 3.0, ParamGroup.POST, "Hz"),
    TREM_DEPTH("DEPTH", 0.0, 1.0, 0.85, ParamGroup.POST),
    TREM_SHAPE("SHAPE", 0.0, 1.0, 0.0, ParamGroup.POST),
    TREM_MIX("MIX", 0.0, 1.0, 0.6, ParamGroup.POST),
    TREM_XOVER("X-OVER", 200.0, 2000.0, 800.0, ParamGroup.POST, "Hz"),
    TREM_SYNC("TEMPO SYNC", 0.0, 1.0, 0.0, ParamGroup.POST),
    TREM_DIVISION("DIVISION", 0.0, 7.0, 1.0, ParamGroup.POST, options = DIVISIONS),
    ;

    val isSwitch: Boolean get() = min == 0.0 && max == 1.0 && options.isEmpty() && name.endsWith("_ON") ||
        this == DELAY_PING_PONG || this == DELAY_SYNC || this == TREM_SYNC || this == SUPPORT_POLARITY
    val isEnum: Boolean get() = options.isNotEmpty()

    fun clamp(v: Double): Double = v.coerceIn(min, max)
    fun normalized(v: Double): Double = if (max == min) 0.0 else (v - min) / (max - min)
    fun fromNormalized(n: Double): Double = clamp(min + n.coerceIn(0.0, 1.0) * (max - min))

    companion object {
        val defaults: Map<Param, Double> = entries.associateWith { it.default }
    }
}

val DIVISIONS = listOf("1/2", "1/4", "1/4.", "1/4T", "1/8", "1/8.", "1/8T", "1/16")

/** Knob rows per effect mode; each mode keeps its own values of these. */
object ModeParams {
    val chorus = listOf(Param.CHORUS_RATE, Param.CHORUS_DEPTH, Param.CHORUS_TONE, Param.CHORUS_WIDTH, Param.CHORUS_MIX)
    val delay = listOf(Param.DELAY_TIME, Param.DELAY_FEEDBACK, Param.DELAY_MIX, Param.DELAY_TONE, Param.DELAY_AGE)
    val reverb = listOf(Param.REVERB_MIX, Param.REVERB_DECAY, Param.REVERB_TONE, Param.REVERB_PRE_DELAY, Param.REVERB_SHIMMER)
    val trem = listOf(Param.TREM_RATE, Param.TREM_DEPTH, Param.TREM_SHAPE, Param.TREM_MIX, Param.TREM_XOVER)

    fun forModeParam(mode: Param): List<Param> = when (mode) {
        Param.CHORUS_MODE -> chorus
        Param.DELAY_MODE -> delay
        Param.REVERB_MODE -> reverb
        Param.TREM_MODE -> trem
        else -> emptyList()
    }
}

/** The knob row the desktop shows for a block in its current mode (`VoLumKeyboardModel.h`). */
object KnobRows {
    fun forPre(block: PreBlock, s: VoLumState): List<Param> = when (block) {
        PreBlock.PITCH -> if (s.param(Param.PITCH_MODE) == 0.0) listOf(Param.PITCH_SEMI, Param.PITCH_MIX, Param.PITCH_LEVEL)
        else listOf(Param.PITCH_OCT_DOWN, Param.PITCH_OCT_UP, Param.PITCH_DRY, Param.PITCH_LEVEL)
        PreBlock.COMP -> listOf(Param.COMP_AMOUNT, Param.COMP_ATTACK, Param.COMP_RELEASE, Param.COMP_LEVEL)
        PreBlock.NAM1 -> listOf(Param.NAM1_GAIN, Param.NAM1_BASS, Param.NAM1_MID, Param.NAM1_MID_HZ, Param.NAM1_TREBLE, Param.NAM1_LEVEL)
        PreBlock.NAM2 -> listOf(Param.NAM2_GAIN, Param.NAM2_BASS, Param.NAM2_MID, Param.NAM2_MID_HZ, Param.NAM2_TREBLE, Param.NAM2_LEVEL)
    }

    fun forPost(block: PostBlock, s: VoLumState): List<Param> = when (block) {
        PostBlock.CHORUS -> ModeParams.chorus
        PostBlock.DELAY -> if (s.param(Param.DELAY_SYNC) == 1.0) ModeParams.delay - Param.DELAY_TIME else ModeParams.delay
        PostBlock.REVERB -> if (s.param(Param.REVERB_MODE) == 2.0) ModeParams.reverb else ModeParams.reverb - Param.REVERB_SHIMMER
        PostBlock.TREM -> {
            val base = if (s.param(Param.TREM_MODE) == 2.0) ModeParams.trem else ModeParams.trem - Param.TREM_XOVER
            if (s.param(Param.TREM_SYNC) == 1.0) base - Param.TREM_RATE else base
        }
    }

    fun forAmp(lane: Lane, dual: Boolean): List<Param> = when {
        lane == Lane.SUPPORT -> listOf(Param.SUPPORT_INPUT, Param.SUPPORT_GATE, Param.SUPPORT_BASS, Param.SUPPORT_MID, Param.SUPPORT_TREBLE, Param.SUPPORT_OUTPUT, Param.SUPPORT_PAN)
        dual -> listOf(Param.INPUT, Param.GATE, Param.BASS, Param.MID, Param.TREBLE, Param.OUTPUT, Param.MAIN_PAN)
        else -> listOf(Param.INPUT, Param.GATE, Param.BASS, Param.MID, Param.TREBLE, Param.OUTPUT)
    }

    /** Desktop delay character label: GRIT (Digital), WEAR (Analog), BLOOM (Reverse). */
    fun delayCharacterLabel(s: VoLumState): String = listOf("GRIT", "WEAR", "BLOOM")[s.param(Param.DELAY_MODE).toInt().coerceIn(0, 2)]
}
