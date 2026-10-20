package com.volum.core

import kotlin.math.PI
import kotlin.math.floor
import kotlin.math.sin

/** What the audio engine reports every frame. Deterministic in [timeMs]. */
data class Telemetry(
    val timeMs: Long = 0,
    /** Input peak in dBFS (-inf as [Double.NEGATIVE_INFINITY]). */
    val inDb: Double = Double.NEGATIVE_INFINITY,
    val outDb: Double = Double.NEGATIVE_INFINITY,
    val outRightDb: Double = Double.NEGATIVE_INFINITY,
    /** 0..1 playing intensity that drives the PLAY art (desktop `VoLumArtAnimator` energy). */
    val energy: Double = 0.0,
    /** Milliseconds since the last pick transient, or -1. */
    val pickAgeMs: Long = -1,
    val tunerNote: String = "E",
    val tunerOctave: Int = 2,
    val tunerHz: Double = 82.41,
    val tunerCents: Double = 0.0,
    /** Current metronome beat (0-based) or -1 when the click is off. */
    val beat: Int = -1,
    val beatPhase: Double = 0.0,
    val muted: Boolean = false,
) {
    /** Meter position 0..1 for a -60..0 dBFS ladder. */
    fun meter(db: Double): Double = if (db.isInfinite()) 0.0 else ((db + 60.0) / 60.0).coerceIn(0.0, 1.0)
}

/**
 * Deterministic stand-in for the native engine: meters, tuner, metronome clock,
 * MIDI controllers, audio devices, and the answers to file picks and Pack opens.
 */
object FakeEngine {
    val builtinMic = AudioDevice("builtin-mic", "Phone microphone", usb = false, channels = 1)
    val builtinSpeaker = AudioDevice("builtin-speaker", "Phone speaker", usb = false, channels = 2)
    val usbInterface = AudioDevice("usb-2i2", "Scarlett 2i2 USB", usb = true, channels = 2)
    val usbController = MidiDevice("usb-mc6", "Morningstar MC6", ble = false)
    val bleController = MidiDevice("ble-fs1", "FS-1 BLE", ble = true)

    private val strings = listOf("E" to 82.41, "A" to 110.0, "D" to 146.83, "G" to 196.0, "B" to 246.94, "E" to 329.63)

    fun telemetry(s: VoLumState, timeMs: Long, energyOverride: Double? = null): Telemetry {
        val live = s.audio.permission == Permission.GRANTED && s.audio.status == InterfaceStatus.OK
        val t = timeMs / 1000.0
        val energy = when {
            !live -> 0.0
            energyOverride != null -> energyOverride.coerceIn(0.0, 1.0)
            else -> (0.45 + 0.35 * sin(2 * PI * t / 4.0) + 0.1 * sin(2 * PI * t / 0.9)).coerceIn(0.0, 1.0)
        }
        val inDb = if (energy <= 0.0) Double.NEGATIVE_INFINITY else -48.0 + 42.0 * energy
        val muted = s.tunerOpen || s.param(Param.OUTPUT) <= Param.OUTPUT.min
        val outDb = if (muted || inDb.isInfinite()) Double.NEGATIVE_INFINITY else (inDb + 3.0 + s.param(Param.OUTPUT) * 0.5).coerceAtMost(0.0)
        val pickPeriod = 500L
        val pickAge = if (energy > 0.2) timeMs % pickPeriod else -1L
        val stringIdx = ((timeMs / 3000) % strings.size).toInt()
        val (note, hz) = strings[stringIdx]
        val cents = 18.0 * sin(2 * PI * (timeMs % 3000) / 3000.0)
        val beatLen = 60000.0 / s.metronome.bpm
        val beats = timeMs / beatLen
        return Telemetry(
            timeMs = timeMs,
            inDb = inDb,
            outDb = outDb,
            outRightDb = if (outDb.isInfinite()) outDb else outDb - 0.6,
            energy = energy,
            pickAgeMs = pickAge,
            tunerNote = note,
            tunerOctave = if (stringIdx < 2) 2 else if (stringIdx < 5) 3 else 4,
            tunerHz = hz * Math.pow(2.0, cents / 1200.0),
            tunerCents = cents,
            beat = if (s.metronome.on) (floor(beats).toLong() % s.metronome.sig.beats).toInt() else -1,
            beatPhase = beats - floor(beats),
            muted = muted,
        )
    }

    fun answerPick(s: VoLumState, kind: PickKind): List<PickedFile> = when (kind) {
        PickKind.NAM_AMP_FILES -> if (s.host.nextPickInvalid) listOf(PickedFile("notes.txt", 12, valid = false))
        else listOf(PickedFile("DI-MyAmp-1.nam", 310_000), PickedFile("V30-MyAmp-1.nam", 310_000), PickedFile("G12-MyAmp-2.nam", 310_000))
        PickKind.IR_FILES -> if (s.host.nextPickInvalid) listOf(PickedFile("renamed-junk.wav", 900, valid = false))
        else listOf(PickedFile("Mesa 4x12 _ SM57.wav", 96_000), PickedFile("Mesa 4x12 _ R121.wav", 96_000))
        PickKind.PEDAL_FILES -> if (s.host.nextPickInvalid) listOf(PickedFile("pedal.zip", 1200, valid = false))
        else listOf(PickedFile("FX-Boss-SD1-1.nam", 120_000))
        PickKind.PACK_OPEN -> listOf(PickedFile("seed.volumpack", 48_000, valid = !s.host.packDamaged))
        PickKind.PACK_SAVE -> listOf(PickedFile("VoLum-backup.volumpack", 0))
    }

    fun packPreview(s: VoLumState, file: PickedFile): PackImportState {
        if (!file.valid) return PackImportState(name = file.name, damaged = true)
        val rows = if (s.host.packConflict) listOf(
            PackRow("c:1", "Monomyth Skeleton Key", "amp", PackOutcome.REPLACE),
            PackRow("ir:royer", "Marshall 4x12 _ Royer", "ir", PackOutcome.KEEP_MINE),
            PackRow("pedal:klon5000", "5000\$ Klon", "pedal", PackOutcome.ADD),
            PackRow("u:1", "Sunset Crunch", "preset", PackOutcome.RELOADS),
            PackRow("ir:missing", "Missing Cab", "ir", PackOutcome.SKIP),
        ) else listOf(
            PackRow("ir:pack-greenback", "Greenback 2x12", "ir", PackOutcome.ADD),
            PackRow("pedal:pack-fuzz", "Tone Bender Fuzz", "pedal", PackOutcome.ADD),
            PackRow("u:pack-1", "Pack Lead", "preset", PackOutcome.ADD),
        )
        return PackImportState(name = file.name, everything = true, rows = rows, refused = s.host.packOverCapacity)
    }

    /** Desktop custom-amp file naming: CAB-NAME-CHANNEL.nam, AMP / DI / DIRECT mean no cab. */
    fun parseAmpFile(name: String): BuilderFile {
        val stem = name.substringBeforeLast('.')
        val parts = stem.split('-')
        val cab = when (parts.firstOrNull()?.uppercase()) {
            "AMP", "DI", "DIRECT" -> 0
            "G12" -> 1
            "G65" -> 2
            "V30" -> 3
            else -> 1
        }
        val channel = parts.lastOrNull()?.toIntOrNull()?.coerceIn(1, 6) ?: 1
        return BuilderFile(name, cab, channel)
    }

    fun shortenMiddle(name: String, max: Int = 24): String =
        if (name.length <= max) name else name.take(max / 2 - 1) + "…" + name.takeLast(max / 2)
}
