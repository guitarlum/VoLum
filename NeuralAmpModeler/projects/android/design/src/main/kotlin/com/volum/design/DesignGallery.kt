package com.volum.design

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.volum.core.Section
import com.volum.core.View
import com.volum.core.VoLumState
import com.volum.core.VoLumStore
import com.volum.design.components.VoLumLogo
import com.volum.design.components.VoLumSubModePill
import com.volum.design.components.volumCanvas
import com.volum.design.components.windowFrame
import com.volum.design.gallery.ArtsPage
import com.volum.design.gallery.ControlsPage
import com.volum.design.gallery.DialogsPage
import com.volum.design.gallery.MotifsPage
import com.volum.design.gallery.StagePage
import com.volum.design.gallery.TokensPage
import com.volum.design.theme.VText
import com.volum.design.theme.VoLumTheme
import com.volum.design.theme.VoLumType

/** The gallery's pages, in tab order. */
enum class GalleryPage(val title: String) { STAGE("Stage"), TOKENS("Tokens"), CONTROLS("Controls"), DIALOGS("Dialogs"), ARTS("Amp art"), MOTIFS("Motifs") }

/**
 * Which page a fixture opens on. The gallery has no state of its own in
 * `:core`, so the `gallery_*` fixtures pick a page through the UI state they
 * already carry: a pending confirm is the dialogs page; otherwise (view,
 * section). `default` (PLAY / AMP) and `pre_nam1` (BUILD / PRE) both land on
 * the stage page, which the three design motion scripts record.
 */
fun galleryPageOf(s: VoLumState): GalleryPage = when {
    s.ui.confirm != null -> GalleryPage.DIALOGS
    s.ui.view == View.PLAY && s.ui.section == Section.PRE -> GalleryPage.TOKENS
    s.ui.view == View.PLAY && s.ui.section == Section.POST -> GalleryPage.CONTROLS
    s.ui.view == View.BUILD && s.ui.section == Section.AMP -> GalleryPage.ARTS
    s.ui.view == View.BUILD && s.ui.section == Section.POST -> GalleryPage.MOTIFS
    else -> GalleryPage.STAGE
}

/**
 * Every VoLum token, component state and art piece `:design` ships, on the
 * desktop canvas. The `design` flavor renders it as the `design-gallery`
 * screen; the tab pill switches pages by hand.
 */
@Composable
fun DesignGallery(store: VoLumStore, modifier: Modifier = Modifier) {
    VoLumTheme {
        val state by store.state.collectAsState()
        var page by remember(galleryPageOf(store.current)) { mutableStateOf(galleryPageOf(store.current)) }
        Column(modifier.fillMaxSize().volumCanvas().windowFrame().padding(horizontal = 14.dp, vertical = 6.dp)) {
            Row(Modifier.fillMaxWidth().height(48.dp), verticalAlignment = Alignment.CenterVertically) {
                VoLumLogo(Modifier.width(120.dp), compact = true)
                VoLumSubModePill(GalleryPage.entries.map { it.title }, page.ordinal, { page = GalleryPage.entries[it] }, Modifier.weight(1f).padding(horizontal = 24.dp))
                VText("design-gallery", style = VoLumType.label(9.sp, com.volum.design.theme.VoLumColors.CreamDim))
            }
            Spacer(Modifier.height(4.dp))
            Box(Modifier.fillMaxWidth().weight(1f)) {
                when (page) {
                    GalleryPage.STAGE -> StagePage(store, state)
                    GalleryPage.TOKENS -> TokensPage()
                    GalleryPage.CONTROLS -> ControlsPage(store)
                    GalleryPage.DIALOGS -> DialogsPage(store, state)
                    GalleryPage.ARTS -> ArtsPage()
                    GalleryPage.MOTIFS -> MotifsPage()
                }
            }
        }
    }
}

internal val GalleryGap = Arrangement.spacedBy(10.dp)
