package com.lum.volum

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.io.File

/**
 * On-device / emulator smoke: loads a bundled .nam through the shared C++ core
 * and processes audio blocks. Fails if the model can't load or produces
 * non-finite output. Logs the real-time factor so we get a first perf number
 * on the S20's arm64 CPU.
 *
 *   ./gradlew connectedDebugAndroidTest
 */
@RunWith(AndroidJUnit4::class)
class NamSmokeTest {

    @Test
    fun loadsAndProcessesBundledModel() {
        val ctx = InstrumentationRegistry.getInstrumentation().targetContext
        val names = ctx.assets.list("models")?.filter { it.endsWith(".nam") }
        assertNotNull("No bundled models under assets/models", names)
        val first = names!!.firstOrNull()
        assertNotNull("No .nam asset found", first)

        val dest = File(ctx.filesDir, first!!)
        ctx.assets.open("models/$first").use { input ->
            dest.outputStream().use { out -> input.copyTo(out) }
        }

        val result = NativeBridge.nativeNamSmoke(dest.absolutePath, 48000.0, 64, 4000)
        android.util.Log.i("VoLumSmokeTest", result)
        assertFalse("Native smoke reported failure: $result", result.startsWith("FAIL"))
    }

    /**
     * Deterministic full signal-chain check: bypass identity, unity passthrough,
     * output-gain law, gate/model finiteness, Delay/Reverb decay tails, and
     * Tremolo envelope modulation - all through the real desktop DSP classes.
     */
    @Test
    fun engineSignalChainSelfTest() {
        val ctx = InstrumentationRegistry.getInstrumentation().targetContext
        val first = ctx.assets.list("models")?.firstOrNull { it.endsWith(".nam") }
        val modelPath = if (first != null) {
            val dest = File(ctx.filesDir, first)
            ctx.assets.open("models/$first").use { input ->
                dest.outputStream().use { out -> input.copyTo(out) }
            }
            dest.absolutePath
        } else ""

        val result = NativeBridge.nativeEngineSelfTest(modelPath)
        android.util.Log.i("VoLumSelfTest", result)
        assertTrue("Engine self-test failed: $result", result.startsWith("PASS"))
    }
}
