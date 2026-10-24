// Includes the Linux leaf headers so the compile check parses them:
// IPlug_include_in_plug_hdr.h (EXPORT, BUNDLE_ID), IGraphicsLinux.h,
// the run loop, fontconfig lookup, and the GLX loader.

#define APP_API
#include "IPlug_include_in_plug_hdr.h"
#include "Linux/IPlugLinuxRunLoop.h"
#include "IGraphicsLinuxFont.h"
#include "IGraphicsLinuxGL.h"

extern "C" EXPORT const char* volum_linux_leaf_bundle()
{
  return BUNDLE_ID[0] ? BUNDLE_ID : APP_GROUP_ID;
}
