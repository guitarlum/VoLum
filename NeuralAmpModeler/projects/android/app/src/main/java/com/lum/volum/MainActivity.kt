package com.lum.volum

import android.Manifest
import android.content.pm.ActivityInfo
import android.content.pm.PackageManager
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.runtime.mutableStateOf
import androidx.core.content.ContextCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import com.lum.volum.ui.prototype.VoLumPrototype
import com.lum.volum.ui.theme.VoLumTheme

/**
 * Prototype-only launcher for the landscape signal-path UX.
 *
 * The existing native engine, USB discovery, and JNI bridge remain untouched,
 * while the UX prototype intentionally uses deterministic mock state.
 */
class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE
        setContent {
            var microphoneGranted by remember {
                mutableStateOf(
                    ContextCompat.checkSelfPermission(
                        this,
                        Manifest.permission.RECORD_AUDIO,
                    ) == PackageManager.PERMISSION_GRANTED
                )
            }
            var permissionResultHandler by remember {
                mutableStateOf<((Boolean) -> Unit)?>(null)
            }

            val permLauncher = rememberLauncherForActivityResult(
                ActivityResultContracts.RequestPermission()
            ) { granted ->
                microphoneGranted = granted
                permissionResultHandler?.invoke(granted)
                permissionResultHandler = null
            }

            VoLumTheme {
                VoLumPrototype(
                    microphoneGranted = microphoneGranted,
                    requestMicrophonePermission = { onResult ->
                        permissionResultHandler = onResult
                        permLauncher.launch(Manifest.permission.RECORD_AUDIO)
                    },
                    startEntry = intent.getStringExtra("entry"),
                    onImmersiveChange = ::setWorkspaceImmersive,
                )
            }
        }
    }

    private fun setWorkspaceImmersive(enabled: Boolean) {
        WindowCompat.setDecorFitsSystemWindows(window, !enabled)
        WindowCompat.getInsetsController(window, window.decorView).apply {
            systemBarsBehavior =
                WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
            if (enabled) hide(WindowInsetsCompat.Type.systemBars())
            else show(WindowInsetsCompat.Type.systemBars())
        }
    }
}
