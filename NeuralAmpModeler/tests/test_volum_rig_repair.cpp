#include "third_party/doctest.h"

#include <string>

#include "../VoLumContentStore.h"
#include "../VoLumProcessingPlan.h"
#include "../VoLumDspStaging.h"
#include "../VoLumRigRepair.h"

using namespace volum::rig;

namespace
{
// Which payloads the audio thread actually has in RAM. Separate from the rig on
// purpose: the whole delete-while-playing bug was the two disagreeing.
struct LoadedPayloads
{
  bool mainModel = true;
  bool supportModel = true;
  bool preNam[2] = {true, true};
  bool ir = true;
  bool supportIr = true;
};

// Derive the real DSP graph for a rig through the function ProcessBlock uses.
// This is what makes these tests DSP tests and not JSON tests: a repair that only
// rewrites the catalog leaves runMainModel/runIR/runPreNam pointing at a payload
// that is gone.
volum::ProcessingPlan GraphFor(const SoundingRig& rig, const LoadedPayloads& have = {})
{
  const bool preActive[2] = {rig.preActive[0], rig.preActive[1]};
  const bool havePre[2] = {have.preNam[0] && rig.preCapture[0] != 0, have.preNam[1] && rig.preCapture[1] != 0};
  const bool haveSupportAmp = have.supportModel && (!rig.supportCustomAmpId.empty() || rig.supportFactoryAmp);
  return volum::MakeProcessingPlan(have.mainModel, /*noiseGateActive=*/false, /*toneStackActive=*/true,
                                   /*irActive=*/!rig.activeIrId.empty(),
                                   /*haveIR=*/have.ir && !rig.activeIrId.empty(),
                                   /*preCompActive=*/false, preActive, havePre, /*delayActive=*/false,
                                   /*reverbActive=*/false, /*tunerActive=*/false, rig.dualAmpActive, haveSupportAmp,
                                   /*supportToneStackActive=*/true, /*supportIrActive=*/!rig.supportActiveIrId.empty(),
                                   /*haveSupportIR=*/have.supportIr && !rig.supportActiveIrId.empty());
}

// A dual-amp rig on two custom amps, a custom IR on MAIN, a custom pedal in PRE 1
// and a factory capture in PRE 2, with a named preset selected.
SoundingRig FullRig()
{
  SoundingRig rig;
  rig.factoryAmpIdx = 3;
  rig.mainCustomAmpId = "amp_main";
  rig.dualAmpActive = true;
  rig.supportCustomAmpId = "amp_support";
  rig.activeIrId = "ir_mesa";
  rig.supportActiveIrId = "ir_orange";
  rig.preActive[0] = true;
  rig.preCapture[0] = 64; // custom pedal
  rig.preActive[1] = true;
  rig.preCapture[1] = 5; // factory capture
  rig.recalledPresetId = "preset_lead";
  return rig;
}

LibraryItemRef Amp(const char* id, const char* name)
{
  LibraryItemRef it;
  it.kind = LibraryKind::CustomAmp;
  it.id = id;
  it.displayName = name;
  return it;
}

LibraryItemRef Ir(const char* id, const char* name)
{
  LibraryItemRef it;
  it.kind = LibraryKind::IR;
  it.id = id;
  it.displayName = name;
  return it;
}

LibraryItemRef Pedal(const char* id, const char* name, int capture)
{
  LibraryItemRef it;
  it.kind = LibraryKind::Pedal;
  it.id = id;
  it.displayName = name;
  it.pedalCapture = capture;
  return it;
}

LibraryItemRef PresetRef(const char* id, const char* name)
{
  LibraryItemRef it;
  it.kind = LibraryKind::Preset;
  it.id = id;
  it.displayName = name;
  return it;
}

RigLabels Labels(const char* factoryAmpName)
{
  RigLabels l;
  l.factoryAmpName = factoryAmpName;
  return l;
}

bool Contains(const std::string& haystack, const char* needle)
{
  INFO(haystack);
  return haystack.find(needle) != std::string::npos;
}
} // namespace

// ---------------------------------------------------------------------------
// Delete of the id that is sounding
// ---------------------------------------------------------------------------

TEST_CASE("Deleting the custom amp on MAIN sends MAIN to the sidebar factory amp")
{
  const SoundingRig before = FullRig();
  const auto plan = PlanDelete(before, Amp("amp_main", "My Plexi"), Labels("JMP-1 2203"));

  REQUIRE(plan.TouchesSoundingRig());
  CHECK(plan.Has(RigRepair::RevertMainToFactoryAmp));
  CHECK_FALSE(plan.Has(RigRepair::DropSupportLane)); // the other lane is a different amp
  CHECK(plan.after.mainCustomAmpId.empty());
  CHECK(plan.after.factoryAmpIdx == 3); // the amp already in the sidebar, not a jump

  // DSP: with the custom capture unloaded and the factory one loaded in its place,
  // the graph still runs a main model - it is not a hole, and it is not the dead
  // capture. SUPPORT is untouched.
  LoadedPayloads afterLoad;
  const auto graph = GraphFor(plan.after, afterLoad);
  CHECK(graph.runMainModel);
  CHECK_FALSE(graph.runFallback);
  CHECK(graph.runSupportModel);
  CHECK(graph.runDualAmp);

  // And the moment before the replacement lands the lane is honest about it: no
  // main model means the fallback path, not a stale one.
  LoadedPayloads midSwap;
  midSwap.mainModel = false;
  const auto midGraph = GraphFor(plan.after, midSwap);
  CHECK_FALSE(midGraph.runMainModel);
  CHECK(midGraph.runFallback);
  CHECK_FALSE(midGraph.runDualAmp); // dual needs both lanes

  CHECK(Contains(plan.confirmBody, "playing on MAIN"));
  CHECK(Contains(plan.confirmBody, "JMP-1 2203"));
}

TEST_CASE("Deleting the custom amp used only on SUPPORT drops that lane and leaves MAIN alone")
{
  SoundingRig before = FullRig();
  before.mainCustomAmpId.clear(); // MAIN is a factory amp
  const auto plan = PlanDelete(before, Amp("amp_support", "Partner"), Labels("JMP-1 2203"));

  CHECK(plan.Has(RigRepair::DropSupportLane));
  CHECK_FALSE(plan.Has(RigRepair::RevertMainToFactoryAmp));
  CHECK_FALSE(plan.after.dualAmpActive);
  CHECK(plan.after.supportCustomAmpId.empty());
  // The support lane's IR goes with the lane: nothing is left to convolve.
  CHECK(plan.after.supportActiveIrId.empty());
  CHECK(plan.after.activeIrId == "ir_mesa"); // MAIN untouched
  CHECK(plan.after.preCapture[0] == 64);

  const auto graph = GraphFor(plan.after);
  CHECK_FALSE(graph.runSupportModel);
  CHECK_FALSE(graph.runDualAmp);
  CHECK_FALSE(graph.runSupportIR);
  CHECK_FALSE(graph.runSupportToneStack);
  CHECK(graph.runMainModel);
  CHECK(graph.runIR); // MAIN's custom IR still convolves

  CHECK(Contains(plan.confirmBody, "playing on SUPPORT"));
  CHECK(Contains(plan.confirmBody, "SUPPORT will switch off"));
}

TEST_CASE("Deleting a custom amp that is on both lanes moves both")
{
  SoundingRig before = FullRig();
  before.supportCustomAmpId = "amp_main"; // same amp on both lanes
  const auto plan = PlanDelete(before, Amp("amp_main", "My Plexi"), Labels("Ampete 204"));

  CHECK(plan.Has(RigRepair::RevertMainToFactoryAmp));
  CHECK(plan.Has(RigRepair::DropSupportLane));
  CHECK(Contains(plan.confirmBody, "MAIN and SUPPORT"));
  CHECK(Contains(plan.confirmBody, "Ampete 204"));
  CHECK(Contains(plan.confirmBody, "SUPPORT will switch off"));
}

TEST_CASE("Deleting an unused library row leaves the sounding rig alone")
{
  // Identity, not row position. Deleting a neighbour must not retarget a lane -
  // the pre-1.3.0 index bookkeeping was exactly the kind of code that did.
  const SoundingRig before = FullRig();

  SUBCASE("another custom amp")
  {
    const auto plan = PlanDelete(before, Amp("amp_unused", "Never picked"), Labels("JMP-1 2203"));
    CHECK_FALSE(plan.TouchesSoundingRig());
    CHECK(plan.after.mainCustomAmpId == "amp_main");
    CHECK(plan.after.supportCustomAmpId == "amp_support");
    CHECK(Contains(plan.confirmBody, "cannot be undone"));
    CHECK_FALSE(Contains(plan.confirmBody, "right now"));
  }

  SUBCASE("another IR")
  {
    const auto plan = PlanDelete(before, Ir("ir_unused", "Spare"), Labels("JMP-1 2203"));
    CHECK_FALSE(plan.TouchesSoundingRig());
    CHECK(plan.after.activeIrId == "ir_mesa");
  }

  SUBCASE("another pedal")
  {
    const auto plan = PlanDelete(before, Pedal("pedal_unused", "Spare", 65), Labels("JMP-1 2203"));
    CHECK_FALSE(plan.TouchesSoundingRig());
    CHECK(plan.after.preCapture[0] == 64);
  }

  SUBCASE("another preset")
  {
    const auto plan = PlanDelete(before, PresetRef("preset_other", "Other"), Labels("JMP-1 2203"));
    CHECK_FALSE(plan.TouchesSoundingRig());
    CHECK(plan.after.recalledPresetId == "preset_lead");
  }

  // In every case the DSP graph is bit-for-bit the one it was.
  const auto planned = PlanDelete(before, Amp("amp_unused", "Never picked"));
  const auto a = GraphFor(before);
  const auto b = GraphFor(planned.after);
  CHECK(a.runMainModel == b.runMainModel);
  CHECK(a.runSupportModel == b.runSupportModel);
  CHECK(a.runIR == b.runIR);
  CHECK(a.runPreNam[0] == b.runPreNam[0]);
  CHECK(a.runPreNam[1] == b.runPreNam[1]);
}

TEST_CASE("Deleting the pedal in PRE 1 empties that slot and leaves PRE 2 running")
{
  const SoundingRig before = FullRig();
  REQUIRE(GraphFor(before).runPreNam[0]);

  const auto plan = PlanDelete(before, Pedal("pedal_klon", "Klon", 64), Labels("JMP-1 2203"));
  CHECK(plan.Has(RigRepair::ClearPreSlot1));
  CHECK_FALSE(plan.Has(RigRepair::ClearPreSlot2));
  CHECK(plan.after.preCapture[0] == 0);
  CHECK_FALSE(plan.after.preActive[0]);
  CHECK(plan.after.preCapture[1] == 5);
  CHECK(plan.after.preActive[1]);

  // DSP: the live PRE model is dropped, not left convolving a deleted capture.
  const auto graph = GraphFor(plan.after);
  CHECK_FALSE(graph.runPreNam[0]);
  CHECK(graph.runPreNam[1]);

  CHECK(Contains(plan.confirmBody, "loaded in PRE 1"));
  CHECK(Contains(plan.confirmBody, "PRE 1 will be empty"));
}

TEST_CASE("A pedal loaded in both PRE slots empties both")
{
  SoundingRig before = FullRig();
  before.preCapture[1] = 64; // same pedal in both slots
  const auto plan = PlanDelete(before, Pedal("pedal_klon", "Klon", 64));

  CHECK(plan.Has(RigRepair::ClearPreSlot1));
  CHECK(plan.Has(RigRepair::ClearPreSlot2));
  const auto graph = GraphFor(plan.after);
  CHECK_FALSE(graph.runPreNam[0]);
  CHECK_FALSE(graph.runPreNam[1]);
  CHECK(Contains(plan.confirmBody, "PRE 1 and PRE 2"));
}

TEST_CASE("A pedal loaded but bypassed still counts as in use")
{
  // The pill turns back on without another pick, so leaving the capture in place
  // would arm a slot to load a file that no longer exists.
  SoundingRig before = FullRig();
  before.preActive[0] = false;
  const auto plan = PlanDelete(before, Pedal("pedal_klon", "Klon", 64));
  CHECK(plan.Has(RigRepair::ClearPreSlot1));
  CHECK(plan.after.preCapture[0] == 0);
}

TEST_CASE("Deleting the active IR falls back to the baked cab and the convolver goes")
{
  const SoundingRig before = FullRig();
  REQUIRE(GraphFor(before).runIR);

  const auto plan = PlanDelete(before, Ir("ir_mesa", "Mesa OS"), Labels("JMP-1 2203"));
  CHECK(plan.Has(RigRepair::ClearMainIr));
  CHECK_FALSE(plan.Has(RigRepair::ClearSupportIr)); // SUPPORT convolves a different IR
  CHECK(plan.after.activeIrId.empty());
  CHECK(plan.after.supportActiveIrId == "ir_orange");

  const auto graph = GraphFor(plan.after);
  CHECK_FALSE(graph.runIR);
  CHECK(graph.runSupportIR);
  CHECK(graph.runMainModel); // the amp keeps playing, now through its baked cab

  CHECK(Contains(plan.confirmBody, "convolving MAIN"));
  CHECK(Contains(plan.confirmBody, "baked cab"));
}

TEST_CASE("An IR convolving both lanes is dropped from both")
{
  SoundingRig before = FullRig();
  before.supportActiveIrId = "ir_mesa";
  const auto plan = PlanDelete(before, Ir("ir_mesa", "Mesa OS"));
  CHECK(plan.Has(RigRepair::ClearMainIr));
  CHECK(plan.Has(RigRepair::ClearSupportIr));
  const auto graph = GraphFor(plan.after);
  CHECK_FALSE(graph.runIR);
  CHECK_FALSE(graph.runSupportIR);
}

TEST_CASE("An IR on a switched-off SUPPORT lane is not treated as sounding")
{
  SoundingRig before = FullRig();
  before.dualAmpActive = false;
  const auto plan = PlanDelete(before, Ir("ir_orange", "Orange"));
  CHECK_FALSE(plan.TouchesSoundingRig());
}

TEST_CASE("Deleting the selected preset forgets the name and changes nothing else")
{
  const SoundingRig before = FullRig();
  const auto plan = PlanDelete(before, PresetRef("preset_lead", "My Lead"), Labels("JMP-1 2203"));

  REQUIRE(plan.repairs.size() == 1);
  CHECK(plan.Has(RigRepair::ForgetPresetName));
  CHECK(plan.after.recalledPresetId.empty());

  // Deleting "My Lead" does not rewind PRE/AMP/POST: the whole graph is identical.
  const auto a = GraphFor(before);
  const auto b = GraphFor(plan.after);
  CHECK(a.runMainModel == b.runMainModel);
  CHECK(a.runSupportModel == b.runSupportModel);
  CHECK(a.runDualAmp == b.runDualAmp);
  CHECK(a.runIR == b.runIR);
  CHECK(a.runSupportIR == b.runSupportIR);
  CHECK(a.runPreNam[0] == b.runPreNam[0]);
  CHECK(a.runPreNam[1] == b.runPreNam[1]);
  CHECK(plan.after.mainCustomAmpId == before.mainCustomAmpId);
  CHECK(plan.after.preCapture[0] == before.preCapture[0]);

  CHECK(Contains(plan.confirmBody, "selected preset"));
  CHECK(Contains(plan.confirmBody, "only the name is forgotten"));
}

// ---------------------------------------------------------------------------
// Pack replace is not a delete
// ---------------------------------------------------------------------------

TEST_CASE("A confirmed Pack replace reloads the lane instead of falling back")
{
  const SoundingRig before = FullRig();

  SUBCASE("custom amp on MAIN")
  {
    const auto plan = PlanReplace(before, Amp("amp_main", "My Plexi"), Labels("JMP-1 2203"));
    CHECK(plan.Has(RigRepair::ReloadMainCapture));
    CHECK_FALSE(plan.Has(RigRepair::RevertMainToFactoryAmp));
    CHECK(plan.after.mainCustomAmpId == "amp_main"); // the lane does not move
    const auto graph = GraphFor(plan.after);
    CHECK(graph.runMainModel);
    CHECK(graph.runDualAmp);
    CHECK(Contains(plan.confirmBody, "reload with the imported capture"));
  }

  SUBCASE("active IR")
  {
    const auto plan = PlanReplace(before, Ir("ir_mesa", "Mesa OS"));
    CHECK(plan.Has(RigRepair::ReloadMainIr));
    CHECK_FALSE(plan.Has(RigRepair::ClearMainIr));
    CHECK(plan.after.activeIrId == "ir_mesa"); // not the baked cab
    CHECK(GraphFor(plan.after).runIR);
  }

  SUBCASE("pedal in PRE 1")
  {
    const auto plan = PlanReplace(before, Pedal("pedal_klon", "Klon", 64));
    CHECK(plan.Has(RigRepair::ReloadPreSlot1));
    CHECK_FALSE(plan.Has(RigRepair::ClearPreSlot1));
    CHECK(plan.after.preCapture[0] == 64); // not empty
    CHECK(GraphFor(plan.after).runPreNam[0]);
  }

  SUBCASE("custom amp on both lanes")
  {
    SoundingRig both = before;
    both.supportCustomAmpId = "amp_main";
    const auto plan = PlanReplace(both, Amp("amp_main", "My Plexi"));
    CHECK(plan.Has(RigRepair::ReloadMainCapture));
    CHECK(plan.Has(RigRepair::ReloadSupportCapture));
  }
}

TEST_CASE("Replacing a preset does not touch the sounding rig")
{
  // A preset is a snapshot, not a capture. Overwriting the stored values leaves
  // what is currently sounding exactly where it is; the "(unsaved)" flag then
  // reports the difference, which is the truth.
  const SoundingRig before = FullRig();
  const auto plan = PlanReplace(before, PresetRef("preset_lead", "My Lead"));
  CHECK_FALSE(plan.TouchesSoundingRig());
  CHECK(plan.after.recalledPresetId == "preset_lead");
}

TEST_CASE("Replacing an unused library row touches nothing")
{
  const SoundingRig before = FullRig();
  const auto plan = PlanReplace(before, Amp("amp_unused", "Never picked"));
  CHECK_FALSE(plan.TouchesSoundingRig());
  CHECK(Contains(plan.confirmBody, "replaces the one in your library"));
  CHECK_FALSE(Contains(plan.confirmBody, "right now"));
}

// ---------------------------------------------------------------------------
// The swap is deferred, so there is no burst of raw amp
// ---------------------------------------------------------------------------

TEST_CASE("The IR teardown a delete triggers waits for the replacement cab")
{
  // Same deferred family as a cab <-> IR switch: dropping the convolver the block
  // the user confirms exposes raw, cab-less amp until the baked-cab capture
  // arrives. The convolver keeps running while the removal is pending.
  using namespace volum::dsp_staging;
  CHECK(IrConvolutionActive(/*toggleOn=*/false, /*deferredRemovalPending=*/true));

  int waited = 0;
  auto step = StepDeferredIrSwap(/*pending=*/true, waited, /*replacementStaged=*/false, /*maxWaitBlocks=*/8);
  CHECK_FALSE(step.fire);
  CHECK(step.stillPending);
  waited = step.waitedBlocks;

  step = StepDeferredIrSwap(true, waited, /*replacementStaged=*/true, 8);
  CHECK(step.fire); // both land on one block
  CHECK_FALSE(IrConvolutionActive(false, false)); // and then the convolver is gone
}

TEST_CASE("A replacement capture that never arrives does not strand the lane")
{
  using namespace volum::dsp_staging;
  int waited = 0;
  bool fired = false;
  for (int i = 0; i < 8; ++i)
  {
    const auto step = StepDeferredIrSwap(true, waited, /*replacementStaged=*/false, /*maxWaitBlocks=*/4);
    waited = step.waitedBlocks;
    if (step.fire)
    {
      fired = true;
      break;
    }
  }
  CHECK(fired);
}

// ---------------------------------------------------------------------------
// Sibling instances
// ---------------------------------------------------------------------------

TEST_CASE("A sibling keeps its RAM copy until it next needs the deleted id")
{
  // A catalog delete does not rewrite another VoLum's sounding rig. Its already
  // loaded model is allowed to keep playing even though the file is gone; the
  // reckoning is the next time it needs the id.
  CHECK_FALSE(SiblingMustRepairOnNextNeed(/*resolvable=*/true));
  CHECK(SiblingMustRepairOnNextNeed(/*resolvable=*/false));

  // And when that moment comes, the sibling takes the same fallback a local delete
  // would have taken - it does not crash and it does not go silent.
  const SoundingRig sibling = FullRig();
  const auto plan = PlanDelete(sibling, Amp("amp_main", "My Plexi"), Labels("JMP-1 2203"));
  CHECK(plan.Has(RigRepair::RevertMainToFactoryAmp));
  LoadedPayloads afterFallback;
  const auto graph = GraphFor(plan.after, afterFallback);
  CHECK(graph.runMainModel);
  CHECK_FALSE(graph.runFallback);
}

// ---------------------------------------------------------------------------
// MIDI slots (schema from ticket 01; the decoder is a separate effort)
// ---------------------------------------------------------------------------

TEST_CASE("A MIDI slot whose Sound was deleted goes invalid and its neighbour is untouched")
{
  using namespace volum::content;
  ContentStore store;
  volum::custom::CustomAmp amp;
  amp.id = "amp_1";
  store.reg().amps.push_back(amp);
  volum::custom::CustomAmp other;
  other.id = "amp_2";
  store.reg().amps.push_back(other);
  Preset pr;
  pr.id = "preset_1";
  pr.name = "Lead";
  store.reg().presetBanks["amp_1"] = {pr};

  store.SetMidiSlot(5, "amp_1", "preset_1");
  store.SetMidiSlot(6, "amp_2", "");
  REQUIRE(ResolveMidiSlot(store.reg(), 5, 12) == MidiSlotState::Valid);
  REQUIRE(ResolveMidiSlot(store.reg(), 6, 12) == MidiSlotState::Valid);

  SUBCASE("the preset it points at is deleted")
  {
    store.reg().presetBanks["amp_1"].clear();
    CHECK(ResolveMidiSlot(store.reg(), 5, 12) == MidiSlotState::Invalid);
    CHECK(ResolveMidiSlot(store.reg(), 6, 12) == MidiSlotState::Valid);
    // The number stays with the player's pedalboard: slot 6 does not inherit 5.
    CHECK(store.reg().midiSoundMap.count(5) == 1);
    CHECK(store.reg().midiSoundMap.at(6).ampId == "amp_2");
  }

  SUBCASE("the amp it points at is deleted")
  {
    store.RemoveCustomAmp("amp_1");
    CHECK(ResolveMidiSlot(store.reg(), 5, 12) == MidiSlotState::Invalid);
    CHECK(ResolveMidiSlot(store.reg(), 6, 12) == MidiSlotState::Valid);
  }

  SUBCASE("reassigning an invalid slot makes it valid again")
  {
    store.RemoveCustomAmp("amp_1");
    REQUIRE(ResolveMidiSlot(store.reg(), 5, 12) == MidiSlotState::Invalid);
    store.SetMidiSlot(5, "amp_2", "");
    CHECK(ResolveMidiSlot(store.reg(), 5, 12) == MidiSlotState::Valid);
  }

  SUBCASE("clearing an invalid slot makes it unassigned")
  {
    store.RemoveCustomAmp("amp_1");
    store.ClearMidiSlot(5);
    CHECK(ResolveMidiSlot(store.reg(), 5, 12) == MidiSlotState::Unassigned);
    CHECK(store.reg().midiSoundMap.count(5) == 0);
    CHECK(ResolveMidiSlot(store.reg(), 6, 12) == MidiSlotState::Valid);
  }
}

// ---------------------------------------------------------------------------
// Confirm copy
// ---------------------------------------------------------------------------

TEST_CASE("Confirm copy names the item, the in-use case, and the destination")
{
  const SoundingRig rig = FullRig();

  const auto ampPlan = PlanDelete(rig, Amp("amp_main", "My Plexi"), Labels("Soldano SLO"));
  CHECK(ampPlan.confirmTitle == "Delete?");
  CHECK(Contains(ampPlan.confirmBody, "custom amp \"My Plexi\""));
  CHECK(Contains(ampPlan.confirmBody, "MAIN will switch to Soldano SLO"));
  CHECK(Contains(ampPlan.confirmBody, "cannot be undone"));

  const auto irPlan = PlanDelete(rig, Ir("ir_mesa", "Mesa OS"), Labels("Soldano SLO"));
  CHECK(Contains(irPlan.confirmBody, "IR \"Mesa OS\""));
  CHECK(Contains(irPlan.confirmBody, "Soldano SLO's baked cab"));

  const auto pedalPlan = PlanDelete(rig, Pedal("pedal_klon", "Klon", 64));
  CHECK(Contains(pedalPlan.confirmBody, "pedal \"Klon\""));
  CHECK(Contains(pedalPlan.confirmBody, "PRE 1 will be empty"));

  const auto presetPlan = PlanDelete(rig, PresetRef("preset_lead", "My Lead"));
  CHECK(Contains(presetPlan.confirmBody, "preset \"My Lead\""));

  // With no label to work from the copy still reads as a sentence rather than
  // trailing off into an empty quote.
  const auto noLabel = PlanDelete(rig, Amp("amp_main", "My Plexi"));
  CHECK(Contains(noLabel.confirmBody, "MAIN will switch to the factory amp"));

  const auto replacePlan = PlanReplace(rig, Amp("amp_main", "My Plexi"));
  CHECK(replacePlan.confirmTitle == "Replace?");
  CHECK(Contains(replacePlan.confirmBody, "Replace custom amp \"My Plexi\""));
  CHECK_FALSE(Contains(replacePlan.confirmBody, "cannot be undone"));
}
