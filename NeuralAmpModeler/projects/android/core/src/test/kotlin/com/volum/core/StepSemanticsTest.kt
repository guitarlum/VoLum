package com.volum.core

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

/** Prev / Next wrap and stop where the desktop does, and the registry checks accept only the specified direction at every end. */
class StepSemanticsTest {
    private fun spec(id: String) = FeatureRegistry.byId.getValue(id)

    private fun assertStep(id: String, before: VoLumState, action: Action, expect: (VoLumState) -> Unit) {
        val after = Reducer.reduce(before, action)
        expect(after)
        assertTrue("$id rejects $action from this state", spec(id).check(before, after))
    }

    private fun onPreset(index: Int): VoLumState {
        val s = Fixtures.build("default")
        val id = s.library.presetsFor(s.ampId)[index].id
        return s.copy(rigs = s.rigs + (s.ampId to s.rig.copy(presetId = id)))
    }

    @Test
    fun presetsWrapAtBothEnds() {
        val s = Fixtures.build("default")
        val list = s.library.presetsFor(s.ampId).map { it.id }
        assertTrue("the direction checks need at least three presets on the seed amp", list.size >= 3)
        assertStep("header.preset.prev", onPreset(0), Action.StepPreset(-1)) { assertEquals(list.last(), it.rig.presetId) }
        assertStep("header.preset.next", onPreset(list.lastIndex), Action.StepPreset(1)) { assertEquals(list.first(), it.rig.presetId) }
        assertStep("header.preset.next", onPreset(0), Action.StepPreset(1)) { assertEquals(list[1], it.rig.presetId) }
        assertStep("header.preset.prev", onPreset(1), Action.StepPreset(-1)) { assertEquals(list[0], it.rig.presetId) }
        val none = s.copy(rigs = s.rigs + (s.ampId to s.rig.copy(presetId = null)))
        assertStep("header.preset.next", none, Action.StepPreset(1)) { assertEquals(list.first(), it.rig.presetId) }
        assertStep("header.preset.prev", none, Action.StepPreset(-1)) { assertEquals(list.last(), it.rig.presetId) }
        assertFalse(spec("header.preset.next").check(onPreset(0), Reducer.reduce(onPreset(0), Action.StepPreset(-1))))
        assertFalse(spec("header.preset.prev").check(onPreset(0), Reducer.reduce(onPreset(0), Action.StepPreset(1))))
    }

    @Test
    fun soundsWrapAtBothEndsAndSkipInvalidSlots() {
        val s = Fixtures.build("invalid_slot")
        val valid = s.library.sounds.keys.filter { s.library.soundValid(it) }.sorted()
        fun live(p: Int?) = s.copy(library = s.library.copy(live = p))
        assertStep("play.sound.next", live(valid.last()), Action.StepSound(1)) { assertEquals(valid.first(), it.library.live) }
        assertStep("play.sound.prev", live(valid.first()), Action.StepSound(-1)) { assertEquals(valid.last(), it.library.live) }
        assertStep("play.sound.next", live(null), Action.StepSound(1)) { assertEquals(valid.first(), it.library.live) }
        assertStep("play.sound.prev", live(null), Action.StepSound(-1)) { assertEquals(valid.last(), it.library.live) }
        assertTrue("program 6 is invalid and never a step target", 6 !in valid && 6 in s.library.sounds)
        assertFalse(spec("play.sound.next").check(live(valid[1]), Reducer.reduce(live(valid[1]), Action.StepSound(-1))))
        assertFalse(spec("play.sound.prev").check(live(valid[1]), Reducer.reduce(live(valid[1]), Action.StepSound(1))))
    }

    @Test
    fun midiBanksStopAtTheEnds() {
        val s = Fixtures.build("default")
        fun bank(b: Int) = s.copy(midi = s.midi.copy(bank = b))
        assertEquals(15, Reducer.reduce(bank(15), Action.SetMidiBank(16)).midi.bank)
        assertEquals(0, Reducer.reduce(bank(0), Action.SetMidiBank(-1)).midi.bank)
        assertFalse("Next on the last bank does not wrap", spec("midi.bank.next").check(bank(15), Reducer.reduce(bank(15), Action.SetMidiBank(16))))
        assertFalse("Prev on the first bank does not wrap", spec("midi.bank.prev").check(bank(0), Reducer.reduce(bank(0), Action.SetMidiBank(-1))))
        assertStep("midi.bank.next", bank(14), Action.SetMidiBank(15)) { assertEquals(15, it.midi.bank) }
        assertStep("midi.bank.prev", bank(1), Action.SetMidiBank(0)) { assertEquals(0, it.midi.bank) }
    }
}
