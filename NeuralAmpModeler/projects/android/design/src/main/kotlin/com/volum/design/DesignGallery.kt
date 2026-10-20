package com.volum.design

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.text.BasicText
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import com.volum.core.VoLumStore

/** Placeholder until ticket 02 ports the VoLum identity; the `design` flavor renders this as the `design-gallery` screen. */
@Composable
fun DesignGallery(store: VoLumStore, modifier: Modifier = Modifier) {
    Box(modifier.fillMaxSize().background(Color.Black), contentAlignment = Alignment.Center) {
        BasicText("design-gallery placeholder (ticket 02)", style = TextStyle(color = Color.Gray))
    }
}
