#pragma once

// Hotspot action-code layout for the custom-content overlay.
//
// Most hotspots in that overlay are per-row: "this family's base plus the row
// index", decoded on click by testing which range the code falls in. The families
// used to sit 100 apart, which silently breaks at 100 rows. The overwrite icon on
// row 100 encodes as 500 + 100 = 600, which is the rename base, and
// HandleManageAction tests rename before overwrite - so clicking overwrite on the
// 101st preset renamed the first one, and the trash icon on that row opened the
// first IR's shaping editor instead of deleting anything. Row bodies were capped at
// 256, so rows past that could not be clicked at all. A hundred presets or IRs is
// an ordinary library, not an edge case.
//
// Kept free of IControl so the one property that matters can be tested without a
// graphics host: no row index a real library can reach may encode as a code
// belonging to another family. See tests/test_volum_overlay_action_codes.cpp.

namespace volum::custom
{

// Row indices per family. Far beyond any library a user can build by hand, and
// small enough that the whole layout stays well inside int.
inline constexpr int kActionStride = 1 << 16;

// Families start above every fixed action code in the overlay (close, add, save,
// the cab-name chips at 70, the art swatches at 80).
inline constexpr int kActionFamilyBase = 1 << 20;

enum class ActionFamily
{
  Row = 0, // manage row body
  Overwrite, // manage inline overwrite icon (presets)
  Rename, // manage inline pen icon
  Delete, // manage inline trash icon
  IrCfg, // manage inline gear icon (IRs)
  FileSpeaker, // builder capture -> cab slot
  FileChannel, // builder capture -> channel
  FileRemove, // builder capture removal
  Popup, // popup-local codes
  kCount
};

inline constexpr int ActionBase(ActionFamily family)
{
  return kActionFamilyBase + static_cast<int>(family) * kActionStride;
}

inline constexpr int ActionCode(ActionFamily family, int index)
{
  return ActionBase(family) + index;
}

inline constexpr bool InActionFamily(int code, ActionFamily family)
{
  return code >= ActionBase(family) && code < ActionBase(family) + kActionStride;
}

inline constexpr int ActionIndex(int code, ActionFamily family)
{
  return code - ActionBase(family);
}

} // namespace volum::custom
