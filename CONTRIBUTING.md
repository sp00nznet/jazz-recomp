# Contributing

Issues and pull requests are welcome.

## The one rule

**Nothing from the game goes in the repository.** No game files, no lifted C
or memory image (`work/`), no disassembly listings, nothing generated from
`FILE0001.EXE`, and no sound-card files `make_soundcrd.py` writes into `game/`. The tool ships; its output is made on each
person's machine from their own copy. `.gitignore` covers the usual paths;
check `git status` before you commit anyway. Screenshots of the game running
are fine.

## Before a pull request

1. Lift from clean and build (pcrecomp on `pcrecomp-ne-jazz`):
   ```
   py -3 tools\relift.py
   cmake --build build
   ```
   The lift must end with 0 unresolved calls.
2. There is no conformance harness yet ([ROADMAP.md](ROADMAP.md)). Script a
   headless run through the intro, title, menu and into Diamondus 1
   ([docs/testing.md](docs/testing.md), `JAZZ_HEADLESS=1`) and say in the PR
   what you checked.

## Where a fix belongs

- A bug in the NE lifter, the code map or the Win16 host (a missing or wrong
  KERNEL call) belongs in
  [pcrecomp](https://github.com/sp00nznet/pcrecomp), against
  `pcrecomp-ne-jazz`, not here.
- Something only this game does goes in `runtime/` (the DOS machine) or `tools/relift.py`,
  with a comment that says what it fixes and why.
- A wall you got past goes in `docs/lifting.md`: what it looked like, what it
  was, what moved it.
