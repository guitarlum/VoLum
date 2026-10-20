package com.volum.core

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.runners.Parameterized

/** One test per UI feature id: its fixture reaches its screen, and its canonical action satisfies its check while a no-op does not. */
@RunWith(Parameterized::class)
class FeatureSpecTest(private val id: String) {
    companion object {
        @JvmStatic
        @Parameterized.Parameters(name = "{0}")
        fun ids(): List<Array<Any>> = FeatureRegistry.ui.map { arrayOf<Any>(it.id) }
    }

    private val spec = FeatureRegistry.byId.getValue(id)

    @Test
    fun fixturePlacesTheFeatureOnItsScreenAndTheCanonicalActionSatisfiesTheCheck() {
        val screen = assertNotNullAnd(spec.screen)
        val store = VoLumStore.forScreen(screen, spec.fixture)
        val before = store.current
        assertEquals("fixture '${spec.fixture}' does not land on ${screen.id}", screen, before.screen())
        assertTrue("precondition fails in fixture '${spec.fixture}'", spec.precondition(before))
        if (spec.isView) return
        assertFalse("the check passes without any change", spec.check(before, before))
        val canonical = assertNotNullAnd(spec.canonical)
        store.dispatch(canonical(before))
        val after = store.current
        assertTrue("canonical action ${canonical(before)} does not satisfy the check", spec.check(before, after))
        val p = spec.param
        if (spec.interaction == "drag" && p != null) {
            val target = FeatureRegistry.dragTarget(p, before.param(p))
            assertTrue("drag moved away from its target", (after.param(p) - before.param(p)) * (target - before.param(p)) > 0)
        }
        val opposite = spec.opposite ?: return
        val reversed = VoLumStore.forScreen(screen, spec.fixture)
        reversed.dispatch(opposite(before))
        assertTrue("the opposite action ${opposite(before)} must change the state, or rejecting it proves nothing", reversed.current != before)
        assertFalse("a control wired the wrong way round (${opposite(before)}) passes the check", spec.check(before, reversed.current))
    }

    private fun <T : Any> assertNotNullAnd(v: T?): T {
        assertNotNull(v)
        return v!!
    }
}
