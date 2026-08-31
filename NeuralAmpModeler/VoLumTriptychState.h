#pragma once

enum class EVoLumSection { PRE, AMP, POST };
// CHORUS is appended (not inserted before DELAY) so the numeric values of the
// existing focus targets stay put; POST draw order comes from the slot/card
// tables, not from this enum.
enum class EVoLumEffectFocus { PITCH, COMP, PRE_NAM1, PRE_NAM2, AMP, DELAY, REVERB, TREMOLO, CHORUS };
