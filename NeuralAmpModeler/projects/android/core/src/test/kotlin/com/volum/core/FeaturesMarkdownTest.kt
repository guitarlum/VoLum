package com.volum.core

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

class FeaturesMarkdownTest {
    private data class Row(val id: String, val screen: String, val interaction: String, val naReason: String)

    private val markdown: List<String> by lazy { File(System.getProperty("volum.featuresMd")).readLines() }

    private fun cells(line: String): List<String> = line.trim().removePrefix("|").removeSuffix("|").split("|").map { it.trim() }

    private val rows: List<Row> by lazy {
        val checklistStart = markdown.indexOfFirst { it.startsWith("## Desktop parameters") }.let { if (it < 0) markdown.size else it }
        markdown.take(checklistStart)
            .filter { it.trim().startsWith("|") }
            .map { cells(it) }
            .filter { it.size == 6 && it[0] != "id" && !it[0].startsWith("---") && !it[0].startsWith("interaction") }
            .map { Row(it[0], it[3], it[4], it[5]) }
    }

    @Test
    fun everyRegistryFeatureHasExactlyOneRow() {
        val mdIds = rows.map { it.id }
        assertEquals("duplicate ids in features.md", mdIds.size, mdIds.toSet().size)
        val registryIds = FeatureRegistry.all.map { it.id }
        assertEquals("duplicate ids in FeatureRegistry", registryIds.size, registryIds.toSet().size)
        assertEquals("in features.md but not in FeatureRegistry", emptySet<String>(), mdIds.toSet() - registryIds.toSet())
        assertEquals("in FeatureRegistry but not in features.md", emptySet<String>(), registryIds.toSet() - mdIds.toSet())
    }

    @Test
    fun screensInteractionsAndNaReasonsMatch() {
        val mismatches = rows.mapNotNull { row ->
            val spec = FeatureRegistry.byId[row.id] ?: return@mapNotNull null
            val screen = spec.screen?.id ?: "-"
            when {
                screen != row.screen -> "${row.id}: screen md='${row.screen}' registry='$screen'"
                spec.interaction != row.interaction -> "${row.id}: interaction md='${row.interaction}' registry='${spec.interaction}'"
                spec.isNa != row.naReason.isNotEmpty() -> "${row.id}: N/A md='${row.naReason}' registry='${spec.naReason}'"
                else -> null
            }
        }
        assertEquals(emptyList<String>(), mismatches)
    }

    @Test
    fun everyNaRowGivesAReasonAndNoScreen() {
        val bad = rows.filter { it.naReason.isNotEmpty() && (it.screen != "-" || it.interaction != "-") } +
            rows.filter { it.interaction == "-" && it.naReason.isEmpty() }
        assertEquals(emptyList<Row>(), bad)
    }

    @Test
    fun everyUserGuideHeadingIsMappedToKnownFeatureIds() {
        val headings = File(System.getProperty("volum.userGuide")).readLines()
            .filter { it.startsWith("## ") || it.startsWith("### ") }
            .map { it.trim() }
        val start = markdown.indexOfFirst { it.startsWith("## User-guide heading checklist") }
        assertTrue("features.md has no heading checklist", start >= 0)
        val checklist = markdown.drop(start).filter { it.trim().startsWith("| #") }.map { cells(it) }
            .associate { it[0] to it[1] }
        val unmapped = headings.filter { checklist[it].isNullOrBlank() }
        assertEquals("user-guide headings missing from the checklist", emptyList<String>(), unmapped)
        val stale = checklist.keys.filter { it !in headings }
        assertEquals("checklist headings that are not in the user guide", emptyList<String>(), stale)
        val unknown = checklist.flatMap { (heading, ids) ->
            ids.replace(Regex("\\(.*?\\)"), "").split(",").map { it.trim() }.filter { it.isNotEmpty() }
                .filter { !FeatureRegistry.isKnown(it) }.map { "$heading -> $it" }
        }
        assertEquals("checklist ids that are not features", emptyList<String>(), unknown)
    }
}
