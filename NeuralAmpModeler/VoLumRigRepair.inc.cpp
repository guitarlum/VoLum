// Delete / Pack-replace of a library id this instance is playing.
//
// The decisions are in VoLumRigRepair.h (pure, unit-tested). This file is only the
// mechanical half: read the live rig into a snapshot, and push a planned repair
// back out through the ordinary select/clear paths so a repair is indistinguishable
// from the user having done it by hand.
//
// Tail-included by NeuralAmpModeler.cpp.

volum::rig::SoundingRig NeuralAmpModeler::_VolumSnapshotSoundingRig() const
{
  volum::rig::SoundingRig rig;
  rig.factoryAmpIdx = mVolumAmpIdx;
  if (mVolumCustomMainIdx >= 0)
    rig.mainCustomAmpId = volum::custom::CustomAmpIdAt(mVolumCustomMainIdx);
  rig.dualAmpActive = GetParam(kDualAmpActive)->Bool();
  if (mVolumCustomSupportIdx >= 0)
    rig.supportCustomAmpId = volum::custom::CustomAmpIdAt(mVolumCustomSupportIdx);
  rig.supportFactoryAmp = mVolumCustomSupportIdx < 0 && GetParam(kSupportAmpIdx)->Int() >= 0;

  // The scene of whichever lane is focused owns both IR ids (see _VolumActiveScene).
  const auto& scene = const_cast<NeuralAmpModeler*>(this)->_VolumActiveScene();
  rig.activeIrId = scene.activeIrId;
  rig.supportActiveIrId = scene.supportActiveIrId;

  rig.preActive[0] = GetParam(kPreNam1Active)->Bool();
  rig.preActive[1] = GetParam(kPreNam2Active)->Bool();
  rig.preCapture[0] = GetParam(kPreNam1Capture)->Int();
  rig.preCapture[1] = GetParam(kPreNam2Capture)->Int();

  rig.recalledPresetId = mVolumActivePresetId;
  return rig;
}

// Fill in the parts of the reference only the catalog knows: a pedal is addressed
// in the rig by its capture index, not by its library id.
static volum::rig::LibraryItemRef VolumMakeLibraryItemRef(volum::rig::LibraryKind kind, const std::string& id,
                                                          const std::string& displayName)
{
  volum::rig::LibraryItemRef item;
  item.kind = kind;
  item.id = id;
  item.displayName = displayName;
  if (kind == volum::rig::LibraryKind::Pedal)
    item.pedalCapture = volum::custom::PedalLegacyIndexById(id);
  return item;
}

std::string NeuralAmpModeler::_VolumPlanLibraryDelete(volum::rig::LibraryKind kind, const std::string& id,
                                                      const std::string& displayName)
{
  volum::rig::RigLabels labels;
  labels.factoryAmpName = volum::kAmps[std::clamp(mVolumAmpIdx, 0, volum::kAmpCount - 1)].displayName;
  mVolumPendingRigRepair =
    volum::rig::PlanDelete(_VolumSnapshotSoundingRig(), VolumMakeLibraryItemRef(kind, id, displayName), labels);
  return mVolumPendingRigRepair.confirmBody;
}

std::string NeuralAmpModeler::_VolumPlanLibraryReplace(volum::rig::LibraryKind kind, const std::string& id,
                                                       const std::string& displayName)
{
  volum::rig::RigLabels labels;
  labels.factoryAmpName = volum::kAmps[std::clamp(mVolumAmpIdx, 0, volum::kAmpCount - 1)].displayName;
  mVolumPendingRigRepair =
    volum::rig::PlanReplace(_VolumSnapshotSoundingRig(), VolumMakeLibraryItemRef(kind, id, displayName), labels);
  return mVolumPendingRigRepair.confirmBody;
}

void NeuralAmpModeler::_VolumApplyPendingRigRepair()
{
  const volum::rig::RigRepairPlan plan = mVolumPendingRigRepair;
  mVolumPendingRigRepair = volum::rig::RigRepairPlan{};
  _VolumApplyRigRepair(plan);
}

void NeuralAmpModeler::_VolumApplyRigRepair(const volum::rig::RigRepairPlan& plan)
{
  using volum::rig::RigRepair;
  if (!plan.TouchesSoundingRig())
    return;

  // Order matters. The PRE slots and the IR belong to the scene of the lane that
  // is about to change, so they are cleared while that lane is still the current
  // one; the MAIN revert comes last because it snapshots the outgoing scene.

  if (plan.Has(RigRepair::ClearPreSlot1))
  {
    if (GetParam(kPreNam1Active)->Bool())
      GetParam(kPreNam1Active)->Set(0.0);
    SendParameterValueFromDelegate(kPreNam1Active, 0.0, true);
    _VolumSetPreNamCapture(0, 0); // EMPTY: drops the live PRE model
  }
  if (plan.Has(RigRepair::ClearPreSlot2))
  {
    if (GetParam(kPreNam2Active)->Bool())
      GetParam(kPreNam2Active)->Set(0.0);
    SendParameterValueFromDelegate(kPreNam2Active, 0.0, true);
    _VolumSetPreNamCapture(1, 0);
  }
  // Reload keeps the slot and re-reads the file behind the same capture index.
  if (plan.Has(RigRepair::ReloadPreSlot1))
    _VolumRequestPreNamLoad(0);
  if (plan.Has(RigRepair::ReloadPreSlot2))
    _VolumRequestPreNamLoad(1);

  // deferToCabSwap: the convolver keeps running until the baked-cab capture is
  // staged, so the lane moves from one coherent sound to the next instead of
  // exposing a burst of raw, cab-less amp (see VoLumDspStaging.h).
  if (plan.Has(RigRepair::ClearMainIr))
    _VolumClearIR(false, true);
  if (plan.Has(RigRepair::ClearSupportIr))
    _VolumClearIR(true, true);
  if (plan.Has(RigRepair::ReloadMainIr))
    _VolumApplyActiveIr(plan.after.activeIrId, false);
  if (plan.Has(RigRepair::ReloadSupportIr))
    _VolumApplyActiveIr(plan.after.supportActiveIrId, true);

  if (plan.Has(RigRepair::DropSupportLane))
  {
    _VolumSetSupportAmp(-1); // "(none)": clears the custom partner too
    if (GetParam(kDualAmpActive)->Bool())
    {
      GetParam(kDualAmpActive)->Set(0.0);
      SendParameterValueFromDelegate(kDualAmpActive, 0.0, true);
      OnParamChange(kDualAmpActive);
    }
    _VolumClampSupportFocus();
  }
  else if (plan.Has(RigRepair::ReloadSupportCapture))
  {
    mVolumSupportNeedsLoad.store(true);
  }

  if (plan.Has(RigRepair::RevertMainToFactoryAmp))
  {
    // The factory amp already in the sidebar, as if clicked: this instance's last
    // knobs on that amp, not shipped defaults - and without folding the deleted
    // amp's knobs into that factory slot on the way past.
    _VolumSelectFactoryAmp(mVolumAmpIdx, /*snapshotOutgoing=*/false);
  }
  else if (plan.Has(RigRepair::ReloadMainCapture))
  {
    mVolumNeedsLoad.store(true);
  }

  // Last: the name. A recalled preset whose row is gone must stop claiming the
  // bar, but the sound it recalled stays exactly as it is.
  if (plan.Has(RigRepair::ForgetPresetName))
  {
    _VolumForgetActivePreset();
    _VolumRefreshPresetBar();
  }

  _VolumMarkPresetDirty();
}

void NeuralAmpModeler::_VolumRepairRigForMissingContent()
{
  // A sibling instance deleted content this one is still playing. Nothing rewrote
  // our rig at the time - by design, the RAM copy is allowed to keep sounding - so
  // reconcile now, at the first moment we need the id again.
  const auto rig = _VolumSnapshotSoundingRig();
  volum::rig::RigRepairPlan plan;
  plan.after = rig;

  const auto& reg = volum::content::GlobalContentStore().reg();
  auto ampGone = [&reg](const std::string& id) {
    if (id.empty())
      return false;
    for (const auto& a : reg.amps)
      if (a.id == id)
        return false;
    return true;
  };
  auto irGone = [&reg](const std::string& id) {
    if (id.empty())
      return false;
    for (const auto& ir : reg.irs)
      if (ir.id == id)
        return false;
    return true;
  };
  auto pedalGone = [&reg](int capture) {
    if (capture < volum::content::kCustomPedalIndexBase)
      return false; // factory capture, always there
    for (const auto& p : reg.pedals)
      if (p.legacyIndex == capture)
        return false;
    return true;
  };

  using volum::rig::RigRepair;
  if (volum::rig::SiblingMustRepairOnNextNeed(!ampGone(rig.mainCustomAmpId)) && !rig.mainCustomAmpId.empty())
  {
    plan.repairs.push_back(RigRepair::RevertMainToFactoryAmp);
    plan.after.mainCustomAmpId.clear();
  }
  if (rig.dualAmpActive && ampGone(rig.supportCustomAmpId))
  {
    plan.repairs.push_back(RigRepair::DropSupportLane);
    plan.after.dualAmpActive = false;
    plan.after.supportCustomAmpId.clear();
  }
  if (irGone(rig.activeIrId))
  {
    plan.repairs.push_back(RigRepair::ClearMainIr);
    plan.after.activeIrId.clear();
  }
  if (rig.dualAmpActive && irGone(rig.supportActiveIrId))
  {
    plan.repairs.push_back(RigRepair::ClearSupportIr);
    plan.after.supportActiveIrId.clear();
  }
  if (pedalGone(rig.preCapture[0]))
  {
    plan.repairs.push_back(RigRepair::ClearPreSlot1);
    plan.after.preCapture[0] = 0;
  }
  if (pedalGone(rig.preCapture[1]))
  {
    plan.repairs.push_back(RigRepair::ClearPreSlot2);
    plan.after.preCapture[1] = 0;
  }
  _VolumApplyRigRepair(plan);
}
