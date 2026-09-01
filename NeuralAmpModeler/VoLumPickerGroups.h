#pragma once

// Factory / User accordion: first-open rules + session memory. Shared by the
// PLAY Add picker, the Settings MIDI picker, and the BUILD preset menu.

namespace volum
{

struct PickerGroupSession
{
  bool initialized = false;
  bool factoryOpen = false;
  bool userOpen = false;
};

// First open: only one section exists → that section starts open. Both exist →
// both start collapsed. Later opens keep whatever the player toggled.
inline void InitPickerGroups(PickerGroupSession& session, bool hasFactory, bool hasUser)
{
  if (session.initialized)
    return;
  session.initialized = true;
  if (hasFactory && hasUser)
  {
    session.factoryOpen = false;
    session.userOpen = false;
    return;
  }
  session.factoryOpen = hasFactory;
  session.userOpen = hasUser;
}

inline void TogglePickerGroup(PickerGroupSession& session, bool factory)
{
  if (factory)
    session.factoryOpen = !session.factoryOpen;
  else
    session.userOpen = !session.userOpen;
  session.initialized = true;
}

} // namespace volum
