# How does a current build read a future DAW chunk?

Type: grilling
Status: open

## Question

The current chunk reader consumes exactly this build’s `kNumParams` param doubles, then the per-amp tail (`Unserialization.cpp`). A later 1.3.0 build that appends Chorus (or any) params will write a longer list. Opening that project in 1.2.1/1.2.2 misaligns the tail — the same shape as the 1.2.0 “every project resets” bug. The id-tail JSON is already skippable; unknown **param doubles** are not.

This cannot fire until 1.3.0 writes extra params, and it has to land **before** that write. Chorus is the first planned param bump.

What skip/version rule does 1.3.0 ship so:

- 1.2.2 can open a future chunk without scrambling the rig (or fail cleanly),
- and 1.3.0 still reads every older chunk?

Recommend: reader-first, writers second. Do not add Chorus params until this lands.
