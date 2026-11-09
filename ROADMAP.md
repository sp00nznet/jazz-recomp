# Roadmap

## Next
- The playfield: why the start address settles 97 rows into a page, and why
  most foreground tiles are missing (the latch blitter from the tile cache).
- Play Diamondus 1 through.
- Crt.Delay in real time (DelayCnt is pinned at its maximum).

## Then
- Sound: answer the MASI driver interface in the host (Sound Blaster), or keep
  it off.
- Setup script, `launcher/launcher.json`, a netlab project file.

## Toolkit (pcrecomp, win16-dos)
- NE lifter: a seeded function's branch to an offset that is not a function
  start adds the function's start twice (seg001_3D6B, 0x7B10).
- The lifted `iret` returns without popping its frame: fine for the host
  calling a handler, wrong for a guest `pushf; call far` into a lifted handler.
