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
import com.lum.volum.ui.proposals.ProposalLab
import com.lum.volum.ui.proposals.ProposalOrientation
import com.lum.volum.ui.theme.VoLumTheme

/**
 * Prototype-only launcher for the Android UI Proposal Lab.
 *
 * The existing native engine, USB discovery, and JNI bridge remain untouched,
 * but every proposal intentionally uses deterministic mock state.
 */
class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
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
                ProposalLab(
                    microphoneGranted = microphoneGranted,
                    requestMicrophonePermission = { onResult ->
                        permissionResultHandler = onResult
                        permLauncher.launch(Manifest.permission.RECORD_AUDIO)
                    },
                    onOrientationChange = { orientation ->
                        requestedOrientation = when (orientation) {
                            ProposalOrientation.Portrait -> ActivityInfo.SCREEN_ORIENTATION_PORTRAIT
                            ProposalOrientation.Landscape -> ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE
                            null -> ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED
                        }
                    },
                    startProposal = intent.getStringExtra("proposal"),
                    startEntry = intent.getStringExtra("entry"),
                )
            }
        }
    }
}
