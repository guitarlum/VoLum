package com.lum.volum

/**
 * JNI seam to the shared VoLum C++ core.
 *
 * M0 exposes only the DSP smoke; later milestones extend this with the Oboe
 * audio host controls and the IGraphics render/input surface hooks.
 */
object NativeBridge {
    init {
        System.loadLibrary("volum")
    }

    /** Human-readable native build tag. */
    external fun nativeVersion(): String

    /**
     * Load a .nam model from [path], reset/prewarm it, and process [numBlocks]
     * blocks of [blockSize] frames at [sampleRate]. Returns a status/timing
     * string (real-time factor, peak, finiteness). Prefixed with "FAIL:" on
     * a non-finite result or load error.
     */
    external fun nativeNamSmoke(
        path: String,
        sampleRate: Double,
        blockSize: Int,
        numBlocks: Int
    ): String

    // --- M2 real-time audio host ---------------------------------------
    /** Start the Oboe duplex stream. deviceId 0 = AAudio default. */
    external fun nativeAudioStart(inputDeviceId: Int, outputDeviceId: Int, sampleRate: Int): Boolean
    external fun nativeAudioStop()
    external fun nativeAudioIsRunning(): Boolean

    /** Load a .nam into the live engine. Returns "" on success, else an error. */
    external fun nativeLoadModel(path: String): String
    external fun nativeClearModel()

    external fun nativeSetInputGainDb(db: Double)
    external fun nativeSetOutputGainDb(db: Double)
    external fun nativeSetTone(bass: Double, mid: Double, treble: Double)
    external fun nativeSetToneEnabled(on: Boolean)
    external fun nativeSetBypass(on: Boolean)

    // --- POST / dynamics effects (faithful desktop DSP) -----------------
    external fun nativeSetGate(enabled: Boolean, thresholdDb: Double)
    external fun nativeSetDelay(
        enabled: Boolean, timeMs: Double, feedback: Double, mix: Double,
        mode: Int, tone: Double, age: Double, pingPong: Boolean
    )
    external fun nativeSetReverb(
        enabled: Boolean, mix: Double, decay: Double, tone: Double,
        preDelayMs: Double, shimmer: Double, mode: Int, subMode: Int
    )
    external fun nativeSetTremolo(
        enabled: Boolean, rateHz: Double, depthKnob: Double, shape: Double,
        mix: Double, crossoverHz: Double, mode: Int
    )

    // --- Tuner ----------------------------------------------------------
    external fun nativeSetTunerEnabled(on: Boolean)
    /** Detected fundamental in Hz from the raw DI, or 0 when unvoiced. */
    external fun nativeTunerHz(): Double

    /** One-line status: sample rate, callback size, latency, xruns, peak. */
    external fun nativeStatus(): String

    /**
     * Deterministic native engine self-test (bypass identity, unity passthrough,
     * output-gain law, gate/model finiteness, delay/reverb tail, tremolo
     * modulation). Returns "PASS ..." or "FAIL: <names>". [modelPath] may be
     * empty to skip the model group.
     */
    external fun nativeEngineSelfTest(modelPath: String): String
}

