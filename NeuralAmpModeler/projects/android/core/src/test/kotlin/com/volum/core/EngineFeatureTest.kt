package com.volum.core

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.runners.Parameterized

/** One test per `engine` feature id: behaviour of the fake engine and reducer that has no UI node of its own. */
@RunWith(Parameterized::class)
class EngineFeatureTest(private val id: String) {
    companion object {
        private fun store(fixture: String = "default") = VoLumStore(Fixtures.build(fixture))
        private const val THC = "amp:14"

        private val behaviours: Map<String, () -> Unit> = mapOf(
            "play.starting_sounds" to {
                val names = Catalog.startingSounds.toSortedMap().values.map { s -> Catalog.factoryPresets.first { it.id == s.presetId }.name }
                assertEquals(listOf("The bestest Clean", "SLO Crunch", "Modern Rhythm", "Crack the Skye", "Ampete Lead"), names)
                assertEquals(Catalog.startingSounds, Fixtures.build("first_launch").library.sounds)
            },
            "play.view_memory" to {
                val s = store()
                s.dispatch(Action.SetView(View.BUILD))
                s.dispatch(Action.Open(Overlay.Tuner))
                s.dispatch(Action.Back)
                assertEquals(View.BUILD, s.current.ui.view)
            },
            "amp.rig_memory" to {
                val s = store()
                s.dispatch(Action.SetParam(Param.BASS, 9.0))
                s.dispatch(Action.SelectCab(Cab.G65))
                s.dispatch(Action.SelectAmp(Catalog.ampId(0)))
                assertEquals(Param.BASS.default, s.current.param(Param.BASS), 0.0)
                s.dispatch(Action.SelectAmp(THC))
                assertEquals(9.0, s.current.param(Param.BASS), 0.0)
                assertEquals(Cab.G65, s.current.rig.cab)
            },
            "amp.lite_note" to {
                val s = store()
                s.dispatch(Action.SetLite(true))
                assertTrue(s.current.signal.lite)
            },
            "pre.lock.play_unlocks" to {
                val s = store()
                s.dispatch(Action.SetView(View.BUILD))
                s.dispatch(Action.SetLock(Section.PRE, true))
                s.dispatch(Action.ToggleParam(Param.PITCH_ON))
                s.dispatch(Action.SelectAmp(Catalog.ampId(13)))
                assertTrue("locked pedals follow the amp change", s.current.on(Param.PITCH_ON))
                s.dispatch(Action.SetView(View.PLAY))
                assertNull(s.current.locks.pre)
                assertTrue("switching to PLAY stores the locked section to the current amp", s.current.rig.params[Param.PITCH_ON] == 1.0)
            },
            "post.mode_memory" to {
                val s = store("post_delay")
                s.dispatch(Action.SetParam(Param.DELAY_TIME, 900.0))
                s.dispatch(Action.SetParam(Param.DELAY_MODE, 1.0))
                assertEquals(Param.DELAY_TIME.default, s.current.param(Param.DELAY_TIME), 0.0)
                s.dispatch(Action.SetParam(Param.DELAY_TIME, 500.0))
                s.dispatch(Action.SetParam(Param.DELAY_MODE, 0.0))
                assertEquals(900.0, s.current.param(Param.DELAY_TIME), 0.0)
                s.dispatch(Action.SetParam(Param.DELAY_MODE, 1.0))
                assertEquals(500.0, s.current.param(Param.DELAY_TIME), 0.0)
            },
            "post.tempo" to {
                val on = Fixtures.build("metronome_on")
                assertEquals(1, FakeEngine.telemetry(on, 700).beat)
                val faster = on.copy(metronome = on.metronome.copy(bpm = 192.0))
                assertEquals(2, FakeEngine.telemetry(faster, 700).beat)
                assertEquals(-1, FakeEngine.telemetry(Fixtures.build("default"), 700).beat)
            },
            "presets.factory_catalog" to {
                assertEquals(21, Catalog.factoryPresets.size)
                assertTrue(Catalog.factoryPresets.all { it.factory })
                assertEquals(listOf("SLO Lead", "SLO Crunch"), Catalog.factoryPresets.filter { it.ampId == Catalog.ampId(13) }.map { it.name })
            },
            "presets.save.dedupe" to {
                val s = store()
                s.dispatch(Action.LoadPreset(Catalog.factoryId(13, 0)))
                s.dispatch(Action.Open(Overlay.NameDialog(NamePurpose.SAVE_PRESET, "slo rhythm")))
                s.dispatch(Action.CommitName)
                assertEquals("slo rhythm 2", s.current.presetName)
                s.dispatch(Action.LoadPreset(Catalog.factoryId(13, 0)))
                s.dispatch(Action.Open(Overlay.NameDialog(NamePurpose.SAVE_PRESET, "SLO Lead")))
                s.dispatch(Action.CommitName)
                assertEquals("a User preset may share a Factory name", "SLO Lead", s.current.presetName)
            },
            "midi.program_change" to {
                val s = store()
                s.dispatch(Action.Midi(MidiEvent.ProgramChange(1, 1)))
                assertEquals(1, s.current.library.live)
                assertEquals(Catalog.ampId(13), s.current.ampId)
            },
            "midi.recall_cc_message" to {
                val s = store()
                s.dispatch(Action.Midi(MidiEvent.ControlChange(1, 102, 2)))
                assertEquals(2, s.current.library.live)
                s.dispatch(Action.Midi(MidiEvent.ControlChange(1, 7, 3)))
                assertEquals(2, s.current.library.live)
            },
            "midi.empty_program" to {
                val s = store()
                val before = s.current
                s.dispatch(Action.Midi(MidiEvent.ProgramChange(1, 50)))
                assertEquals(before, s.current)
            },
            "midi.channel_filter" to {
                val s = store("midi_one_channel")
                s.dispatch(Action.Midi(MidiEvent.ProgramChange(1, 2)))
                assertEquals(5, s.current.library.live)
                s.dispatch(Action.Midi(MidiEvent.ProgramChange(3, 2)))
                assertEquals(2, s.current.library.live)
            },
            "builder.autofill" to {
                assertEquals(BuilderFile("V30-MyAmp-2.nam", 3, 2), FakeEngine.parseAmpFile("V30-MyAmp-2.nam"))
                assertEquals(0, FakeEngine.parseAmpFile("DIRECT-MyAmp-1.nam").cabSlot)
                assertEquals(0, FakeEngine.parseAmpFile("DI-x-4.nam").cabSlot)
                assertEquals(4, FakeEngine.parseAmpFile("AMP-x-4.nam").channel)
            },
            "ir.naming" to {
                val s = store()
                s.dispatch(Action.FilesPicked(PickKind.IR_FILES, listOf(PickedFile("Cab_with_a_very_long_name_from_the_vendor_1.wav", 10))))
                s.dispatch(Action.FilesPicked(PickKind.IR_FILES, listOf(PickedFile("Cab_with_a_very_long_name_from_the_vendor_2.wav", 10))))
                val names = s.current.library.irs.map { it.name }
                assertTrue(names.toString(), names.any { it.endsWith("vendor_1") && it.contains("…") })
                assertTrue(names.toString(), names.any { it.endsWith("vendor_2") })
                val count = s.current.library.irs.size
                s.dispatch(Action.FilesPicked(PickKind.IR_FILES, listOf(PickedFile("Cab_with_a_very_long_name_from_the_vendor_2.wav", 10))))
                assertEquals("a file already in the library is skipped", count, s.current.library.irs.size)
                s.dispatch(Action.FilesPicked(PickKind.IR_FILES, listOf(PickedFile("Marshall 4x12 _ Royer.wav", 99))))
                assertTrue(s.current.library.irs.any { it.name == "Marshall 4x12 _ Royer 2" })
            },
            "content.delete_fallback" to {
                val s = store()
                s.dispatch(Action.SelectIr("ir:royer"))
                s.dispatch(Action.RequestDeleteIr("ir:royer"))
                s.dispatch(Action.AcceptConfirm)
                assertEquals(Cab.G12, s.current.rig.cab)
                s.dispatch(Action.RequestDeletePedal(64))
                s.dispatch(Action.AcceptConfirm)
                s.dispatch(Action.SelectAmp("c:1"))
                s.dispatch(Action.RequestDeleteAmp("c:1"))
                s.dispatch(Action.AcceptConfirm)
                assertEquals(Catalog.ampId(0), s.current.ampId)
                assertTrue(s.current.library.amps.none { it.custom })

                val support = store("dual_support_ir")
                val mainCab = support.current.rig.cab
                support.dispatch(Action.RequestDeleteIr("ir:royer"))
                support.dispatch(Action.AcceptConfirm)
                assertEquals("a deleted SUPPORT IR returns SUPPORT to cab 1 of its bundled amp", Cab.G12, support.current.rig.support.cab)
                assertNull(support.current.rig.support.irId)
                assertEquals(mainCab, support.current.rig.cab)

                val custom = store("dual_amp_empty_support")
                custom.dispatch(Action.SelectSupportAmp("c:1"))
                custom.dispatch(Action.FocusLane(Lane.SUPPORT))
                custom.dispatch(Action.SelectSupportIr("ir:royer"))
                custom.dispatch(Action.RequestDeleteIr("ir:royer"))
                custom.dispatch(Action.AcceptConfirm)
                assertEquals("a custom SUPPORT amp keeps its channel on the real cab that covers it", Cab.V30, custom.current.rig.support.cab)
                assertEquals(0, custom.current.rig.support.channel)
                custom.dispatch(Action.RequestDeleteAmp("c:1"))
                custom.dispatch(Action.AcceptConfirm)
                assertNull(custom.current.rig.support.ampId)
                assertTrue("deleting the custom SUPPORT amp switches Dual Amp off", !custom.current.dual)
                assertEquals(Lane.MAIN, custom.current.ui.lane)

                val main = store("custom_levels")
                main.dispatch(Action.SelectIr("ir:royer"))
                main.dispatch(Action.RequestDeleteIr("ir:royer"))
                main.dispatch(Action.AcceptConfirm)
                assertEquals("a custom MAIN amp keeps its channel on the real cab that covers it", Cab.V30, main.current.rig.cab)
            },
            "pack.export.also.support" to {
                val s = Fixtures.build("export_sounds_support")
                val also = s.packExport.alsoIncluding(s.library)
                assertTrue(also.toString(), "Marshall 4x12 _ Royer" in also)
                assertTrue(also.toString(), "Monomyth Skeleton Key" in also)
                val dualOff = s.copy(library = s.library.copy(presets = s.library.presets.map {
                    if (it.id == "u:4") it.copy(rig = it.rig.copy(params = it.rig.params + (Param.DUAL_ON to 0.0))) else it
                }))
                assertEquals("a switched-off SUPPORT lane needs nothing", emptyList<String>(), dualOff.packExport.alsoIncluding(dualOff.library))
            },
            "pack.packbak" to {
                val s = store("pack_preview")
                s.dispatch(Action.ConfirmImport)
                assertEquals(1, s.current.library.packBackups)
                val damaged = store("pack_damaged")
                val before = damaged.current.library
                damaged.dispatch(Action.ConfirmImport)
                assertEquals("a damaged Pack changes nothing", before, damaged.current.library)
                val refused = store("pack_refused")
                refused.dispatch(Action.ConfirmImport)
                assertNotEquals(true, refused.current.packImport?.imported)
            },
        )

        @JvmStatic
        @Parameterized.Parameters(name = "{0}")
        fun ids(): List<Array<Any>> = FeatureRegistry.all.filter { it.isEngine }.map { arrayOf<Any>(it.id) }

        fun covered(): Set<String> = behaviours.keys
    }

    @Test
    fun behaves() {
        val test = behaviours[id] ?: throw AssertionError("engine feature $id has no behaviour test")
        test()
    }
}

class EngineFeatureCoverageTest {
    @Test
    fun everyBehaviourTestNamesAnEngineFeature() {
        val engineIds = FeatureRegistry.all.filter { it.isEngine }.map { it.id }.toSet()
        assertEquals(emptySet<String>(), EngineFeatureTest.covered() - engineIds)
    }
}
