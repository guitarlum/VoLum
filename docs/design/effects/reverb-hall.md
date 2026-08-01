# Hall Reverb

Staging Hall uses the good Cathedral-ish iteration-2 recipe under the simple `Hall` label. It is not intended to be an extreme cathedral preset; it is the larger, smoother Hall flavor that sounded best in review.

Visible controls:

- `Mix`, `Decay`, `Tone`, `Pre-Dly`.

The Hall sub-mode UI is not exposed in staging because the non-Cathedral variants did not add useful separation.

## Structure

An eight-line feedback delay network with a Hadamard mixing matrix, fed through a
four-stage allpass diffuser. Each channel then gets one further allpass, longer than the
shared ones and mutually prime with them, which builds the early field and decorrelates
left from right. The delay lines are injected with alternating sign so they are not all
excited identically.

Up to 1.2.1 there was no diffuser: the dry signal went straight into all eight lines and
the output read only the far end of each. The wet signal was therefore silent for the
first 52.7 ms, then delivered eight discrete impulses spread over 52.7-151.3 ms - the
"slapback" a user reported hearing on muted notes, most obvious at low Mix where nothing
masks it. `Pre-Dly` sat on top of that floor rather than setting the onset, so it could
only ever make the gap longer.

Line lengths, loop gain, the Hadamard matrix and the tone curve are unchanged, so the
tail is the same one. What changed is everything before it.
