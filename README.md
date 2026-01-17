# bog

A c translation of danja's amazing [dogalog](https://github.com/danja/dogalog) into c so that it can be used by [psnd](https://github.com/shakfu/psnd) to drive MIDI events, or used on its own.

## MIDI backend

Build the project with CMake to produce a `bog_midi` executable that streams scheduler events to any RtMidi-compatible output (CoreMIDI, ALSA, Windows MM, or a virtual port). List the available ports:

```
bog_midi --list
```

Run a Bog program at a given tempo:

```
bog_midi examples/basic.pl 120 0   # (program bpm midi-port-index)
```

If no physical ports are found, a virtual port named `bog` is created automatically so you can route it with your OS MIDI patch bay.
