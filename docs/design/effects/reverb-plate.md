# Plate Reverb

Plate is restored to the dev-era Dattorro-style plate path. The Steel/Brass/Copper experiment from the WIP branch is removed from staging.

Visible controls:

- `Mix`, `Decay`, `Tone`, `Pre-Dly`.

The staging goal is a stable known-good plate before future mode-specific tuning resumes.

## Structure

Dattorro's plate: an input diffuser feeding a figure-of-eight tank whose two halves each
run decay diffusion 1, a delay, decay diffusion 2 and a second delay, with the seven-tap
output network reading across both halves for each channel.

Until 1.2.1 only the first half of each tank half existed - one allpass and one delay -
and the output was a single tap. Two consequences, both audible. Dattorro's asymmetry
lives entirely in the stage that was missing, so the two halves came out the same total
length (672 + 4453 = 908 + 4217 = 5125 samples, 172.2 ms at 29761 Hz): both channels
fired together, every 172.2 ms, as a periodic train rather than a tail. And with one tap
per channel the output was nearly mono - measured L/R correlation 0.93.

The tap gain is set so the completed network is as loud as the single tap it replaced.
