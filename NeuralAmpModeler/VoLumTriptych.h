#pragma once

#include "VoLumColorHelpers.h"
#include "VoLumTriptychLayout.h"
#include "VoLumTriptychState.h"
#include "VoLumTriptychMotifs.h"
#include "VoLumTriptychMenus.h"
#include "VoLumPedalCardControl.h"
#include "NeuralAmpModeler.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <cstddef>
#include <string>
#include <vector>

using namespace iplug;
using namespace igraphics;

//==============================================================================
// Reverb & Delay Extension Controls (PRE / AMP / POST)
//==============================================================================

// Per-effect "motif" drawn in pedal cards and in the Quiet PRE/POST slots is
// declared in VoLumTriptychMotifs.h (included above).

class VoLumChainConnectorControl : public IControl
{
public:
  VoLumChainConnectorControl(const IRECT& bounds)
  : IControl(bounds)
  {
    mIgnoreMouse = true;
  }
  void Draw(IGraphics& g) override
  {
    g.DrawLine(VoLumColors::TEAL.WithOpacity(0.55f), mRECT.L, mRECT.MH(), mRECT.R, mRECT.MH(), nullptr, 1.f);
    g.FillCircle(VoLumColors::TEAL, mRECT.MW(), mRECT.MH(), 2.f);
  }
};

class VoLumTriptychControl : public IControl
{
public:
  using StateCallback = std::function<void(EVoLumSection, EVoLumEffectFocus)>;

  struct QuietSlot
  {
    EVoLumEffectFocus focus;
    const char* label;
    int paramIdx;
  };

  VoLumTriptychControl(const IRECT& bounds, StateCallback cb)
  : IControl(bounds)
  , mCallback(std::move(cb))
  {
    mIgnoreMouse = false;
  }

  void Draw(IGraphics& g) override
  {
    const EVoLumSection displaySection = mExpandedSection;
    const auto frames = volum::triptych_layout::ComputeFrames(volum::triptych_layout::FromRect(mRECT), displaySection);
    const IRECT preRect = frames.pre.As<IRECT>();
    const IRECT ampRect = frames.amp.As<IRECT>();
    const IRECT postRect = frames.post.As<IRECT>();

    mPreRect = preRect;
    mAmpRect = ampRect;
    mPostRect = postRect;

    // Reset hit-zone tracking before re-populating during slot draw.
    mSlotNavRects.clear();
    mSlotToggleRects.clear();
    mSlotParams.clear();
    mSlotFocuses.clear();
    mSlotSection.clear();
    mPreHeaderRect = IRECT();
    mPostHeaderRect = IRECT();
    mPreLockRect = IRECT();
    mPostLockRect = IRECT();
    mPreStoreRect = IRECT();
    mPostStoreRect = IRECT();
    mAmpBlockRect = IRECT();

    if (displaySection == EVoLumSection::PRE)
      _DrawExpandedFrame(g, preRect, "PRE");
    else
      _DrawQuietBlock(g, preRect, EVoLumSection::PRE);

    if (displaySection == EVoLumSection::AMP)
    {
      // VoLumHeroImageControl draws the AMP frame on top.
    }
    else
    {
      _DrawAmpStrip(g, ampRect);
    }

    if (displaySection == EVoLumSection::POST)
      _DrawExpandedFrame(g, postRect, "POST");
    else
      _DrawQuietBlock(g, postRect, EVoLumSection::POST);
  }

  void SetState(bool preActive, bool postActive, int ampIdx, const char* ampName, const char* preNam1Label = "NAM 1",
                const char* preNam2Label = "NAM 2")
  {
    (void)preActive;
    (void)postActive;
    mAmpIdx = ampIdx;
    if (mAmpName != ampName)
    {
      mAmpName = ampName;
      // Invalidate the spine font-size cache so the next redraw re-measures
      // for the new name.
      mCachedSpineSize = 0.f;
      mCachedSpineMaxLen = -1.f;
      mCachedSpineName.clear();
    }
    mPreNam1Label = (preNam1Label && preNam1Label[0] != '\0') ? preNam1Label : "NAM 1";
    mPreNam2Label = (preNam2Label && preNam2Label[0] != '\0') ? preNam2Label : "NAM 2";
    SetDirty(false);
  }

  void SetExpandedSection(EVoLumSection s)
  {
    mExpandedSection = s;
    SetDirty(false);
  }

  IRECT GetPostExpandedRect() const { return mPostRect; }
  IRECT GetPreExpandedRect() const { return mPreRect; }

private:
  static constexpr QuietSlot kPreSlots[4] = {
    {EVoLumEffectFocus::PITCH, "PITCH", kPrePitchActive},
    {EVoLumEffectFocus::COMP, "COMP", kPreCompActive},
    {EVoLumEffectFocus::PRE_NAM1, "NAM 1", kPreNam1Active},
    {EVoLumEffectFocus::PRE_NAM2, "NAM 2", kPreNam2Active},
  };
  static constexpr QuietSlot kPostSlots[4] = {
    {EVoLumEffectFocus::CHORUS, "CHORUS", kChorusActive},
    {EVoLumEffectFocus::DELAY, "DELAY", kDelayActive},
    {EVoLumEffectFocus::REVERB, "REVRB", kReverbActive},
    {EVoLumEffectFocus::TREMOLO, "TREM", kTremoloActive},
  };

  bool _IsPreLocked() const
  {
    auto* del = const_cast<VoLumTriptychControl*>(this)->GetDelegate();
    if (auto* plugin = dynamic_cast<PLUG_CLASS_NAME*>(del))
      return plugin->_VolumIsPreLocked();
    return false;
  }

  bool _IsPostLocked() const
  {
    auto* del = const_cast<VoLumTriptychControl*>(this)->GetDelegate();
    if (auto* plugin = dynamic_cast<PLUG_CLASS_NAME*>(del))
      return plugin->_VolumIsPostLocked();
    return false;
  }

  bool _IsPreDirty() const
  {
    auto* del = const_cast<VoLumTriptychControl*>(this)->GetDelegate();
    if (auto* plugin = dynamic_cast<PLUG_CLASS_NAME*>(del))
      return plugin->_VolumIsPreDirty();
    return false;
  }

  bool _IsPostDirty() const
  {
    auto* del = const_cast<VoLumTriptychControl*>(this)->GetDelegate();
    if (auto* plugin = dynamic_cast<PLUG_CLASS_NAME*>(del))
      return plugin->_VolumIsPostDirty();
    return false;
  }

  static bool _HitContains(const IRECT& r, float x, float y, float pad = 2.f)
  {
    return r.W() > 0.f && r.GetPadded(pad).Contains(x, y);
  }

  float _MeasureHeaderLabelWidth(IGraphics& g, const char* label, bool locked) const
  {
    IText labelStyle(
      10.f, locked ? VoLumColors::GOLD : VoLumColors::GOLD_DIM, "Josefin-Bold", EAlign::Center, EVAlign::Middle);
    IRECT measured;
    g.MeasureText(labelStyle, label, measured);
    return std::max(measured.W(), 18.f);
  }

  void _DrawLockIcon(IGraphics& g, const IRECT& r, bool locked, bool hovered)
  {
    const float cx = r.MW();
    const float cy = r.MH();
    // Keep the unlocked rest state opaque so shackle/body overlaps do not double-composite into speckles.
    const IColor unlockedRest(255, 118, 110, 100);
    const IColor col = locked ? (hovered ? VoLumColors::GOLD : VoLumColors::GOLD.WithOpacity(0.72f))
                              : (hovered ? VoLumColors::TEXT_BRIGHT : unlockedRest);

    // Small padlock silhouette: compact body plus an explicit shackle path.
    const float bodyW = r.W() * 0.58f;
    const float bodyH = r.H() * 0.38f;
    const float stroke = std::max(2.6f, r.W() * 0.13f);
    const float shackleRise = r.H() * 0.34f;
    const float totalH = bodyH + shackleRise;
    const float bboxTop = cy - totalH * 0.5f;
    const float bodyTop = bboxTop + shackleRise;
    const float bodyBottom = bodyTop + bodyH;
    const float bodyL = cx - bodyW * 0.5f;
    const float bodyR = cx + bodyW * 0.5f;
    const IRECT body(bodyL, bodyTop, bodyR, bodyBottom);

    g.FillRoundRect(col, body, std::min(2.f, bodyW * 0.12f));

    IStrokeOptions shackleStroke;
    shackleStroke.mCapOption = ELineCap::Round;
    shackleStroke.mJoinOption = ELineJoin::Round;

    if (locked)
    {
      const float footInset = bodyW * 0.18f;
      const float leftFoot = bodyL + footInset;
      const float rightFoot = bodyR - footInset;
      const float topY = bodyTop - shackleRise * 0.98f;
      const float ctrlY = bodyTop - shackleRise * 1.12f;

      g.PathClear();
      g.PathMoveTo(leftFoot, bodyTop + 0.4f);
      g.PathLineTo(leftFoot, bodyTop - shackleRise * 0.40f);
      g.PathCubicBezierTo(leftFoot, ctrlY, rightFoot, ctrlY, rightFoot, bodyTop - shackleRise * 0.40f);
      g.PathLineTo(rightFoot, bodyTop + 0.4f);
      g.PathStroke(col, stroke, shackleStroke, nullptr);
    }
    else
    {
      // Right-hinged open shackle: attached to the body, then swinging up-left.
      const float hingeX = bodyR - bodyW * 0.18f;
      const float hingeY = bodyTop + 0.4f;
      const float topY = bodyTop - shackleRise * 1.16f;
      const float looseX = bodyL + bodyW * 0.10f;
      const float looseY = bodyTop - shackleRise * 0.54f;

      g.PathClear();
      g.PathMoveTo(hingeX, hingeY);
      g.PathLineTo(hingeX, bodyTop - shackleRise * 0.45f);
      g.PathCubicBezierTo(hingeX - bodyW * 0.04f, topY, looseX + bodyW * 0.34f, topY, looseX, looseY);
      g.PathStroke(col, stroke, shackleStroke, nullptr);
    }
  }

  void _DrawStoreToAmpIcon(IGraphics& g, const IRECT& r, bool hovered)
  {
    const float cx = r.MW();
    const float cy = r.MH();
    if (hovered)
      g.FillCircle(VoLumColors::TEAL.WithOpacity(0.18f), cx, cy, r.W() * 0.46f);

    const IColor col = hovered ? VoLumColors::TEAL : VoLumColors::TEAL_DIM.WithOpacity(0.48f);

    // Store-to-amp arrow: down arrow only, no tray/download bar.
    IStrokeOptions arrowStroke;
    arrowStroke.mCapOption = ELineCap::Round;
    arrowStroke.mJoinOption = ELineJoin::Round;
    const float stroke = hovered ? 2.7f : 2.25f;
    const float stemTop = cy - r.H() * 0.30f;
    const float tipY = cy + r.H() * 0.28f;
    const float wingY = cy - r.H() * 0.02f;
    const float wingX = r.W() * 0.23f;

    g.PathClear();
    g.PathMoveTo(cx, stemTop);
    g.PathLineTo(cx, tipY);
    g.PathMoveTo(cx - wingX, wingY);
    g.PathLineTo(cx, tipY);
    g.PathLineTo(cx + wingX, wingY);
    g.PathStroke(col, stroke, arrowStroke, nullptr);
  }

  void _DrawChevron(IGraphics& g, const IRECT& r, const IColor& col)
  {
    const float cxC = r.MW();
    const float cyC = r.MH();
    const float chs = 3.5f;
    g.DrawLine(col, cxC, cyC - chs, cxC + chs, cyC, nullptr, 1.2f);
    g.DrawLine(col, cxC + chs, cyC, cxC, cyC + chs, nullptr, 1.2f);
  }

  void _LayoutHeaderControls(IGraphics& g, const IRECT& header, const char* label, bool leftAlignedLabel,
                             bool showChevron, bool locked, bool dirty, bool isPre, IRECT& outStore, IRECT& outLock,
                             IRECT& outChevron, float& outHeaderClickRight)
  {
    const float iconSize = 16.f;
    const float iconGap = 2.f;
    const float storeW = (locked && dirty) ? iconSize : 0.f;
    const float lockW = iconSize;
    const float chevronW = showChevron ? 7.f : 0.f;
    const float rowCY = header.MH();
    // All-caps label reads slightly above geometric midline; nudge icons up 1px to match.
    const float iconY = rowCY - iconSize * 0.5f - 1.f;

    const float labelW = _MeasureHeaderLabelWidth(g, label, locked);
    const float labelPad = leftAlignedLabel ? 2.f : 0.f;
    outChevron = IRECT();
    if (showChevron)
      outChevron = IRECT(header.R - chevronW, rowCY - header.H() * 0.5f, header.R, rowCY + header.H() * 0.5f);

    const float groupW = lockW + ((storeW > 0.f) ? (iconGap + storeW) : 0.f);
    const float groupRight = showChevron ? outChevron.L - 3.f : header.R - 2.f;
    float x = groupRight - groupW;

    outStore = IRECT();
    if (!isPre && storeW > 0.f)
    {
      outStore = IRECT(x, iconY, x + storeW, iconY + iconSize);
      x += storeW + iconGap;
    }

    outLock = IRECT(x, iconY, x + lockW, iconY + iconSize);
    x += lockW + iconGap;

    if (isPre && storeW > 0.f)
      outStore = IRECT(x, iconY, x + storeW, iconY + iconSize);

    const float labelRight = leftAlignedLabel ? header.L + labelPad + labelW : header.MW() + labelW * 0.5f;
    outHeaderClickRight = std::min(groupRight - groupW - 2.f, labelRight + 2.f);
    if (isPre)
    {
      mPreStoreRect = outStore;
      mPreLockRect = outLock;
    }
    else
    {
      mPostStoreRect = outStore;
      mPostLockRect = outLock;
    }
  }

  void _UpdateHeaderTooltip(bool preStoreHov, bool postStoreHov, bool preLockHov, bool postLockHov)
  {
    mHeaderTooltip.clear();
    if (preStoreHov)
      mHeaderTooltip = "Store PRE to " + mAmpName;
    else if (postStoreHov)
      mHeaderTooltip = "Store POST to " + mAmpName;
    else if (preLockHov)
      mHeaderTooltip =
        _IsPreLocked() ? "Unlock PRE (restore this amp's saved scene)" : "Lock PRE (carry scene across amp switches)";
    else if (postLockHov)
      mHeaderTooltip = _IsPostLocked() ? "Unlock POST (restore this amp's saved scene)"
                                       : "Lock POST (carry scene across amp switches)";
    SetTooltip(mHeaderTooltip.c_str());
  }

  void _DrawExpandedFrame(IGraphics& g, const IRECT& r, const char* label)
  {
    DrawPanelDepth(g, r);
    const bool isLocked = (strcmp(label, "PRE") == 0) ? _IsPreLocked() : _IsPostLocked();
    g.DrawRect(
      isLocked ? VoLumColors::GOLD.WithOpacity(0.45f) : VoLumColors::FRAME, r, nullptr, isLocked ? 1.2f : 1.0f);
    const float cs = 8.f;
    DrawCornerAccent(g, r.L + 4.f, r.T + 4.f, cs, false, false,
                     isLocked ? VoLumColors::GOLD.WithOpacity(0.6f) : VoLumColors::TEAL_DIM);
    DrawCornerAccent(
      g, r.R - 4.f, r.T + 4.f, cs, true, false, isLocked ? VoLumColors::GOLD.WithOpacity(0.6f) : VoLumColors::TEAL_DIM);
    DrawCornerAccent(
      g, r.L + 4.f, r.B - 4.f, cs, false, true, isLocked ? VoLumColors::GOLD.WithOpacity(0.6f) : VoLumColors::TEAL_DIM);
    DrawCornerAccent(
      g, r.R - 4.f, r.B - 4.f, cs, true, true, isLocked ? VoLumColors::GOLD.WithOpacity(0.6f) : VoLumColors::TEAL_DIM);
    IText txt(
      10.f, isLocked ? VoLumColors::GOLD : VoLumColors::GOLD_DIM, "Josefin-Bold", EAlign::Near, EVAlign::Middle);
    g.DrawText(txt, label, IRECT(r.L + 10.f, r.T + 4.f, r.L + 80.f, r.T + 22.f));

    const bool isPre = (strcmp(label, "PRE") == 0);
    const bool dirty = isPre ? _IsPreDirty() : _IsPostDirty();
    const IRECT header(r.L + 4.f, r.T + 2.f, r.R - 4.f, r.T + 24.f);
    IRECT storeRect;
    IRECT lockRect;
    IRECT chevronRect;
    float headerClickRight = header.R;
    _LayoutHeaderControls(
      g, header, label, true, false, isLocked, dirty, isPre, storeRect, lockRect, chevronRect, headerClickRight);
    _DrawLockIcon(g, lockRect, isLocked, isPre ? mPreLockHovered : mPostLockHovered);
    if (storeRect.W() > 0)
      _DrawStoreToAmpIcon(g, storeRect, isPre ? mPreStoreHovered : mPostStoreHovered);
  }

  int _GetParamInt(int paramIdx) const
  {
    if (paramIdx < 0)
      return 0;
    auto* del = const_cast<VoLumTriptychControl*>(this)->GetDelegate();
    if (auto* plugin = dynamic_cast<PLUG_CLASS_NAME*>(del))
      return plugin->GetParam(paramIdx)->Int();
    return 0;
  }

  bool _GetParamBool(int paramIdx) const
  {
    if (paramIdx < 0)
      return false;
    auto* del = const_cast<VoLumTriptychControl*>(this)->GetDelegate();
    if (auto* plugin = dynamic_cast<PLUG_CLASS_NAME*>(del))
      return plugin->GetParam(paramIdx)->Bool();
    return false;
  }

  void _DrawMiniPill(IGraphics& g, const IRECT& r, bool on, bool dimmed)
  {
    // 24x11 horizontal pill. Uses the same gold-track / teal-LED idiom as
    // VoLumPowerSwitchControl + the toggles below the amp panel.
    const float radius = r.H() * 0.5f;
    const IColor track = on ? VoLumColors::GOLD.WithOpacity(dimmed ? 0.18f : 0.35f) : VoLumColors::FRAME;
    const IColor knob = on ? VoLumColors::GOLD : VoLumColors::TEXT_DIM;

    g.FillRoundRect(IColor(255, 8, 10, 14), r, radius);
    g.DrawRoundRect(track, r, radius);

    const float knobR = radius - 1.5f;
    const float knobCY = r.MH();
    const float knobCX = on ? r.R - radius : r.L + radius;
    g.FillEllipse(knob, IRECT(knobCX - knobR, knobCY - knobR, knobCX + knobR, knobCY + knobR));
  }

  void _DrawQuietBlock(IGraphics& g, const IRECT& r, EVoLumSection section)
  {
    // Center an 80W x 140H block vertically inside the strip rect (strip is
    // already 80 wide, so we only adjust height).
    const float blockH = 140.f;
    const float blockTop = r.MH() - blockH / 2.f;
    const IRECT block(r.L, blockTop, r.R, blockTop + blockH);

    const bool isLocked = (section == EVoLumSection::PRE) ? _IsPreLocked() : _IsPostLocked();

    DrawPanelDepth(g, block);
    g.DrawRect(
      isLocked ? VoLumColors::GOLD.WithOpacity(0.45f) : VoLumColors::FRAME, block, nullptr, isLocked ? 1.2f : 1.0f);
    const float cs = 6.f;
    DrawCornerAccent(g, block.L + 3.f, block.T + 3.f, cs, false, false,
                     isLocked ? VoLumColors::GOLD.WithOpacity(0.6f) : VoLumColors::TEAL_DIM);
    DrawCornerAccent(g, block.R - 3.f, block.T + 3.f, cs, true, false,
                     isLocked ? VoLumColors::GOLD.WithOpacity(0.6f) : VoLumColors::TEAL_DIM);
    DrawCornerAccent(g, block.L + 3.f, block.B - 3.f, cs, false, true,
                     isLocked ? VoLumColors::GOLD.WithOpacity(0.6f) : VoLumColors::TEAL_DIM);
    DrawCornerAccent(g, block.R - 3.f, block.B - 3.f, cs, true, true,
                     isLocked ? VoLumColors::GOLD.WithOpacity(0.6f) : VoLumColors::TEAL_DIM);

    // Header: label + chevron + 1px teal underline.
    const float headerH = 22.f;
    const IRECT header(block.L + 4.f, block.T + 2.f, block.R - 4.f, block.T + 2.f + headerH);
    const char* hdrLabel = (section == EVoLumSection::PRE) ? "PRE" : "POST";

    IText hdrText(
      10.f, isLocked ? VoLumColors::GOLD : VoLumColors::GOLD_DIM, "Josefin-Bold", EAlign::Near, EVAlign::Middle);
    const bool dirty = (section == EVoLumSection::PRE) ? _IsPreDirty() : _IsPostDirty();
    const bool isPre = (section == EVoLumSection::PRE);
    IRECT storeRect;
    IRECT lockRect;
    IRECT chevronRect;
    float headerClickRight = header.R;
    _LayoutHeaderControls(
      g, header, hdrLabel, true, true, isLocked, dirty, isPre, storeRect, lockRect, chevronRect, headerClickRight);

    const IRECT hdrTextRect(header.L + 2.f, header.T, headerClickRight, header.B);
    g.DrawText(hdrText, hdrLabel, hdrTextRect);

    const IColor chevronCol = isLocked ? VoLumColors::GOLD : VoLumColors::GOLD_DIM;
    _DrawChevron(g, chevronRect, chevronCol);
    _DrawLockIcon(g, lockRect, isLocked, isPre ? mPreLockHovered : mPostLockHovered);
    if (storeRect.W() > 0)
      _DrawStoreToAmpIcon(g, storeRect, isPre ? mPreStoreHovered : mPostStoreHovered);

    // Hairline under the header.
    const float underlineY = header.B + 1.f;
    g.DrawLine(isLocked ? VoLumColors::GOLD.WithOpacity(0.35f) : VoLumColors::TEAL_DIM.WithOpacity(0.55f),
               block.L + 6.f, underlineY, block.R - 6.f, underlineY, nullptr, 1.f);

    if (section == EVoLumSection::PRE)
      mPreHeaderRect = IRECT(block.L, block.T, headerClickRight, header.B + 2.f);
    else
      mPostHeaderRect = IRECT(block.L, block.T, headerClickRight, header.B + 2.f);

    // Slot iteration.
    std::array<QuietSlot, 4> preSlots = {kPreSlots[0], kPreSlots[1], kPreSlots[2], kPreSlots[3]};
    preSlots[2].label = mPreNam1Label.c_str();
    preSlots[3].label = mPreNam2Label.c_str();
    const QuietSlot* slots = (section == EVoLumSection::PRE) ? preSlots.data() : kPostSlots;
    const int slotCount = 4;
    const float innerTop = underlineY + 2.f;
    const float innerBot = block.B - 4.f;
    // Each section fills its own box; PRE and POST now both carry 4 slots.
    const float slotH = (innerBot - innerTop) / (float)slotCount;

    for (int i = 0; i < slotCount; ++i)
    {
      const IRECT slotR(block.L + 2.f, innerTop + i * slotH, block.R - 2.f, innerTop + (i + 1) * slotH);
      _DrawQuietSlot(g, slotR, slots[i], section, i, i < slotCount - 1);
    }
  }

  void _DrawQuietSlot(IGraphics& g, const IRECT& slotR, const QuietSlot& slot, EVoLumSection section, int slotIdx,
                      bool drawDivider)
  {
    const bool placeholder = (slot.paramIdx < 0);
    const bool active = _GetParamBool(slot.paramIdx);
    const bool bypassed = !active;
    const int globalIdx = (int)mSlotNavRects.size();
    const bool hovered = (mHoveredSlot == globalIdx) && !placeholder;

    if (hovered)
    {
      // Subtle hover lift (~5% brighter background) + faint teal inner edge.
      g.FillRect(IColor(20, 80, 140, 160), slotR);
      g.DrawRect(VoLumColors::TEAL_DIM.WithOpacity(0.35f), slotR.GetPadded(-1.f, -1.f, -1.f, -1.f));
    }

    // Active indicator: 3 px teal edge bar pinned to the left of the slot.
    if (active)
    {
      const IRECT edge(slotR.L + 1.f, slotR.T + 2.f, slotR.L + 4.f, slotR.B - 2.f);
      g.FillRect(VoLumColors::TEAL, edge);
    }

    // Layout inside the slot. Block is 100 wide, slot inner ~96 wide:
    //   leftPad(6) + motif(20) + gap(4) + label(36) + gap(4) + pill(20) + rightPad(6)
    const float leftPad = 6.f;
    const float motifSize = std::min(20.f, slotR.H() - 6.f);
    const float motifL = slotR.L + leftPad;
    const float motifT = slotR.MH() - motifSize / 2.f;
    const IRECT motifR(motifL, motifT, motifL + motifSize, motifT + motifSize);

    // Mini pill on the right edge. Bumped to 22x14 for a friendlier hit target
    // while still leaving label space for "REVERB".
    const float pillW = 22.f;
    const float pillH = 14.f;
    const IRECT pillR(slotR.R - pillW - 6.f, slotR.MH() - pillH / 2.f, slotR.R - 6.f, slotR.MH() + pillH / 2.f);

    // Label region between motif and pill.
    const IRECT labelR(motifR.R + 4.f, slotR.T + 2.f, pillR.L - 4.f, slotR.B - 2.f);

    // Motif drawing is the single most expensive operation in this control
    // (recursive fractal art). Cache it to a layer keyed by (focus, bypass)
    // so hover redraws skip the recursion. The iPlug2 idiom is to recreate
    // the layer whenever CheckLayer reports it is no longer valid (scale or
    // owner-RECT change), and we additionally rebuild on a bypass flip
    // because that changes the colour ramp inside DrawEffectMotif.
    // PITCH slot reflects the current sub-mode: motif + label switch between
    // Transpose (helix / "PITCH") and Octaver (octave chevrons / "OCT").
    const bool isPitchSlot = (slot.focus == EVoLumEffectFocus::PITCH);
    const bool pitchOctaver = isPitchSlot && (_GetParamInt(kPrePitchMode) != volum::kVoLumPitchModeTranspose);
    const int variant = pitchOctaver ? 1 : 0;
    const size_t focusIdx = static_cast<size_t>(slot.focus);
    auto& motifLayer = mSlotMotifLayers[focusIdx];
    auto& cachedBypass = mSlotMotifCachedBypass[focusIdx];
    auto& cachedVariant = mSlotMotifCachedVariant[focusIdx];
    if (!g.CheckLayer(motifLayer) || cachedBypass != bypassed || cachedVariant != variant)
    {
      g.StartLayer(this, motifR);
      DrawEffectMotif(g, motifR, slot.focus, bypassed, variant);
      motifLayer = g.EndLayer();
      cachedBypass = bypassed;
      cachedVariant = variant;
    }
    g.DrawLayer(motifLayer);
    std::string slotLabel = pitchOctaver ? "OCT" : slot.label;
    std::transform(slotLabel.begin(), slotLabel.end(), slotLabel.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    IText labelText(
      10.f, bypassed ? VoLumColors::CREAM_DIM : VoLumColors::CREAM, "Josefin-Bold", EAlign::Near, EVAlign::Middle);
    // Clip to the label region so a long capture name (e.g. a custom PRE-NAM
    // pedal) can never bleed into the pill to its right. The label is already
    // truncated upstream; this is a belt-and-suspenders guard.
    g.PathClipRegion(labelR);
    g.DrawText(labelText, slotLabel.c_str(), labelR);
    g.PathClipRegion();

    if (placeholder)
    {
      // SOON badge instead of a toggle pill for the no-DSP scaffold tile.
      IText soonTxt(8.f, VoLumColors::AMBER.WithOpacity(0.85f), "Josefin-Bold", EAlign::Far, EVAlign::Middle);
      const IRECT soonR(pillR.L - 8.f, pillR.T, pillR.R, pillR.B);
      g.DrawText(soonTxt, "SOON", soonR);
    }
    else
    {
      _DrawMiniPill(g, pillR, active, bypassed && !active);
    }

    // Hairline divider between slots (skip after the last).
    if (drawDivider)
    {
      g.DrawLine(VoLumColors::FRAME.WithOpacity(0.5f), slotR.L + 6.f, slotR.B, slotR.R - 6.f, slotR.B, nullptr, 1.f);
    }

    // Track interactive zones. Toggle hit-rect is padded a few px beyond the
    // visible pill for forgiving hit-testing; nav zone covers everything to
    // the left of that toggle hit-rect. Placeholder slots get no toggle (empty
    // hit-rect) and a full-width nav zone that just expands the section.
    const IRECT toggleHitR = placeholder ? IRECT() : pillR.GetPadded(4.f, 3.f, 4.f, 3.f);
    IRECT navR = slotR;
    if (!placeholder)
      navR.R = toggleHitR.L - 1.f;
    mSlotNavRects.push_back(navR);
    mSlotToggleRects.push_back(toggleHitR);
    mSlotParams.push_back(slot.paramIdx);
    mSlotFocuses.push_back(slot.focus);
    mSlotSection.push_back(section);
    (void)slotIdx;
  }

  void _DrawAmpStrip(IGraphics& g, const IRECT& r)
  {
    // Quiet block in the same frame language as the PRE/POST collapsed blocks
    // but taller (180 H vs 140 H) so the AMP reads as the centerpiece of the
    // row and the rotated amp name has room to breathe. No thumbnail motif -
    // the spine is the whole point. Click anywhere in the strip navigates to
    // the AMP-expanded view (handled by OnMouseDown via mAmpRect).
    const float blockH = 180.f;
    const float blockTop = r.MH() - blockH / 2.f;
    const IRECT block(r.L, blockTop, r.R, blockTop + blockH);
    mAmpBlockRect = block;

    DrawPanelDepth(g, block);
    if (mAmpHovered)
    {
      g.FillRect(IColor(20, 80, 140, 160), block);
      g.DrawRect(VoLumColors::TEAL_DIM.WithOpacity(0.55f), block.GetPadded(-1.f, -1.f, -1.f, -1.f));
    }
    g.DrawRect(VoLumColors::FRAME, block);
    const float cs = 6.f;
    DrawCornerAccent(g, block.L + 3.f, block.T + 3.f, cs, false, false, VoLumColors::TEAL_DIM);
    DrawCornerAccent(g, block.R - 3.f, block.T + 3.f, cs, true, false, VoLumColors::TEAL_DIM);
    DrawCornerAccent(g, block.L + 3.f, block.B - 3.f, cs, false, true, VoLumColors::TEAL_DIM);
    DrawCornerAccent(g, block.R - 3.f, block.B - 3.f, cs, true, true, VoLumColors::TEAL_DIM);

    // Header: "AMP" + chevron + 1px teal hairline (mirrors PRE/POST exactly).
    const float headerH = 22.f;
    const IRECT header(block.L + 4.f, block.T + 2.f, block.R - 4.f, block.T + 2.f + headerH);
    IText hdrText(10.f, VoLumColors::GOLD_DIM, "Josefin-Bold", EAlign::Center, EVAlign::Middle);
    const float chevronW = 7.f;
    const IRECT hdrTextRect(header.L, header.T, header.R - chevronW - 2.f, header.B);
    g.DrawText(hdrText, "AMP", hdrTextRect);

    const float cxC = header.R - chevronW;
    const float cyC = header.MH();
    const float chs = 3.5f;
    const IColor chevronCol = VoLumColors::GOLD_DIM;
    g.DrawLine(chevronCol, cxC, cyC - chs, cxC + chs, cyC, nullptr, 1.2f);
    g.DrawLine(chevronCol, cxC + chs, cyC, cxC, cyC + chs, nullptr, 1.2f);

    const float underlineY = header.B + 1.f;
    g.DrawLine(
      VoLumColors::TEAL_DIM.WithOpacity(0.55f), block.L + 6.f, underlineY, block.R - 6.f, underlineY, nullptr, 1.f);

    // Rotated spine fills the rest of the block. With ~150 px of vertical
    // budget the auto-shrink picks ~14pt for "Diezel Herbert Mk1" and ~16pt
    // for short names like "AMP" or "Marshall JMP", floor at 8pt.
    const IRECT spineR(block.L + 4.f, underlineY + 4.f, block.R - 4.f, block.B - 6.f);
    const char* name = mAmpName.empty() ? "AMP" : mAmpName.c_str();
    const float maxLen = spineR.H() - 2.f;
    const float chosenSize = _ResolveSpineFontSize(g, name, maxLen);
    IText spineText(chosenSize, VoLumColors::CREAM, "Josefin-Bold", EAlign::Center, EVAlign::Middle);
    spineText.mAngle = -90.f; // bottom-to-top: head tilts left to read.

    // Drawn directly (no layer cache). Wrapping rotated DrawText in
    // StartLayer/EndLayer/DrawLayer caused intermittently-empty spine
    // bitmaps on some hover transitions; the rotated text is a single
    // glyph-run draw which is cheap enough not to need caching, especially
    // since the auto-shrink size is already cached separately.
    g.DrawText(spineText, name, spineR);
  }

  // Picks the largest font size from a fixed descending table whose rendered
  // width fits in maxLen (the rotated text's vertical extent). MeasureText
  // hits GDI/DirectWrite on Windows so cache the answer keyed by amp name +
  // spine height; the cache is invalidated in SetState when the amp name
  // changes and is also implicitly refreshed if the layout changes maxLen.
  float _ResolveSpineFontSize(IGraphics& g, const char* name, float maxLen)
  {
    if (mCachedSpineSize > 0.f && mCachedSpineMaxLen == maxLen && mCachedSpineName == name)
      return mCachedSpineSize;

    static const float kSpineSizes[] = {16.f, 14.f, 12.f, 11.f, 10.f, 9.f, 8.f};
    float chosenSize = 8.f;
    for (float s : kSpineSizes)
    {
      IText probe(s, VoLumColors::CREAM, "Josefin-Bold");
      IRECT measured;
      g.MeasureText(probe, name, measured);
      if (measured.W() <= maxLen)
      {
        chosenSize = s;
        break;
      }
    }

    mCachedSpineName = name;
    mCachedSpineMaxLen = maxLen;
    mCachedSpineSize = chosenSize;
    return chosenSize;
  }


  EVoLumEffectFocus _FirstActiveOrFirst(EVoLumSection section) const
  {
    const QuietSlot* slots = (section == EVoLumSection::PRE) ? kPreSlots : kPostSlots;
    const int count = 4;
    for (int i = 0; i < count; ++i)
      if (_GetParamBool(slots[i].paramIdx))
        return slots[i].focus;
    return slots[0].focus;
  }

  void _ToggleParam(int paramIdx)
  {
    if (paramIdx < 0)
      return;
    auto* del = GetDelegate();
    auto* plugin = dynamic_cast<PLUG_CLASS_NAME*>(del);
    if (!plugin)
      return;
    auto* param = plugin->GetParam(paramIdx);
    if (!param)
      return;
    const double cur = param->Value();
    const double next = (cur > 0.5) ? 0.0 : 1.0;
    del->BeginInformHostOfParamChangeFromUI(paramIdx);
    del->SendParameterValueFromUI(paramIdx, next);
    del->EndInformHostOfParamChangeFromUI(paramIdx);

    // Mirror the value to all peer controls bound to this param so their
    // cached value stays in sync. SendParameterValueFromUI pushes to the
    // host/audio side but does NOT refresh other GUI controls; the standard
    // IControl::SetDirty(true) path calls UpdatePeers internally to do that,
    // and we mimic the same peer-refresh here because VoLumTriptychControl
    // is not itself linked to the param. Without this, e.g. the on/off
    // switch in the expanded POST view's knob row keeps its stale cached
    // value after a mini-pill toggle from the AMP-view.
    if (auto* gfx = GetUI())
    {
      const double normalized = param->ToNormalized(next);
      gfx->ForControlWithParam(paramIdx, [normalized, paramIdx](IControl* pControl) {
        const int nVals = pControl->NVals();
        for (int v = 0; v < nVals; ++v)
        {
          if (pControl->GetParamIdx(v) == paramIdx)
            pControl->SetValueFromDelegate(normalized, v);
        }
      });
    }

    SetDirty(false);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    (void)mod;

    // 1) Toggle pills first (highest precedence so users can flip bypass
    // without leaving the AMP view).
    for (size_t i = 0; i < mSlotToggleRects.size(); ++i)
    {
      if (mSlotToggleRects[i].Contains(x, y))
      {
        _ToggleParam(mSlotParams[i]);
        return;
      }
    }

    // 2) Slot navigation zones: jump to that section and focus the slot.
    for (size_t i = 0; i < mSlotNavRects.size(); ++i)
    {
      if (mSlotNavRects[i].Contains(x, y))
      {
        const EVoLumSection sec = mSlotSection[i];
        const EVoLumEffectFocus focus = mSlotFocuses[i];
        mExpandedSection = sec;
        if (mCallback)
          mCallback(sec, focus);
        SetDirty(false);
        return;
      }
    }

    // 3) PRE/POST store (locked + dirty) then lock toggle (before header expand).
    if (mPreStoreRect.W() > 0 && _HitContains(mPreStoreRect, x, y))
    {
      if (auto* plugin = dynamic_cast<PLUG_CLASS_NAME*>(GetDelegate()))
        plugin->_VolumStorePreToCurrentAmp();
      SetDirty(false);
      return;
    }
    if (mPostStoreRect.W() > 0 && _HitContains(mPostStoreRect, x, y))
    {
      if (auto* plugin = dynamic_cast<PLUG_CLASS_NAME*>(GetDelegate()))
        plugin->_VolumStorePostToCurrentAmp();
      SetDirty(false);
      return;
    }
    if (mPreLockRect.W() > 0 && _HitContains(mPreLockRect, x, y))
    {
      if (auto* plugin = dynamic_cast<PLUG_CLASS_NAME*>(GetDelegate()))
        plugin->_VolumSetPreLocked(!plugin->_VolumIsPreLocked());
      SetDirty(false);
      return;
    }
    if (mPostLockRect.W() > 0 && _HitContains(mPostLockRect, x, y))
    {
      if (auto* plugin = dynamic_cast<PLUG_CLASS_NAME*>(GetDelegate()))
        plugin->_VolumSetPostLocked(!plugin->_VolumIsPostLocked());
      SetDirty(false);
      return;
    }

    // 4) Header click: navigate, focus first active or first slot.
    if (mPreHeaderRect.W() > 0 && mPreHeaderRect.Contains(x, y))
    {
      mExpandedSection = EVoLumSection::PRE;
      if (mCallback)
        mCallback(EVoLumSection::PRE, _FirstActiveOrFirst(EVoLumSection::PRE));
      SetDirty(false);
      return;
    }
    if (mPostHeaderRect.W() > 0 && mPostHeaderRect.Contains(x, y))
    {
      mExpandedSection = EVoLumSection::POST;
      if (mCallback)
        mCallback(EVoLumSection::POST, _FirstActiveOrFirst(EVoLumSection::POST));
      SetDirty(false);
      return;
    }

    // 5) Whole-block fallback (covers gaps between rects, e.g. block frame
    // padding). Behaviour matches a header click on that block.
    if (mPreRect.Contains(x, y) && mExpandedSection != EVoLumSection::PRE)
    {
      mExpandedSection = EVoLumSection::PRE;
      if (mCallback)
        mCallback(EVoLumSection::PRE, _FirstActiveOrFirst(EVoLumSection::PRE));
      SetDirty(false);
      return;
    }
    if (mAmpRect.Contains(x, y) && mExpandedSection != EVoLumSection::AMP)
    {
      mExpandedSection = EVoLumSection::AMP;
      if (mCallback)
        mCallback(EVoLumSection::AMP, EVoLumEffectFocus::AMP);
      SetDirty(false);
      return;
    }
    if (mPostRect.Contains(x, y) && mExpandedSection != EVoLumSection::POST)
    {
      mExpandedSection = EVoLumSection::POST;
      if (mCallback)
        mCallback(EVoLumSection::POST, _FirstActiveOrFirst(EVoLumSection::POST));
      SetDirty(false);
      return;
    }
  }

  void OnMouseOver(float x, float y, const IMouseMod& mod) override
  {
    (void)mod;
    int next = -1;
    for (size_t i = 0; i < mSlotNavRects.size(); ++i)
    {
      if (mSlotNavRects[i].Contains(x, y))
      {
        next = (int)i;
        break;
      }
    }
    // AMP strip hover only matters when AMP is collapsed (i.e. PRE or POST is
    // expanded), so a subtle hover invites a click back to AMP view. Hover is
    // gated on the visible block rect so the empty whitespace around it does
    // not light up.
    const bool ampHov =
      (mExpandedSection != EVoLumSection::AMP) && mAmpBlockRect.W() > 0 && mAmpBlockRect.Contains(x, y);
    const bool preStoreHov = _HitContains(mPreStoreRect, x, y);
    const bool postStoreHov = _HitContains(mPostStoreRect, x, y);
    const bool preLockHov = _HitContains(mPreLockRect, x, y);
    const bool postLockHov = _HitContains(mPostLockRect, x, y);
    bool dirty = false;
    if (next != mHoveredSlot)
    {
      mHoveredSlot = next;
      dirty = true;
    }
    if (ampHov != mAmpHovered)
    {
      mAmpHovered = ampHov;
      dirty = true;
    }
    if (preStoreHov != mPreStoreHovered)
    {
      mPreStoreHovered = preStoreHov;
      dirty = true;
    }
    if (postStoreHov != mPostStoreHovered)
    {
      mPostStoreHovered = postStoreHov;
      dirty = true;
    }
    if (preLockHov != mPreLockHovered)
    {
      mPreLockHovered = preLockHov;
      dirty = true;
    }
    if (postLockHov != mPostLockHovered)
    {
      mPostLockHovered = postLockHov;
      dirty = true;
    }

    _UpdateHeaderTooltip(preStoreHov, postStoreHov, preLockHov, postLockHov);

    if (dirty)
      SetDirty(false);
  }

  void OnMouseOut() override
  {
    bool dirty = false;
    if (mHoveredSlot != -1)
    {
      mHoveredSlot = -1;
      dirty = true;
    }
    if (mAmpHovered)
    {
      mAmpHovered = false;
      dirty = true;
    }
    if (mPreLockHovered)
    {
      mPreLockHovered = false;
      dirty = true;
    }
    if (mPostLockHovered)
    {
      mPostLockHovered = false;
      dirty = true;
    }
    if (mPreStoreHovered)
    {
      mPreStoreHovered = false;
      dirty = true;
    }
    if (mPostStoreHovered)
    {
      mPostStoreHovered = false;
      dirty = true;
    }
    SetTooltip("");
    if (dirty)
      SetDirty(false);
  }

  StateCallback mCallback;
  int mAmpIdx = 0;
  std::string mAmpName;
  std::string mPreNam1Label = "NAM 1";
  std::string mPreNam2Label = "NAM 2";
  std::string mHeaderTooltip;
  EVoLumSection mExpandedSection = EVoLumSection::AMP;

  IRECT mPreRect;
  IRECT mAmpRect;
  // Visible AMP block (180 H, centered inside the 196 H strip rect) - used for
  // hover hit-testing so the hover lift only fires when the cursor is over the
  // block, not the empty whitespace above/below it. Click hit-testing still
  // uses mAmpRect (the full strip) so clicks near the block still register.
  IRECT mAmpBlockRect;

  // Cached auto-shrink result for the rotated spine font size. MeasureText
  // hits GDI/DirectWrite on Windows; caching by (name, maxLen) skips up to
  // 7 measure calls per redraw in the steady state.
  std::string mCachedSpineName;
  float mCachedSpineMaxLen = -1.f;
  float mCachedSpineSize = 0.f;

  // Per-slot motif layers indexed by EVoLumEffectFocus (6 values - one slot
  // is "AMP" and unused here since the AMP block does not use a slot motif).
  // DrawEffectMotif is the most expensive op in this control (recursive
  // fractal art); caching the rendered output to a layer means hover
  // transitions only redraw cheap overlays + frames on top.
  static constexpr size_t kEffectFocusCount = static_cast<size_t>(EVoLumEffectFocus::CHORUS) + 1;
  std::array<ILayerPtr, kEffectFocusCount> mSlotMotifLayers;
  std::array<bool, kEffectFocusCount> mSlotMotifCachedBypass{};
  std::array<int, kEffectFocusCount> mSlotMotifCachedVariant{}; // PITCH sub-mode the cached layer used.
  IRECT mPostRect;

  // Quiet block hit-zones, repopulated each Draw().
  std::vector<IRECT> mSlotNavRects;
  std::vector<IRECT> mSlotToggleRects;
  std::vector<int> mSlotParams;
  std::vector<EVoLumEffectFocus> mSlotFocuses;
  std::vector<EVoLumSection> mSlotSection;
  IRECT mPreHeaderRect;
  IRECT mPostHeaderRect;
  IRECT mPreLockRect;
  IRECT mPostLockRect;
  IRECT mPreStoreRect;
  IRECT mPostStoreRect;
  int mHoveredSlot = -1;
  bool mAmpHovered = false;
  bool mPreLockHovered = false;
  bool mPostLockHovered = false;
  bool mPreStoreHovered = false;
  bool mPostStoreHovered = false;
};
