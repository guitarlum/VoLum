package com.volum.harness

import androidx.compose.ui.Modifier
import androidx.compose.ui.semantics.SemanticsPropertyKey
import androidx.compose.ui.semantics.semantics
import com.volum.core.FeatureRegistry
import com.volum.core.ScreenId

/** Feature ids a node implements. Merges upward so an id on a child of a clickable lands on the clickable. */
val FeatureIdsKey = SemanticsPropertyKey<List<String>>(
    name = "VoLumFeatureIds",
    mergePolicy = { parent, child -> (parent.orEmpty() + child).distinct() },
)

/** Screen ids a surface shows. A surface that merges two logical screens lists both. */
val ScreenIdsKey = SemanticsPropertyKey<List<String>>(
    name = "VoLumScreenIds",
    mergePolicy = { parent, child -> (parent.orEmpty() + child).distinct() },
)

/**
 * Marks the node that implements [ids] (rows of `.scratch/android-shootout/features.md`).
 * The harness coverage test finds features only through this key. Unknown ids
 * throw, so a typo fails the build's instrumented run instead of silently
 * dropping coverage.
 */
fun Modifier.featureId(vararg ids: String): Modifier {
    ids.forEach { require(FeatureRegistry.isKnown(it)) { "unknown feature id '$it'" } }
    return semantics { this[FeatureIdsKey] = ids.toList() }
}

/** Marks the root of a surface that shows [screens]. */
fun Modifier.screen(vararg screens: ScreenId): Modifier =
    semantics { this[ScreenIdsKey] = screens.map { it.id } }
