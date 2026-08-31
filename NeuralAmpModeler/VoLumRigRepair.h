#pragma once

// Rig repair: what this instance's sounding rig does when a library id it is
// currently using is deleted, or replaced by a Pack import.
//
// Before 1.3.0 a delete updated the catalog and the chrome and stopped there, so
// the audio thread kept playing a capture whose file had just been removed -
// factory amp name in the sidebar, dead custom amp in the speakers. This header
// is the headless decision half of the fix: given a snapshot of the sounding rig
// and the item that is going away, it answers which lanes have to move, what the
// rig looks like afterwards, and what the confirm dialog says. The plugin owns
// only the mechanical part (push params, queue loads, refresh chrome).
//
// Deliberately free of iPlug and of the content store: the same answers have to
// be reachable from a unit test, from the Manage panel, from the sidebar bin, and
// later from a Pack import, and only a pure function is reachable from all four.
//
// Two rules shape everything here:
//
//   Identity, not row position. A lane moves only when the id it is playing is
//   the id going away. Deleting a neighbour must never retarget a slot.
//
//   Delete falls back, replace reloads. A delete sends the lane to the available
//   default (factory amp / empty PRE slot / baked cab). A confirmed Pack replace
//   keeps the lane where it is and loads the new payload behind the same id -
//   bouncing it to the fallback first would be an audible detour to a sound the
//   user did not ask for.
//
// Tests: tests/test_volum_rig_repair.cpp (plan + confirm copy, and the resulting
// DSP graph via MakeProcessingPlan), tests/test_volum_ui_regressions.cpp (that
// the plugin's delete paths actually run the plan).

#include <algorithm>
#include <string>
#include <vector>

namespace volum::rig
{

enum class LibraryKind
{
  CustomAmp,
  IR,
  Pedal,
  Preset
};

// This instance's live graph in library terms. Only the fields a delete can
// invalidate: knob values are irrelevant to the question "what stops sounding".
struct SoundingRig
{
  int factoryAmpIdx = 0; // the sidebar factory amp behind a custom MAIN
  std::string mainCustomAmpId; // empty: MAIN is the factory amp
  bool dualAmpActive = false;
  std::string supportCustomAmpId; // empty: SUPPORT is a factory amp or none
  bool supportFactoryAmp = false; // SUPPORT is a factory amp (not custom, not none)
  std::string activeIrId; // MAIN lane custom IR ("" = baked cab)
  std::string supportActiveIrId;
  bool preActive[2] = {false, false};
  int preCapture[2] = {0, 0}; // capture index as the rig stores it
  std::string recalledPresetId; // the name showing in the preset bar
};

// The item going away. Pedals are addressed by the capture index the rig stores
// rather than by library id, because that is what a PRE slot holds.
struct LibraryItemRef
{
  LibraryKind kind = LibraryKind::CustomAmp;
  std::string id;
  int pedalCapture = -1; // pedals only
  std::string displayName; // for the confirm copy
};

// Labels only the caller knows. Used for confirm copy and nothing else, so a
// headless caller may leave them empty.
struct RigLabels
{
  std::string factoryAmpName; // the amp MAIN reverts to
};

enum class RigRepair
{
  RevertMainToFactoryAmp, // custom MAIN gone: back to the sidebar factory amp
  DropSupportLane, // custom SUPPORT gone: that lane switches off
  ClearPreSlot1,
  ClearPreSlot2,
  ClearMainIr, // back to the current amp's baked cab
  ClearSupportIr,
  ForgetPresetName, // keep the sound, drop the name
  ReloadMainCapture, // Pack replace: same id, new payload
  ReloadSupportCapture,
  ReloadPreSlot1,
  ReloadPreSlot2,
  ReloadMainIr,
  ReloadSupportIr
};

struct RigRepairPlan
{
  SoundingRig after;
  std::vector<RigRepair> repairs;
  std::string confirmTitle;
  std::string confirmBody;

  bool TouchesSoundingRig() const { return !repairs.empty(); }
  bool Has(RigRepair r) const { return std::find(repairs.begin(), repairs.end(), r) != repairs.end(); }
};

namespace detail
{
inline void Add(RigRepairPlan& plan, RigRepair r)
{
  if (!plan.Has(r))
    plan.repairs.push_back(r);
}

inline std::string Quoted(const std::string& s)
{
  return "\"" + s + "\"";
}

inline const char* KindNoun(LibraryKind kind)
{
  switch (kind)
  {
    case LibraryKind::CustomAmp: return "custom amp";
    case LibraryKind::IR: return "IR";
    case LibraryKind::Pedal: return "pedal";
    case LibraryKind::Preset: return "preset";
  }
  return "item";
}

// "PRE 1", "PRE 2", or "PRE 1 and PRE 2".
inline std::string PreSlotList(bool one, bool two)
{
  if (one && two)
    return "PRE 1 and PRE 2";
  return one ? "PRE 1" : "PRE 2";
}

inline std::string FactoryAmpLabel(const RigLabels& labels)
{
  return labels.factoryAmpName.empty() ? std::string("the factory amp") : labels.factoryAmpName;
}
} // namespace detail

// Which lanes a delete of `item` disturbs, and the rig that is left. `labels` only
// affects the confirm copy.
inline RigRepairPlan PlanDelete(const SoundingRig& rig, const LibraryItemRef& item, const RigLabels& labels = {})
{
  using namespace detail;
  RigRepairPlan plan;
  plan.after = rig;
  plan.confirmTitle = "Delete?";

  const std::string what = std::string("Delete ") + KindNoun(item.kind) + " " + Quoted(item.displayName) + "?";
  const std::string undo = " This cannot be undone.";
  std::string inUse;

  switch (item.kind)
  {
    case LibraryKind::CustomAmp:
    {
      const bool onMain = !item.id.empty() && rig.mainCustomAmpId == item.id;
      // A support lane that is switched off is not playing anything, whatever id
      // it remembers.
      const bool onSupport = !item.id.empty() && rig.dualAmpActive && rig.supportCustomAmpId == item.id;
      if (onMain)
      {
        Add(plan, RigRepair::RevertMainToFactoryAmp);
        plan.after.mainCustomAmpId.clear();
      }
      if (onSupport)
      {
        Add(plan, RigRepair::DropSupportLane);
        plan.after.dualAmpActive = false;
        plan.after.supportCustomAmpId.clear();
        plan.after.supportActiveIrId.clear();
      }
      if (onMain && onSupport)
        inUse = " It is playing on MAIN and SUPPORT right now. MAIN will switch to " + FactoryAmpLabel(labels)
                + " and SUPPORT will switch off.";
      else if (onMain)
        inUse = " It is playing on MAIN right now. MAIN will switch to " + FactoryAmpLabel(labels) + ".";
      else if (onSupport)
        inUse = " It is playing on SUPPORT right now. SUPPORT will switch off.";
      break;
    }
    case LibraryKind::IR:
    {
      const bool onMain = !item.id.empty() && rig.activeIrId == item.id;
      const bool onSupport = !item.id.empty() && rig.dualAmpActive && rig.supportActiveIrId == item.id;
      if (onMain)
      {
        Add(plan, RigRepair::ClearMainIr);
        plan.after.activeIrId.clear();
      }
      if (onSupport)
      {
        Add(plan, RigRepair::ClearSupportIr);
        plan.after.supportActiveIrId.clear();
      }
      if (onMain && onSupport)
        inUse = " It is convolving MAIN and SUPPORT right now. Both lanes will fall back to their baked cab.";
      else if (onMain)
        inUse = " It is convolving MAIN right now. MAIN will fall back to " + FactoryAmpLabel(labels)
                + "'s baked cab.";
      else if (onSupport)
        inUse = " It is convolving SUPPORT right now. SUPPORT will fall back to its baked cab.";
      break;
    }
    case LibraryKind::Pedal:
    {
      // An inactive PRE slot still holds the capture, and the pill turns back on
      // without a further pick, so a loaded-but-bypassed slot is in use too.
      const bool one = item.pedalCapture >= 0 && rig.preCapture[0] == item.pedalCapture;
      const bool two = item.pedalCapture >= 0 && rig.preCapture[1] == item.pedalCapture;
      if (one)
      {
        Add(plan, RigRepair::ClearPreSlot1);
        plan.after.preActive[0] = false;
        plan.after.preCapture[0] = 0;
      }
      if (two)
      {
        Add(plan, RigRepair::ClearPreSlot2);
        plan.after.preActive[1] = false;
        plan.after.preCapture[1] = 0;
      }
      if (one || two)
        inUse = " It is loaded in " + PreSlotList(one, two) + " right now. " + PreSlotList(one, two)
                + " will be empty.";
      break;
    }
    case LibraryKind::Preset:
    {
      if (!item.id.empty() && rig.recalledPresetId == item.id)
      {
        Add(plan, RigRepair::ForgetPresetName);
        plan.after.recalledPresetId.clear();
        inUse = " It is the selected preset. Your sound stays exactly as it is - only the name is forgotten.";
      }
      break;
    }
  }

  plan.confirmBody = what + inUse + undo;
  return plan;
}

// A confirmed Pack import that overwrites an id this instance is playing. Same
// question, opposite answer: the lane stays and reloads.
inline RigRepairPlan PlanReplace(const SoundingRig& rig, const LibraryItemRef& item, const RigLabels& labels = {})
{
  using namespace detail;
  RigRepairPlan plan;
  plan.after = rig; // a replace never moves a lane
  plan.confirmTitle = "Replace?";

  const std::string what = std::string("Replace ") + KindNoun(item.kind) + " " + Quoted(item.displayName) + "?";
  std::string inUse;

  switch (item.kind)
  {
    case LibraryKind::CustomAmp:
    {
      const bool onMain = !item.id.empty() && rig.mainCustomAmpId == item.id;
      const bool onSupport = !item.id.empty() && rig.dualAmpActive && rig.supportCustomAmpId == item.id;
      if (onMain)
        Add(plan, RigRepair::ReloadMainCapture);
      if (onSupport)
        Add(plan, RigRepair::ReloadSupportCapture);
      if (onMain && onSupport)
        inUse = " It is playing on MAIN and SUPPORT right now. Both lanes will reload with the imported capture.";
      else if (onMain)
        inUse = " It is playing on MAIN right now. MAIN will reload with the imported capture.";
      else if (onSupport)
        inUse = " It is playing on SUPPORT right now. SUPPORT will reload with the imported capture.";
      break;
    }
    case LibraryKind::IR:
    {
      const bool onMain = !item.id.empty() && rig.activeIrId == item.id;
      const bool onSupport = !item.id.empty() && rig.dualAmpActive && rig.supportActiveIrId == item.id;
      if (onMain)
        Add(plan, RigRepair::ReloadMainIr);
      if (onSupport)
        Add(plan, RigRepair::ReloadSupportIr);
      if (onMain || onSupport)
        inUse = " It is convolving right now and will reload with the imported IR.";
      break;
    }
    case LibraryKind::Pedal:
    {
      const bool one = item.pedalCapture >= 0 && rig.preCapture[0] == item.pedalCapture;
      const bool two = item.pedalCapture >= 0 && rig.preCapture[1] == item.pedalCapture;
      if (one)
        Add(plan, RigRepair::ReloadPreSlot1);
      if (two)
        Add(plan, RigRepair::ReloadPreSlot2);
      if (one || two)
        inUse = " It is loaded in " + PreSlotList(one, two) + " right now and will reload with the imported capture.";
      break;
    }
    case LibraryKind::Preset:
      // A preset is a snapshot, not a capture: overwriting the stored values does
      // not change what is currently sounding. The bar keeps the name and the
      // "(unsaved)" flag then reports the difference, which is the truth.
      break;
  }

  plan.confirmBody = what + inUse + " The imported version replaces the one in your library.";
  return plan;
}

// A sibling instance does not get its rig rewritten when another one deletes a
// library id: it keeps its RAM copy until it next needs that id (re-picks it,
// recalls a Sound, reloads a capture). This is the check for that next need.
//
// `resolvable` is whether the id is still in the catalog. False means the payload
// is gone, so a fresh need has to fall back exactly as a local delete would; the
// already-loaded model may keep sounding until then.
inline bool SiblingMustRepairOnNextNeed(bool resolvable)
{
  return !resolvable;
}

} // namespace volum::rig
