package com.volum.core

import java.io.File

/**
 * Writes the lists the box scripts iterate:
 * `captures-<scope>.tsv` (screen, fixture), `motion-<scope>.tsv` (id, duration ms),
 * `screens.tsv` (screen, scope) and `features.tsv` (id, screen, interaction).
 */
fun main(args: Array<String>) {
    val out = File(args.firstOrNull() ?: "build/registry").apply { mkdirs() }
    for (scope in Scope.entries) {
        val tag = scope.name.lowercase()
        File(out, "captures-$tag.tsv").writeText(
            ScreenRegistry.inScope(scope).flatMap { spec -> spec.captures.map { "${spec.screen.id}\t$it" } }.joinToString("\n", postfix = "\n"),
        )
        File(out, "motion-$tag.tsv").writeText(
            MotionRegistry.inScope(scope).joinToString("\n", postfix = "\n") { "${it.id}\t${it.durationMs}" },
        )
    }
    File(out, "screens.tsv").writeText(ScreenRegistry.specs.joinToString("\n", postfix = "\n") { "${it.screen.id}\t${it.scope.name.lowercase()}" })
    File(out, "features.tsv").writeText(FeatureRegistry.all.joinToString("\n", postfix = "\n") { "${it.id}\t${it.screen?.id ?: "-"}\t${it.interaction}" })
}
