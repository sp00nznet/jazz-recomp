# Roadmap

## Next
- The playfield: why the start address settles 97 rows into a page, and why
  most foreground tiles are missing (the latch blitter from the tile cache).
- Play Diamondus 1 through.
- Crt.Delay in real time (DelayCnt is pinned at its maximum).

## Then
- Sound: answer the MASI driver interface in the host (Sound Blaster), or keep
  it off.
- Build and publish the recomp-launcher installer (the workflow is manual
  while the repo is private).

## House style still missing (REPO_RULES sections 9 and 10)
- Lift without IDA: the default build path must use only our own tools. A
  lift with no code map today linear-sweeps 7,595 "functions" and the game
  stops with Runtime error 105. That is a pcrecomp gap (the NE front end's
  code/data split), to fix there and score against IDA's map.
- A conformance harness: scripted headless runs with a pass/fail count
  against fixed milestones (intro, title, menu, episodes, level), SKIP
  without game data, wired as the netlab QA, and its figure in the README.
- The house CLI: `--headless --record out.mp4` (offscreen frames to ffmpeg)
  in place of the `JAZZ_*` environment variables.
- Paths not compiled in (`game\`, `work\mem_image.bin`), so a farm build runs
  on a test VM.
- Merge `win16-dos` into pcrecomp `main` (through a PR) and pin `main`.

## Toolkit (pcrecomp, win16-dos)
- NE lifter: a seeded function's branch to an offset that is not a function
  start adds the function's start twice (seg001_3D6B, 0x7B10).
- The lifted `iret` returns without popping its frame: fine for the host
  calling a handler, wrong for a guest `pushf; call far` into a lifted handler.
