package com.volum.engine

import androidx.test.core.app.ApplicationProvider
import androidx.test.ext.junit.runners.AndroidJUnit4
import java.io.File
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class EngineParityInstrumentedTest {
    @Test
    fun rendersFixedA2Rig() {
        val context = ApplicationProvider.getApplicationContext<android.content.Context>()
        val input = File(context.filesDir, "input.wav")
        val rig = File(context.filesDir, "ampete.nam")
        val output = File(context.filesDir, "android.wav")
        context.assets.open("_input.wav").use { source ->
            input.outputStream().use(source::copyTo)
        }
        context.assets.open("AMP-Ampt-1.nam").use { source ->
            rig.outputStream().use(source::copyTo)
        }

        val result = NativeEngine.render(input.path, rig.path, output.path)
        File(context.filesDir, "engine-result.json").writeText(result)

        assertTrue(output.isFile && output.length() > 44)
        assertTrue(result.contains("\"architecture\":\"SlimmableContainer\""))
        assertTrue(result.contains("\"slimmable\":true"))
    }
}
