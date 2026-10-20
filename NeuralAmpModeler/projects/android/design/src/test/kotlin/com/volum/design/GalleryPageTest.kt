package com.volum.design

import com.volum.core.Fixtures
import com.volum.core.MotionRegistry
import com.volum.core.Scope
import com.volum.core.ScreenId
import com.volum.core.ScreenRegistry
import org.junit.Assert.assertEquals
import org.junit.Test

class GalleryPageTest {
    @Test
    fun everyGalleryFixtureOpensItsPageAndTheMotionFixturesOpenTheStage() {
        val expected = mapOf(
            "default" to GalleryPage.STAGE,
            "pre_nam1" to GalleryPage.STAGE,
            "gallery_tokens" to GalleryPage.TOKENS,
            "gallery_controls" to GalleryPage.CONTROLS,
            "gallery_dialogs" to GalleryPage.DIALOGS,
            "gallery_arts" to GalleryPage.ARTS,
            "gallery_motifs" to GalleryPage.MOTIFS,
        )
        assertEquals(expected, expected.mapValues { (f, _) -> galleryPageOf(Fixtures.build(f)) })
        val captured = ScreenRegistry.spec(ScreenId.DESIGN_GALLERY).captures
        assertEquals(expected.keys, captured.toSet())
        assertEquals(GalleryPage.entries.toSet(), captured.map { galleryPageOf(Fixtures.build(it)) }.toSet())
        MotionRegistry.inScope(Scope.DESIGN).forEach { assertEquals(it.id, GalleryPage.STAGE, galleryPageOf(Fixtures.build(it.fixture))) }
    }
}
