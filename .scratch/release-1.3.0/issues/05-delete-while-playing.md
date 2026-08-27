# What happens when we delete content that is currently playing?

Type: grilling
Status: open
Blocked by: 04

## Question

Deleting a custom amp that is loaded updates the UI toward a factory amp and clears the custom index, but does not reload/clear the live model — DSP can keep the dead capture. Pedal delete updates the registry only; a live PRE capture can keep playing. Pack import/delete and MIDI preset recall make this more common.

This wants the same removal transaction as [How does the content library survive two writers?](04-two-writer-library.md) (identity, not row position; graph swap like cab-source changes).

What does 1.3.0 guarantee when the user deletes (or a Pack import replaces) the amp, IR, pedal, or preset that is currently sounding?
