package com.volum.ui.a

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.text.BasicText
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import com.volum.core.VoLumStore
import com.volum.harness.CandidateUi
import com.volum.harness.screen

/** Placeholder until ticket 03: renders the store's current screen id and nothing operable. */
object CandidateA : CandidateUi {
    override val name = "A Signal Flow"

    @Composable
    override fun App(store: VoLumStore) {
        val state by store.state.collectAsState()
        val screen = state.screen()
        Box(Modifier.fillMaxSize().background(Color.Black).screen(screen), contentAlignment = Alignment.Center) {
            BasicText("Candidate A placeholder: ${screen.id}", style = TextStyle(color = Color.Gray))
        }
    }
}
