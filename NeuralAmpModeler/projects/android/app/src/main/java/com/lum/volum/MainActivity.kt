package com.lum.volum

import android.Manifest
import android.content.pm.PackageManager
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.remember
import androidx.compose.ui.platform.LocalContext
import androidx.core.content.ContextCompat
import com.lum.volum.ui.VoLumScreen
import com.lum.volum.ui.theme.VoLumTheme
import kotlinx.coroutines.delay

/**
 * VoLum Android host. A single Compose screen drives the shared C++ engine
 * (NAM + tone + noise gate + delay/reverb/tremolo) through an Oboe low-latency
 * duplex stream, intended for real-time monitoring via a USB audio interface.
 */
class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            val ctx = LocalContext.current.applicationContext
            val state = remember { VoLumState(ctx) }

            val permLauncher = rememberLauncherForActivityResult(
                ActivityResultContracts.RequestPermission()
            ) { state.refreshDevices() }

            LaunchedEffect(Unit) {
                if (ContextCompat.checkSelfPermission(ctx, Manifest.permission.RECORD_AUDIO)
                    != PackageManager.PERMISSION_GRANTED
                ) {
                    permLauncher.launch(Manifest.permission.RECORD_AUDIO)
                }
            }

            // Poll native status ~5 Hz to drive the meter / latency readout.
            LaunchedEffect(Unit) {
                while (true) {
                    state.poll()
                    delay(200)
                }
            }

            VoLumTheme { VoLumScreen(state) }
        }
    }
}
