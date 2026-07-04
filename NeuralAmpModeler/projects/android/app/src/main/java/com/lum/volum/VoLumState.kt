package com.lum.volum

import android.content.Context
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import java.io.File

/**
 * Holds all UI-facing amp/effect state and pushes changes to the native engine.
 * Real (not normalized) values live here; the UI maps them to knob angles.
 *
 * Audio start/stop and model loading run off the UI thread (they open Oboe
 * streams and prewarm the NAM model). Parameter setters are lock-free atomics
 * in native, so they're safe to call directly on any thread.
 */
class VoLumState(private val appContext: Context) {

    // ---- Transport / status ------------------------------------------------
    var running by mutableStateOf(false)
    var busy by mutableStateOf(false)
    var status by mutableStateOf("Idle")
    var peak by mutableFloatStateOf(0f)
    var latencyMs by mutableStateOf("--")
    var bypass by mutableStateOf(false)

    // ---- Devices / model ---------------------------------------------------
    val inputDevices = mutableStateListOf<AudioEndpoint>()
    var deviceIndex by mutableIntStateOf(0)
    val models = mutableStateListOf<String>()
    var modelIndex by mutableIntStateOf(0)
    var modelLoaded by mutableStateOf(false)

    // ---- Tuner -------------------------------------------------------------
    var tunerEnabled by mutableStateOf(false)
    var tunerHz by mutableFloatStateOf(0f)

    // ---- Amp ---------------------------------------------------------------
    var inputGainDb by mutableFloatStateOf(0f)
    var outputGainDb by mutableFloatStateOf(0f)
    var bass by mutableFloatStateOf(5f)
    var mid by mutableFloatStateOf(5f)
    var treble by mutableFloatStateOf(5f)
    var toneEnabled by mutableStateOf(true)

    // ---- Noise gate --------------------------------------------------------
    var gateEnabled by mutableStateOf(false)
    var gateThresholdDb by mutableFloatStateOf(-70f)

    // ---- Delay -------------------------------------------------------------
    var delayEnabled by mutableStateOf(false)
    var delayTimeMs by mutableFloatStateOf(380f)
    var delayFeedback by mutableFloatStateOf(0.35f)
    var delayMix by mutableFloatStateOf(0.28f)
    var delayMode by mutableIntStateOf(0)

    // ---- Reverb ------------------------------------------------------------
    var reverbEnabled by mutableStateOf(false)
    var reverbMix by mutableFloatStateOf(0.35f)
    var reverbDecay by mutableFloatStateOf(3f)
    var reverbTone by mutableFloatStateOf(4.5f)
    var reverbMode by mutableIntStateOf(0)

    // ---- Tremolo -----------------------------------------------------------
    var tremEnabled by mutableStateOf(false)
    var tremRate by mutableFloatStateOf(5f)
    var tremDepth by mutableFloatStateOf(0.6f)
    var tremMode by mutableIntStateOf(1)

    init {
        refreshDevices()
        models.clear()
        appContext.assets.list("models")?.filter { it.endsWith(".nam") }?.sorted()?.let { models.addAll(it) }
    }

    fun refreshDevices() {
        val list = AudioDevices.inputs(appContext)
        inputDevices.clear()
        inputDevices.addAll(list)
        if (deviceIndex >= inputDevices.size) deviceIndex = 0
    }

    // ---- Transport ---------------------------------------------------------
    fun toggleAudio() {
        if (busy) return
        if (running) {
            busy = true
            Thread {
                NativeBridge.nativeAudioStop()
                running = false
                busy = false
            }.start()
        } else {
            busy = true
            status = "Starting..."
            Thread {
                val devId = inputDevices.getOrNull(deviceIndex)?.id ?: 0
                val ok = NativeBridge.nativeAudioStart(devId, devId, 48000)
                if (ok) pushAll()
                running = ok
                busy = false
                if (!ok) status = "Start failed (permission / device)"
            }.start()
        }
    }

    fun loadSelectedModel() {
        val name = models.getOrNull(modelIndex) ?: return
        busy = true
        status = "Loading $name..."
        Thread {
            val dest = copyAsset(name)
            val err = NativeBridge.nativeLoadModel(dest.absolutePath)
            modelLoaded = err.isEmpty()
            status = if (err.isEmpty()) "Loaded $name" else "Load error: $err"
            busy = false
        }.start()
    }

    fun clearModel() {
        NativeBridge.nativeClearModel()
        modelLoaded = false
    }

    private fun copyAsset(name: String): File {
        val dest = File(appContext.filesDir, name)
        if (!dest.exists()) {
            appContext.assets.open("models/$name").use { input ->
                dest.outputStream().use { out -> input.copyTo(out) }
            }
        }
        return dest
    }

    // ---- Push helpers ------------------------------------------------------
    fun pushAll() {
        applyBypass(bypass)
        NativeBridge.nativeSetInputGainDb(inputGainDb.toDouble())
        NativeBridge.nativeSetOutputGainDb(outputGainDb.toDouble())
        pushTone(); pushGate(); pushDelay(); pushReverb(); pushTremolo()
    }

    fun applyBypass(on: Boolean) { bypass = on; NativeBridge.nativeSetBypass(on) }
    fun setInputGain(db: Float) { inputGainDb = db; NativeBridge.nativeSetInputGainDb(db.toDouble()) }
    fun setOutputGain(db: Float) { outputGainDb = db; NativeBridge.nativeSetOutputGainDb(db.toDouble()) }

    fun pushTone() = NativeBridge.nativeSetTone(bass.toDouble(), mid.toDouble(), treble.toDouble())
    fun applyToneEnabled(on: Boolean) { toneEnabled = on; NativeBridge.nativeSetToneEnabled(on) }

    fun pushGate() = NativeBridge.nativeSetGate(gateEnabled, gateThresholdDb.toDouble())

    fun pushDelay() = NativeBridge.nativeSetDelay(
        delayEnabled, delayTimeMs.toDouble(), delayFeedback.toDouble(), delayMix.toDouble(),
        delayMode, 0.5, 0.0, false
    )

    fun pushReverb() = NativeBridge.nativeSetReverb(
        reverbEnabled, reverbMix.toDouble(), reverbDecay.toDouble(), reverbTone.toDouble(),
        20.0, 0.5, reverbMode, 0
    )

    fun pushTremolo() = NativeBridge.nativeSetTremolo(
        tremEnabled, tremRate.toDouble(), tremDepth.toDouble(), 0.0, 1.0, 800.0, tremMode
    )

    fun setTuner(on: Boolean) { tunerEnabled = on; NativeBridge.nativeSetTunerEnabled(on) }

    // ---- Status poll (called ~5 Hz from the UI) ----------------------------
    fun poll() {
        val s = NativeBridge.nativeStatus()
        running = s.startsWith("running")
        if (running) {
            peak = Regex("peak=([0-9.]+)").find(s)?.groupValues?.get(1)?.toFloatOrNull() ?: 0f
            val lat = Regex("latency=([0-9.]+)ms").find(s)?.groupValues?.get(1)
            val xr = Regex("xruns=([0-9]+)").find(s)?.groupValues?.get(1)
            latencyMs = if (lat != null) "$lat ms" + (if (xr != null) " · $xr xr" else "") else "--"
            if (!busy) status = if (modelLoaded) "Live" else "Live · no amp (dry)"
            if (tunerEnabled) tunerHz = NativeBridge.nativeTunerHz().toFloat()
        } else {
            peak = 0f
            latencyMs = "--"
            tunerHz = 0f
        }
    }
}
