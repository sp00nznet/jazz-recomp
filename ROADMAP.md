# Roadmap

## Next
- Background areas that draw black at the start of Diamondus 1 (behind the
  first hill): a layer or palette effect not yet answered.
- Play Diamondus 1 through.
- Crt.Delay in real time (DelayCnt is pinned at its maximum).

## Then
- Sound: answer the MASI driver interface in the host (Sound Blaster), or keep
  it off.
- Build and publish the recomp-launcher installer (the workflow is manual
  while the repo is private).

## House style still missing (REPO_RULES sections 9 and 10)
- A conformance harness: scripted headless runs with a pass/fail count
  against fixed milestones (intro, title, menu, episodes, level), SKIP
  without game data, wired as the netlab QA, and its figure in the README.
- The house CLI: `--headless --record out.mp4` (offscreen frames to ffmpeg)
  in place of the `JAZZ_*` environment variables.
- Paths not compiled in (`game\`, `work\mem_image.bin`), so a farm build runs
  on a test VM.
- Merge `pcrecomp-ne-jazz` (on `pcrecomp-ne-nativemap`) into pcrecomp `main` (through a PR) and pin `main`.

## Toolkit (pcrecomp, pcrecomp-ne-nativemap)
- The native code map misses six functions IDA found (seg 9: 375F, 38F2;
  seg 10: 05BD, 05E5, 05EB, 1701); they are seeded by hand. Without them the
  game stops with Runtime error 105 before the intro.
- NE lifter: a seeded function's branch to an offset that is not a function
  start adds the function's start twice (seg001_3D6B, 0x7B10).
- The lifted `iret` returns without popping its frame: fine for the host
  calling a handler, wrong for a guest `pushf; call far` into a lifted handler.
