package com.volum.app

import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import com.volum.core.ScreenId
import com.volum.core.VoLumStore
import com.volum.design.DesignGallery
import com.volum.harness.CandidateUi
import com.volum.harness.screen

val flavorCandidate: CandidateUi = object : CandidateUi {
    override val name = "design"

    @Composable
    override fun App(store: VoLumStore) {
        DesignGallery(store, Modifier.screen(ScreenId.DESIGN_GALLERY))
    }
}
